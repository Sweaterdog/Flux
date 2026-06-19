#include "transpiler.h"
#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>

// ============================================================================
// Flux AOT Transpiler — Flux AST -> C++ source code
//
// Design: Maps Flux types directly to C++ types.
//   - int/long/float/bool/string/char/byte -> C++ equivalents
//   - Lists -> std::vector<T>
//   - Classes -> C++ structs with methods
//   - Enums -> namespaces with static const int members
//   - Lambdas -> C++ lambdas with [&] capture
//   - new ClassName(args) -> IIFE constructing the object
//   - super.init() -> ParentClass::init()
//   - String interpolation -> ostringstream IIFE
//   - Casts -> overloaded conversion helpers
// ============================================================================

Transpiler::Transpiler() : indentLevel(0), tempVarCounter(0) {}

std::string Transpiler::indent() {
    return std::string(indentLevel * 4, ' ');
}

void Transpiler::pushIndent() { indentLevel++; }
void Transpiler::popIndent() { if (indentLevel > 0) indentLevel--; }

std::string Transpiler::tempVar() {
    return "_t" + std::to_string(tempVarCounter++);
}

void Transpiler::requireHeader(const std::string& h) {
    usedHeaders.insert(h);
}

std::string Transpiler::fluxTypeToC(const std::string& fluxType) {
    if (fluxType == "int") return "int32_t";
    if (fluxType == "long") return "int64_t";
    if (fluxType == "float") return "double";
    if (fluxType == "string") return freestandingMode ? "FluxString" : "std::string";
    if (fluxType == "bool") return "bool";
    if (fluxType == "char") return "char";
    if (fluxType == "byte") return "uint8_t";
    if (fluxType == "void" || fluxType == "nil") return "void";
    if (fluxType == "object") return "FluxObject";
    if (fluxType == "color32") return "uint32_t";
    if (fluxType == "word") return "uint16_t";

    // Pointer types: byte*, int*, etc.
    if (fluxType.size() > 1 && fluxType.back() == '*') {
        std::string inner = fluxType.substr(0, fluxType.size() - 1);
        return fluxTypeToC(inner) + "*";
    }

    // Fixed-size array types: int[256], byte[1024]
    auto bracket = fluxType.find('[');
    if (bracket != std::string::npos) {
        auto closeBracket = fluxType.find(']');
        if (closeBracket != std::string::npos && closeBracket > bracket + 1) {
            std::string inner = fluxType.substr(0, bracket);
            std::string size = fluxType.substr(bracket + 1, closeBracket - bracket - 1);
            return fluxTypeToC(inner) + "[" + size + "]";
        }
        // Dynamic array: int[]
        if (closeBracket == bracket + 1) {
            std::string inner = fluxType.substr(0, bracket);
            return "std::vector<" + fluxTypeToC(inner) + ">";
        }
    }

    // func type -> function pointer in freestanding, std::function in hosted
    if (fluxType == "func") return freestandingMode ? "FuncPtr" : "std::function<void()>";

    // Known class name
    if (classNames.count(fluxType)) return fluxType;

    // Standard library types that exist as C++ structs in stubs
    static const std::set<std::string> stdlibTypes = {
        "Window", "Timer", "Socket", "Map", "Stack", "Queue",
        "HttpClient", "Response", "FluxObject", "Video"
    };
    if (stdlibTypes.count(fluxType)) return fluxType;

    // Generic list types: list<int>, list<string>, list<ClassName>
    if (fluxType.rfind("list", 0) == 0 || fluxType.rfind("List", 0) == 0) {
        // Extract inner type from list<X> or List<X>
        auto lt = fluxType.find('<');
        auto gt = fluxType.rfind('>');
        if (lt != std::string::npos && gt != std::string::npos) {
            std::string inner = fluxType.substr(lt + 1, gt - lt - 1);
            if (freestandingMode) return "FluxList<" + fluxTypeToC(inner) + ">";
            return "std::vector<" + fluxTypeToC(inner) + ">";
        }
        // bare "list" or "List"
        if (freestandingMode) return "FluxList<int32_t>";
        return "std::vector<int32_t>";
    }

    return "auto";
}

// ============================================================================
// Pre-scan: collect class and enum names before emitting code
// ============================================================================

static void prescanDeclarations(ASTNodePtr node,
                                std::set<std::string>& classNames,
                                std::set<std::string>& enumNames) {
    if (!node) return;
    if (node->nodeType == NodeType::PROGRAM) {
        auto prog = std::dynamic_pointer_cast<ProgramNode>(node);
        for (auto& d : prog->declarations)
            prescanDeclarations(d, classNames, enumNames);
    } else if (node->nodeType == NodeType::CLASS_DECL) {
        auto cls = std::dynamic_pointer_cast<ClassDeclNode>(node);
        classNames.insert(cls->name);
    } else if (node->nodeType == NodeType::ENUM_DECL) {
        auto en = std::dynamic_pointer_cast<EnumDeclNode>(node);
        enumNames.insert(en->name);
    } else if (node->nodeType == NodeType::STRUCT_DECL) {
        auto st = std::dynamic_pointer_cast<StructDeclNode>(node);
        classNames.insert(st->name);  // Treat structs like classes for type resolution
    } else if (node->nodeType == NodeType::EXPORT_STMT) {
        auto exp = std::dynamic_pointer_cast<ExportStmtNode>(node);
        if (exp && exp->declaration) {
            prescanDeclarations(exp->declaration, classNames, enumNames);
        }
    }
}

// ============================================================================
// Transpile: AST -> C++ source string
// ============================================================================

std::string Transpiler::transpile(ASTNodePtr program) {
    // Reset state
    header.str(""); forward.str(""); body.str(""); functions.str("");
    usedHeaders.clear();
    importedModules.clear();
    classNames.clear();
    enumNames.clear();
    listVars.clear();
    objectVars.clear();
    pointerVars.clear();
    currentParentClass.clear();
    needsFluxObject = false;
    indentLevel = 0;
    tempVarCounter = 0;
    inTopLevel = true;  // Start in top-level (main) context

    // Pre-scan for class/enum names
    prescanDeclarations(program, classNames, enumNames);

    if (!freestandingMode) {
        // Always-needed headers for hosted mode
        requireHeader("<iostream>");
        requireHeader("<string>");
        requireHeader("<vector>");
        requireHeader("<cstdint>");
        requireHeader("<functional>");
        requireHeader("<sstream>");
        requireHeader("<stdexcept>");
        requireHeader("<cmath>");
        requireHeader("<cstdlib>");
        requireHeader("<algorithm>");
        requireHeader("<limits>");
        requireHeader("<map>");
        requireHeader("<memory>");
    }

    // Emit the program body into `body`
    emitNode(program, body);

    // Add headers required by imported stdlib modules
    // (must be after emitNode so importedModules is populated)
    // Skip all std library headers in freestanding mode
    if (!freestandingMode) {
        if (importedModules.count("std.io")) {
            requireHeader("<fstream>");
            requireHeader("<filesystem>");
        }
        if (importedModules.count("std.sys")) {
            requireHeader("<thread>");
            requireHeader("<mutex>");
            requireHeader("<chrono>");
            requireHeader("<csignal>");
            requireHeader("<future>");
        }
        if (importedModules.count("std.time")) {
            requireHeader("<chrono>");
            requireHeader("<ctime>");
            requireHeader("<iomanip>");
        }
        if (importedModules.count("std.collections")) {
            requireHeader("<stack>");
            requireHeader("<queue>");
            requireHeader("<deque>");
        }
        if (importedModules.count("std.regex")) {
            requireHeader("<regex>");
        }
        if (importedModules.count("std.os")) {
            requireHeader("<cstdio>");
            requireHeader("<unistd.h>");
        }
        if (importedModules.count("std.graphics")) {
            requireHeader("<SDL2/SDL.h>");
            requireHeader("<SDL2/SDL_ttf.h>");
            requireHeader("<SDL2/SDL_image.h>");
            requireHeader("<GLFW/glfw3.h>");
            requireHeader("<GL/gl.h>");
            requireHeader("<GL/glu.h>");
        }
        if (importedModules.count("std.audio")) {
            requireHeader("<SDL2/SDL.h>");
            requireHeader("<SDL2/SDL_mixer.h>");
        }
        if (importedModules.count("std.video")) {
            requireHeader("<SDL2/SDL.h>");
            requireHeader("<SDL2/SDL_mixer.h>");
        }
        if (importedModules.count("std.net")) {
            requireHeader("<sys/socket.h>");
            requireHeader("<netinet/in.h>");
            requireHeader("<arpa/inet.h>");
            requireHeader("<netdb.h>");
            requireHeader("<unistd.h>");
            requireHeader("<cstring>");
        }
        if (importedModules.count("std.crypto")) {
            requireHeader("<cstring>");
        }
    }

    // Assemble the final C++ source
    std::stringstream output;
    output << "// Auto-generated by Flux AOT Compiler\n";
    output << "// Do not edit — regenerate from .flux source\n\n";

    if (freestandingMode) {
        // ======================== FREESTANDING MODE ========================
        // Kernel/OS compilation: no standard library, bare-metal types only
        output << R"(
// ====================== Freestanding Runtime ======================
// Bare-metal environment — no C/C++ standard library available

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// Prevent C++ name mangling for kernel entry points
#ifdef __cplusplus
extern "C" {
#endif

// Basic type aliases
using int32_t = __INT32_TYPE__;
using int64_t = __INT64_TYPE__;
using uint8_t = __UINT8_TYPE__;
using uint16_t = __UINT16_TYPE__;
using uint32_t = __UINT32_TYPE__;
using uint64_t = __UINT64_TYPE__;
using size_t = __SIZE_TYPE__;

#ifdef __cplusplus
}
#endif

// Placement new for freestanding C++
inline void* operator new(size_t, void* p) noexcept { return p; }
inline void* operator new[](size_t, void* p) noexcept { return p; }
inline void operator delete(void*, size_t) noexcept {}
inline void operator delete[](void*, size_t) noexcept {}

// Freestanding string class (no heap allocation by default, fixed buffer)
class FluxString {
public:
    static constexpr size_t MAX_LEN = 256;
    char data[MAX_LEN];
    size_t len;

    FluxString() : len(0) { data[0] = 0; }
    FluxString(const char* s) : len(0) {
        if (s) { while (s[len] && len < MAX_LEN - 1) { data[len] = s[len]; len++; } }
        data[len] = 0;
    }
    FluxString(const char* s, size_t n) : len(0) {
        while (len < n && len < MAX_LEN - 1) { data[len] = s[len]; len++; }
        data[len] = 0;
    }

    size_t size() const { return len; }
    size_t length() const { return len; }
    bool empty() const { return len == 0; }
    const char* c_str() const { return data; }
    char& operator[](size_t i) { return data[i]; }
    char operator[](size_t i) const { return data[i]; }

    FluxString& operator=(const char* s) {
        len = 0;
        if (s) { while (s[len] && len < MAX_LEN - 1) { data[len] = s[len]; len++; } }
        data[len] = 0;
        return *this;
    }

    FluxString operator+(const FluxString& other) const {
        FluxString result;
        size_t i = 0;
        while (i < len && i < MAX_LEN - 1) { result.data[i] = data[i]; i++; }
        size_t j = 0;
        while (j < other.len && i < MAX_LEN - 1) { result.data[i] = other.data[j]; i++; j++; }
        result.data[i] = 0; result.len = i;
        return result;
    }

    FluxString& operator+=(const FluxString& other) {
        size_t j = 0;
        while (j < other.len && len < MAX_LEN - 1) { data[len] = other.data[j]; len++; j++; }
        data[len] = 0;
        return *this;
    }
    FluxString& operator+=(const char* s) {
        if (s) {
            while (*s && len < MAX_LEN - 1) { data[len] = *s; len++; s++; }
            data[len] = 0;
        }
        return *this;
    }
    FluxString& operator+=(char c) {
        if (len < MAX_LEN - 1) { data[len] = c; len++; data[len] = 0; }
        return *this;
    }

    bool operator==(const FluxString& other) const {
        if (len != other.len) return false;
        for (size_t i = 0; i < len; i++) { if (data[i] != other.data[i]) return false; }
        return true;
    }
    bool operator!=(const FluxString& other) const { return !(*this == other); }

    int find(const FluxString& sub, size_t pos = 0) const {
        if (sub.len == 0) return (int)pos;
        for (size_t i = pos; i + sub.len <= len; i++) {
            bool match = true;
            for (size_t j = 0; j < sub.len; j++) {
                if (data[i + j] != sub.data[j]) { match = false; break; }
            }
            if (match) return (int)i;
        }
        return -1;
    }

    FluxString substr(size_t pos, size_t count = MAX_LEN) const {
        FluxString result;
        if (pos >= len) return result;
        size_t end = pos + count;
        if (end > len) end = len;
        for (size_t i = pos; i < end; i++) {
            result.data[result.len++] = data[i];
        }
        result.data[result.len] = 0;
        return result;
    }
};

// Use FluxString as std::string replacement in freestanding mode
using string = FluxString;
namespace std { using string = FluxString; }

// Function pointer typedef for 'func' type
using FuncPtr = void(*)(...);
static const FuncPtr nullfunc = nullptr;

// std::initializer_list — provided by compiler even in freestanding mode
#include <initializer_list>

// Forward-declare memory operations (defined later in preamble)
extern "C" {
    void* memset(void* dest, int val, size_t count);
    void* memcpy(void* dest, const void* src, size_t count);
    void* memmove(void* dest, const void* src, size_t count);
}

// Freestanding FluxList<T> — simple dynamic array backed by bump allocator
template <typename T>
class FluxList {
    T* items;
    int32_t len;
    int32_t cap;

    void grow() {
        int32_t newCap = cap == 0 ? 8 : cap * 2;
        // Use placement in heap area — caller must have a heap set up
        extern void* flux_heap_alloc(size_t);
        T* newItems = (T*)flux_heap_alloc(newCap * sizeof(T));
        if (items && len > 0) {
            memcpy(newItems, items, len * sizeof(T));
        }
        items = newItems;
        cap = newCap;
    }
public:
    FluxList() : items(nullptr), len(0), cap(0) {}
    FluxList(std::initializer_list<T> init) : items(nullptr), len(0), cap(0) {
        for (auto& v : init) add(v);
    }

    int32_t size() const { return len; }
    bool empty() const { return len == 0; }

    void add(const T& val) {
        if (len >= cap) grow();
        items[len++] = val;
    }
    void push_back(const T& val) { add(val); }

    T& operator[](int32_t i) { return items[i]; }
    const T& operator[](int32_t i) const { return items[i]; }

    void removeAt(int32_t idx) {
        if (idx < 0 || idx >= len) return;
        for (int32_t i = idx; i < len - 1; i++) items[i] = items[i + 1];
        len--;
    }

    void insertAt(int32_t idx, const T& val) {
        if (idx < 0) idx = 0;
        if (idx > len) idx = len;
        if (len >= cap) grow();
        for (int32_t i = len; i > idx; i--) items[i] = items[i - 1];
        items[idx] = val;
        len++;
    }

    // erase() for std::vector API compatibility
    T* erase(T* pos) {
        int32_t idx = (int32_t)(pos - items);
        removeAt(idx);
        return items + idx;
    }

    void clear() { len = 0; }

    // Iterator support for range-for
    T* begin() { return items; }
    T* end() { return items + len; }
    const T* begin() const { return items; }
    const T* end() const { return items + len; }
};

// Simple heap allocator for FluxList (uses bump allocator from MemoryManager)
static uint8_t* flux_heap_ptr = nullptr;
static size_t flux_heap_remaining = 0;

__attribute__((weak))
void* flux_heap_alloc(size_t bytes) {
    // Align to 8 bytes
    bytes = (bytes + 7) & ~7;
    if (flux_heap_ptr && flux_heap_remaining >= bytes) {
        void* p = flux_heap_ptr;
        flux_heap_ptr += bytes;
        flux_heap_remaining -= bytes;
        return p;
    }
    // If no heap yet, use a static emergency buffer
    static uint8_t emergency[262144];
    static size_t emergency_used = 0;
    if (emergency_used + bytes <= sizeof(emergency)) {
        void* p = &emergency[emergency_used];
        emergency_used += bytes;
        return p;
    }
    return nullptr;  // Out of memory
}

// Alias std::vector for compatibility
namespace std { template<typename T> using vector = FluxList<T>; }

// Minimal memory operations (additional declarations)
extern "C" {
    int memcmp(const void* a, const void* b, size_t count);
    size_t strlen(const char* s);
    char* strcpy(char* dest, const char* src);
    char* strncpy(char* dest, const char* src, size_t n);
    int strcmp(const char* a, const char* b);
    int strncmp(const char* a, const char* b, size_t n);
}

// Freestanding memset/memcpy/etc implementations (weak — can be overridden)
__attribute__((weak))
void* memset(void* dest, int val, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) d[i] = (uint8_t)val;
    return dest;
}

__attribute__((weak))
void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) d[i] = s[i];
    return dest;
}

__attribute__((weak))
void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) { for (size_t i = 0; i < count; i++) d[i] = s[i]; }
    else { for (size_t i = count; i > 0; i--) d[i-1] = s[i-1]; }
    return dest;
}

__attribute__((weak))
int memcmp(const void* a, const void* b, size_t count) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    for (size_t i = 0; i < count; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

__attribute__((weak))
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

__attribute__((weak))
char* strcpy(char* dest, const char* src) {
    size_t i = 0;
    while (src[i]) { dest[i] = src[i]; i++; }
    dest[i] = 0;
    return dest;
}

__attribute__((weak))
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) { dest[i] = src[i]; i++; }
    while (i < n) { dest[i] = 0; i++; }
    return dest;
}

__attribute__((weak))
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

__attribute__((weak))
int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

// I/O port access for x86
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outd(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t ind(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait() {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

// Halt CPU
static inline void hlt() {
    __asm__ volatile("hlt");
}

// Disable/enable interrupts
static inline void cli() {
    __asm__ volatile("cli");
}

static inline void sti() {
    __asm__ volatile("sti");
}

// Freestanding FluxError (simple — no heap)
struct FluxError {
    const char* message;
    FluxError(const char* msg) : message(msg) {}
    FluxError(const FluxString& msg) : message(msg.c_str()) {}
};

// Freestanding number-to-string conversion
static FluxString flux_int_to_string(int64_t val) {
    char buf[24];
    bool neg = val < 0;
    if (neg) val = -val;
    int i = 0;
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    if (neg) buf[i++] = '-';
    FluxString result;
    for (int j = i - 1; j >= 0; j--) result.data[result.len++] = buf[j];
    result.data[result.len] = 0;
    return result;
}

static FluxString flux_to_string(int32_t v) { return flux_int_to_string(v); }
static FluxString flux_to_string(int64_t v) { return flux_int_to_string(v); }
static FluxString flux_to_string(uint32_t v) { return flux_int_to_string((int64_t)v); }
static FluxString flux_to_string(bool v) { return v ? FluxString("true") : FluxString("false"); }
static FluxString flux_to_string(const FluxString& v) { return v; }
static FluxString flux_to_string(const char* v) { return FluxString(v); }
static FluxString flux_to_string(char v) { FluxString s; s.data[0] = v; s.data[1] = 0; s.len = 1; return s; }

// Freestanding cast helpers
static int32_t flux_to_int(int32_t v) { return v; }
static int32_t flux_to_int(int64_t v) { return (int32_t)v; }
static int32_t flux_to_int(uint32_t v) { return (int32_t)v; }
static int32_t flux_to_int(uint64_t v) { return (int32_t)v; }
static int32_t flux_to_int(uint8_t v) { return (int32_t)v; }
static int32_t flux_to_int(uint16_t v) { return (int32_t)v; }
static int32_t flux_to_int(double v) { return (int32_t)v; }
static int32_t flux_to_int(float v) { return (int32_t)v; }
static int32_t flux_to_int(bool v) { return v ? 1 : 0; }
static int32_t flux_to_int(const FluxString& s) {
    // Simple atoi for freestanding
    int32_t result = 0; bool neg = false; size_t i = 0;
    if (s.len > 0 && s.data[0] == '-') { neg = true; i = 1; }
    for (; i < s.len; i++) {
        if (s.data[i] >= '0' && s.data[i] <= '9')
            result = result * 10 + (s.data[i] - '0');
        else break;
    }
    return neg ? -result : result;
}
static int64_t flux_to_long(int32_t v) { return (int64_t)v; }
static int64_t flux_to_long(int64_t v) { return v; }
static int64_t flux_to_long(uint32_t v) { return (int64_t)v; }
template<typename T>
static int64_t flux_to_long(T* v) { return (int64_t)(uintptr_t)v; }
static bool flux_to_bool(int32_t v) { return v != 0; }
static bool flux_to_bool(bool v) { return v; }

// Freestanding string helper functions
static FluxString flux_substring(const FluxString& s, int32_t start, int32_t len = -1) {
    if (start < 0) start = 0;
    if (start >= (int32_t)s.len) return FluxString();
    size_t count = (len < 0) ? (s.len - start) : (size_t)len;
    if (start + count > s.len) count = s.len - start;
    return FluxString(s.data + start, count);
}

static int32_t flux_indexOf(const FluxString& s, const FluxString& sub) {
    return s.find(sub);
}

static bool flux_contains(const FluxString& s, const FluxString& sub) {
    return s.find(sub) >= 0;
}

static bool flux_startsWith(const FluxString& s, const FluxString& prefix) {
    if (prefix.len > s.len) return false;
    for (size_t i = 0; i < prefix.len; i++) {
        if (s.data[i] != prefix.data[i]) return false;
    }
    return true;
}

static bool flux_endsWith(const FluxString& s, const FluxString& suffix) {
    if (suffix.len > s.len) return false;
    size_t offset = s.len - suffix.len;
    for (size_t i = 0; i < suffix.len; i++) {
        if (s.data[offset + i] != suffix.data[i]) return false;
    }
    return true;
}

static FluxString flux_trim(const FluxString& s) {
    size_t start = 0, end = s.len;
    while (start < end && (s.data[start] == ' ' || s.data[start] == '\t' || s.data[start] == '\n' || s.data[start] == '\r')) start++;
    while (end > start && (s.data[end-1] == ' ' || s.data[end-1] == '\t' || s.data[end-1] == '\n' || s.data[end-1] == '\r')) end--;
    return FluxString(s.data + start, end - start);
}

static FluxString flux_toUpper(const FluxString& s) {
    FluxString result = s;
    for (size_t i = 0; i < result.len; i++) {
        if (result.data[i] >= 'a' && result.data[i] <= 'z') result.data[i] -= 32;
    }
    return result;
}

static FluxString flux_toLower(const FluxString& s) {
    FluxString result = s;
    for (size_t i = 0; i < result.len; i++) {
        if (result.data[i] >= 'A' && result.data[i] <= 'Z') result.data[i] += 32;
    }
    return result;
}

static FluxString flux_replace(const FluxString& s, const FluxString& from, const FluxString& to) {
    FluxString result;
    size_t i = 0;
    while (i < s.len) {
        bool match = true;
        if (i + from.len <= s.len) {
            for (size_t j = 0; j < from.len; j++) {
                if (s.data[i + j] != from.data[j]) { match = false; break; }
            }
        } else { match = false; }
        if (match && from.len > 0) {
            for (size_t j = 0; j < to.len && result.len < FluxString::MAX_LEN - 1; j++)
                result.data[result.len++] = to.data[j];
            i += from.len;
        } else {
            if (result.len < FluxString::MAX_LEN - 1)
                result.data[result.len++] = s.data[i];
            i++;
        }
    }
    result.data[result.len] = 0;
    return result;
}

static char flux_charAt(const FluxString& s, int32_t idx) {
    if (idx < 0 || idx >= (int32_t)s.len) return '\0';
    return s.data[idx];
}

static FluxString flux_reverse_str(const FluxString& s) {
    FluxString result = s;
    for (size_t i = 0; i < result.len / 2; i++) {
        char tmp = result.data[i];
        result.data[i] = result.data[result.len - 1 - i];
        result.data[result.len - 1 - i] = tmp;
    }
    return result;
}

static FluxList<FluxString> flux_split(const FluxString& s, const FluxString& delim) {
    FluxList<FluxString> parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.len; i++) {
        bool atDelim = false;
        if (delim.len > 0 && i + delim.len <= s.len) {
            atDelim = true;
            for (size_t j = 0; j < delim.len; j++) {
                if (s.data[i + j] != delim.data[j]) { atDelim = false; break; }
            }
        }
        if (atDelim || i == s.len) {
            parts.add(FluxString(s.data + start, i - start));
            if (atDelim) i += delim.len - 1;
            start = i + 1;
        }
    }
    return parts;
}

static int32_t len(const FluxString& s) { return (int32_t)s.len; }
template<typename T>
static int32_t len(const FluxList<T>& l) { return l.size(); }

// String concat helpers for mixed types
static FluxString flux_str_concat(const FluxString& a, int32_t b) { return a + flux_to_string(b); }
static FluxString flux_str_concat(const FluxString& a, int64_t b) { return a + flux_to_string(b); }
static FluxString flux_str_concat(const FluxString& a, uint32_t b) { return a + flux_to_string(b); }
static FluxString flux_str_concat(const FluxString& a, bool b) { return a + flux_to_string(b); }
static FluxString flux_str_concat(const FluxString& a, const FluxString& b) { return a + b; }
static FluxString flux_str_concat(int32_t a, const FluxString& b) { return flux_to_string(a) + b; }
static FluxString flux_str_concat(int64_t a, const FluxString& b) { return flux_to_string(a) + b; }
static FluxString flux_str_concat(bool a, const FluxString& b) { return flux_to_string(a) + b; }

// Freestanding min/max/abs (no <algorithm> or <cstdlib>)
template<typename T>
static T flux_min(T a, T b) { return a < b ? a : b; }
template<typename T>
static T flux_max(T a, T b) { return a > b ? a : b; }
static int32_t flux_abs(int32_t a) { return a < 0 ? -a : a; }
static int64_t flux_abs(int64_t a) { return a < 0 ? -a : a; }
namespace std {
    template<typename T> T min(T a, T b) { return a < b ? a : b; }
    template<typename T> T max(T a, T b) { return a > b ? a : b; }
    template<typename T> T abs(T a) { return a < 0 ? -a : a; }
}

// flux_to_float for freestanding
static double flux_to_float(int32_t v) { return (double)v; }
static double flux_to_float(int64_t v) { return (double)v; }
static double flux_to_float(double v) { return v; }
static double flux_to_float(uint32_t v) { return (double)v; }
static double flux_to_float(bool v) { return v ? 1.0 : 0.0; }

// FluxString lastIndexOf
static int32_t flux_lastIndexOf(const FluxString& s, const FluxString& sub) {
    if (sub.len == 0) return (int32_t)s.len;
    int32_t lastPos = -1;
    for (size_t i = 0; i + sub.len <= s.len; i++) {
        bool match = true;
        for (size_t j = 0; j < sub.len; j++) {
            if (s.data[i + j] != sub.data[j]) { match = false; break; }
        }
        if (match) lastPos = (int32_t)i;
    }
    return lastPos;
}

// Freestanding print (no-op in kernel, override in console driver)
__attribute__((weak))
void flux_print(const FluxString&) {}
__attribute__((weak))
void flux_print(int32_t) {}
__attribute__((weak))
void flux_print(const char*) {}

// Freestanding panic (halt CPU on panic)
[[noreturn]] static void flux_panic(const char* msg) {
    // In kernel, we can't do much — just halt
    (void)msg;
    cli();
    for (;;) hlt();
}

// ===================== End Freestanding Runtime ======================

)";
    } else {
        // ======================== HOSTED MODE ========================
        // Normal compilation with full C++ standard library

        // Headers
        for (auto& h : usedHeaders) {
            output << "#include " << h << "\n";
        }
        output << "\n";

        // Flux runtime support types
        output << R"(
// ====================== Flux Runtime Support ======================

// Flux exception type (for throw/catch)
struct FluxError {
    std::string message;
    FluxError(const std::string& msg) : message(msg) {}
};

// Helper: convert any type to string for interpolation / cast
static std::string flux_to_string(int32_t v) { return std::to_string(v); }
static std::string flux_to_string(int64_t v) { return std::to_string(v); }
static std::string flux_to_string(double v) {
    std::ostringstream oss; oss << v; return oss.str();
}
static std::string flux_to_string(bool v) { return v ? "true" : "false"; }
static std::string flux_to_string(const std::string& v) { return v; }
static std::string flux_to_string(char v) { return std::string(1, v); }

// Cast helpers for (int), (float), (string), (bool) casts
static int32_t flux_to_int(int32_t v) { return v; }
static int32_t flux_to_int(int64_t v) { return static_cast<int32_t>(v); }
static int32_t flux_to_int(double v) { return static_cast<int32_t>(v); }
static int32_t flux_to_int(bool v) { return v ? 1 : 0; }
static int32_t flux_to_int(const std::string& v) { return std::stoi(v); }

static int64_t flux_to_long(int32_t v) { return static_cast<int64_t>(v); }
static int64_t flux_to_long(int64_t v) { return v; }
static int64_t flux_to_long(double v) { return static_cast<int64_t>(v); }
template<typename T>
static int64_t flux_to_long(T* v) { return (int64_t)(uintptr_t)v; }

static double flux_to_float(int32_t v) { return static_cast<double>(v); }
static double flux_to_float(int64_t v) { return static_cast<double>(v); }
static double flux_to_float(double v) { return v; }
static double flux_to_float(const std::string& v) { return std::stod(v); }
static double flux_to_float(bool v) { return v ? 1.0 : 0.0; }

static bool flux_to_bool(int32_t v) { return v != 0; }
static bool flux_to_bool(double v) { return v != 0.0; }
static bool flux_to_bool(bool v) { return v; }
static bool flux_to_bool(const std::string& v) { return !v.empty(); }

// Flux string concatenation: string + anything -> string
static std::string flux_str_concat(const std::string& a, int32_t b) { return a + std::to_string(b); }
static std::string flux_str_concat(const std::string& a, int64_t b) { return a + std::to_string(b); }
static std::string flux_str_concat(const std::string& a, double b) { return a + flux_to_string(b); }
static std::string flux_str_concat(const std::string& a, bool b) { return a + flux_to_string(b); }
static std::string flux_str_concat(const std::string& a, const std::string& b) { return a + b; }
static std::string flux_str_concat(int32_t a, const std::string& b) { return std::to_string(a) + b; }
static std::string flux_str_concat(int64_t a, const std::string& b) { return std::to_string(a) + b; }
static std::string flux_str_concat(double a, const std::string& b) { return flux_to_string(a) + b; }
static std::string flux_str_concat(bool a, const std::string& b) { return flux_to_string(a) + b; }

// Print functions
static void flux_print(const std::string& s) { std::cout << s << std::endl; }
static void flux_print(int32_t i) { std::cout << i << std::endl; }
static void flux_print(int64_t l) { std::cout << l << std::endl; }
static void flux_print(double d) {
    std::ostringstream oss; oss << d; std::cout << oss.str() << std::endl;
}
static void flux_print(bool b) { std::cout << (b ? "true" : "false") << std::endl; }

// Print without newline
static void print_raw(const std::string& s) { std::cout << s; }
static void print_raw(int32_t i) { std::cout << i; }
static void print_raw(double d) { std::cout << d; }
static void print_raw(bool b) { std::cout << (b ? "true" : "false"); }

// Built-in len()
static int32_t len(const std::string& s) { return (int32_t)s.length(); }
template<typename T>
static int32_t len(const std::vector<T>& v) { return (int32_t)v.size(); }

// Built-in typeof()
static std::string flux_typeof(int32_t) { return "int"; }
static std::string flux_typeof(int64_t) { return "long"; }
static std::string flux_typeof(double) { return "float"; }
static std::string flux_typeof(bool) { return "bool"; }
static std::string flux_typeof(const std::string&) { return "string"; }

// Equality helpers for =num= and =word= operators
template<typename A, typename B>
static bool flux_num_equal(A a, B b) {
    return (double)a == (double)b;
}
template<typename A, typename B>
static bool flux_word_equal(A a, B b) {
    return flux_to_string(a) == flux_to_string(b);
}

// Lerp helper
static double flux_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

// ===================== String Method Helpers ======================

static std::string flux_substring(const std::string& s, int start, int len = -1) {
    if (start < 0) start = 0;
    if (start >= (int)s.size()) return "";
    if (len < 0) return s.substr(start);
    return s.substr(start, len);
}

static int flux_indexOf(const std::string& s, const std::string& sub) {
    auto pos = s.find(sub);
    return pos == std::string::npos ? -1 : (int)pos;
}

static bool flux_contains(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

static bool flux_startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool flux_endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::vector<std::string> flux_split(const std::string& s, const std::string& delim) {
    std::vector<std::string> result;
    if (delim.empty()) {
        for (char c : s) result.push_back(std::string(1, c));
        return result;
    }
    size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        result.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    result.push_back(s.substr(start));
    return result;
}

static std::string flux_trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static std::string flux_toUpper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = toupper(c);
    return r;
}

static std::string flux_toLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = tolower(c);
    return r;
}

static std::string flux_replace(const std::string& s, const std::string& oldStr, const std::string& newStr) {
    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(oldStr, pos)) != std::string::npos) {
        result.replace(pos, oldStr.length(), newStr);
        pos += newStr.length();
    }
    return result;
}

static std::string flux_charAt(const std::string& s, int idx) {
    if (idx < 0 || idx >= (int)s.size()) return "";
    return std::string(1, s[idx]);
}

static std::string flux_reverse_str(const std::string& s) {
    std::string r = s;
    std::reverse(r.begin(), r.end());
    return r;
}

// ===================== End Runtime Support ======================

)";
    } // end hosted mode runtime

    // Emit std library stubs if imported (only in hosted mode)
    if (!freestandingMode && importedModules.count("std.graphics")) {
        output << R"(
// ====================== std.graphics (SDL2) =======================

struct Window {
    // Use shared_ptr for SDL resources so Window can be passed by value
    // (matches Flux's reference-type object semantics)
    std::shared_ptr<SDL_Window> _window;
    std::shared_ptr<SDL_Renderer> _renderer;
    bool _open = false;
    int width = 0, height = 0;
    std::string backend = "sdl2+glfw";
    std::string _title;

    // Aspect ratio snapping
    bool snapEnabled = false;
    float lockedAspectRatio = 0.0f;

    // GLFW members for 3D rendering
    GLFWwindow* _glfwWindow = nullptr;
    bool _using3D = false;
    static bool _glfw_initialized;

    static bool _sdl_initialized;

    Window() = default;

    Window(const std::string& title, int w, int h) : width(w), height(h), _title(title) {
        if (!_sdl_initialized) {
            SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
            _sdl_initialized = true;
        }
        SDL_Window* win = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h, SDL_WINDOW_SHOWN);
        if (win) {
            _window = std::shared_ptr<SDL_Window>(win, SDL_DestroyWindow);
            SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (ren) _renderer = std::shared_ptr<SDL_Renderer>(ren, SDL_DestroyRenderer);
            _open = true;
        }
    }

    bool isOpen() {
        if (_using3D && _glfwWindow) return !glfwWindowShouldClose(_glfwWindow) && _open;
        return _open;
    }

    void pollEvents() {
        if (_using3D && _glfwWindow) {
            glfwPollEvents();
            if (glfwWindowShouldClose(_glfwWindow)) _open = false;
            int fbW, fbH;
            glfwGetFramebufferSize(_glfwWindow, &fbW, &fbH);
            if (fbW != width || fbH != height) {
                // Enforce aspect ratio if snap is enabled
                if (snapEnabled && lockedAspectRatio > 0.0f) {
                    int newW = fbW;
                    int newH = (int)(newW / lockedAspectRatio);
                    if (newH != fbH) {
                        glfwSetWindowSize(_glfwWindow, newW, newH);
                        fbH = newH;
                    }
                }
                width = fbW; height = fbH; glViewport(0, 0, width, height);
            }
            return;
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) _open = false;
            // Enforce aspect ratio on SDL2 window resize
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                if (snapEnabled && lockedAspectRatio > 0.0f) {
                    int newW = e.window.data1;
                    int newH = (int)(newW / lockedAspectRatio);
                    if (newH != e.window.data2) {
                        SDL_SetWindowSize(_window.get(), newW, newH);
                    }
                    width = newW;
                    height = newH;
                }
            }
        }
    }

    void clear(int r, int g, int b) {
        if (_using3D && _glfwWindow) {
            glClearColor(r/255.0f, g/255.0f, b/255.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }
        if (_renderer) {
            SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, 255);
            SDL_RenderClear(_renderer.get());
        }
    }

    void present() {
        if (_using3D && _glfwWindow) { glfwSwapBuffers(_glfwWindow); return; }
        if (_renderer) SDL_RenderPresent(_renderer.get());
    }

    void close() {
        _open = false;
        if (_using3D && _glfwWindow) {
            glfwDestroyWindow(_glfwWindow);
            _glfwWindow = nullptr;
            _using3D = false;
        }
    }

    void setTitle(const std::string& t) {
        if (_window) SDL_SetWindowTitle(_window.get(), t.c_str());
    }

    void resize(int w, int h) {
        // Enforce aspect ratio if snap is enabled
        if (snapEnabled && lockedAspectRatio > 0.0f) {
            h = (int)(w / lockedAspectRatio);
        }
        if (_using3D && _glfwWindow) {
            glfwSetWindowSize(_glfwWindow, w, h);
            width = w; height = h;
            return;
        }
        if (_window) { SDL_SetWindowSize(_window.get(), w, h); width = w; height = h; }
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void snapAspectRatio(float ratio) {
        if (ratio > 0.0f) {
            snapEnabled = true;
            lockedAspectRatio = ratio;
            // Apply immediately to current window
            int newH = (int)(width / ratio);
            resize(width, newH);
        } else {
            snapEnabled = false;
            lockedAspectRatio = 0.0f;
        }
    }

    void drawPixel(int x, int y, int r, int g, int b, int a = 255) {
        if (_renderer) {
            SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_RenderDrawPoint(_renderer.get(), x, y);
        }
    }

    void drawLine(int x1, int y1, int x2, int y2, int r, int g, int b, int a = 255) {
        if (_renderer) {
            SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_RenderDrawLine(_renderer.get(), x1, y1, x2, y2);
        }
    }

    void drawRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        if (_renderer) {
            SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_Rect rect = {x, y, rw, rh};
            SDL_RenderDrawRect(_renderer.get(), &rect);
        }
    }

    void fillRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        if (_renderer) {
            SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_Rect rect = {x, y, rw, rh};
            SDL_RenderFillRect(_renderer.get(), &rect);
        }
    }

    void drawCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        int x = radius, y = 0, err = 0;
        while (x >= y) {
            SDL_RenderDrawPoint(_renderer.get(), cx + x, cy + y);
            SDL_RenderDrawPoint(_renderer.get(), cx + y, cy + x);
            SDL_RenderDrawPoint(_renderer.get(), cx - y, cy + x);
            SDL_RenderDrawPoint(_renderer.get(), cx - x, cy + y);
            SDL_RenderDrawPoint(_renderer.get(), cx - x, cy - y);
            SDL_RenderDrawPoint(_renderer.get(), cx - y, cy - x);
            SDL_RenderDrawPoint(_renderer.get(), cx + y, cy - x);
            SDL_RenderDrawPoint(_renderer.get(), cx + x, cy - y);
            if (err <= 0) { y++; err += 2 * y + 1; }
            if (err > 0)  { x--; err -= 2 * x + 1; }
        }
    }

    void fillCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        for (int dy = -radius; dy <= radius; dy++) {
            int dx = (int)std::sqrt((double)(radius * radius - dy * dy));
            SDL_RenderDrawLine(_renderer.get(), cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        SDL_RenderDrawLine(_renderer.get(), x1, y1, x2, y2);
        SDL_RenderDrawLine(_renderer.get(), x2, y2, x3, y3);
        SDL_RenderDrawLine(_renderer.get(), x3, y3, x1, y1);
    }

    void setBlendMode(bool enable) {
        if (_renderer) {
            SDL_SetRenderDrawBlendMode(_renderer.get(),
                enable ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
        }
    }

    // ---- Additional Shapes ----

    void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        int minY = std::min({y1, y2, y3}), maxY = std::max({y1, y2, y3});
        for (int y = minY; y <= maxY; y++) {
            std::vector<int> xs;
            auto edge = [&](int ax, int ay, int bx, int by) {
                if (ay == by) return;
                if (y < std::min(ay, by) || y >= std::max(ay, by)) return;
                xs.push_back(ax + (y - ay) * (bx - ax) / (by - ay));
            };
            edge(x1,y1,x2,y2); edge(x2,y2,x3,y3); edge(x3,y3,x1,y1);
            if (xs.size() >= 2) { std::sort(xs.begin(), xs.end()); SDL_RenderDrawLine(_renderer.get(), xs[0], y, xs[1], y); }
        }
    }

    void drawEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        int segs = std::max(32, std::max(rx, ry) * 4);
        for (int i = 0; i < segs; i++) {
            float a1 = 2.0f * M_PI * i / segs, a2 = 2.0f * M_PI * (i+1) / segs;
            SDL_RenderDrawLine(_renderer.get(), cx+(int)(rx*cos(a1)), cy+(int)(ry*sin(a1)),
                               cx+(int)(rx*cos(a2)), cy+(int)(ry*sin(a2)));
        }
    }

    void fillEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        for (int dy = -ry; dy <= ry; dy++) {
            int dx = (int)(rx * sqrt(1.0 - (double)(dy*dy)/(double)(ry*ry)));
            SDL_RenderDrawLine(_renderer.get(), cx-dx, cy+dy, cx+dx, cy+dy);
        }
    }

    void drawRoundedRect(int x, int y, int rw, int rh, int radius,
                         int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        radius = std::min(radius, std::min(rw/2, rh/2));
        SDL_RenderDrawLine(_renderer.get(), x+radius, y, x+rw-radius, y);
        SDL_RenderDrawLine(_renderer.get(), x+radius, y+rh, x+rw-radius, y+rh);
        SDL_RenderDrawLine(_renderer.get(), x, y+radius, x, y+rh-radius);
        SDL_RenderDrawLine(_renderer.get(), x+rw, y+radius, x+rw, y+rh-radius);
        auto arc = [&](int cx, int cy, float sa, float ea) {
            int s = std::max(8, radius);
            for (int i = 0; i < s; i++) {
                float a1 = sa + (ea-sa)*i/s, a2 = sa + (ea-sa)*(i+1)/s;
                SDL_RenderDrawLine(_renderer.get(), cx+(int)(radius*cos(a1)), cy+(int)(radius*sin(a1)),
                    cx+(int)(radius*cos(a2)), cy+(int)(radius*sin(a2)));
            }
        };
        arc(x+radius, y+radius, M_PI, 1.5*M_PI);
        arc(x+rw-radius, y+radius, 1.5*M_PI, 2.0*M_PI);
        arc(x+rw-radius, y+rh-radius, 0, 0.5*M_PI);
        arc(x+radius, y+rh-radius, 0.5*M_PI, M_PI);
    }

    void fillRoundedRect(int x, int y, int rw, int rh, int radius,
                         int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        SDL_SetRenderDrawColor(_renderer.get(), (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        radius = std::min(radius, std::min(rw/2, rh/2));
        SDL_Rect c = {x, y+radius, rw+1, rh-2*radius}; SDL_RenderFillRect(_renderer.get(), &c);
        SDL_Rect t = {x+radius, y, rw-2*radius+1, radius}; SDL_RenderFillRect(_renderer.get(), &t);
        SDL_Rect b2 = {x+radius, y+rh-radius, rw-2*radius+1, radius+1}; SDL_RenderFillRect(_renderer.get(), &b2);
        auto corner = [&](int cx, int cy) {
            for (int dy = -radius; dy <= radius; dy++) {
                int dx = (int)sqrt((double)(radius*radius - dy*dy));
                SDL_RenderDrawLine(_renderer.get(), cx-dx, cy+dy, cx+dx, cy+dy);
            }
        };
        corner(x+radius,y+radius); corner(x+rw-radius,y+radius);
        corner(x+radius,y+rh-radius); corner(x+rw-radius,y+rh-radius);
    }

    // ---- Text Rendering (SDL_ttf) ----

    static bool _ttf_initialized;
    std::map<std::string, TTF_Font*> _fontCache;

    TTF_Font* _getFont(const std::string& path, int sz) {
        std::string key = path + ":" + std::to_string(sz);
        auto it = _fontCache.find(key);
        if (it != _fontCache.end()) return it->second;
        if (!_ttf_initialized) { TTF_Init(); _ttf_initialized = true; }
        TTF_Font* f = TTF_OpenFont(path.c_str(), sz);
        if (f) _fontCache[key] = f;
        return f;
    }

    void drawText(const std::string& text, int x, int y,
                  const std::string& fontPath, int fontSize,
                  int r, int g, int b, int a = 255) {
        if (!_renderer) return;
        TTF_Font* font = _getFont(fontPath, fontSize);
        if (!font) return;
        SDL_Color c = {(uint8_t)r,(uint8_t)g,(uint8_t)b,(uint8_t)a};
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), c);
        if (!s) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(_renderer.get(), s);
        if (tex) { SDL_Rect d = {x,y,s->w,s->h}; SDL_RenderCopy(_renderer.get(), tex, nullptr, &d); SDL_DestroyTexture(tex); }
        SDL_FreeSurface(s);
    }

    std::vector<int32_t> measureText(const std::string& text, const std::string& fontPath, int fontSize) {
        TTF_Font* font = _getFont(fontPath, fontSize);
        int w=0,h=0;
        if (font) TTF_SizeUTF8(font, text.c_str(), &w, &h);
        return {w, h};
    }

    // ---- Image Loading (SDL2_image) ----

    static bool _img_initialized;
    std::map<std::string, SDL_Texture*> _imgCache;

    SDL_Texture* _loadTex(const std::string& path) {
        auto it = _imgCache.find(path);
        if (it != _imgCache.end()) return it->second;
        if (!_img_initialized) { IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG); _img_initialized = true; }
        if (!_renderer) return nullptr;
        SDL_Texture* t = IMG_LoadTexture(_renderer.get(), path.c_str());
        if (t) _imgCache[path] = t;
        return t;
    }

    void drawImage(const std::string& path, int x, int y) {
        SDL_Texture* t = _loadTex(path);
        if (!t || !_renderer) return;
        int w,h; SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        SDL_Rect d = {x,y,w,h}; SDL_RenderCopy(_renderer.get(), t, nullptr, &d);
    }

    void drawImageScaled(const std::string& path, int x, int y, int dw, int dh) {
        SDL_Texture* t = _loadTex(path);
        if (!t || !_renderer) return;
        SDL_Rect d = {x,y,dw,dh}; SDL_RenderCopy(_renderer.get(), t, nullptr, &d);
    }

    std::vector<int32_t> getImageSize(const std::string& path) {
        SDL_Texture* t = _loadTex(path);
        if (!t) return {0,0};
        int w,h; SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        return {w,h};
    }

    // ---- 3D Rendering (GLFW + OpenGL) ----
    void enable3D() {
        if (!_using3D) {
            // Destroy SDL2 window/renderer; create GLFW window
            _renderer.reset();
            _window.reset();
            if (!_glfw_initialized) { glfwInit(); _glfw_initialized = true; }
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            _glfwWindow = glfwCreateWindow(width, height, _title.c_str(), nullptr, nullptr);
            if (_glfwWindow) glfwMakeContextCurrent(_glfwWindow);
            _using3D = true;
        }
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    void disable3D() {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
    }

    void clearDepth() {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void setPerspective(double fov, double aspect, double near, double far) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov, aspect, near, far);
        glMatrixMode(GL_MODELVIEW);
    }

    void setCamera(double eyeX, double eyeY, double eyeZ,
                   double centerX, double centerY, double centerZ,
                   double upX, double upY, double upZ) {
        glLoadIdentity();
        gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
    }

    void pushMatrix() { glPushMatrix(); }
    void popMatrix() { glPopMatrix(); }

    void translate(double x, double y, double z) { glTranslated(x, y, z); }
    void rotate(double angle, double x, double y, double z) { glRotated(angle, x, y, z); }
    void scale(double x, double y, double z) { glScaled(x, y, z); }

    int loadTexture(const std::string& path) {
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) return 0;
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (!converted) return 0;
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
        SDL_FreeSurface(converted);
        return (int)texId;
    }

    void bindTexture(int texId) {
        if (texId > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, (GLuint)texId);
        } else {
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void drawTexturedCube(double size, int texId) {
        float s = (float)(size / 2.0);
        if (texId > 0) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, (GLuint)texId); }
        glBegin(GL_QUADS);
        // Front
        glTexCoord2f(0,0); glVertex3f(-s,-s, s);
        glTexCoord2f(1,0); glVertex3f( s,-s, s);
        glTexCoord2f(1,1); glVertex3f( s, s, s);
        glTexCoord2f(0,1); glVertex3f(-s, s, s);
        // Back
        glTexCoord2f(0,0); glVertex3f( s,-s,-s);
        glTexCoord2f(1,0); glVertex3f(-s,-s,-s);
        glTexCoord2f(1,1); glVertex3f(-s, s,-s);
        glTexCoord2f(0,1); glVertex3f( s, s,-s);
        // Left
        glTexCoord2f(0,0); glVertex3f(-s,-s,-s);
        glTexCoord2f(1,0); glVertex3f(-s,-s, s);
        glTexCoord2f(1,1); glVertex3f(-s, s, s);
        glTexCoord2f(0,1); glVertex3f(-s, s,-s);
        // Right
        glTexCoord2f(0,0); glVertex3f( s,-s, s);
        glTexCoord2f(1,0); glVertex3f( s,-s,-s);
        glTexCoord2f(1,1); glVertex3f( s, s,-s);
        glTexCoord2f(0,1); glVertex3f( s, s, s);
        // Top
        glTexCoord2f(0,0); glVertex3f(-s, s, s);
        glTexCoord2f(1,0); glVertex3f( s, s, s);
        glTexCoord2f(1,1); glVertex3f( s, s,-s);
        glTexCoord2f(0,1); glVertex3f(-s, s,-s);
        // Bottom
        glTexCoord2f(0,0); glVertex3f(-s,-s,-s);
        glTexCoord2f(1,0); glVertex3f( s,-s,-s);
        glTexCoord2f(1,1); glVertex3f( s,-s, s);
        glTexCoord2f(0,1); glVertex3f(-s,-s, s);
        glEnd();
        if (texId > 0) glDisable(GL_TEXTURE_2D);
    }

    // ---- Color & Primitive Methods ----
    void setColor(float r, float g, float b, float a = 1.0f) {
        glColor4f(r, g, b, a);
    }
    void drawQuad(double x1, double y1, double z1,
                  double x2, double y2, double z2,
                  double x3, double y3, double z3,
                  double x4, double y4, double z4) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        GLboolean cullingWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullingWasEnabled) glDisable(GL_CULL_FACE);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f((float)x1,(float)y1,(float)z1);
        glTexCoord2f(1.0f, 0.0f); glVertex3f((float)x2,(float)y2,(float)z2);
        glTexCoord2f(1.0f, 1.0f); glVertex3f((float)x3,(float)y3,(float)z3);
        glTexCoord2f(0.0f, 1.0f); glVertex3f((float)x4,(float)y4,(float)z4);
        glEnd();
        if (cullingWasEnabled) glEnable(GL_CULL_FACE);
    }

    // ---- Input Methods ----
    bool keyPressed(const std::string& key) {
        if (_using3D && _glfwWindow) {
            int k = 0;
            if (key == "W" || key == "w") k = GLFW_KEY_W;
            else if (key == "A" || key == "a") k = GLFW_KEY_A;
            else if (key == "S" || key == "s") k = GLFW_KEY_S;
            else if (key == "D" || key == "d") k = GLFW_KEY_D;
            else if (key == "E" || key == "e") k = GLFW_KEY_E;
            else if (key == "SPACE") k = GLFW_KEY_SPACE;
            else if (key == "SHIFT") k = GLFW_KEY_LEFT_SHIFT;
            else if (key == "CTRL") k = GLFW_KEY_LEFT_CONTROL;
            else if (key == "ESC") k = GLFW_KEY_ESCAPE;
            if (k != 0) return glfwGetKey(_glfwWindow, k) == GLFW_PRESS;
            return false;
        }
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (key == "W" || key == "w") return state[SDL_SCANCODE_W];
        if (key == "A" || key == "a") return state[SDL_SCANCODE_A];
        if (key == "S" || key == "s") return state[SDL_SCANCODE_S];
        if (key == "D" || key == "d") return state[SDL_SCANCODE_D];
        if (key == "E" || key == "e") return state[SDL_SCANCODE_E];
        if (key == "SPACE") return state[SDL_SCANCODE_SPACE];
        if (key == "SHIFT") return state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
        if (key == "CTRL") return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
        if (key == "ESC") return state[SDL_SCANCODE_ESCAPE];
        return false;
    }
    std::vector<int32_t> getMousePos() {
        if (_using3D && _glfwWindow) {
            double x, y;
            glfwGetCursorPos(_glfwWindow, &x, &y);
            return {(int32_t)x, (int32_t)y};
        }
        int x, y;
        SDL_GetMouseState(&x, &y);
        return {x, y};
    }
    void setMousePos(int x, int y) {
        if (_using3D && _glfwWindow) { glfwSetCursorPos(_glfwWindow, (double)x, (double)y); return; }
        if (_window) SDL_WarpMouseInWindow(_window.get(), x, y);
    }
    void setCursorMode(const std::string& mode) {
        if (_using3D && _glfwWindow) {
            glfwSetInputMode(_glfwWindow, GLFW_CURSOR,
                mode == "disabled" ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            return;
        }
        if (mode == "disabled") SDL_SetRelativeMouseMode(SDL_TRUE);
        else SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    bool mouseButtonPressed(int button) {
        if (_using3D && _glfwWindow) {
            int glfwBtn = GLFW_MOUSE_BUTTON_LEFT;
            if (button == 1) glfwBtn = GLFW_MOUSE_BUTTON_MIDDLE;
            else if (button == 2) glfwBtn = GLFW_MOUSE_BUTTON_RIGHT;
            return glfwGetMouseButton(_glfwWindow, glfwBtn) == GLFW_PRESS;
        }
        Uint32 state = SDL_GetMouseState(nullptr, nullptr);
        switch (button) {
            case 0: return (state & SDL_BUTTON_LMASK) != 0;
            case 1: return (state & SDL_BUTTON_MMASK) != 0;
            case 2: return (state & SDL_BUTTON_RMASK) != 0;
            default: return false;
        }
    }
};

bool Window::_sdl_initialized = false;
bool Window::_ttf_initialized = false;
bool Window::_img_initialized = false;
bool Window::_glfw_initialized = false;

struct InputStub {
    static bool keyPressed(const std::string& key) {
        const uint8_t* state = SDL_GetKeyboardState(nullptr);
        SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
        return sc != SDL_SCANCODE_UNKNOWN && state[sc];
    }
    static int mouseX() { int x; SDL_GetMouseState(&x, nullptr); return x; }
    static int mouseY() { int y; SDL_GetMouseState(nullptr, &y); return y; }
    static bool mouseDown(int button) {
        return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(button + 1)) != 0;
    }
};
static InputStub Input;

// =================== End std.graphics ======================

)";
    }

    // Emit std library stubs only in hosted mode
    if (!freestandingMode) {

    // Emit std.audio stubs if imported
    if (importedModules.count("std.audio")) {
        output << R"(
// ====================== std.audio (SDL2_mixer) ======================
#include <SDL2/SDL_mixer.h>
#include <cmath>

struct FluxAudio {
    bool _initialized = false;
    std::map<int, Mix_Chunk*> _sounds;
    std::map<int, Mix_Music*> _musics;
    int _nextSoundId = 1;
    int _nextMusicId = 1;

    bool init() {
        if (_initialized) return true;
        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return false;
        }
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return false;
        Mix_AllocateChannels(32);
        _initialized = true;
        return true;
    }
    void quit() {
        if (!_initialized) return;
        for (auto& p : _sounds) Mix_FreeChunk(p.second);
        for (auto& p : _musics) Mix_FreeMusic(p.second);
        _sounds.clear();
        _musics.clear();
        Mix_CloseAudio();
        _initialized = false;
    }
    int loadSound(const std::string& path) {
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (!chunk) return -1;
        int id = _nextSoundId++;
        _sounds[id] = chunk;
        return id;
    }
    int loadMusic(const std::string& path) {
        Mix_Music* mus = Mix_LoadMUS(path.c_str());
        if (!mus) return -1;
        int id = _nextMusicId++;
        _musics[id] = mus;
        return id;
    }
    int playSound(int id, int loops = 0) {
        auto it = _sounds.find(id);
        if (it == _sounds.end()) return -1;
        return Mix_PlayChannel(-1, it->second, loops);
    }
    void playMusic(int id, int loops = -1) {
        auto it = _musics.find(id);
        if (it == _musics.end()) return;
        Mix_PlayMusic(it->second, loops);
    }
    void stopMusic() { Mix_HaltMusic(); }
    void pauseMusic() { Mix_PauseMusic(); }
    void resumeMusic() { Mix_ResumeMusic(); }
    bool isPlayingMusic() { return Mix_PlayingMusic() != 0; }
    void setSoundVolume(int id, int vol) {
        auto it = _sounds.find(id);
        if (it != _sounds.end()) Mix_VolumeChunk(it->second, vol);
    }
    void setMusicVolume(int vol) { Mix_VolumeMusic(vol); }
    void stopChannel(int ch) { Mix_HaltChannel(ch); }
    int generateTone(int freq, int durationMs) {
        int sampleRate = 44100;
        int samples = (sampleRate * durationMs) / 1000;
        int bufSize = samples * 2; // 16-bit mono
        Uint8* buf = new Uint8[bufSize];
        for (int i = 0; i < samples; i++) {
            double t = (double)i / sampleRate;
            double envelope = 1.0;
            int fadeOut = sampleRate / 20;
            if (i > samples - fadeOut) envelope = (double)(samples - i) / fadeOut;
            double val = std::sin(2.0 * M_PI * freq * t) * 32000.0 * envelope;
            int16_t s = (int16_t)val;
            buf[i*2] = s & 0xFF;
            buf[i*2+1] = (s >> 8) & 0xFF;
        }
        SDL_RWops* rw = SDL_RWFromMem(buf, bufSize);
        // Build a WAV in memory
        int wavSize = 44 + bufSize;
        Uint8* wav = new Uint8[wavSize];
        auto w16 = [](Uint8* p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; };
        auto w32 = [](Uint8* p, uint32_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; };
        memcpy(wav, "RIFF", 4); w32(wav+4, wavSize-8);
        memcpy(wav+8, "WAVE", 4);
        memcpy(wav+12, "fmt ", 4); w32(wav+16, 16);
        w16(wav+20, 1); w16(wav+22, 1);
        w32(wav+24, sampleRate); w32(wav+28, sampleRate*2);
        w16(wav+32, 2); w16(wav+34, 16);
        memcpy(wav+36, "data", 4); w32(wav+40, bufSize);
        memcpy(wav+44, buf, bufSize);
        delete[] buf;
        SDL_RWops* wavRw = SDL_RWFromMem(wav, wavSize);
        Mix_Chunk* chunk = Mix_LoadWAV_RW(wavRw, 0);
        delete[] wav;
        if (!chunk) return -1;
        // Copy chunk data so our buffer can be freed
        int id = _nextSoundId++;
        _sounds[id] = chunk;
        return id;
    }
};
static FluxAudio Audio;

// =================== End std.audio ======================

)";
    }

    // Emit std.video stubs if imported
    if (importedModules.count("std.video")) {
        output << R"(
// ====================== std.video (FFmpeg + OpenGL) ======================
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}
#include <GL/gl.h>
#include <mutex>

struct Video {
    bool opened = false;
    bool finished = false;
    std::string filePath;
    int videoWidth = 0;
    int videoHeight = 0;
    double framerate = 0.0;
    double durationSeconds = 0.0;
    std::vector<uint8_t> frameData;
    bool hasFrame = false;
    unsigned int glTextureId = 0;
    bool textureAllocated = false;
    bool audioPlaying = false;
    bool audioPreloaded = false;
    bool audioAutoStarted = false;
    int audioVolume = 128;

    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    AVCodecContext* audioCodecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    SwrContext* swrCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    AVFrame* audioFrame = nullptr;
    AVPacket* packet = nullptr;
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    uint8_t* rgbBuffer = nullptr;
    std::vector<uint8_t> audioBuffer;
    std::mutex audioMutex;
    Mix_Chunk* audioChunk = nullptr;
    int audioChannel = -1;

    Video() = default;
    Video(const std::string& path) { open(path); }
    bool open(const std::string& path) {
        filePath = path;
        fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) < 0) return false;
        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) { avformat_close_input(&fmtCtx); return false; }
        videoStreamIdx = -1; audioStreamIdx = -1;
        for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIdx < 0) videoStreamIdx = (int)i;
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIdx < 0) audioStreamIdx = (int)i;
        }
        if (videoStreamIdx < 0) { avformat_close_input(&fmtCtx); return false; }
        AVCodecParameters* vp = fmtCtx->streams[videoStreamIdx]->codecpar;
        const AVCodec* vc = avcodec_find_decoder(vp->codec_id);
        if (!vc) { avformat_close_input(&fmtCtx); return false; }
        videoCodecCtx = avcodec_alloc_context3(vc);
        avcodec_parameters_to_context(videoCodecCtx, vp);
        if (avcodec_open2(videoCodecCtx, vc, nullptr) < 0) { avcodec_free_context(&videoCodecCtx); avformat_close_input(&fmtCtx); return false; }
        videoWidth = videoCodecCtx->width; videoHeight = videoCodecCtx->height;
        AVRational fr = fmtCtx->streams[videoStreamIdx]->avg_frame_rate;
        framerate = (fr.den > 0 && fr.num > 0) ? (double)fr.num / fr.den : 30.0;
        if (fmtCtx->duration > 0) durationSeconds = (double)fmtCtx->duration / AV_TIME_BASE;
        swsCtx = sws_getContext(videoWidth, videoHeight, videoCodecCtx->pix_fmt, videoWidth, videoHeight, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
        frame = av_frame_alloc(); rgbFrame = av_frame_alloc(); packet = av_packet_alloc();
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
        rgbBuffer = (uint8_t*)av_malloc(numBytes);
        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuffer, AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
        frameData.resize(videoWidth * videoHeight * 3);
        if (audioStreamIdx >= 0) {
            AVCodecParameters* ap = fmtCtx->streams[audioStreamIdx]->codecpar;
            const AVCodec* ac = avcodec_find_decoder(ap->codec_id);
            if (ac) {
                audioCodecCtx = avcodec_alloc_context3(ac);
                avcodec_parameters_to_context(audioCodecCtx, ap);
                if (avcodec_open2(audioCodecCtx, ac, nullptr) == 0) {
                    audioFrame = av_frame_alloc();
                    swrCtx = swr_alloc();
                    AVChannelLayout outL = AV_CHANNEL_LAYOUT_STEREO;
                    AVChannelLayout inL;
                    if (audioCodecCtx->ch_layout.nb_channels > 0) { inL = audioCodecCtx->ch_layout; } else { inL = AV_CHANNEL_LAYOUT_STEREO; }
                    swr_alloc_set_opts2(&swrCtx, &outL, AV_SAMPLE_FMT_S16, 44100, &inL, audioCodecCtx->sample_fmt, audioCodecCtx->sample_rate, 0, nullptr);
                    swr_init(swrCtx);
                } else { avcodec_free_context(&audioCodecCtx); audioCodecCtx = nullptr; audioStreamIdx = -1; }
            } else { audioStreamIdx = -1; }
        }
        opened = true; finished = false;
        preloadAudio();
        return true;
    }
    void preloadAudio() {
        if (audioPreloaded || audioStreamIdx < 0 || !audioCodecCtx) return;
        AVPacket* apkt = av_packet_alloc();
        while (av_read_frame(fmtCtx, apkt) >= 0) {
            if (apkt->stream_index == audioStreamIdx) {
                int r2 = avcodec_send_packet(audioCodecCtx, apkt);
                if (r2 >= 0) {
                    while (true) {
                        r2 = avcodec_receive_frame(audioCodecCtx, audioFrame);
                        if (r2 < 0) break;
                        int outS = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
                        if (outS <= 0) continue;
                        std::vector<uint8_t> tmp2(outS * 4);
                        uint8_t* ob2[1] = {tmp2.data()};
                        int c2 = swr_convert(swrCtx, ob2, outS, (const uint8_t**)audioFrame->data, audioFrame->nb_samples);
                        if (c2 > 0) { int b2 = c2 * 4; std::lock_guard<std::mutex> lk(audioMutex); audioBuffer.insert(audioBuffer.end(), tmp2.data(), tmp2.data()+b2); }
                    }
                }
            }
            av_packet_unref(apkt);
        }
        av_packet_free(&apkt);
        av_seek_frame(fmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
        if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx);
        if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
        finished = false;
        audioPreloaded = true;
    }
    void decodeAudioPacket() {
        if (!audioCodecCtx || !swrCtx) return;
        int ret = avcodec_send_packet(audioCodecCtx, packet);
        if (ret < 0) return;
        while (true) {
            ret = avcodec_receive_frame(audioCodecCtx, audioFrame);
            if (ret < 0) break;
            int outSamples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
            if (outSamples <= 0) continue;
            std::vector<uint8_t> tmp(outSamples * 4);
            uint8_t* ob[1] = {tmp.data()};
            int conv = swr_convert(swrCtx, ob, outSamples, (const uint8_t**)audioFrame->data, audioFrame->nb_samples);
            if (conv > 0) { int b = conv * 4; std::lock_guard<std::mutex> lk(audioMutex); audioBuffer.insert(audioBuffer.end(), tmp.data(), tmp.data()+b); }
        }
    }
    bool nextFrame() {
        if (!opened || finished) return false;
        while (true) {
            int ret = av_read_frame(fmtCtx, packet);
            if (ret < 0) { finished = true; hasFrame = false; return false; }
            if (packet->stream_index == videoStreamIdx) {
                ret = avcodec_send_packet(videoCodecCtx, packet); av_packet_unref(packet);
                if (ret < 0) continue;
                ret = avcodec_receive_frame(videoCodecCtx, frame);
                if (ret < 0) continue;
                sws_scale(swsCtx, frame->data, frame->linesize, 0, videoHeight, rgbFrame->data, rgbFrame->linesize);
                for (int y = 0; y < videoHeight; y++) memcpy(frameData.data()+y*videoWidth*3, rgbFrame->data[0]+y*rgbFrame->linesize[0], videoWidth*3);
                hasFrame = true;
                if (!audioAutoStarted && audioPreloaded && !audioBuffer.empty()) { playAudio(); audioAutoStarted = true; }
                return true;
            } else if (packet->stream_index == audioStreamIdx && audioCodecCtx) {
                av_packet_unref(packet); continue;
            } else { av_packet_unref(packet); continue; }
        }
    }
    int getTextureId() {
        if (!hasFrame || videoWidth <= 0 || videoHeight <= 0) return 0;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!textureAllocated) {
            glGenTextures(1, &glTextureId); glBindTexture(GL_TEXTURE_2D, glTextureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoWidth, videoHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, frameData.data());
            textureAllocated = true;
        } else {
            glBindTexture(GL_TEXTURE_2D, glTextureId);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight, GL_RGB, GL_UNSIGNED_BYTE, frameData.data());
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        return (int)glTextureId;
    }
    void playAudio() {
        if (audioStreamIdx < 0 || audioBuffer.empty()) return;
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) { if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return; }
        { int f=0,c=0; Uint16 fm=0; if (Mix_QuerySpec(&f,&fm,&c)==0) { if (Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)<0) return; } }
        std::lock_guard<std::mutex> lk(audioMutex);
        int ds = (int)audioBuffer.size(); int ws = 44 + ds;
        std::vector<uint8_t> wav(ws);
        auto w16=[](uint8_t*p,uint16_t v){p[0]=v&0xFF;p[1]=(v>>8)&0xFF;};
        auto w32=[](uint8_t*p,uint32_t v){p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;};
        memcpy(wav.data(),"RIFF",4);w32(wav.data()+4,ws-8);memcpy(wav.data()+8,"WAVE",4);
        memcpy(wav.data()+12,"fmt ",4);w32(wav.data()+16,16);w16(wav.data()+20,1);w16(wav.data()+22,2);
        w32(wav.data()+24,44100);w32(wav.data()+28,44100*4);w16(wav.data()+32,4);w16(wav.data()+34,16);
        memcpy(wav.data()+36,"data",4);w32(wav.data()+40,ds);memcpy(wav.data()+44,audioBuffer.data(),ds);
        if (audioChunk) { Mix_FreeChunk(audioChunk); audioChunk = nullptr; }
        SDL_RWops* rw = SDL_RWFromMem(wav.data(), ws);
        audioChunk = Mix_LoadWAV_RW(rw, 1);
        if (audioChunk) { Mix_VolumeChunk(audioChunk, audioVolume); audioChannel = Mix_PlayChannel(-1, audioChunk, 0); audioPlaying = true; }
    }
    void stopAudio() { if (audioChannel >= 0) { Mix_HaltChannel(audioChannel); audioChannel = -1; } audioPlaying = false; }
    void setAudioVolume(int v) { audioVolume = v; if (audioChunk) Mix_VolumeChunk(audioChunk, v); }
    void seek(double sec) {
        if (!opened) return;
        int64_t ts = (int64_t)(sec * AV_TIME_BASE);
        av_seek_frame(fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD);
        if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx);
        if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
        finished = false; hasFrame = false;
    }
    void restart() { seek(0.0); }
    void close() {
        stopAudio();
        if (audioChunk) { Mix_FreeChunk(audioChunk); audioChunk = nullptr; }
        if (textureAllocated && glTextureId > 0) { glDeleteTextures(1, &glTextureId); glTextureId = 0; textureAllocated = false; }
        if (rgbBuffer) { av_free(rgbBuffer); rgbBuffer = nullptr; }
        if (frame) av_frame_free(&frame); if (rgbFrame) av_frame_free(&rgbFrame);
        if (audioFrame) av_frame_free(&audioFrame); if (packet) av_packet_free(&packet);
        if (swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }
        if (swrCtx) { swr_free(&swrCtx); swrCtx = nullptr; }
        if (videoCodecCtx) avcodec_free_context(&videoCodecCtx);
        if (audioCodecCtx) avcodec_free_context(&audioCodecCtx);
        if (fmtCtx) avformat_close_input(&fmtCtx);
        opened = false; finished = true; hasFrame = false; audioBuffer.clear();
    }
    int width() { return videoWidth; }
    int height() { return videoHeight; }
    double fps() { return framerate; }
    double duration() { return durationSeconds; }
    bool isOpen() { return opened; }
    bool isFinished() { return finished; }
    ~Video() { close(); }
};

// =================== End std.video ======================

)";
    }

    // Emit std.io stubs if imported
    if (importedModules.count("std.io")) {
        output << R"(
// ====================== std.io Stubs ======================
struct FluxFS {
    std::string read(const std::string& path) {
        std::ifstream f(path); std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); return s;
    }
    void write(const std::string& path, const std::string& data) {
        std::ofstream f(path); f << data;
    }
    void append(const std::string& path, const std::string& data) {
        std::ofstream f(path, std::ios::app); f << data;
    }
    bool exists(const std::string& path) { return std::filesystem::exists(path); }
    void remove(const std::string& path) { std::filesystem::remove(path); }
    void del(const std::string& path) { std::filesystem::remove(path); }
    std::vector<std::string> list(const std::string& dir) {
        std::vector<std::string> out;
        for (auto& e : std::filesystem::directory_iterator(dir)) out.push_back(e.path().filename().string());
        return out;
    }
    void mkdir(const std::string& path) { std::filesystem::create_directories(path); }
    int64_t size(const std::string& path) { return (int64_t)std::filesystem::file_size(path); }
    void copy(const std::string& src, const std::string& dst) { std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing); }
    void rename(const std::string& oldP, const std::string& newP) { std::filesystem::rename(oldP, newP); }
};
static FluxFS fs;
// =================== End std.io Stubs ======================

)";
    }

    // Emit std.sys stubs if imported
    if (importedModules.count("std.sys")) {
        output << R"(
// ====================== std.sys Stubs ======================

// --- Signal support ---
static std::map<int, std::function<void()>> _flux_signal_handlers;
static void _flux_signal_dispatch(int sig) {
    if (_flux_signal_handlers.count(sig)) _flux_signal_handlers[sig]();
}
struct FluxSignal {
    // Use method accessors to avoid macro clashes with <csignal>
    int _SIGINT = 2, _SIGTERM = 15, _SIGABRT = 6, _SIGFPE = 8, _SIGSEGV = 11;
#ifndef _WIN32
    int _SIGHUP = 1, _SIGUSR1 = 10, _SIGUSR2 = 12, _SIGPIPE = 13, _SIGALRM = 14, _SIGCHLD = 17;
#endif
    void handle(int sig, std::function<void()> handler) {
        _flux_signal_handlers[sig] = handler;
        std::signal(sig, _flux_signal_dispatch);
    }
    void raise(int sig) { std::raise(sig); }
    void ignore(int sig) { std::signal(sig, SIG_IGN); _flux_signal_handlers.erase(sig); }
    void reset(int sig) { std::signal(sig, SIG_DFL); _flux_signal_handlers.erase(sig); }
};
static FluxSignal Signal;

// --- Threading ---
struct FluxThreadHandle {
    std::thread _t;
    bool _joined = false;
    std::string _error;
    FluxThreadHandle() {}
    void join() { if (_t.joinable()) { _t.join(); _joined = true; } }
};

struct FluxThread {
    template<typename Func, typename... Args>
    FluxThreadHandle run(Func&& f, Args&&... args) {
        FluxThreadHandle h;
        h._t = std::thread(std::forward<Func>(f), std::forward<Args>(args)...);
        return h;
    }
    void sleep(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
};
static FluxThread thread;

// --- Mutex ---
struct Mutex {
    std::mutex _m;
    void lock() { _m.lock(); }
    void unlock() { _m.unlock(); }
    bool tryLock() { return _m.try_lock(); }
};

// --- sys namespace ---
struct FluxSys {
    int64_t time() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    std::string env(const std::string& key) {
        const char* v = std::getenv(key.c_str());
        return v ? std::string(v) : "";
    }
    void exit(int code = 0) { std::exit(code); }
    int cpuCount() { return (int)std::thread::hardware_concurrency(); }
    std::string platform =
#if defined(__linux__)
        "linux";
#elif defined(__APPLE__)
        "macos";
#elif defined(_WIN32)
        "windows";
#else
        "unknown";
#endif
    std::string arch =
#if defined(__x86_64__) || defined(_M_X64)
        "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
        "arm";
#else
        "unknown";
#endif
    std::vector<std::string> args; // populated from main args if needed
};
static FluxSys sys;
// =================== End std.sys Stubs ======================

)";
    }

    // Emit std.time stubs if imported
    if (importedModules.count("std.time")) {
        output << R"(
// ====================== std.time Stubs ======================
struct Timer {
    std::chrono::high_resolution_clock::time_point _start, _stop;
    void start() { _start = std::chrono::high_resolution_clock::now(); }
    void stop() { _stop = std::chrono::high_resolution_clock::now(); }
    double elapsed() {
        return std::chrono::duration<double>(_stop - _start).count();
    }
};

struct FluxTimeNS {
    double now() {
        return (double)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    std::string format(double ts, const std::string& fmt) {
        time_t t = (time_t)ts;
        char buf[256]; struct tm* tm = localtime(&t);
        strftime(buf, sizeof(buf), fmt.c_str(), tm);
        return std::string(buf);
    }
    int parse(const std::string& str, const std::string& fmt) {
        struct tm tm = {}; strptime(str.c_str(), fmt.c_str(), &tm);
        return (int)mktime(&tm);
    }
    int year(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_year+1900; }
    int month(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_mon+1; }
    int day(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_mday; }
    int hour(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_hour; }
    int minute(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_min; }
    int second(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_sec; }
    int dayOfWeek(double ts) { time_t t=(time_t)ts; return localtime(&t)->tm_wday; }
    double elapsed(int64_t startMs) {
        return (double)(nowMs() - startMs) / 1000.0;
    }
};
static FluxTimeNS Time;
// =================== End std.time Stubs ======================

)";
    }

    // Emit std.net stubs if imported
    if (importedModules.count("std.net")) {
        output << R"(
// ====================== std.net (Real POSIX Sockets) ======================

struct Response {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HttpClient {
    std::map<std::string, std::string> _headers;
    void setHeader(const std::string& key, const std::string& val) { _headers[key] = val; }

    // Use curl for HTTPS requests (raw sockets can't do TLS)
    Response doCurlRequest(const std::string& method, const std::string& url, const std::string& reqBody = "") {
        // Build curl command: output headers to stderr, body to stdout
        std::string cmd = "curl -s -S -D /dev/stderr -X " + method;
        for (auto& [k, v] : _headers) {
            cmd += " -H \"" + k + ": " + v + "\"";
        }
        if (!reqBody.empty()) {
            cmd += " -d \"" + reqBody + "\"";
        }
        // Follow redirects (common for CDN URLs)
        cmd += " -L";
        cmd += " \"" + url + "\" 2>/tmp/_flux_curl_headers";

        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) return Response{0, "curl exec failed", {}};

        // Read body
        std::string body;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) body.append(buf, n);
        pclose(fp);

        Response resp;
        resp.body = body;

        // Parse the dumped headers
        std::ifstream hf("/tmp/_flux_curl_headers");
        if (hf.is_open()) {
            std::string line;
            while (std::getline(hf, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                // Status line: HTTP/x.x NNN ...
                if (line.substr(0, 5) == "HTTP/" && resp.statusCode == 0) {
                    size_t sp = line.find(' ');
                    if (sp != std::string::npos) {
                        try { resp.statusCode = std::stoi(line.substr(sp + 1, 3)); } catch (...) {}
                    }
                }
                // On redirect, curl dumps multiple header blocks; keep parsing to get final status
                if (line.substr(0, 5) == "HTTP/") {
                    size_t sp = line.find(' ');
                    if (sp != std::string::npos) {
                        try { resp.statusCode = std::stoi(line.substr(sp + 1, 3)); } catch (...) {}
                    }
                    continue;
                }
                size_t cp = line.find(':');
                if (cp != std::string::npos) {
                    std::string k = line.substr(0, cp);
                    std::string v = line.substr(cp + 1);
                    while (!v.empty() && v[0] == ' ') v.erase(0, 1);
                    resp.headers[k] = v;
                }
            }
            hf.close();
            std::remove("/tmp/_flux_curl_headers");
        }
        if (resp.statusCode == 0 && !body.empty()) resp.statusCode = 200;
        return resp;
    }

    // Raw socket HTTP/1.1 client (for plain HTTP only)
    Response doRequest(const std::string& method, const std::string& url, const std::string& reqBody = "") {
        // Use curl for HTTPS since raw sockets can't do TLS
        if (url.substr(0, 8) == "https://") {
            return doCurlRequest(method, url, reqBody);
        }

        // Parse URL: http://host[:port]/path
        std::string host, path = "/";
        int port = 80;
        size_t start = 0;
        if (url.substr(0, 7) == "http://") start = 7;
        size_t slashPos = url.find('/', start);
        std::string hostPort = (slashPos != std::string::npos) ? url.substr(start, slashPos - start) : url.substr(start);
        if (slashPos != std::string::npos) path = url.substr(slashPos);
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            host = hostPort.substr(0, colonPos);
            port = std::stoi(hostPort.substr(colonPos + 1));
        } else {
            host = hostPort;
        }

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return Response{0, "socket error", {}};

        struct hostent* he = gethostbyname(host.c_str());
        if (!he) { ::close(fd); return Response{0, "DNS lookup failed", {}}; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);

        if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(fd); return Response{0, "connection failed", {}};
        }

        // Build HTTP request
        std::string req = method + " " + path + " HTTP/1.1\r\n";
        req += "Host: " + host + "\r\n";
        req += "Connection: close\r\n";
        for (auto& [k, v] : _headers) req += k + ": " + v + "\r\n";
        if (!reqBody.empty()) {
            req += "Content-Length: " + std::to_string(reqBody.size()) + "\r\n";
        }
        req += "\r\n";
        if (!reqBody.empty()) req += reqBody;

        ::send(fd, req.c_str(), req.size(), 0);

        // Read response
        std::string raw;
        char buf[4096];
        ssize_t n;
        while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) raw.append(buf, n);
        ::close(fd);

        // Parse response
        Response resp;
        size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos) { resp.body = raw; return resp; }
        std::string headerSection = raw.substr(0, headerEnd);
        resp.body = raw.substr(headerEnd + 4);

        // Parse status line
        size_t firstLine = headerSection.find("\r\n");
        std::string statusLine = headerSection.substr(0, firstLine);
        size_t spacePos = statusLine.find(' ');
        if (spacePos != std::string::npos) {
            resp.statusCode = std::stoi(statusLine.substr(spacePos + 1, 3));
        }

        // Parse headers
        std::istringstream hstream(headerSection.substr(firstLine + 2));
        std::string line;
        while (std::getline(hstream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t cp = line.find(':');
            if (cp != std::string::npos) {
                std::string k = line.substr(0, cp);
                std::string v = line.substr(cp + 1);
                while (!v.empty() && v[0] == ' ') v.erase(0, 1);
                resp.headers[k] = v;
            }
        }
        return resp;
    }

    // No-op init() so `new HttpClient()` works (transpiler emits _obj.init())
    void init() {}

    Response get(const std::string& url) { return doRequest("GET", url); }
    Response post(const std::string& url, const std::string& b = "") { return doRequest("POST", url, b); }
    Response put(const std::string& url, const std::string& b = "") { return doRequest("PUT", url, b); }
    Response httpDelete(const std::string& url) { return doRequest("DELETE", url); }

    // Download binary content to a file
    bool download(const std::string& url, const std::string& filePath) {
        // For HTTPS, use curl directly for binary download (more reliable for large files)
        if (url.substr(0, 8) == "https://") {
            std::string cmd = "curl -s -S -L -o \"" + filePath + "\"";
            for (auto& [k, v] : _headers) {
                cmd += " -H \"" + k + ": " + v + "\"";
            }
            cmd += " \"" + url + "\"";
            int ret = system(cmd.c_str());
            return (ret == 0);
        }
        Response resp = doRequest("GET", url);
        if (resp.statusCode >= 200 && resp.statusCode < 300) {
            std::ofstream out(filePath, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(resp.body.c_str(), resp.body.size());
            return true;
        }
        return false;
    }
};

struct Socket {
    int _fd = -1;
    bool _connected = false;
    bool _isServer = false;

    // No-op init() so `new Socket()` works (transpiler emits _obj.init())
    void init() {}

    Socket() {
        _fd = ::socket(AF_INET, SOCK_STREAM, 0);
    }

    void connect(const std::string& host, int port) {
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) throw FluxError("DNS lookup failed for: " + host);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        if (::connect(_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            throw FluxError("Connection failed to " + host + ":" + std::to_string(port));
        _connected = true;
    }

    void bind(int port) {
        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (::bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            throw FluxError("Failed to bind to port " + std::to_string(port));
        _isServer = true;
    }

    void listen(int backlog = 5) {
        if (::listen(_fd, backlog) < 0) throw FluxError("Failed to listen");
    }

    Socket accept() {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = ::accept(_fd, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) throw FluxError("Failed to accept connection");
        Socket s;
        if (s._fd >= 0) ::close(s._fd);
        s._fd = clientFd;
        s._connected = true;
        return s;
    }

    int write(const std::string& data) {
        ssize_t sent = ::send(_fd, data.c_str(), data.size(), 0);
        if (sent < 0) throw FluxError("Failed to send data");
        return (int)sent;
    }

    std::string readLine() {
        std::string line;
        char c;
        while (::recv(_fd, &c, 1, 0) > 0) {
            if (c == '\n') break;
            if (c != '\r') line += c;
        }
        return line;
    }

    std::string read(int maxBytes = 4096) {
        std::vector<char> buf(maxBytes);
        ssize_t received = ::recv(_fd, buf.data(), maxBytes, 0);
        if (received < 0) throw FluxError("Failed to read data");
        return std::string(buf.data(), received);
    }

    void close() {
        if (_fd >= 0) { ::close(_fd); _fd = -1; }
        _connected = false;
    }
};

struct FluxProtocolNS {
    int TCP = 0, UDP = 1;
};
static FluxProtocolNS Protocol;
// =================== End std.net ======================

)";
    }

    // Emit std.collections stubs if imported
    if (importedModules.count("std.collections")) {
        output << R"(
// ====================== std.collections Stubs ======================
// Note: These provide C++ implementations rather than stubs

template<typename K = std::string, typename V = std::string>
struct Map {
    std::map<K, V> _store;
    void put(const K& key, const V& val) { _store[key] = val; }
    V get(const K& key) { return _store.count(key) ? _store[key] : V{}; }
    bool hasKey(const K& key) { return _store.count(key) > 0; }
    void remove(const K& key) { _store.erase(key); }
    std::vector<K> keys() { std::vector<K> r; for(auto& p:_store) r.push_back(p.first); return r; }
    std::vector<V> values() { std::vector<V> r; for(auto& p:_store) r.push_back(p.second); return r; }
    int length() { return (int)_store.size(); }
    int size() { return (int)_store.size(); }
};

template<typename T = std::string>
struct Stack {
    std::vector<T> _data;
    void push(const T& item) { _data.push_back(item); }
    T pop() { T v = _data.back(); _data.pop_back(); return v; }
    T peek() { return _data.back(); }
    int size() { return (int)_data.size(); }
    bool isEmpty() { return _data.empty(); }
};

template<typename T = std::string>
struct Queue {
    std::deque<T> _data;
    void enqueue(const T& item) { _data.push_back(item); }
    T dequeue() { T v = _data.front(); _data.pop_front(); return v; }
    T peek() { return _data.front(); }
    int size() { return (int)_data.size(); }
    bool isEmpty() { return _data.empty(); }
};
// =================== End std.collections Stubs ======================

)";
    }

    // Emit FluxObject if needed (by 'object' type vars or std.json)
    if (needsFluxObject || importedModules.count("std.json")) {
        output << R"(
// ====================== FluxObject — Dynamic AOT type ======================
// Supports property access (obj.key), array indexing, and string conversion.
// Used for JSON.parse() return values and generic 'object' typed variables.

struct FluxObject {
    enum Type { NIL, STRING, NUMBER, BOOLEAN, ARRAY, OBJECT };
    Type type = NIL;
    std::string strVal;
    double numVal = 0;
    bool boolVal = false;
    std::map<std::string, FluxObject> properties;
    std::vector<FluxObject> elements;

    FluxObject() : type(NIL) {}
    FluxObject(const std::string& s) : type(STRING), strVal(s) {}
    FluxObject(const char* s) : type(STRING), strVal(s) {}
    FluxObject(double n) : type(NUMBER), numVal(n) {}
    FluxObject(int n) : type(NUMBER), numVal(n) {}
    FluxObject(int64_t n) : type(NUMBER), numVal((double)n) {}
    FluxObject(bool b) : type(BOOLEAN), boolVal(b) {}

    // No-op init() so `new FluxObject()` works (transpiler emits _obj.init())
    void init() {}

    // Property access by key (returns reference for chaining)
    FluxObject& get(const std::string& key) {
        return properties[key];
    }

    // Set a property
    void set(const std::string& key, const FluxObject& val) {
        properties[key] = val;
    }

    // Array access by index
    FluxObject& at(int index) {
        if (index < 0 || index >= (int)elements.size())
            throw FluxError("Index out of bounds: " + std::to_string(index));
        return elements[index];
    }

    // Length — works for arrays, objects (key count), and strings
    int length() const {
        if (type == ARRAY) return (int)elements.size();
        if (type == OBJECT) return (int)properties.size();
        if (type == STRING) return (int)strVal.size();
        return 0;
    }
    int size() const { return length(); }

    // Check if property exists
    bool hasKey(const std::string& key) const {
        return properties.count(key) > 0;
    }

    // Get object keys
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        for (auto& p : properties) result.push_back(p.first);
        return result;
    }

    // Conversion to string
    std::string toString() const {
        switch (type) {
            case NIL: return "nil";
            case STRING: return strVal;
            case NUMBER: {
                if (numVal == (int64_t)numVal) return std::to_string((int64_t)numVal);
                return std::to_string(numVal);
            }
            case BOOLEAN: return boolVal ? "true" : "false";
            case ARRAY: {
                std::string r = "[";
                for (size_t i = 0; i < elements.size(); i++) {
                    if (i > 0) r += ", ";
                    if (elements[i].type == STRING) r += "\"" + elements[i].toString() + "\"";
                    else r += elements[i].toString();
                }
                return r + "]";
            }
            case OBJECT: {
                std::string r = "{";
                bool first = true;
                for (auto& p : properties) {
                    if (!first) r += ", ";
                    first = false;
                    r += "\"" + p.first + "\": ";
                    if (p.second.type == STRING) r += "\"" + p.second.toString() + "\"";
                    else r += p.second.toString();
                }
                return r + "}";
            }
        }
        return "";
    }

    // Implicit conversion to string for printing / concatenation
    operator std::string() const { return toString(); }
    operator int32_t() const { return (int32_t)numVal; }
    operator int64_t() const { return (int64_t)numVal; }
    operator double() const { return numVal; }
    operator bool() const {
        switch (type) {
            case NIL: return false;
            case STRING: return !strVal.empty();
            case NUMBER: return numVal != 0;
            case BOOLEAN: return boolVal;
            case ARRAY: return !elements.empty();
            case OBJECT: return !properties.empty();
        }
        return false;
    }

    // Comparison
    bool operator==(const FluxObject& o) const {
        if (type != o.type) return false;
        switch (type) {
            case NIL: return true;
            case STRING: return strVal == o.strVal;
            case NUMBER: return numVal == o.numVal;
            case BOOLEAN: return boolVal == o.boolVal;
            default: return false;
        }
    }
    bool operator!=(const FluxObject& o) const { return !(*this == o); }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const FluxObject& obj) {
        return os << obj.toString();
    }
};
// =================== End FluxObject ======================

)";
    }

    // Emit std.json stubs if imported
    if (importedModules.count("std.json")) {
        output << R"(
// ====================== std.json (Real Implementation) ======================
// AOT JSON: parse() returns a std::string (pretty-printed parsed JSON),
// stringify() returns a compact/indented JSON string.
// For full object manipulation, use JIT mode. AOT provides validation + reformatting.

namespace {

class AOTJSONParser {
    std::string src;
    size_t pos;

    void skipWhitespace() {
        while (pos < src.size() && (src[pos]==' '||src[pos]=='\t'||src[pos]=='\n'||src[pos]=='\r')) pos++;
    }

    std::string parseString() {
        if (pos >= src.size() || src[pos] != '"') throw FluxError("JSON: expected '\"'");
        pos++; // skip opening "
        std::string result = "\"";
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\') {
                result += src[pos++];
                if (pos < src.size()) result += src[pos++];
            } else {
                result += src[pos++];
            }
        }
        if (pos >= src.size()) throw FluxError("JSON: unterminated string");
        pos++; // skip closing "
        result += '"';
        return result;
    }

    std::string parseNumber() {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && isdigit(src[pos])) pos++;
        if (pos < src.size() && src[pos] == '.') {
            pos++;
            while (pos < src.size() && isdigit(src[pos])) pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && isdigit(src[pos])) pos++;
        }
        return src.substr(start, pos - start);
    }

    std::string parseValue(int depth = 0) {
        skipWhitespace();
        if (pos >= src.size()) throw FluxError("JSON: unexpected end of input");
        char c = src[pos];
        if (c == '"') return parseString();
        if (c == '{') return parseObject(depth);
        if (c == '[') return parseArray(depth);
        if (c == 't') { pos += 4; return "true"; }
        if (c == 'f') { pos += 5; return "false"; }
        if (c == 'n') { pos += 4; return "null"; }
        if (c == '-' || isdigit(c)) return parseNumber();
        throw FluxError(std::string("JSON: unexpected character '") + c + "'");
    }

    std::string parseObject(int depth) {
        pos++; // skip {
        skipWhitespace();
        std::string result = "{";
        bool first = true;
        while (pos < src.size() && src[pos] != '}') {
            if (!first) { result += ", "; }
            first = false;
            skipWhitespace();
            result += parseString();
            skipWhitespace();
            if (pos >= src.size() || src[pos] != ':') throw FluxError("JSON: expected ':'");
            pos++;
            result += ": ";
            result += parseValue(depth + 1);
            skipWhitespace();
            if (pos < src.size() && src[pos] == ',') pos++;
        }
        if (pos >= src.size()) throw FluxError("JSON: unterminated object");
        pos++; // skip }
        result += "}";
        return result;
    }

    std::string parseArray(int depth) {
        pos++; // skip [
        skipWhitespace();
        std::string result = "[";
        bool first = true;
        while (pos < src.size() && src[pos] != ']') {
            if (!first) { result += ", "; }
            first = false;
            result += parseValue(depth + 1);
            skipWhitespace();
            if (pos < src.size() && src[pos] == ',') pos++;
        }
        if (pos >= src.size()) throw FluxError("JSON: unterminated array");
        pos++; // skip ]
        result += "]";
        return result;
    }

public:
    std::string parse(const std::string& json) {
        src = json;
        pos = 0;
        std::string result = parseValue();
        return result;
    }
};

static std::string indentJSON(const std::string& json, int indent) {
    if (indent <= 0) return json;
    std::string result;
    int depth = 0;
    bool inString = false;
    std::string indentStr(indent, ' ');

    for (size_t i = 0; i < json.size(); i++) {
        char c = json[i];
        if (inString) {
            result += c;
            if (c == '"' && (i == 0 || json[i-1] != '\\')) inString = false;
            continue;
        }
        switch (c) {
            case '"': inString = true; result += c; break;
            case '{': case '[':
                result += c;
                result += '\n';
                depth++;
                for (int d = 0; d < depth; d++) result += indentStr;
                break;
            case '}': case ']':
                result += '\n';
                depth--;
                for (int d = 0; d < depth; d++) result += indentStr;
                result += c;
                break;
            case ',':
                result += c;
                result += '\n';
                for (int d = 0; d < depth; d++) result += indentStr;
                break;
            case ':':
                result += ": ";
                break;
            case ' ': case '\t': case '\n': case '\r':
                break; // skip whitespace outside strings
            default:
                result += c;
        }
    }
    return result;
}

} // end anonymous namespace

// ====================== JSON Namespace (uses FluxObject) ======================

namespace {

// Parse raw JSON string into FluxObject tree
FluxObject parseJSONValue(const std::string& src, size_t& pos);

void skipJSONWS(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) p++;
}

std::string parseJSONString(const std::string& s, size_t& p) {
    if (p >= s.size() || s[p] != '"') throw FluxError("JSON: expected '\"'");
    p++; // skip "
    std::string result;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\') {
            p++;
            if (p >= s.size()) throw FluxError("JSON: unexpected end in string");
            switch (s[p]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                default: result += s[p]; break;
            }
        } else {
            result += s[p];
        }
        p++;
    }
    if (p >= s.size()) throw FluxError("JSON: unterminated string");
    p++; // skip closing "
    return result;
}

FluxObject parseJSONValue(const std::string& src, size_t& pos) {
    skipJSONWS(src, pos);
    if (pos >= src.size()) throw FluxError("JSON: unexpected end");

    char c = src[pos];

    // String
    if (c == '"') {
        return FluxObject(parseJSONString(src, pos));
    }

    // Object
    if (c == '{') {
        pos++; // skip {
        FluxObject obj;
        obj.type = FluxObject::OBJECT;
        skipJSONWS(src, pos);
        while (pos < src.size() && src[pos] != '}') {
            std::string key = parseJSONString(src, pos);
            skipJSONWS(src, pos);
            if (pos >= src.size() || src[pos] != ':') throw FluxError("JSON: expected ':'");
            pos++;
            obj.properties[key] = parseJSONValue(src, pos);
            skipJSONWS(src, pos);
            if (pos < src.size() && src[pos] == ',') pos++;
        }
        if (pos >= src.size()) throw FluxError("JSON: unterminated object");
        pos++; // skip }
        return obj;
    }

    // Array
    if (c == '[') {
        pos++; // skip [
        FluxObject arr;
        arr.type = FluxObject::ARRAY;
        skipJSONWS(src, pos);
        while (pos < src.size() && src[pos] != ']') {
            arr.elements.push_back(parseJSONValue(src, pos));
            skipJSONWS(src, pos);
            if (pos < src.size() && src[pos] == ',') pos++;
        }
        if (pos >= src.size()) throw FluxError("JSON: unterminated array");
        pos++; // skip ]
        return arr;
    }

    // true / false / null
    if (src.compare(pos, 4, "true") == 0)  { pos += 4; return FluxObject(true); }
    if (src.compare(pos, 5, "false") == 0) { pos += 5; return FluxObject(false); }
    if (src.compare(pos, 4, "null") == 0)  { pos += 4; return FluxObject(); }

    // Number
    if (c == '-' || isdigit(c)) {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && isdigit(src[pos])) pos++;
        bool isFloat = false;
        if (pos < src.size() && src[pos] == '.') { isFloat = true; pos++; while (pos < src.size() && isdigit(src[pos])) pos++; }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) { isFloat = true; pos++; if (pos < src.size() && (src[pos]=='+' || src[pos]=='-')) pos++; while (pos < src.size() && isdigit(src[pos])) pos++; }
        return FluxObject(std::stod(src.substr(start, pos - start)));
    }

    throw FluxError(std::string("JSON: unexpected character '") + c + "'");
}

// Stringify FluxObject back to JSON
std::string stringifyFluxObject(const FluxObject& obj, int indent = 0, int depth = 0) {
    std::string pad = indent > 0 ? std::string(indent * depth, ' ') : "";
    std::string pad1 = indent > 0 ? std::string(indent * (depth + 1), ' ') : "";
    std::string nl = indent > 0 ? "\n" : "";
    std::string sep = indent > 0 ? ": " : ":";

    switch (obj.type) {
        case FluxObject::NIL: return "null";
        case FluxObject::STRING: {
            std::string r = "\"";
            for (char c : obj.strVal) {
                if (c == '"') r += "\\\"";
                else if (c == '\\') r += "\\\\";
                else if (c == '\n') r += "\\n";
                else if (c == '\t') r += "\\t";
                else r += c;
            }
            return r + "\"";
        }
        case FluxObject::NUMBER: {
            if (obj.numVal == (int64_t)obj.numVal) return std::to_string((int64_t)obj.numVal);
            return std::to_string(obj.numVal);
        }
        case FluxObject::BOOLEAN: return obj.boolVal ? "true" : "false";
        case FluxObject::ARRAY: {
            if (obj.elements.empty()) return "[]";
            std::string r = "[" + nl;
            for (size_t i = 0; i < obj.elements.size(); i++) {
                if (i > 0) r += "," + nl;
                r += pad1 + stringifyFluxObject(obj.elements[i], indent, depth + 1);
            }
            return r + nl + pad + "]";
        }
        case FluxObject::OBJECT: {
            if (obj.properties.empty()) return "{}";
            std::string r = "{" + nl;
            bool first = true;
            for (auto& p : obj.properties) {
                if (!first) r += "," + nl;
                first = false;
                r += pad1 + "\"" + p.first + "\"" + sep + stringifyFluxObject(p.second, indent, depth + 1);
            }
            return r + nl + pad + "}";
        }
    }
    return "null";
}

} // end anonymous namespace

struct FluxJSONNS {
    FluxObject parse(const std::string& str) {
        size_t pos = 0;
        return parseJSONValue(str, pos);
    }
    std::string stringify(const FluxObject& val, int indent = 0) {
        return stringifyFluxObject(val, indent);
    }
    std::string stringify(const std::string& val, int indent = 0) {
        // Parse string to FluxObject first, then stringify
        size_t pos = 0;
        FluxObject obj = parseJSONValue(val, pos);
        return stringifyFluxObject(obj, indent);
    }
};
static FluxJSONNS JSON;
// =================== End std.json ======================

)";
    }

    // Emit std.crypto stubs if imported
    if (importedModules.count("std.crypto")) {
        output << R"(
// ====================== std.crypto (Real Implementations) ======================

// --- SHA-256 (FIPS 180-4) ---
namespace {
static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t sha256_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t sha256_sigma0(uint32_t x) { return sha256_rotr(x,2) ^ sha256_rotr(x,13) ^ sha256_rotr(x,22); }
static uint32_t sha256_sigma1(uint32_t x) { return sha256_rotr(x,6) ^ sha256_rotr(x,11) ^ sha256_rotr(x,25); }
static uint32_t sha256_gamma0(uint32_t x) { return sha256_rotr(x,7) ^ sha256_rotr(x,18) ^ (x >> 3); }
static uint32_t sha256_gamma1(uint32_t x) { return sha256_rotr(x,17) ^ sha256_rotr(x,19) ^ (x >> 10); }

static std::string compute_sha256(const std::string& input) {
    uint32_t h0=0x6a09e667, h1=0xbb67ae85, h2=0x3c6ef372, h3=0xa54ff53a;
    uint32_t h4=0x510e527f, h5=0x9b05688c, h6=0x1f83d9ab, h7=0x5be0cd19;

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bitLen = msg.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bitLen >> (i * 8)));

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[offset+i*4]<<24)|((uint32_t)msg[offset+i*4+1]<<16)|
                   ((uint32_t)msg[offset+i*4+2]<<8)|((uint32_t)msg[offset+i*4+3]);
        for (int i = 16; i < 64; i++)
            w[i] = sha256_gamma1(w[i-2]) + w[i-7] + sha256_gamma0(w[i-15]) + w[i-16];

        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,h=h7;
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + sha256_sigma1(e) + sha256_ch(e,f,g) + SHA256_K[i] + w[i];
            uint32_t t2 = sha256_sigma0(a) + sha256_maj(a,b,c);
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e; h5+=f; h6+=g; h7+=h;
    }

    char buf[65];
    snprintf(buf, sizeof(buf), "%08x%08x%08x%08x%08x%08x%08x%08x",
             h0, h1, h2, h3, h4, h5, h6, h7);
    return std::string(buf);
}

// --- MD5 (RFC 1321) ---
static const uint32_t MD5_S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};
static const uint32_t MD5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static uint32_t md5_rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

static std::string compute_md5(const std::string& input) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bitLen = msg.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 0; i < 8; i++) msg.push_back((uint8_t)(bitLen >> (i * 8)));

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = ((uint32_t)msg[offset+i*4]) | ((uint32_t)msg[offset+i*4+1]<<8) |
                   ((uint32_t)msg[offset+i*4+2]<<16) | ((uint32_t)msg[offset+i*4+3]<<24);

        uint32_t A=a0, B=b0, C=c0, D=d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16)      { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5*i+1)%16; }
            else if (i < 48) { F = B ^ C ^ D;           g = (3*i+5)%16; }
            else              { F = C ^ (B | ~D);        g = (7*i)%16; }
            F = F + A + MD5_K[i] + M[g];
            A = D; D = C; C = B; B = B + md5_rotl(F, MD5_S[i]);
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    auto to_hex_le = [](uint32_t v) -> std::string {
        char buf[9];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x",
                 v & 0xff, (v>>8)&0xff, (v>>16)&0xff, (v>>24)&0xff);
        return std::string(buf);
    };
    return to_hex_le(a0) + to_hex_le(b0) + to_hex_le(c0) + to_hex_le(d0);
}

// --- Base64 ---
static const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::string& input) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string base64_decode(const std::string& input) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(int)BASE64_CHARS[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((char)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
} // end anonymous namespace

struct FluxCryptoNS {
    std::string sha256(const std::string& s) { return compute_sha256(s); }
    std::string md5(const std::string& s) { return compute_md5(s); }
};
static FluxCryptoNS Crypto;

struct FluxBase64NS {
    std::string encode(const std::string& s) { return base64_encode(s); }
    std::string decode(const std::string& s) { return base64_decode(s); }
};
static FluxBase64NS Base64;
// =================== End std.crypto ======================

)";
    }

    // Emit std.regex stubs if imported
    if (importedModules.count("std.regex")) {
        output << R"(
// ====================== std.regex Stubs ======================
struct Regex {
    std::string pattern;
    std::regex _re;
    Regex(const std::string& pat) : pattern(pat), _re(pat) {}
    bool match(const std::string& str) { return std::regex_match(str, _re); }
    std::string search(const std::string& str) {
        std::smatch m; return std::regex_search(str, m, _re) ? m[0].str() : "";
    }
    std::vector<std::string> findAll(const std::string& str) {
        std::vector<std::string> out;
        auto begin = std::sregex_iterator(str.begin(), str.end(), _re);
        for (auto it = begin; it != std::sregex_iterator(); ++it) out.push_back((*it)[0].str());
        return out;
    }
    std::string replace(const std::string& str, const std::string& rep) {
        return std::regex_replace(str, _re, rep);
    }
    std::vector<std::string> split(const std::string& str) {
        std::vector<std::string> out;
        std::sregex_token_iterator it(str.begin(), str.end(), _re, -1);
        for (; it != std::sregex_token_iterator(); ++it) out.push_back(*it);
        return out;
    }
    std::vector<std::string> groups(const std::string& str) {
        std::vector<std::string> out; std::smatch m;
        if (std::regex_search(str, m, _re))
            for (size_t i = 1; i < m.size(); ++i) out.push_back(m[i].str());
        return out;
    }
};
// =================== End std.regex Stubs ======================

)";
    }

    // Emit std.os stubs if imported
    if (importedModules.count("std.os")) {
        output << R"(
// ====================== std.os Stubs ======================
struct FluxOSNS {
    std::string exec(const std::string& cmd) {
        char buf[256]; std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (p) { while(fgets(buf, sizeof(buf), p)) out += buf; pclose(p); }
        return out;
    }
    int execStatus(const std::string& cmd) { return WEXITSTATUS(system(cmd.c_str())); }
    std::string env(const std::string& key) { const char* v = std::getenv(key.c_str()); return v ? v : ""; }
    void setEnv(const std::string& key, const std::string& val) { setenv(key.c_str(), val.c_str(), 1); }
    std::string cwd() { char buf[4096]; return getcwd(buf, sizeof(buf)) ? buf : ""; }
    void chdir(const std::string& path) { ::chdir(path.c_str()); }
    int pid() { return (int)getpid(); }
    std::string hostname() { char buf[256]; gethostname(buf, sizeof(buf)); return buf; }
    std::string username() { const char* u = std::getenv("USER"); return u ? u : "unknown"; }
    std::string tempDir() { return "/tmp"; }
    std::string platform =
#if defined(__linux__)
        "linux";
#elif defined(__APPLE__)
        "macos";
#elif defined(_WIN32)
        "windows";
#else
        "unknown";
#endif
};
static FluxOSNS OS;
// =================== End std.os Stubs ======================

)";
    }

    // Emit std.gpu stubs if imported
    if (importedModules.count("std.gpu")) {
        output << R"(
// ====================== std.gpu (CPU Fallback) ======================
// No CUDA/ROCm/Vulkan at AOT compile time. Buffer operations use CPU memory
// so code that conditionally uses GPU still functions correctly.

struct FluxGPUBufferNS {
    int size = 0;
    double* _data = nullptr;
    std::string _backend = "cpu";

    FluxGPUBufferNS() = default;
    FluxGPUBufferNS(int sz) : size(sz), _data(new double[sz]()) {}
    ~FluxGPUBufferNS() { delete[] _data; }

    // Allow move semantics
    FluxGPUBufferNS(FluxGPUBufferNS&& o) noexcept : size(o.size), _data(o._data), _backend(o._backend) {
        o._data = nullptr; o.size = 0;
    }
    FluxGPUBufferNS& operator=(FluxGPUBufferNS&& o) noexcept {
        if (this != &o) { delete[] _data; _data = o._data; size = o.size; _backend = o._backend; o._data = nullptr; o.size = 0; }
        return *this;
    }
    FluxGPUBufferNS(const FluxGPUBufferNS&) = delete;
    FluxGPUBufferNS& operator=(const FluxGPUBufferNS&) = delete;
};

struct FluxGPUNS {
    bool available = false;
    std::string backend = "cpu_fallback";

    bool isAvailable() { return false; } // No real GPU
    int deviceCount() { return 0; }
    std::string deviceName(int id = 0) { return "cpu_fallback"; }

    FluxGPUBufferNS allocate(int sizeInFloats) {
        return FluxGPUBufferNS(sizeInFloats);
    }

    void memcpyToDevice(FluxGPUBufferNS& buf, const std::vector<double>& data) {
        if (buf._data) {
            int n = std::min((int)data.size(), buf.size);
            for (int i = 0; i < n; i++) buf._data[i] = data[i];
        }
    }

    std::vector<double> memcpyToHost(const FluxGPUBufferNS& buf) {
        std::vector<double> out(buf.size, 0.0);
        if (buf._data) {
            for (int i = 0; i < buf.size; i++) out[i] = buf._data[i];
        }
        return out;
    }

    void free(FluxGPUBufferNS& buf) { delete[] buf._data; buf._data = nullptr; buf.size = 0; }
    void sync() {} // No-op for CPU
};
static FluxGPUNS GPU;
// =================== End std.gpu ======================

)";
    }

    } // end if (!freestandingMode) — std library stubs

    // Forward declarations
    if (forward.str().size() > 0) {
        output << forward.str() << "\n";
    }

    // Function definitions (includes classes, enums, and functions)
    if (functions.str().size() > 0) {
        output << functions.str() << "\n";
    }

    // Main function — only in hosted mode
    // In freestanding mode, the entry point is defined by the kernel code itself
    if (!freestandingMode) {
        output << "int main() {\n";
        output << body.str();
        output << "    return 0;\n";
        output << "}\n";
    } else {
        // In freestanding mode, emit any top-level code as a global init
        // (StratOS defines its own kernel_main via export func)
        if (!body.str().empty()) {
            output << "// Top-level code (freestanding)\n";
            output << body.str();
        }
    }

    return output.str();
}

// ============================================================================
// Full compile pipeline
// ============================================================================

bool Transpiler::compile(const std::string& fluxSource,
                         const std::string& outputPath,
                         const std::string& optimizationLevel,
                         bool devMode) {
    // Check for freestanding mode via DOCTYPE directive
    freestandingMode = hasDocTypeAOT(fluxSource);

    // Lex
    Lexer lexer(fluxSource, "<compile>");
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (auto& err : lexer.getErrors()) std::cerr << err << std::endl;
        return false;
    }

    // Parse
    Parser parser(tokens, "<compile>");
    auto program = parser.parse();
    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors()) std::cerr << err << std::endl;
        return false;
    }

    // Transpile to C++
    std::string cppSource = transpile(program);

    // Write temp C++ file
    std::string tmpFile = outputPath + ".cpp";
    {
        std::ofstream out(tmpFile);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot write temporary file: " << tmpFile << std::endl;
            return false;
        }
        out << cppSource;
    }

    // Compile with g++
    // Build link flags based on what stdlib modules are imported
    std::string linkFlags;
    if (importedModules.count("std.graphics")) {
        linkFlags += " -lSDL2 -lSDL2_ttf -lSDL2_image -lglfw -lGL -lGLU";
    }
    if (importedModules.count("std.audio")) {
        linkFlags += " -lSDL2 -lSDL2_mixer";
    }
    if (importedModules.count("std.video")) {
        linkFlags += " -lSDL2 -lSDL2_mixer -lavcodec -lavformat -lswscale -lswresample -lavutil -lGL -lGLU";
    }
    if (importedModules.count("std.sys")) {
        linkFlags += " -lpthread";
    }
    if (importedModules.count("std.io")) {
        linkFlags += " -lstdc++fs";
    }
    if (importedModules.count("std.net")) {
        linkFlags += " -lpthread";
    }

    std::string cmd;
    if (freestandingMode) {
        // Freestanding compilation for OS/kernel — use cross-compiler if available
        std::string cxx = "g++";
        // Check for x86_64-elf-g++
        if (std::system("which x86_64-elf-g++ > /dev/null 2>&1") == 0) {
            cxx = "x86_64-elf-g++";
        }
        cmd = cxx + " -std=c++17 " + optimizationLevel +
              " -ffreestanding -fno-exceptions -fno-rtti -nostdlib"
              " -mno-red-zone"
              " -mcmodel=kernel -fno-pic -fno-stack-protector"
              " -fpermissive -Wno-write-strings -Wno-narrowing"
              " -c -o " + outputPath + " " + tmpFile + " 2>&1";
    } else {
        cmd = "g++ -std=c++17 " + optimizationLevel +
              " -o " + outputPath + " " + tmpFile +
              linkFlags + " 2>&1";
    }

    // Capture g++ output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: Failed to run g++" << std::endl;
        std::filesystem::remove(tmpFile);
        return false;
    }

    std::string gccOutput;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        gccOutput += buffer;
    }
    int result = pclose(pipe);

    // Clean up temp file on success; keep on failure for debugging
    if (result == 0) {
        if (devMode) {
            // Keep generated C++ source in --dev mode
            std::filesystem::copy(tmpFile, outputPath + ".gen.cpp",
                std::filesystem::copy_options::overwrite_existing);
        }
        std::filesystem::remove(tmpFile);
        std::cout << "Compiled: " << outputPath << std::endl;
    } else {
        std::cerr << "Compilation failed. C++ source kept at: " << tmpFile << std::endl;
        // Clean up error messages: remove .cpp extension references
        size_t pos = 0;
        while ((pos = gccOutput.find(tmpFile, pos)) != std::string::npos) {
            gccOutput.replace(pos, tmpFile.length(), outputPath);
            pos += outputPath.length();
        }
        std::cerr << gccOutput;
    }

    return result == 0;
}

// ============================================================================
// Node emission dispatch
// ============================================================================

void Transpiler::emitNode(ASTNodePtr node, std::stringstream& out) {
    if (!node) return;

    switch (node->nodeType) {
        case NodeType::PROGRAM:
            emitProgram(std::dynamic_pointer_cast<ProgramNode>(node), out);
            break;
        case NodeType::VAR_DECL:
            emitVarDecl(std::dynamic_pointer_cast<VarDeclNode>(node), out);
            break;
        case NodeType::FUNC_DECL:
            emitFuncDecl(std::dynamic_pointer_cast<FuncDeclNode>(node));
            break;
        case NodeType::CLASS_DECL:
            emitClassDecl(std::dynamic_pointer_cast<ClassDeclNode>(node));
            break;
        case NodeType::ENUM_DECL:
            emitEnumDecl(std::dynamic_pointer_cast<EnumDeclNode>(node));
            break;
        case NodeType::BLOCK:
            emitBlock(std::dynamic_pointer_cast<BlockNode>(node), out);
            break;
        case NodeType::IF_STMT:
            emitIf(std::dynamic_pointer_cast<IfStmtNode>(node), out);
            break;
        case NodeType::SWITCH_STMT:
            emitSwitch(std::dynamic_pointer_cast<SwitchStmtNode>(node), out);
            break;
        case NodeType::WHILE_STMT:
            emitWhile(std::dynamic_pointer_cast<WhileStmtNode>(node), out);
            break;
        case NodeType::DO_WHILE_STMT:
            emitDoWhile(std::dynamic_pointer_cast<DoWhileStmtNode>(node), out);
            break;
        case NodeType::FOR_STMT:
            emitFor(std::dynamic_pointer_cast<ForStmtNode>(node), out);
            break;
        case NodeType::FOR_EACH_STMT:
            emitForEach(std::dynamic_pointer_cast<ForEachStmtNode>(node), out);
            break;
        case NodeType::RETURN_STMT:
            emitReturn(std::dynamic_pointer_cast<ReturnStmtNode>(node), out);
            break;
        case NodeType::EXPRESSION_STMT:
            emitExprStmt(std::dynamic_pointer_cast<ExpressionStmtNode>(node), out);
            break;
        case NodeType::TRY_CATCH:
            emitTryCatch(std::dynamic_pointer_cast<TryCatchNode>(node), out);
            break;
        case NodeType::THROW_STMT:
            emitThrow(std::dynamic_pointer_cast<ThrowStmtNode>(node), out);
            break;
        case NodeType::BREAK_STMT:
            out << indent() << "break;\n";
            break;
        case NodeType::CONTINUE_STMT:
            out << indent() << "continue;\n";
            break;
        case NodeType::IMPORT_STMT: {
            // Track imported modules for AOT stub generation
            auto imp = std::dynamic_pointer_cast<ImportStmtNode>(node);
            if (imp && !imp->path.empty()) {
                if (imp->path.rfind("std.", 0) == 0) {
                    // Standard library module import
                    importedModules.insert(imp->path);
                } else {
                    // File import — resolve and inline the imported declarations
                    resolveImport(imp->path, out);
                }
            }
            break;
        }
        case NodeType::STRUCT_DECL: {
            auto st = std::dynamic_pointer_cast<StructDeclNode>(node);
            if (st) {
                forward << "struct " << st->name << ";\n";
                functions << "struct " << st->name << " {\n";
                for (auto& field : st->fields) {
                    std::string ft = fluxTypeToC(field.typeName);
                    functions << "    " << ft << " " << field.name << " = {};\n";
                }
                functions << "};\n\n";
            }
            break;
        }
        case NodeType::EXPORT_STMT: {
            auto exp = std::dynamic_pointer_cast<ExportStmtNode>(node);
            if (exp && exp->declaration) {
                emitNode(exp->declaration, out);
            }
            break;
        }
        case NodeType::UNSAFE_BLOCK: {
            auto unsafe = std::dynamic_pointer_cast<UnsafeBlockNode>(node);
            if (unsafe && unsafe->body) {
                out << indent() << "// unsafe block\n";
                emitNode(unsafe->body, out);
            }
            break;
        }
        case NodeType::PANIC_STMT: {
            auto panic = std::dynamic_pointer_cast<PanicStmtNode>(node);
            if (panic) {
                if (freestandingMode) {
                    out << indent() << "flux_panic(\"PANIC\");\n";
                } else {
                    out << indent() << "{ std::cerr << \"PANIC: \" << " << emitExpr(panic->message) << " << std::endl; std::abort(); }\n";
                }
            }
            break;
        }
        case NodeType::ASM_STMT: {
            auto asmNode = std::dynamic_pointer_cast<AsmStmtNode>(node);
            if (asmNode) {
                out << indent() << "__asm__ volatile(\"" << asmNode->asmString << "\"";
                bool hasOutputs = !asmNode->outputs.empty();
                bool hasInputs = !asmNode->inputs.empty();
                bool hasClobbers = !asmNode->clobbers.empty();
                if (hasOutputs || hasInputs || hasClobbers) {
                    // Output operands
                    out << " : ";
                    for (size_t i = 0; i < asmNode->outputs.size(); i++) {
                        if (i > 0) out << ", ";
                        out << "\"" << asmNode->outputs[i].constraint << "\"(";
                        out << emitExpr(asmNode->outputs[i].expr);
                        out << ")";
                    }
                    if (hasInputs || hasClobbers) {
                        // Input operands
                        out << " : ";
                        for (size_t i = 0; i < asmNode->inputs.size(); i++) {
                            if (i > 0) out << ", ";
                            out << "\"" << asmNode->inputs[i].constraint << "\"(";
                            out << emitExpr(asmNode->inputs[i].expr);
                            out << ")";
                        }
                        if (hasClobbers) {
                            // Clobber list
                            out << " : ";
                            for (size_t i = 0; i < asmNode->clobbers.size(); i++) {
                                if (i > 0) out << ", ";
                                out << "\"" << asmNode->clobbers[i] << "\"";
                            }
                        }
                    }
                }
                out << ");\n";
            }
            break;
        }
        case NodeType::DEREF_ASSIGN: {
            auto da = std::dynamic_pointer_cast<DerefAssignNode>(node);
            if (da) {
                out << indent() << "*(";
                emitNode(da->pointer, out);
                out << ") " << da->op.lexeme << " ";
                emitNode(da->value, out);
                out << ";\n";
            }
            break;
        }
        default:
            out << indent() << "/* [unhandled node: " << (int)node->nodeType << "] */\n";
            break;
    }
}

void Transpiler::emitProgram(std::shared_ptr<ProgramNode> node, std::stringstream& out) {
    pushIndent();
    for (auto& decl : node->declarations) {
        emitNode(decl, out);
    }
    popIndent();
}

// ============================================================================
// Determine the C++ vector element type for a list variable
// Based on the declared Flux type or list literal contents
// ============================================================================

static std::string inferVectorElementType(const std::string& fluxType,
                                          ASTNodePtr init,
                                          const std::set<std::string>& classNames,
                                          bool freestanding = false) {
    std::string stringType = freestanding ? "FluxString" : "std::string";
    // If declared as list<X> or List<X>, extract X
    if (fluxType.rfind("list", 0) == 0 || fluxType.rfind("List", 0) == 0) {
        auto lt = fluxType.find('<');
        auto gt = fluxType.rfind('>');
        if (lt != std::string::npos && gt != std::string::npos) {
            std::string inner = fluxType.substr(lt + 1, gt - lt - 1);
            if (inner == "int") return "int32_t";
            if (inner == "long") return "int64_t";
            if (inner == "float") return "double";
            if (inner == "string") return stringType;
            if (inner == "bool") return "bool";
            if (inner == "byte") return "uint8_t";
            if (inner == "char") return "char";
            if (inner == "func") return freestanding ? "FuncPtr" : "std::function<void()>";
            if (classNames.count(inner)) return inner;
            return inner;
        }
        // bare "list" — fall through to infer from initializer elements
    }

    // Declared as a primitive type but initialized with []
    if (fluxType == "int") return "int32_t";
    if (fluxType == "long") return "int64_t";
    if (fluxType == "float") return "double";
    if (fluxType == "string") return stringType;
    if (fluxType == "bool") return "bool";
    if (classNames.count(fluxType)) return fluxType;

    // Try to infer from list literal elements
    // Scan ALL elements: if any element is a float literal, use double
    if (init && init->nodeType == NodeType::LIST_LITERAL) {
        auto list = std::dynamic_pointer_cast<ListLiteralNode>(init);
        if (!list->elements.empty()) {
            bool hasFloat = false;
            bool hasString = false;
            bool hasLong = false;
            bool hasBool = false;
            bool hasInt = false;
            bool hasChar = false;
            std::string structType;
            for (auto& elem : list->elements) {
                if (elem->nodeType == NodeType::LITERAL) {
                    auto lit = std::dynamic_pointer_cast<LiteralNode>(elem);
                    switch (lit->litType) {
                        case LiteralNode::FLOAT_LIT: hasFloat = true; break;
                        case LiteralNode::STRING_LIT: hasString = true; break;
                        case LiteralNode::LONG_LIT: hasLong = true; break;
                        case LiteralNode::BOOL_LIT: hasBool = true; break;
                        case LiteralNode::INT_LIT: hasInt = true; break;
                        case LiteralNode::CHAR_LIT: hasChar = true; break;
                        default: break;
                    }
                } else if (elem->nodeType == NodeType::NEW_EXPR) {
                    auto newExpr = std::dynamic_pointer_cast<NewExprNode>(elem);
                    if (newExpr && !newExpr->className.empty()) {
                        structType = newExpr->className;
                    }
                } else if (elem->nodeType == NodeType::CALL) {
                    // Calls to math.sin, math.cos, etc. return double
                    auto call = std::dynamic_pointer_cast<CallNode>(elem);
                    if (call && call->callee->nodeType == NodeType::MEMBER_ACCESS) {
                        auto mem = std::dynamic_pointer_cast<MemberAccessNode>(call->callee);
                        if (mem && mem->object->nodeType == NodeType::VARIABLE) {
                            auto v = std::dynamic_pointer_cast<VariableNode>(mem->object);
                            if (v && v->name == "math") hasFloat = true;
                        }
                    }
                } else if (elem->nodeType == NodeType::CAST) {
                    // (float) cast -> double
                    auto cast = std::dynamic_pointer_cast<CastNode>(elem);
                    if (cast && cast->targetType == "float") hasFloat = true;
                } else if (elem->nodeType == NodeType::BINARY) {
                    // If any binary op involves a float literal, treat as float
                    auto bin = std::dynamic_pointer_cast<BinaryNode>(elem);
                    if (bin) {
                        auto checkFloat = [](ASTNodePtr n) -> bool {
                            if (n->nodeType == NodeType::LITERAL) {
                                auto l = std::dynamic_pointer_cast<LiteralNode>(n);
                                return l && l->litType == LiteralNode::FLOAT_LIT;
                            }
                            return false;
                        };
                        if (checkFloat(bin->left) || checkFloat(bin->right)) hasFloat = true;
                    }
                }
            }
            // Priority: struct > string > double > long > int > char > bool
            if (!structType.empty()) return structType;
            if (hasString) return stringType;
            if (hasFloat) return "double";
            if (hasLong) return "int64_t";
            if (hasInt) return "int32_t";
            if (hasChar) return "char";
            if (hasBool) return "bool";
        }
    }

    return "int32_t"; // fallback
}

// ============================================================================
// Variable declaration
// ============================================================================

void Transpiler::emitVarDecl(std::shared_ptr<VarDeclNode> node, std::stringstream& out) {
    // Check if this is a list variable (initialized with list literal or typed as list<X>)
    bool isList = false;
    if (node->initializer && node->initializer->nodeType == NodeType::LIST_LITERAL) {
        isList = true;
    }
    if (!node->typeName.empty() && (node->typeName.rfind("list", 0) == 0 || node->typeName.rfind("List", 0) == 0)) {
        isList = true;
    }

    if (isList) {
        // Track as list variable
        listVars.insert(node->name);

        std::string elemType = inferVectorElementType(
            node->typeName, node->initializer, classNames, freestandingMode);
        std::string vecType = "std::vector<" + elemType + ">";

        if (node->initializer &&
            node->initializer->nodeType == NodeType::LIST_LITERAL) {
            auto list = std::dynamic_pointer_cast<ListLiteralNode>(node->initializer);
            if (list->elements.empty()) {
                out << indent() << vecType << " " << node->name << ";\n";
            } else {
                out << indent() << vecType << " " << node->name << " = {";
                for (size_t i = 0; i < list->elements.size(); i++) {
                    if (i > 0) out << ", ";
                    out << emitExpr(list->elements[i]);
                }
                out << "};\n";
            }
        } else if (node->initializer) {
            out << indent() << vecType << " " << node->name
                << " = " << emitExpr(node->initializer) << ";\n";
        } else {
            out << indent() << vecType << " " << node->name << ";\n";
        }
        return;
    }

    // Regular (non-list) variable
    std::string ctype = "auto";
    if (!node->typeName.empty()) {
        ctype = fluxTypeToC(node->typeName);
    }

    // When type is 'object', try to resolve to a concrete stdlib struct type
    // from the initializer (e.g., object client = HttpClient() -> HttpClient client = ...)
    if (ctype == "FluxObject" || node->typeName == "object") {
        static const std::set<std::string> stdlibConstructors = {
            "Window", "Timer", "Socket", "Map", "Stack", "Queue",
            "HttpClient", "Response"
        };

        bool resolvedConcrete = false;
        if (node->initializer && node->initializer->nodeType == NodeType::CALL) {
            auto call = std::dynamic_pointer_cast<CallNode>(node->initializer);
            if (call && call->callee->nodeType == NodeType::VARIABLE) {
                auto varN = std::dynamic_pointer_cast<VariableNode>(call->callee);
                if (varN && stdlibConstructors.count(varN->name)) {
                    // Use the concrete struct type instead of FluxObject
                    ctype = varN->name;
                    resolvedConcrete = true;
                }
            }
            // Auto-detect FluxObject from JSON.parse() calls
            if (call && call->callee->nodeType == NodeType::MEMBER_ACCESS) {
                auto mem = std::dynamic_pointer_cast<MemberAccessNode>(call->callee);
                if (mem) {
                    auto varN = std::dynamic_pointer_cast<VariableNode>(mem->object);
                    if (varN && varN->name == "JSON" && mem->member == "parse") {
                        objectVars.insert(node->name);
                        ctype = "FluxObject";
                        resolvedConcrete = true;
                    }
                }
            }
        }

        // Also check if initializer is a new ClassName() expression
        if (!resolvedConcrete && node->initializer &&
            node->initializer->nodeType == NodeType::NEW_EXPR) {
            auto newExpr = std::dynamic_pointer_cast<NewExprNode>(node->initializer);
            if (newExpr && (stdlibConstructors.count(newExpr->className) ||
                           classNames.count(newExpr->className))) {
                ctype = newExpr->className;
                resolvedConcrete = true;
            }
        }

        // If we couldn't resolve to a concrete type, fall back to FluxObject
        if (!resolvedConcrete) {
            objectVars.insert(node->name);
            ctype = "FluxObject";
            needsFluxObject = true;
        }
    }

    // Don't emit C++ const — Flux const enforcement is runtime behavior
    // that interacts with try/catch (assigning to const throws at runtime)

    // Track pointer variables for -> member access
    if (!node->typeName.empty() && node->typeName.find('*') != std::string::npos) {
        pointerVars.insert(node->name);
    }

    if (node->initializer) {
        std::string expr = emitExpr(node->initializer);
        if (ctype == "auto" && expr == "0") ctype = "int32_t";
        // Handle C-style array type: e.g. bool[256] -> bool name[256]
        auto arrBracket = ctype.find('[');
        if (arrBracket != std::string::npos) {
            std::string baseType = ctype.substr(0, arrBracket);
            std::string arrSize = ctype.substr(arrBracket);
            out << indent() << baseType << " " << node->name << arrSize << " = " << expr << ";\n";
        } else {
            out << indent() << ctype << " " << node->name << " = " << expr << ";\n";
        }
    } else {
        if (ctype == "auto") ctype = "int32_t";
        // Handle C-style array type: e.g. bool[256] -> bool name[256]
        auto arrBracket = ctype.find('[');
        if (arrBracket != std::string::npos) {
            std::string baseType = ctype.substr(0, arrBracket);
            std::string arrSize = ctype.substr(arrBracket);
            out << indent() << baseType << " " << node->name << arrSize << " = {};\n";
        } else {
            out << indent() << ctype << " " << node->name << " = {};\n";
        }
    }
}

// ============================================================================
// Function declaration
// ============================================================================

void Transpiler::emitFuncDecl(std::shared_ptr<FuncDeclNode> node) {
    // "main" function: merge body into the generated main()
    if (node->name == "main") {
        if (node->body) {
            pushIndent();
            emitNode(node->body, body);
            popIndent();
        }
        return;
    }

    std::string retType = "auto";
    if (!node->returnType.empty()) {
        retType = fluxTypeToC(node->returnType);
    } else {
        retType = "int32_t";
    }

    // Build parameter list
    auto emitParams = [&](std::stringstream& ss) {
        for (size_t i = 0; i < node->params.size(); i++) {
            if (i > 0) ss << ", ";
            std::string ptype = "int32_t";
            if (!node->params[i].typeName.empty())
                ptype = fluxTypeToC(node->params[i].typeName);
            if (ptype == "auto") ptype = "int32_t";
            // Pass vectors, class types, and stdlib struct types by reference
            bool byRef = false;
            if (ptype.find("std::vector") == 0) byRef = true;
            if (classNames.count(node->params[i].typeName)) byRef = true;
            static const std::set<std::string> refTypes = {
                "Window", "Timer", "Socket", "Map", "Stack", "Queue",
                "HttpClient", "Response", "FluxObject"
            };
            if (refTypes.count(node->params[i].typeName)) byRef = true;
            ss << ptype << (byRef ? "& " : " ") << node->params[i].name;
        }
    };

    // Forward declaration
    if (freestandingMode && (node->name == "kernel_main" || node->name == "isr_dispatch" || node->name == "syscall_dispatch")) {
        forward << "extern \"C\" ";
    }
    forward << retType << " " << node->name << "(";
    emitParams(forward);
    forward << ");\n";

    // Track pointer parameters for -> member access
    // Save current pointer vars and add function-scoped ones
    pointerVarsStack.push_back(pointerVars);
    for (auto& p : node->params) {
        if (!p.typeName.empty() && p.typeName.find('*') != std::string::npos) {
            pointerVars.insert(p.name);
        }
    }

    // Function definition
    if (freestandingMode && (node->name == "kernel_main" || node->name == "isr_dispatch" || node->name == "syscall_dispatch")) {
        functions << "extern \"C\" ";
    }
    functions << retType << " " << node->name << "(";
    emitParams(functions);
    functions << ") {\n";

    if (node->body) {
        bool wasTopLevel = inTopLevel;
        inTopLevel = false;  // Entering function body
        pushIndent();
        emitNode(node->body, functions);
        popIndent();
        inTopLevel = wasTopLevel;  // Restore context
    }

    // Restore pointer vars to pre-function state
    pointerVars = pointerVarsStack.back();
    pointerVarsStack.pop_back();

    // Safety return to prevent UB from falling off a non-void function.
    // Flux defaults to int return with auto-return 0 per the language spec.
    if (retType == "int32_t" || retType == "int64_t") {
        functions << "    return 0;\n";
    } else if (retType == "double") {
        functions << "    return 0.0;\n";
    } else if (retType == "bool") {
        functions << "    return false;\n";
    } else if (retType == "std::string") {
        functions << "    return \"\";\n";
    }

    functions << "}\n\n";
}

// ============================================================================
// Class declaration — emitted as a C++ struct with member functions
// ============================================================================

void Transpiler::emitClassDecl(std::shared_ptr<ClassDeclNode> node) {
    // Forward declare
    forward << "struct " << node->name << ";\n";

    functions << "struct " << node->name;
    if (!node->parentClass.empty()) {
        functions << " : public " << node->parentClass;
    }
    functions << " {\n";

    // Fields first
    for (auto& member : node->members) {
        if (member.isField) {
            std::string ftype = "int32_t";
            if (!member.fieldType.empty()) ftype = fluxTypeToC(member.fieldType);
            functions << "    " << (member.isStatic ? "inline static " : "")
                      << ftype << " " << member.fieldName;
            if (member.fieldInit) {
                functions << " = " << emitExpr(member.fieldInit);
            } else {
                functions << " = {}";
            }
            functions << ";\n";
        }
    }

    // Methods — set currentParentClass so super.init() can resolve
    std::string savedParent = currentParentClass;
    currentParentClass = node->parentClass;

    for (auto& member : node->members) {
        if (!member.isField && member.method) {
            auto fn = std::dynamic_pointer_cast<FuncDeclNode>(member.method);
            if (!fn) continue;

            std::string retType = "void";
            if (fn->name == "init") {
                retType = "void";
            } else if (!fn->returnType.empty()) {
                retType = fluxTypeToC(fn->returnType);
            } else {
                retType = "int32_t";
            }

            functions << "    " << (member.isStatic ? "static " : "") << retType << " " << fn->name << "(";
            for (size_t i = 0; i < fn->params.size(); i++) {
                if (i > 0) functions << ", ";
                std::string ptype = "int32_t";
                if (!fn->params[i].typeName.empty())
                    ptype = fluxTypeToC(fn->params[i].typeName);
                if (ptype == "auto") ptype = "int32_t";                bool byRef = false;
                if (ptype.find("std::vector") == 0) byRef = true;
                if (classNames.count(fn->params[i].typeName)) byRef = true;
                static const std::set<std::string> refTypes = {
                    "Window", "Timer", "Socket", "Map", "Stack", "Queue",
                    "HttpClient", "Response", "FluxObject"
                };
                if (refTypes.count(fn->params[i].typeName)) byRef = true;
                functions << ptype << (byRef ? "& " : " ") << fn->params[i].name;
            }
            functions << ") {\n";

            // Push pointer scope for this method (prevents cross-method pollution)
            pointerVarsStack.push_back(pointerVars);

            // Track pointer parameters in class methods for -> member access
            for (auto& p : fn->params) {
                if (!p.typeName.empty() && p.typeName.find('*') != std::string::npos) {
                    pointerVars.insert(p.name);
                }
            }

            if (fn->body) {
                bool wasTopLevel = inTopLevel;
                inTopLevel = false;  // Entering method body
                pushIndent();
                pushIndent();
                emitNode(fn->body, functions);
                popIndent();
                popIndent();
                inTopLevel = wasTopLevel;  // Restore context
            }

            // Pop pointer scope — restore parent scope
            pointerVars = pointerVarsStack.back();
            pointerVarsStack.pop_back();

            // Safety return for non-void methods to prevent UB
            if (retType == "int32_t" || retType == "int64_t") {
                functions << "        return 0;\n";
            } else if (retType == "double") {
                functions << "        return 0.0;\n";
            } else if (retType == "bool") {
                functions << "        return false;\n";
            } else if (retType == "std::string") {
                functions << "        return \"\";\n";
            }

            functions << "    }\n";
        }
    }

    currentParentClass = savedParent;
    functions << "};\n\n";

    // With inline static, no out-of-line definitions are needed
    // (inline static allows in-class initialization in C++17)
    functions << "\n";
}

// ============================================================================
// Enum declaration — emitted as a namespace with int constants
// ============================================================================

void Transpiler::emitEnumDecl(std::shared_ptr<EnumDeclNode> node) {
    functions << "namespace " << node->name << " {\n";
    int nextVal = 0;
    std::vector<std::pair<std::string, int>> computed;
    for (auto& m : node->members) {
        int val = m.hasValue ? m.value : nextVal;
        computed.push_back({m.name, val});
        functions << "    static const int32_t " << m.name << " = " << val << ";\n";
        nextVal = val + 1;
    }
    // Name lookup function
    functions << "    static std::string name(int32_t v) {\n";
    for (auto& [name, val] : computed) {
        functions << "        if (v == " << val << ") return \"" << name << "\";\n";
    }
    functions << "        return \"UNKNOWN\";\n";
    functions << "    }\n";
    functions << "}\n\n";
}

// ============================================================================
// Block and control flow statements
// ============================================================================

void Transpiler::emitBlock(std::shared_ptr<BlockNode> node, std::stringstream& out) {
    for (auto& stmt : node->statements) {
        emitNode(stmt, out);
    }
}

void Transpiler::emitIf(std::shared_ptr<IfStmtNode> node, std::stringstream& out) {
    out << indent() << "if (" << emitExpr(node->condition) << ") {\n";
    pushIndent();
    emitNode(node->thenBranch, out);
    popIndent();
    out << indent() << "}";
    for (auto& [elifCond, elifBody] : node->elifBranches) {
        out << " else if (" << emitExpr(elifCond) << ") {\n";
        pushIndent();
        emitNode(elifBody, out);
        popIndent();
        out << indent() << "}";
    }
    if (node->elseBranch) {
        out << " else {\n";
        pushIndent();
        emitNode(node->elseBranch, out);
        popIndent();
        out << indent() << "}";
    }
    out << "\n";
}

void Transpiler::emitSwitch(std::shared_ptr<SwitchStmtNode> node, std::stringstream& out) {
    // Emit as if/else-if chain instead of switch to handle return properly
    bool first = true;
    std::string expr = emitExpr(node->expr);
    std::string tmpName = tempVar();
    out << indent() << "auto " << tmpName << " = " << expr << ";\n";

    for (auto& c : node->cases) {
        if (c.isDefault) {
            if (!first) out << " else ";
            else out << indent();
            out << "{\n";
        } else {
            if (!first) out << " else ";
            else out << indent();
            out << "if (" << tmpName << " == " << emitExpr(c.value) << ") {\n";
        }
        pushIndent();
        for (auto& stmt : c.body) {
            // Skip break statements — they are unnecessary in if/else chains
            if (stmt->nodeType == NodeType::BREAK_STMT) continue;
            emitNode(stmt, out);
        }
        popIndent();
        out << indent() << "}";
        first = false;
    }
    out << "\n";
}

void Transpiler::emitWhile(std::shared_ptr<WhileStmtNode> node, std::stringstream& out) {
    out << indent() << "while (" << emitExpr(node->condition) << ") {\n";
    pushIndent();
    emitNode(node->body, out);
    popIndent();
    out << indent() << "}\n";
}

void Transpiler::emitDoWhile(std::shared_ptr<DoWhileStmtNode> node, std::stringstream& out) {
    out << indent() << "do {\n";
    pushIndent();
    emitNode(node->body, out);
    popIndent();
    out << indent() << "} while (" << emitExpr(node->condition) << ");\n";
}

void Transpiler::emitFor(std::shared_ptr<ForStmtNode> node, std::stringstream& out) {
    out << indent() << "for (";
    if (node->initializer) {
        std::stringstream initSS;
        emitNode(node->initializer, initSS);
        std::string initStr = initSS.str();
        // Trim trailing whitespace and semicolons
        while (!initStr.empty() && (initStr.back() == '\n' || initStr.back() == ' '))
            initStr.pop_back();
        if (!initStr.empty() && initStr.back() == ';') initStr.pop_back();
        // Trim leading whitespace
        size_t start = initStr.find_first_not_of(' ');
        if (start != std::string::npos) initStr = initStr.substr(start);
        out << initStr;
    }
    out << "; ";
    if (node->condition) out << emitExpr(node->condition);
    out << "; ";
    if (node->increment) out << emitExpr(node->increment);
    out << ") {\n";
    pushIndent();
    emitNode(node->body, out);
    popIndent();
    out << indent() << "}\n";
}

void Transpiler::emitForEach(std::shared_ptr<ForEachStmtNode> node, std::stringstream& out) {
    std::string iter = emitExpr(node->iterable);
    std::string varType = "auto&";
    if (!node->varType.empty()) {
        varType = fluxTypeToC(node->varType);
    }

    // If iterating with type "string" over a string (not a list), we need special handling
    // because iterating over std::string yields char, not std::string
    // But if the iterable is a list variable, use normal range-for
    bool isIterableList = false;
    if (node->iterable->nodeType == NodeType::VARIABLE) {
        auto varN = std::dynamic_pointer_cast<VariableNode>(node->iterable);
        if (varN && listVars.count(varN->name)) isIterableList = true;
    }
    if (node->varType == "string" && !isIterableList) {
        // Iterate character-by-character, converting each to string
        std::string idx = tempVar();
        out << indent() << "for (size_t " << idx << " = 0; "
            << idx << " < " << iter << ".size(); " << idx << "++) {\n";
        pushIndent();
        out << indent() << "std::string " << node->varName
            << "(1, " << iter << "[" << idx << "]);\n";
        emitNode(node->body, out);
        popIndent();
        out << indent() << "}\n";
    } else {
        out << indent() << "for (" << varType << " " << node->varName
            << " : " << iter << ") {\n";
        pushIndent();
        emitNode(node->body, out);
        popIndent();
        out << indent() << "}\n";
    }
}

void Transpiler::emitReturn(std::shared_ptr<ReturnStmtNode> node, std::stringstream& out) {
    if (node->value) {
        out << indent() << "return " << emitExpr(node->value) << ";\n";
    } else {
        // In top-level (main), bare return must be return 0
        if (inTopLevel) {
            out << indent() << "return 0;\n";
        } else {
            out << indent() << "return;\n";
        }
    }
}

void Transpiler::emitExprStmt(std::shared_ptr<ExpressionStmtNode> node, std::stringstream& out) {
    out << indent() << emitExpr(node->expression) << ";\n";
}

// ============================================================================
// Try-catch, throw
// ============================================================================

void Transpiler::emitTryCatch(std::shared_ptr<TryCatchNode> node, std::stringstream& out) {
    out << indent() << "try {\n";
    pushIndent();
    emitNode(node->tryBody, out);
    popIndent();
    out << indent() << "}";

    for (auto& clause : node->catchClauses) {
        std::string errName = clause.errorName.empty() ? "_flux_err" : clause.errorName;
        out << " catch (const FluxError& " << errName << ") {\n";
        pushIndent();
        emitNode(clause.body, out);
        popIndent();
        out << indent() << "}";
    }

    if (node->finallyBody) {
        out << "\n" << indent() << "/* finally */ {\n";
        pushIndent();
        emitNode(node->finallyBody, out);
        popIndent();
        out << indent() << "}";
    }
    out << "\n";
}

void Transpiler::emitThrow(std::shared_ptr<ThrowStmtNode> node, std::stringstream& out) {
    out << indent() << "throw FluxError(" << emitExpr(node->expr) << ");\n";
}

// ============================================================================
// Expression emission dispatch
// ============================================================================

std::string Transpiler::emitExpr(ASTNodePtr node) {
    if (!node) return "0";

    switch (node->nodeType) {
        case NodeType::LITERAL:
            return emitLiteral(std::dynamic_pointer_cast<LiteralNode>(node));
        case NodeType::VARIABLE:
            return emitVariable(std::dynamic_pointer_cast<VariableNode>(node));
        case NodeType::BINARY:
            return emitBinaryOp(std::dynamic_pointer_cast<BinaryNode>(node));
        case NodeType::UNARY:
            return emitUnaryOp(std::dynamic_pointer_cast<UnaryNode>(node));
        case NodeType::POSTFIX:
            return emitPostfix(std::dynamic_pointer_cast<PostfixNode>(node));
        case NodeType::CALL:
            return emitCall(std::dynamic_pointer_cast<CallNode>(node));
        case NodeType::MEMBER_ACCESS:
            return emitMemberAccess(std::dynamic_pointer_cast<MemberAccessNode>(node));
        case NodeType::MEMBER_SET:
            return emitMemberSet(std::dynamic_pointer_cast<MemberSetNode>(node));
        case NodeType::INDEX_ACCESS:
            return emitIndexAccess(std::dynamic_pointer_cast<IndexAccessNode>(node));
        case NodeType::INDEX_SET:
            return emitIndexSet(std::dynamic_pointer_cast<IndexSetNode>(node));
        case NodeType::ASSIGN:
            return emitAssignment(std::dynamic_pointer_cast<AssignNode>(node));
        case NodeType::CAST:
            return emitCast(std::dynamic_pointer_cast<CastNode>(node));
        case NodeType::NEW_EXPR:
            return emitNewExpr(std::dynamic_pointer_cast<NewExprNode>(node));
        case NodeType::LAMBDA:
            return emitLambda(std::dynamic_pointer_cast<LambdaNode>(node));
        case NodeType::LIST_LITERAL:
            return emitListLiteral(std::dynamic_pointer_cast<ListLiteralNode>(node));
        case NodeType::STRING_INTERPOLATION:
            return emitStringInterpolation(
                std::dynamic_pointer_cast<StringInterpolationNode>(node));
        case NodeType::TERNARY: {
            auto tern = std::dynamic_pointer_cast<TernaryNode>(node);
            return "(" + emitExpr(tern->condition) + " ? " +
                   emitExpr(tern->trueExpr) + " : " +
                   emitExpr(tern->falseExpr) + ")";
        }
        case NodeType::STRUCT_INIT: {
            auto si = std::dynamic_pointer_cast<StructInitNode>(node);
            std::string result;
            if (!si->structName.empty()) {
                result = si->structName + "{";
            } else {
                result = "{";
            }
            for (size_t i = 0; i < si->fields.size(); i++) {
                if (i > 0) result += ", ";
                if (!si->fields[i].first.empty()) {
                    result += "." + si->fields[i].first + " = ";
                }
                result += emitExpr(si->fields[i].second);
            }
            result += "}";
            return result;
        }
        case NodeType::DEREF_ASSIGN: {
            auto da = std::dynamic_pointer_cast<DerefAssignNode>(node);
            if (da) {
                return "*((" + emitExpr(da->pointer) + ")) " + da->op.lexeme + " " + emitExpr(da->value);
            }
            return "/* [deref_assign error] */ 0";
        }
        default:
            return "/* [expr " + std::to_string((int)node->nodeType) + "] */ 0";
    }
}

// ============================================================================
// Literal values
// ============================================================================

std::string Transpiler::emitLiteral(std::shared_ptr<LiteralNode> node) {
    switch (node->litType) {
        case LiteralNode::NULL_LIT: return "0";
        case LiteralNode::BOOL_LIT: return node->boolVal ? "true" : "false";
        case LiteralNode::INT_LIT: return std::to_string(node->intVal);
        case LiteralNode::LONG_LIT: return std::to_string(node->intVal) + "LL";
        case LiteralNode::FLOAT_LIT: {
            std::ostringstream oss;
            oss << node->floatVal;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
                s += ".0";
            return s;
        }
        case LiteralNode::STRING_LIT: {
            // Check if string contains $ interpolation markers
            if (node->stringVal.find('$') != std::string::npos) {
                return emitStringInterpFromRaw(node->stringVal);
            }
            std::string escaped;
            for (char c : node->stringVal) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\r') escaped += "\\r";
                else if (c == '\t') escaped += "\\t";
                else if (c == '\b') escaped += "\\b";
                else if (c == '\0') escaped += "\\0";
                else escaped += c;
            }
            return "std::string(\"" + escaped + "\")";
        }
        case LiteralNode::CHAR_LIT: {
            // Properly escape special characters for C++ char literals
            char c = node->charVal;
            switch (c) {
                case '\n': return "'\\n'";
                case '\t': return "'\\t'";
                case '\r': return "'\\r'";
                case '\b': return "'\\b'";
                case '\0': return "'\\0'";
                case '\\': return "'\\\\'";
                case '\'': return "'\\''";
                default:
                    return std::string("'") + c + "'";
            }
        }
        case LiteralNode::BYTE_LIT:
            return "static_cast<uint8_t>(" + std::to_string(node->intVal) + ")";
        default:
            return "0";
    }
}

std::string Transpiler::emitVariable(std::shared_ptr<VariableNode> node) {
    if (node->name == "true") return "true";
    if (node->name == "false") return "false";
    if (node->name == "nil" || node->name == "null") return "0";
    if (node->name == "inf") return "std::numeric_limits<double>::infinity()";
    return node->name;
}

// ============================================================================
// Binary operators — with string concatenation
// ============================================================================

std::string Transpiler::emitBinaryOp(std::shared_ptr<BinaryNode> node) {
    std::string left = emitExpr(node->left);
    std::string right = emitExpr(node->right);

    // String concatenation detection for PLUS
    if (node->op.type == TokenType::PLUS) {
        bool leftStr = false, rightStr = false;
        if (node->left->nodeType == NodeType::LITERAL) {
            auto lit = std::dynamic_pointer_cast<LiteralNode>(node->left);
            if (lit->litType == LiteralNode::STRING_LIT) leftStr = true;
        }
        if (node->left->nodeType == NodeType::STRING_INTERPOLATION) leftStr = true;
        if (node->right->nodeType == NodeType::LITERAL) {
            auto lit = std::dynamic_pointer_cast<LiteralNode>(node->right);
            if (lit->litType == LiteralNode::STRING_LIT) rightStr = true;
        }
        if (node->right->nodeType == NodeType::STRING_INTERPOLATION) rightStr = true;

        if (leftStr || rightStr) {
            return "flux_str_concat(" + left + ", " + right + ")";
        }
    }

    switch (node->op.type) {
        case TokenType::PLUS:          return "(" + left + " + " + right + ")";
        case TokenType::MINUS:         return "(" + left + " - " + right + ")";
        case TokenType::STAR:          return "(" + left + " * " + right + ")";
        case TokenType::SLASH:         return "(" + left + " / " + right + ")";
        case TokenType::PERCENT:       return "(" + left + " % " + right + ")";
        case TokenType::EQUAL_EQUAL:   return "(" + left + " == " + right + ")";
        case TokenType::BANG_EQUAL:    return "(" + left + " != " + right + ")";
        case TokenType::LESS:          return "(" + left + " < " + right + ")";
        case TokenType::LESS_EQUAL:    return "(" + left + " <= " + right + ")";
        case TokenType::GREATER:       return "(" + left + " > " + right + ")";
        case TokenType::GREATER_EQUAL: return "(" + left + " >= " + right + ")";
        case TokenType::AMP_AMP:       return "(" + left + " && " + right + ")";
        case TokenType::PIPE_PIPE:     return "(" + left + " || " + right + ")";
        case TokenType::KW_BUTNOT:     return "(" + left + " && !" + right + ")";
        case TokenType::AMPERSAND:     return "(" + left + " & " + right + ")";
        case TokenType::PIPE:          return "(" + left + " | " + right + ")";
        case TokenType::CARET:         return "(" + left + " ^ " + right + ")";
        case TokenType::LEFT_SHIFT:    return "(" + left + " << " + right + ")";
        case TokenType::RIGHT_SHIFT:   return "(" + left + " >> " + right + ")";
        case TokenType::EQUAL_NUM_EQUAL:  return "flux_num_equal(" + left + ", " + right + ")";
        case TokenType::EQUAL_WORD_EQUAL: return "flux_word_equal(" + left + ", " + right + ")";
        default:                       return "(" + left + " /* op */ " + right + ")";
    }
}

// ============================================================================
// Unary and postfix operators
// ============================================================================

std::string Transpiler::emitUnaryOp(std::shared_ptr<UnaryNode> node) {
    std::string operand = emitExpr(node->operand);
    switch (node->op.type) {
        case TokenType::MINUS:       return "(-" + operand + ")";
        case TokenType::BANG:        return "(!" + operand + ")";
        case TokenType::PLUS_PLUS:   return "(++" + operand + ")";
        case TokenType::MINUS_MINUS: return "(--" + operand + ")";
        case TokenType::TILDE:       return "(~" + operand + ")";
        case TokenType::STAR:        return "(*" + operand + ")";
        case TokenType::AMPERSAND:   return "(&" + operand + ")";
        default:                     return operand;
    }
}

std::string Transpiler::emitPostfix(std::shared_ptr<PostfixNode> node) {
    std::string operand = emitExpr(node->operand);
    switch (node->op.type) {
        case TokenType::PLUS_PLUS:   return "(" + operand + "++)";
        case TokenType::MINUS_MINUS: return "(" + operand + "--)";
        default:                     return operand;
    }
}

// ============================================================================
// Function/method calls
// ============================================================================

std::string Transpiler::emitCall(std::shared_ptr<CallNode> node) {
    // Member call: obj.method(args)
    if (node->callee->nodeType == NodeType::MEMBER_ACCESS) {
        auto memberNode = std::dynamic_pointer_cast<MemberAccessNode>(node->callee);
        std::string obj = emitExpr(memberNode->object);
        std::string method = memberNode->member;

        // Collect arguments
        std::string args;
        for (size_t i = 0; i < node->arguments.size(); i++) {
            if (i > 0) args += ", ";
            args += emitExpr(node->arguments[i]);
        }

        // super.method() -> ParentClass::method()
        if (memberNode->object->nodeType == NodeType::VARIABLE) {
            auto varNode = std::dynamic_pointer_cast<VariableNode>(memberNode->object);

            if (varNode->name == "super") {
                if (!currentParentClass.empty()) {
                    return currentParentClass + "::" + method + "(" + args + ")";
                }
                return "/* super." + method + "() - no parent */";
            }

            // math.sqrt() etc.
            if (varNode->name == "math") {
                if (method == "sqrt")  return "std::sqrt(" + args + ")";
                if (method == "abs")   return "std::abs(" + args + ")";
                if (method == "pow")   return "std::pow(" + args + ")";
                if (method == "floor") return "std::floor(" + args + ")";
                if (method == "ceil")  return "std::ceil(" + args + ")";
                if (method == "round") return "std::round(" + args + ")";
                if (method == "min")   return "std::min(" + args + ")";
                if (method == "max")   return "std::max(" + args + ")";
                if (method == "clamp") return "std::clamp(" + args + ")";
                if (method == "lerp")  return "flux_lerp(" + args + ")";
                if (method == "sin")   return "std::sin(" + args + ")";
                if (method == "cos")   return "std::cos(" + args + ")";
                if (method == "tan")   return "std::tan(" + args + ")";
                if (method == "atan2") return "std::atan2(" + args + ")";
                if (method == "asin")  return "std::asin(" + args + ")";
                if (method == "acos")  return "std::acos(" + args + ")";
                if (method == "log")   return "std::log(" + args + ")";
                if (method == "log2")  return "std::log2(" + args + ")";
                if (method == "log10") return "std::log10(" + args + ")";
                if (method == "exp")   return "std::exp(" + args + ")";
                return "std::" + method + "(" + args + ")";
            }

            // Class static method call: ClassName.method() -> ClassName::method()
            if (classNames.count(varNode->name)) {
                return varNode->name + "::" + method + "(" + args + ")";
            }

            // Enum method call: EnumName.method() -> EnumName::method()
            if (enumNames.count(varNode->name)) {
                return varNode->name + "::" + method + "(" + args + ")";
            }
        }

        // Rename C++ reserved keywords used as method names
        if (method == "delete") method = "httpDelete";

        // List methods: .add(), .removeAt(), .sort()
        // Only remap when the object is a known list variable
        bool isListObj = false;
        if (memberNode->object->nodeType == NodeType::VARIABLE) {
            auto objVar = std::dynamic_pointer_cast<VariableNode>(memberNode->object);
            if (listVars.count(objVar->name)) isListObj = true;
        }

        if (isListObj && method == "add") {
            return obj + ".push_back(" + args + ")";
        }
        if (isListObj && method == "removeAt") {
            return obj + ".erase(" + obj + ".begin() + " + args + ")";
        }
        if (isListObj && method == "sort") {
            return "std::sort(" + obj + ".begin(), " + obj + ".end(), " + args + ")";
        }

        // Enum .name() method
        // Check if obj is an enum name
        if (method == "name" && enumNames.count(obj)) {
            return obj + "::name(" + args + ")";
        }

        // String methods — map to helper functions
        if (method == "substring")  return "flux_substring(" + obj + ", " + args + ")";
        if (method == "indexOf")    return "flux_indexOf(" + obj + ", " + args + ")";
        if (method == "contains" && !isListObj)
            return "flux_contains(" + obj + ", " + args + ")";
        if (method == "startsWith") return "flux_startsWith(" + obj + ", " + args + ")";
        if (method == "endsWith")   return "flux_endsWith(" + obj + ", " + args + ")";
        if (method == "split")      return "flux_split(" + obj + ", " + args + ")";
        if (method == "trim")       return "flux_trim(" + obj + ")";
        if (method == "toUpper")    return "flux_toUpper(" + obj + ")";
        if (method == "toLower")    return "flux_toLower(" + obj + ")";
        if (method == "replace")    return "flux_replace(" + obj + ", " + args + ")";
        if (method == "charAt")     return "flux_charAt(" + obj + ", " + args + ")";
        if (method == "lastIndexOf") return "flux_lastIndexOf(" + obj + ", " + args + ")";
        if (method == "reverse" && !isListObj)
            return "flux_reverse_str(" + obj + ")";

        // Pointer member method call: ptr.method() -> ptr->method()
        if (memberNode->object->nodeType == NodeType::VARIABLE) {
            auto ptrVar = std::dynamic_pointer_cast<VariableNode>(memberNode->object);
            if (ptrVar && pointerVars.count(ptrVar->name)) {
                return obj + "->" + method + "(" + args + ")";
            }
        }

        // Default: obj.method(args)
        return obj + "." + method + "(" + args + ")";
    }

    // Lambda immediate invocation: (lambda)(args)
    if (node->callee->nodeType == NodeType::LAMBDA) {
        std::string lambda = emitLambda(std::dynamic_pointer_cast<LambdaNode>(node->callee));
        std::string result = lambda + "(";
        for (size_t i = 0; i < node->arguments.size(); i++) {
            if (i > 0) result += ", ";
            result += emitExpr(node->arguments[i]);
        }
        result += ")";
        return result;
    }

    // Regular function call
    std::string callee = emitExpr(node->callee);

    // Map Flux built-ins to C++
    if (callee == "print") callee = "flux_print";
    else if (callee == "println") callee = "flux_print";
    else if (callee == "print_raw") callee = "print_raw";
    else if (callee == "toString") callee = "flux_to_string";
    else if (callee == "typeof") callee = "flux_typeof";
    // len() stays as len() since it's in the runtime

    std::string result = callee + "(";
    for (size_t i = 0; i < node->arguments.size(); i++) {
        if (i > 0) result += ", ";
        result += emitExpr(node->arguments[i]);
    }
    result += ")";
    return result;
}

// ============================================================================
// Member access (field/property reads)
// ============================================================================

std::string Transpiler::emitMemberAccess(std::shared_ptr<MemberAccessNode> node) {
    std::string obj = emitExpr(node->object);

    auto varNode = std::dynamic_pointer_cast<VariableNode>(node->object);
    if (varNode) {
        // math constants
        if (varNode->name == "math") {
            if (node->member == "PI") return "M_PI";
            if (node->member == "E")  return "M_E";
            if (node->member == "TAU") return "(M_PI * 2.0)";
            if (node->member == "INF") return "std::numeric_limits<double>::infinity()";
        }

        // Signal constants: prefix with _ to avoid POSIX macro clash
        if (varNode->name == "Signal") {
            std::string mem = node->member;
            if (mem == "SIGINT" || mem == "SIGTERM" || mem == "SIGABRT" ||
                mem == "SIGFPE" || mem == "SIGSEGV" || mem == "SIGHUP" ||
                mem == "SIGUSR1" || mem == "SIGUSR2" || mem == "SIGPIPE" ||
                mem == "SIGALRM" || mem == "SIGCHLD") {
                return "Signal._" + mem;
            }
        }

        // Enum member access: Direction.NORTH
        if (enumNames.count(varNode->name)) {
            return varNode->name + "::" + node->member;
        }

        // Class static member access: ClassName.method → ClassName::method
        if (classNames.count(varNode->name)) {
            return varNode->name + "::" + node->member;
        }
    }

    // Pointer member access: ptr.field -> ptr->field  (check BEFORE .length mapping)
    if (varNode && pointerVars.count(varNode->name)) {
        return obj + "->" + node->member;
    }

    // .length on lists/strings -> (int32_t)obj.size()
    if (node->member == "length") {
        return "(int32_t)" + obj + ".size()";
    }

    // FluxObject property access: obj.key -> obj.get("key")
    if (varNode && objectVars.count(varNode->name)) {
        return obj + ".get(\"" + node->member + "\")";
    }

    // Chained FluxObject access: if the object itself is a .get() call
    // (e.g., data.get("user").name -> data.get("user").get("name"))
    if (node->object->nodeType == NodeType::MEMBER_ACCESS) {
        // Check if this chains from a FluxObject variable
        auto innerMem = std::dynamic_pointer_cast<MemberAccessNode>(node->object);
        if (innerMem) {
            auto innerVar = std::dynamic_pointer_cast<VariableNode>(innerMem->object);
            if (innerVar && objectVars.count(innerVar->name)) {
                return obj + ".get(\"" + node->member + "\")";
            }
        }
    }

    return obj + "." + node->member;
}
// ============================================================================
// Member set: obj.field = value
// ============================================================================

std::string Transpiler::emitMemberSet(std::shared_ptr<MemberSetNode> node) {
    std::string obj = emitExpr(node->object);
    std::string val = emitExpr(node->value);

    // FluxObject property set: obj.key = val -> obj.set("key", val)
    if (node->object->nodeType == NodeType::VARIABLE) {
        auto varNode = std::dynamic_pointer_cast<VariableNode>(node->object);
        if (varNode && objectVars.count(varNode->name)) {
            return obj + ".set(\"" + node->member + "\", FluxObject(" + val + "))";
        }
        // Class static member set: ClassName.field = val → ClassName::field = val
        if (varNode && classNames.count(varNode->name)) {
            return varNode->name + "::" + node->member + " = " + val;
        }
    }

    return obj + "." + node->member + " = " + val;
}

// ============================================================================
// Index access and set — direct [] operator
// ============================================================================

std::string Transpiler::emitIndexAccess(std::shared_ptr<IndexAccessNode> node) {
    std::string obj = emitExpr(node->object);
    std::string idx = emitExpr(node->index);
    return obj + "[" + idx + "]";
}

std::string Transpiler::emitIndexSet(std::shared_ptr<IndexSetNode> node) {
    std::string obj = emitExpr(node->object);
    std::string idx = emitExpr(node->index);
    std::string val = emitExpr(node->value);
    return obj + "[" + idx + "] = " + val;
}

// ============================================================================
// Cast expression: (int) x, (string) y
// Uses overloaded conversion helpers for type-safe casts
// ============================================================================

std::string Transpiler::emitCast(std::shared_ptr<CastNode> node) {
    std::string expr = emitExpr(node->expr);
    std::string target = node->targetType;

    if (target == "int")    return "flux_to_int(" + expr + ")";
    if (target == "long")   return "flux_to_long(" + expr + ")";
    if (target == "float" || target == "double") return "flux_to_float(" + expr + ")";
    if (target == "bool")   return "flux_to_bool(" + expr + ")";
    if (target == "string") return "flux_to_string(" + expr + ")";
    if (target == "byte")   return "static_cast<uint8_t>(" + expr + ")";

    // For pointer types, use reinterpret_cast (e.g., casting long to SomeType*)
    std::string cTarget = fluxTypeToC(target);
    if (cTarget.back() == '*') {
        return "reinterpret_cast<" + cTarget + ">(" + expr + ")";
    }

    return "static_cast<" + cTarget + ">(" + expr + ")";
}

// ============================================================================
// New expression: new ClassName(args)
// ============================================================================

std::string Transpiler::emitNewExpr(std::shared_ptr<NewExprNode> node) {
    std::string args;
    for (size_t i = 0; i < node->arguments.size(); i++) {
        if (i > 0) args += ", ";
        args += emitExpr(node->arguments[i]);
    }

    // Emit as an IIFE to keep it as a single expression
    return "[&]() -> " + node->className + " { " +
           node->className + " _obj; _obj.init(" + args + "); return _obj; }()";
}

// ============================================================================
// Lambda expression
// ============================================================================

std::string Transpiler::emitLambda(std::shared_ptr<LambdaNode> node) {
    std::string result = "[&](";
    for (size_t i = 0; i < node->params.size(); i++) {
        if (i > 0) result += ", ";
        std::string ptype = "auto";
        if (!node->params[i].typeName.empty())
            ptype = fluxTypeToC(node->params[i].typeName);
        result += ptype + " " + node->params[i].name;
    }
    result += ")";

    if (!node->returnType.empty() && node->returnType != "void") {
        result += " -> " + fluxTypeToC(node->returnType);
    }

    if (node->body->nodeType == NodeType::BLOCK) {
        result += " {\n";
        auto block = std::dynamic_pointer_cast<BlockNode>(node->body);
        pushIndent();
        for (auto& stmt : block->statements) {
            std::stringstream ss;
            emitNode(stmt, ss);
            result += ss.str();
        }
        popIndent();
        result += indent() + "}";
    } else {
        result += " { return " + emitExpr(node->body) + "; }";
    }

    return result;
}

// ============================================================================
// List literal: [1, 2, 3] or []
// When emitted standalone (not from emitVarDecl), use IIFE
// ============================================================================

std::string Transpiler::emitListLiteral(std::shared_ptr<ListLiteralNode> node) {
    if (node->elements.empty()) {
        return "std::vector<int32_t>{}";
    }

    // Build initializer list
    std::string result = "{";
    for (size_t i = 0; i < node->elements.size(); i++) {
        if (i > 0) result += ", ";
        result += emitExpr(node->elements[i]);
    }
    result += "}";
    return result;
}

// ============================================================================
// String interpolation: "Hello $name, you have ${x * 2} points"
// ============================================================================

std::string Transpiler::emitStringInterpolation(
        std::shared_ptr<StringInterpolationNode> node) {
    if (freestandingMode) {
        // Freestanding: use FluxString concatenation instead of ostringstream
        std::string result = "[&]() -> FluxString { FluxString _ss; ";
        for (auto& part : node->parts) {
            if (part->nodeType == NodeType::LITERAL) {
                auto lit = std::dynamic_pointer_cast<LiteralNode>(part);
                if (lit->litType == LiteralNode::STRING_LIT) {
                    std::string escaped;
                    for (char c : lit->stringVal) {
                        if (c == '"')  escaped += "\\\"";
                        else if (c == '\\') escaped += "\\\\";
                        else if (c == '\n') escaped += "\\n";
                        else if (c == '\t') escaped += "\\t";
                        else escaped += c;
                    }
                    result += "_ss = _ss + FluxString(\"" + escaped + "\"); ";
                } else {
                    result += "_ss = _ss + flux_to_string(" + emitLiteral(lit) + "); ";
                }
            } else {
                result += "_ss = _ss + flux_to_string(" + emitExpr(part) + "); ";
            }
        }
        result += "return _ss; }()";
        return result;
    }
    std::string result = "[&]() -> std::string { std::ostringstream _ss; ";
    for (auto& part : node->parts) {
        if (part->nodeType == NodeType::LITERAL) {
            auto lit = std::dynamic_pointer_cast<LiteralNode>(part);
            if (lit->litType == LiteralNode::STRING_LIT) {
                std::string escaped;
                for (char c : lit->stringVal) {
                    if (c == '"')  escaped += "\\\"";
                    else if (c == '\\') escaped += "\\\\";
                    else if (c == '\n') escaped += "\\n";
                    else if (c == '\t') escaped += "\\t";
                    else escaped += c;
                }
                result += "_ss << \"" + escaped + "\"; ";
            } else {
                result += "_ss << " + emitLiteral(lit) + "; ";
            }
        } else {
            result += "_ss << flux_to_string(" + emitExpr(part) + "); ";
        }
    }
    result += "return _ss.str(); }()";
    return result;
}

// ============================================================================
// Raw string interpolation: parse $var and ${expr} patterns from STRING_LIT
// The interpreter does this at runtime; the transpiler must do it at compile
// time by lexing/parsing embedded expressions and emitting C++ equivalents.
// ============================================================================

std::string Transpiler::emitStringInterpFromRaw(const std::string& rawStr) {
    // Build an IIFE — freestanding uses FluxString concat, hosted uses ostringstream
    std::string strType = freestandingMode ? "FluxString" : "std::string";
    std::string ssDecl = freestandingMode ? "FluxString _ss; " : "std::ostringstream _ss; ";
    std::string result = "[&]() -> " + strType + " { " + ssDecl;
    size_t i = 0;
    std::string textBuf;

    auto flushText = [&]() {
        if (!textBuf.empty()) {
            std::string escaped;
            for (char c : textBuf) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\r') escaped += "\\r";
                else if (c == '\t') escaped += "\\t";
                else if (c == '\b') escaped += "\\b";
                else if (c == '\0') escaped += "\\0";
                else escaped += c;
            }
            if (freestandingMode)
                result += "_ss = _ss + FluxString(\"" + escaped + "\"); ";
            else
                result += "_ss << \"" + escaped + "\"; ";
            textBuf.clear();
        }
    };

    while (i < rawStr.size()) {
        if (rawStr[i] == '$' && i + 1 < rawStr.size()) {
            if (rawStr[i + 1] == '{') {
                // ${expression} — find matching }
                flushText();
                size_t start = i + 2;
                int depth = 1;
                size_t end = start;
                while (end < rawStr.size() && depth > 0) {
                    if (rawStr[end] == '{') depth++;
                    else if (rawStr[end] == '}') depth--;
                    if (depth > 0) end++;
                }
                if (depth == 0) {
                    std::string exprStr = rawStr.substr(start, end - start);
                    // Lex and parse the expression, then emit it as C++
                    Lexer lexer(exprStr + ";", "<interp>");
                    auto tokens = lexer.tokenize();
                    if (!lexer.hasErrors() && tokens.size() > 1) {
                        Parser parser(tokens, "<interp>");
                        auto exprNode = parser.parse();
                        if (!parser.hasErrors()) {
                            auto prog = std::static_pointer_cast<ProgramNode>(exprNode);
                            if (!prog->declarations.empty()) {
                                auto exprStmt = std::static_pointer_cast<ExpressionStmtNode>(
                                    prog->declarations[0]);
                                if (freestandingMode)
                                    result += "_ss = _ss + flux_to_string(" + emitExpr(exprStmt->expression) + "); ";
                                else
                                    result += "_ss << flux_to_string(" + emitExpr(exprStmt->expression) + "); ";
                            }
                        }
                    }
                    i = end + 1;
                } else {
                    textBuf += rawStr[i];
                    i++;
                }
            } else if (std::isalpha(rawStr[i + 1]) || rawStr[i + 1] == '_') {
                // $varName — read identifier
                flushText();
                size_t start = i + 1;
                size_t end = start;
                while (end < rawStr.size() && (std::isalnum(rawStr[end]) || rawStr[end] == '_')) {
                    end++;
                }
                std::string varName = rawStr.substr(start, end - start);
                if (freestandingMode)
                    result += "_ss = _ss + flux_to_string(" + varName + "); ";
                else
                    result += "_ss << flux_to_string(" + varName + "); ";
                i = end;
            } else {
                textBuf += rawStr[i];
                i++;
            }
        } else {
            textBuf += rawStr[i];
            i++;
        }
    }

    flushText();
    if (freestandingMode)
        result += "return _ss; }()";
    else
        result += "return _ss.str(); }()";
    return result;
}

// ============================================================================
// Assignment
// ============================================================================

std::string Transpiler::emitAssignment(std::shared_ptr<AssignNode> node) {
    std::string target = node->name;
    std::string value = emitExpr(node->value);

    switch (node->op.type) {
        case TokenType::EQUAL:         return target + " = " + value;
        case TokenType::PLUS_EQUAL:    return target + " += " + value;
        case TokenType::MINUS_EQUAL:   return target + " -= " + value;
        case TokenType::STAR_EQUAL:    return target + " *= " + value;
        case TokenType::SLASH_EQUAL:   return target + " /= " + value;
        case TokenType::PERCENT_EQUAL: return target + " %= " + value;
        case TokenType::AMP_EQUAL:     return target + " &= " + value;
        case TokenType::PIPE_EQUAL:    return target + " |= " + value;
        case TokenType::CARET_EQUAL:   return target + " ^= " + value;
        case TokenType::LEFT_SHIFT_EQUAL:  return target + " <<= " + value;
        case TokenType::RIGHT_SHIFT_EQUAL: return target + " >>= " + value;
        default:                       return target + " = " + value;
    }
}

// ============================================================================
// Check if source file has // DOCTYPE {AOT} directive
// ============================================================================

bool Transpiler::hasDocTypeAOT(const std::string& source) {
    // Look for // DOCTYPE {AOT} anywhere in the source (usually first line)
    size_t pos = source.find("// DOCTYPE");
    if (pos == std::string::npos) return false;
    // Find the rest of the line
    size_t eol = source.find('\n', pos);
    std::string line = (eol != std::string::npos)
                       ? source.substr(pos, eol - pos)
                       : source.substr(pos);
    return line.find("{AOT}") != std::string::npos;
}

// ============================================================================
// Import resolution: read, lex, parse, and merge an imported .lx/.flux file
// ============================================================================

void Transpiler::resolveImport(const std::string& importPath, std::stringstream& out) {
    // Resolve path: try baseDir first, then walk up parent directories
    // to find the project root where the path exists
    std::string fullPath;
    if (!importPath.empty() && (importPath.front() == '/' || importPath.front() == '~')) {
        fullPath = importPath;
    } else {
        // Try baseDir + importPath first
        std::string candidate = baseDir + "/" + importPath;
        if (std::filesystem::exists(candidate)) {
            fullPath = candidate;
        } else {
            // Walk up parent directories to find where importPath exists
            // This supports project-root-relative imports like "kernel/core/bootinfo.lx"
            std::filesystem::path dir = std::filesystem::path(baseDir);
            bool found = false;
            for (int depth = 0; depth < 10 && dir.has_parent_path(); ++depth) {
                dir = dir.parent_path();
                std::string tryPath = (dir / importPath).string();
                if (std::filesystem::exists(tryPath)) {
                    fullPath = tryPath;
                    found = true;
                    break;
                }
            }
            if (!found) {
                fullPath = candidate; // Fall back — will produce warning below
            }
        }
    }

    // Normalize the path
    try {
        fullPath = std::filesystem::canonical(fullPath).string();
    } catch (const std::filesystem::filesystem_error&) {
        // If canonical fails, try weakly_canonical (file might not exist yet)
        try {
            fullPath = std::filesystem::weakly_canonical(fullPath).string();
        } catch (...) {
            std::cerr << "Warning: Cannot resolve import path: " << importPath << std::endl;
            return;
        }
    }

    // Skip if already imported (prevent circular imports)
    if (importedFiles.count(fullPath)) return;
    importedFiles.insert(fullPath);

    // Read the file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open imported file: " << fullPath << std::endl;
        return;
    }
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Lex
    Lexer lexer(source, fullPath);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        for (auto& err : lexer.getErrors())
            std::cerr << "Import " << importPath << ": " << err << std::endl;
        return;
    }

    // Parse
    Parser parser(tokens, fullPath);
    auto program = parser.parse();
    if (parser.hasErrors()) {
        for (auto& err : parser.getErrors())
            std::cerr << "Import " << importPath << ": " << err << std::endl;
        return;
    }

    // Pre-scan for class/enum names from the imported file
    prescanDeclarations(program, classNames, enumNames);

    // Walk the imported AST and emit its declarations
    auto prog = std::dynamic_pointer_cast<ProgramNode>(program);
    if (!prog) return;

    // Save and update baseDir for nested imports
    std::string savedBaseDir = baseDir;
    std::filesystem::path importDir = std::filesystem::path(fullPath).parent_path();
    baseDir = importDir.string();

    for (auto& decl : prog->declarations) {
        if (!decl) continue;

        // Handle nested imports recursively
        if (decl->nodeType == NodeType::IMPORT_STMT) {
            auto imp = std::dynamic_pointer_cast<ImportStmtNode>(decl);
            if (imp && !imp->path.empty()) {
                // Skip std.* module imports in freestanding mode
                if (imp->path.rfind("std.", 0) == 0) {
                    importedModules.insert(imp->path);
                } else {
                    // File import — resolve recursively
                    resolveImport(imp->path, functions);
                }
            }
            continue;
        }

        // Emit all declarations into the 'functions' stream so they
        // appear before main/kernel_main in the final output.
        // This ensures module-level variables from imports are visible
        // to classes and functions that reference them.
        emitNode(decl, functions);
    }

    // Restore baseDir
    baseDir = savedBaseDir;
}
