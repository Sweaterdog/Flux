**FLUX**

PROGRAMMING LANGUAGE

REFERENCE MANUAL

_"From the Void, Structure."_

Version 0.1

The Native Interface for StratOS

Multi-Paradigm | Systems-Level | Visual-First | C++ Interoperable

  

# **Table of Contents**

|     |     |     |
| --- | --- | --- |
| **#** | **Chapter** | **Page** |
| 1.  | Introduction & Philosophy | 4   |
| 2.  | Getting Started | 5   |
| 3.  | The Compiler & Runtime Environment | 6   |
| 4.  | Lexical Structure & Syntax | 9   |
| 5.  | The Type System | 12  |
| 6.  | Variables & Constants | 15  |
| 7.  | Operators & Logic | 17  |
| 8.  | Control Flow | 21  |
| 9.  | Functions, Methods & Closures | 25  |
| 10. | Object-Oriented Programming | 29  |
| 11. | Error Handling & Safety | 33  |
| 12. | Concurrency | 35  |
| 13. | Networking & I/O | 37  |
| 14. | Mathematics & Physics | 40  |
| 15. | Memory Management & Pointers | 42  |
| 16. | Graphics & StratOS Internals | 45  |
| 17. | Interoperability (C++ Bridge) | 48  |
| 18. | Standard Library Reference | 50  |
| 19. | std.audio — Audio Playback | 54  |
| 20. | std.video — Video Playback | 55  |
| Appendix A | Operator Precedence Table | 56  |
| Appendix B | Reserved Keywords | 55  |
| Appendix C | Primitive Type Reference | 56  |

  

# **1\. Introduction & Philosophy**

Flux is a high-performance, multi-paradigm systems programming language designed as the native interface for StratOS. It is built on a core belief that the language a system uses should be as capable as the system itself: able to touch bare hardware when needed, yet expressive enough for rapid application development.

Flux draws inspiration from C++ (performance, control), Swift (clean syntax, ARC memory model), and Python (readability, shell-style scripting). The result is a language that feels familiar to any programmer but breaks new ground in several key areas.

## **1.1 Core Philosophy Pillars**

**Hardware Sovereign.** When running on StratOS, Flux treats itself as the top-level interface. It provides direct intrinsics for GPU framebuffers, network interface cards, and raw memory without relying on an external OS layer. If you want to write a pixel to the screen, you write a pixel to the screen.

**Fluid Identity.** Data types are static and enforced within any given execution scope, preventing the accidental type coercion bugs common in dynamically-typed languages. However, a variable's type can be explicitly changed across its lifecycle using a deliberate re-declaration syntax. The change is intentional, visible, and tracked by the compiler.

**Visual First.** 32-bit color values, 3D vectors (vec3), and 4x4 matrices (mat4) are primitive-class citizens in Flux, not afterthoughts bolted on through libraries. This is a language designed from the ground up to power graphics.

**Safe by Default.** Flux uses Automatic Reference Counting (ARC) to manage memory automatically in standard code. Unsafe operations are only possible inside an explicitly marked **unsafe { }** block, so dangerous code is always visible and intentional.

## **1.2 Who Is This Manual For?**

This manual is written for two audiences. If you are new to programming or coming from a high-level language, the early chapters provide all the context you need. If you are an experienced systems programmer, you can skip ahead to the chapters covering Flux's unique features: the type system (Chapter 5), the semantic comparators (Chapter 7), and memory management (Chapter 15).

## **1.3 File Extensions**

Flux projects use two file types:

- **.flux** — Executable source files. These contain your main functions, classes, and application logic.
- **.lx** — Library and header files. These define APIs, types, and shared functions for import by other files.

**Note:** C++ files (.cpp, .h) are also accepted by the fluxc compiler for interoperability. See Chapter 17.

  

# **2\. Getting Started**

## **2.1 Your First Flux Program**

Every Flux program begins executing from the main() function. Below is the canonical first program:

\# File: hello.flux

func main() {

print("Hello, World!");

}

To compile and run this program using Ahead-of-Time (AOT) compilation:

\# Compile to a native binary

fluxc build hello.flux -o hello

\# Run the binary

./hello

\# Output:

Hello, World!

Or, run it directly using the JIT interpreter without producing a binary:

flux run hello.flux

\# Output:

Hello, World!

## **2.2 A More Complete Example**

Here is a small but complete program that demonstrates variables, functions, a loop, and basic I/O. Each concept shown here is explained in full in the chapters that follow.

\# File: greet.flux

\# A function that builds a greeting string

func greet(string name, int times) -> string {

string result = "";

for (int i = 0; i < times; i++) {

result += "Hello, $name!\\n";

}

return result;

}

func main() {

string user = input("Enter your name: ");

string message = greet(user, 3);

print(message);

}

Running this program will prompt the user for their name and then print a personalized greeting three times. The $ character inside a string is used for variable interpolation, which is covered in Chapter 4.

## **2.3 Program Structure Overview**

A typical Flux source file is structured as follows. There are no mandatory ordering rules, but this is the conventional layout used throughout this manual:

\# 1. Imports

import std.net;

import "my_library.lx";

\# 2. Constants

int MAX_PLAYERS = 8;

\# 3. Structs and Class Definitions

struct Vec2 { float x; float y; }

\# 4. Helper Functions

func helper(int val) -> int { return val \* 2; }

\# 5. Entry Point

func main() {

\# Program logic goes here

}

  

# **3\. The Compiler & Runtime Environment**

Flux uses a dual-stage architecture managed by the fluxc toolchain. Understanding how your code is compiled and executed helps you make the right choices for your project.

## **3.1 Ahead-of-Time (AOT) Compilation**

AOT compilation translates your Flux source code into C++, then compiles that to a native binary using g++. This binary runs directly on the CPU with no interpreter overhead, making it the right choice for production builds and performance-critical applications.

The transpiler generates a self-contained C++ file with a lightweight runtime layer. Flux types map directly to C++ types: `int` → `int32_t`, `float` → `double`, `string` → `std::string`, `bool` → `bool`. Lists become `std::vector<T>` with the element type inferred from the declared Flux type. User-defined classes become C++ structs with member functions, and class inheritance uses C++ struct inheritance. Enums emit as namespaces with `static const int32_t` members.

The AOT compiler supports the full Flux feature set including:

- Functions, lambdas, and immediately-invoked function expressions (IIFEs)
- Classes with fields, methods, constructors (`init`), and multi-level inheritance (`super.init()`)
- Enum declarations and member access
- Lists with `.add()`, `.removeAt()`, `.sort()`, `.length`, and index access
- String interpolation (`$var` and `${expr}`)
- Cast expressions (`(int) x`, `(string) y`, including string-to-int conversion)
- Control flow: `if`/`elif`/`else`, `for`, `for-each`, `while`, `do-while`, `switch`
- Error handling: `try`/`catch`/`throw`
- Built-in functions: `print`, `print_raw`, `len`, `typeof`, `math.sqrt`, `math.PI`, etc.

**AOT behavioral notes:** Runtime const enforcement (UPPER_SNAKE_CASE variables) is not replicated in AOT mode — assignments to const-named variables succeed silently rather than throwing a catchable error. Standard library types (`Window`, `Timer`, `Socket`, `Map`, `Stack`, `Queue`) and user-defined classes are passed by reference in AOT mode to match Flux's reference-type object semantics. Functions without a return type annotation are emitted as returning `int32_t` with an implicit `return 0` at the end.

```
# Basic compile (defaults to -O2)
flux compile main.flux -o my_app

# Fast compile — no optimization, quickest build time
flux compile main.flux --fast -o my_app_debug

# Release — maximum optimization (-O3)
flux compile main.flux --release -o my_app_release

# Size — smallest binary (-Os)
flux compile main.flux --size -o bootloader

# Dev mode — saves the intermediate .gen.cpp file alongside the binary
flux compile main.flux --dev -o my_app
```

|     |     |     |
| --- | --- | --- |
| **Flag** | **Meaning** | **Use When** |
| --fast | No optimization (-O0). Fastest compile time. | Development & debugging |
| (default) | Balanced optimization (-O2). | General use |
| --release | Aggressive optimization (-O3): inlining, loop unrolling, vectorization. | Production releases |
| --size | Minimize binary size (-Os). | Embedded, bootloaders |
| --dev | Saves the generated `.gen.cpp` file next to the output binary. | Debugging transpiler output |

## **3.2 Just-in-Time (JIT) Execution**

The JIT mode compiles source code to an Intermediate Representation (IR) in memory and executes it immediately. There is no output binary. This mode is ideal for rapid iteration, interactive scripting, and hot-reloading during development.

\# Run a script directly

flux run script.flux

\# Launch the interactive Flux shell

flux

\# Inside the interactive shell:

\> int x = 42;

\> print(x \* 2);

84

\>

### **Hot-Reloading**

When running in JIT development mode, the Flux runtime monitors your source files for changes. When a .flux file is saved, the JIT engine can hot-swap the function pointers at runtime without restarting the process. This is particularly powerful for game development, where you can tweak game logic while the game is running.

\# Launch in development mode with hot-reload enabled

flux run --dev game.flux

\# Now edit any .flux file in your project.

\# Changed functions will be live-updated automatically.

## **3.3 The exec() Function**

Flux provides a built-in function that allows you to compile and execute a string of Flux code at runtime using the JIT engine. This is a powerful but security-sensitive feature.

\# Signature

func exec(string code, string mode = "full") -> void

\# Example: dynamic code execution

string code = "print(\\"Executed at runtime!\\");";

exec(code);

### **Sandbox Mode**

Because exec() runs arbitrary code, it supports a sandbox mode that restricts what the executed code is allowed to do. This is critical for any application that runs user-provided code.

\# Mode "full" (default) - full kernel privileges.

\# DANGEROUS with untrusted input!

exec(userInput);

\# Mode "sandbox" - restricted execution environment.

\# - No file system access

\# - No network access

\# - Maximum 512MB RAM

\# - No unsafe{} blocks allowed

exec(userInput, mode: "sandbox");

**Warning:** Never use exec() in "full" mode with user-provided input. Always use "sandbox" mode when executing untrusted code.

## **3.4 Module System (Imports & Exports)**

Flux uses an explicit module system. You must import a file or library before you can use anything it provides.

### **Importing**

\# Import a local library file

import "libs/graphics.lx";

\# Import from the standard library

import std.net;

import std.math;

import std.collections;

\# Import a C++ header (C++ bridge)

import "legacy_driver.cpp";

### **Exporting**

To make a function, class, or variable available to other files that import yours, use the export keyword.

\# In my_library.lx

\# This function is public and importable

export func calculateDistance(float x1, float y1, float x2, float y2) -> float {

return \`\\sqrt{(x2 - x1)^2 + (y2 - y1)^2}\`;

}

\# This function is private to this file

func internalHelper(int v) -> int {

return v \* 2;

}

## **3.5 Freestanding (Bare-Metal) Compilation**

For operating system kernels, bootloaders, and embedded targets that have no hosted C/C++ standard library, Flux supports **freestanding AOT compilation**. In this mode, the transpiler emits a self-contained C++ file that depends on no external headers or runtime — it can be compiled with `-ffreestanding -nostdlib` and linked directly against a bare-metal linker script.

```
# Compile a kernel entry point in freestanding mode
flux compile kernel.flux --dev -o kernel.bin
```

The `--dev` flag is especially useful here: it preserves the generated `.gen.cpp` file so you can inspect the transpiler output, verify linkage, and integrate it into a custom build system (e.g., a Makefile that cross-compiles for a different architecture).

### **Entry Point and Linkage**

When compiling in freestanding mode, the function `kernel_main` is emitted with C linkage (`extern "C"`) to prevent C++ name mangling. This makes it directly callable from assembly boot stubs and compatible with standard multiboot/linker script conventions.

```flux
# File: kernel.flux

func kernel_main() {
    # This becomes extern "C" void kernel_main() in the generated C++
    # Called directly by the bootloader or assembly entry stub
}
```

Other functions retain standard C++ linkage. Only `kernel_main` receives the `extern "C"` treatment.

### **Freestanding Runtime Preamble**

The transpiler automatically generates a lightweight runtime preamble at the top of the freestanding `.gen.cpp` file. This preamble provides the minimum set of types and utilities needed for Flux programs to operate without a hosted standard library.

**Types:**

|     |     |
| --- | --- |
| **Type** | **Description** |
| `FluxString` | Fixed-buffer string type. Stores up to 255 characters inline without heap allocation. Supports assignment, concatenation, comparison, and `c_str()` access. |
| `FluxList<T>` | Dynamic list backed by a simple bump allocator. Provides `.add()`, `.insertAt()`, `.removeAt()`, `.length`, `.empty()`, `.clear()`, `.erase()`, and index access. Suitable for small, bounded collections in bare-metal contexts. |
| `FuncPtr` | Variadic function pointer typedef: `void(*)(...)`. Used for storing and invoking function references. |

**Memory Operations:**

The preamble forward-declares `memset`, `memcpy`, and `memmove` with C linkage. These must be provided by your runtime environment or linked from a minimal C library.

**Cast Helpers:**

|     |     |
| --- | --- |
| **Function** | **Description** |
| `flux_to_int(value)` | Convert a value to a 32-bit signed integer. |
| `flux_to_long(value)` | Convert a value to a 64-bit signed integer. |
| `flux_to_float(value)` | Convert a value to a double-precision float. |
| `flux_to_bool(value)` | Convert a value to a boolean. |
| `flux_to_string(value)` | Convert a numeric or boolean value to a `FluxString`. |

**String Helpers:**

|     |     |
| --- | --- |
| **Function** | **Description** |
| `flux_substring(s, start, len)` | Extract a substring. |
| `flux_indexOf(s, needle)` | Find the first occurrence of a substring. Returns -1 if not found. |
| `flux_lastIndexOf(s, needle)` | Find the last occurrence of a substring. Returns -1 if not found. |
| `flux_contains(s, needle)` | Returns true if the string contains the substring. |
| `flux_split(s, delimiter)` | Split a string by a delimiter into a `FluxList<FluxString>`. |
| `flux_trim(s)` | Remove leading and trailing whitespace. |
| `flux_replace(s, old, rep)` | Replace all occurrences of a substring. |
| `flux_charAt(s, index)` | Return the character at the given index as a `FluxString`. |

**Math Helpers:**

|     |     |
| --- | --- |
| **Function** | **Description** |
| `flux_min(a, b)` | Return the smaller of two values. |
| `flux_max(a, b)` | Return the larger of two values. |
| `flux_abs(x)` | Return the absolute value. |

These helpers are emitted only in freestanding mode. Standard (hosted) AOT compilation uses the C++ standard library directly.

  

# **4\. Lexical Structure & Syntax**

This chapter covers the low-level rules of how Flux source code is written — the "letters and words" of the language.

## **4.1 Comments**

Flux supports three styles of comments. Comments are ignored by the compiler.

\# This is a single-line comment (shell style)

// This is also a single-line comment (C style)

/\*

This is a multi-line block comment.

It can span as many lines as needed.

\*/

int x = 10; # Inline comment after code

## **4.2 Identifiers & Naming Conventions**

An identifier is any name you give to a variable, function, or class. Identifiers are case-sensitive, must begin with a letter or underscore, and can contain letters, digits, and underscores.

\# Valid identifiers

myVariable

\_privateValue

player1Score

MAX_BUFFER_SIZE

\# Invalid identifiers

1stPlayer # Cannot start with a digit

my-variable # Hyphens not allowed

class # Reserved keyword

Flux enforces the following naming conventions by strong community standard:

|     |     |     |
| --- | --- | --- |
| **Style** | **Used For** | **Example** |
| camelCase | Variables, functions | playerHealth, calculateForce() |
| PascalCase | Classes, structs, interfaces | RigidBody, HttpClient |
| UPPER_SNAKE_CASE | Constants | MAX_BUFFER_SIZE, PI |

## **4.3 String Interpolation**

Flux supports embedding variable values directly inside strings using the $ character. This avoids clumsy concatenation and is the preferred way to build strings dynamically.

int hp = 100;

string name = "Aria";

\# Simple variable: $varName

print("Player: $name");

\# Output: Player: Aria

\# Expression in braces: ${expression}

print("Half HP: ${hp / 2}");

\# Output: Half HP: 50

\# Complex expression

print("Status: $name has ${hp \* 0.75} shield points remaining.");

\# Output: Status: Aria has 75.0 shield points remaining.

**Note:** To print a literal dollar sign, escape it with a backslash: \\$

## **4.4 String Methods**

Strings in Flux have built-in methods for common text manipulation. These work in both JIT and AOT modes.

| Method | Signature | Description |
| --- | --- | --- |
| length | `.length` (property) | Returns the number of characters in the string. |
| substring | `.substring(start, length)` | Returns a portion of the string starting at `start` with the given `length`. |
| indexOf | `.indexOf(search)` | Returns the index of the first occurrence of `search`, or `-1` if not found. |
| contains | `.contains(search)` | Returns `true` if the string contains `search`. |
| startsWith | `.startsWith(prefix)` | Returns `true` if the string starts with `prefix`. |
| endsWith | `.endsWith(suffix)` | Returns `true` if the string ends with `suffix`. |
| split | `.split(delimiter)` | Splits the string by `delimiter` and returns a `list` of strings. |
| trim | `.trim()` | Removes leading and trailing whitespace. |
| toUpper | `.toUpper()` | Returns the string converted to uppercase. |
| toLower | `.toLower()` | Returns the string converted to lowercase. |
| replace | `.replace(old, new)` | Replaces all occurrences of `old` with `new`. |
| charAt | `.charAt(index)` | Returns the character at the given index as a single-character string. |
| reverse | `.reverse()` | Returns the string with characters in reverse order. |

```flux
string name = "Hello, World!";
print(name.length);            # 13
print(name.substring(7, 5));   # World
print(name.indexOf("World"));  # 7
print(name.contains("Hello")); # true
print(name.toUpper());         # HELLO, WORLD!

string csv = "a,b,c";
list parts = csv.split(",");   # ["a", "b", "c"]

string padded = "  hi  ";
print(padded.trim());          # hi
```

## **4.5 Reserved Keywords**

The following words are reserved by Flux and cannot be used as identifiers:

|     |     |     |     |
| --- | --- | --- | --- |
| **Keywords A–D** | **Keywords E–I** | **Keywords N–S** | **Keywords T–W** |
| atomic | else | new | thread |
| bool | elif | null | true |
| break | enum | panic | try |
| butnot | exec | private | unsafe |
| byte | export | public | void |
| catch | extends | return | while |
| char | false | struct |     |
| class | for | switch |     |
| cleanup | func | implements |     |
| continue | if  | interface |     |
| do  | import | int |     |

## **4.6 Literals**

A literal is a fixed value written directly in your code.

\# Integer literals

int a = 42;

int b = -7;

int hex = 0xFF; # Hexadecimal (255)

int bin = 0b1010; # Binary (10)

\# Floating-point literals

float pi = 3.14159;

float sci = 1.5e10; # Scientific notation

\# Boolean literals

bool isActive = true;

bool isDead = false;

\# String literals

string greeting = "Hello, World!";

string escaped = "Tab:\\t Newline:\\n";

\# Character literal

char grade = 'A';

\# Null literal

void result = null;

  

# **5\. The Type System**

Flux uses Mutable Static Typing (also called Scope-Locked Mutability). This is one of Flux's most distinctive features and is worth understanding thoroughly.

## **5.1 The Core Rule**

A variable's type is fixed and enforced within any single execution scope. You cannot assign a value of the wrong type to a variable — the compiler will stop you. However, you can explicitly re-declare a variable with a new type using a specific syntax. The old memory is freed, and a new variable of the new type is created. If the variable type changes, but the new type cannot process it, such as `string` becoming a `char`, the value of that variable becomes `null`.

## **5.2 Primitive Types**

Flux provides the following built-in primitive types:

|     |     |     |     |
| --- | --- | --- | --- |
| **Keyword** | **Size** | **Range / Description** | **Example** |
| void | 0 bits | Represents no value / null | void result = null; |
| bool | 1 byte | true or false | bool isAlive = true; |
| char | 1 byte | Single ASCII character (0–127) | char grade = 'A'; |
| byte | 1 byte | Unsigned integer (0–255) | byte flags = 0xFF; |
| int | 4 bytes | Signed 32-bit (-2B to +2B) | int score = 1000; |
| long | 8 bytes | Signed 64-bit integer | long timestamp = 9999999; |
| float | 8 bytes | IEEE 754 double precision | float speed = 9.81; |
| string | Dynamic | UTF-8 text sequence | string name = "Flux"; |
| list | Dynamic | Ordered collection of values | list items = [1, 2, 3]; |
| object | Dynamic | Dynamic key-value container (returned by JSON.parse) | object data = JSON.parse(str); |

### **Visual & Graphics Primitives**

Because Flux is designed for graphics programming, the following types are also primitives (not library types). They are covered in depth in Chapter 16.

|     |     |     |
| --- | --- | --- |
| **Type** | **Description** | **Example** |
| vec2 | 2D vector (x, y) | vec2 pos = vec2(100, 200); |
| vec3 | 3D vector (x, y, z) | vec3 origin = vec3(0, 0, 0); |
| mat4 | 4x4 float matrix | mat4 proj = mat4.identity(); |
| color32 | 32-bit RGBA color value | color32 red = 0xFF0000FF; |

## **5.3 Type Re-Definition (The "Flux" Behavior)**

This is the behavior the language is named for. A variable can flow from one type to another across its lifecycle. To change a variable's type, you use the explicit re-declaration syntax:

\# Standard assignment (type must match)

int count = 100;

count = 200; # OK - still int

count = "two hundred"; # COMPILER ERROR - type mismatch

\# Type re-declaration syntax: identifier = new_type = value

count = string = "two hundred"; # OK - explicit re-type

\# The int memory is freed. count is now a string.

print(count); # Prints: two hundred

### **Scope-Locking Rule**

Type re-definition creates a local shadow. If you re-type a global variable from inside a function, the global is unchanged. Only the local scope is affected.

int globalScore = 9999;

func displayScore() {

\# Re-type creates a LOCAL shadow of globalScore.

globalScore = string = "9999"; # another way of writing this would be globalScore = string;, Since the compiler is automatically capable of changing the type of the variable, while keeping the information constant

print(globalScore); # Prints: 9999 (local string)

}

func main() {

displayScore();

print(globalScore); # Prints: 9999 (global int is untouched)

}

**Note:** The destructor of the old value is called immediately when a re-type occurs. Memory is reclaimed at that exact moment, not later.

## **5.4 Enums**

Enums define a named set of constant integer values. They are ideal for representing a fixed set of states, options, or codes. Enums are always referenced by their type name.

enum Direction {

NORTH, # = 0

SOUTH, # = 1

EAST, # = 2

WEST # = 3

}

\# Explicit values

enum HttpStatus {

OK = 200,

NOT_FOUND = 404,

ERROR = 500

}

Direction heading = Direction.NORTH;

if (heading == Direction.NORTH) {

print("Heading north!");

}

## **5.5 Typed Lists**

Flux supports typed list declarations using the `list<Type>` syntax. A typed list constrains all elements to a single type, which is required for AOT compilation and strongly recommended in freestanding contexts where type inference is limited.

```flux
# Declare typed lists
list<int> scores = [100, 200, 300];
list<string> names = ["Alice", "Bob"];
list<float> coords = [1.0, 2.5, 3.7];
list<func> callbacks = [];
```

Typed lists support the same operations as inferred lists (`.add()`, `.insertAt()`, `.removeAt()`, `.length`, `.empty()`, `.clear()`, `.erase()`, index access, `.sort()`, `.contains()`). The type parameter is used by the transpiler to emit correctly typed C++ containers — `std::vector<T>` in hosted mode, or `FluxList<T>` in freestanding mode.

### List Method Reference

| Method | Description |
| --- | --- |
| `.add(value)` | Append a value to the end of the list. |
| `.insertAt(index, value)` | Insert a value at the given index, shifting subsequent elements right. Out-of-bounds indices are clamped. |
| `.removeAt(index)` | Remove the element at the given index, shifting subsequent elements left. |
| `.length` | Property returning the number of elements in the list. |
| `.empty()` | Returns `true` if the list has no elements. |
| `.clear()` | Remove all elements from the list (resets length to 0). |
| `.erase(value)` | Remove the first occurrence of the given value. |
| `.sort()` | Sort the list in ascending order (hosted mode only). |
| `.contains(value)` | Returns `true` if the list contains the given value (hosted mode only). |

When no type annotation is provided, the element type is inferred from the first element or from usage context. In freestanding compilation, explicit type annotations avoid ambiguity:

```flux
# Freestanding: explicit types are required
list<int> entries = [];
entries.add(42);

# Hosted: type can be inferred
let items = [1, 2, 3];  # inferred as list<int>
```

  

# **6\. Variables & Constants**

## **6.1 Variable Declaration**

All variables must be declared with a type. Flux does not infer types on first assignment. This makes code easier to read and helps the compiler produce better error messages.

\# Syntax: type name = value;

int playerScore = 0;

float gravity = 9.81;

string playerName = "Atlas";

bool isGameOver = false;

\# Variables can be declared without a value (they are zero-initialized)

int counter; # counter = 0

float temperature; # temperature = 0.0

string buffer; # buffer = ""

bool flag; # flag = false

## **6.2 Constants**

A constant is a variable whose value cannot change after it is first assigned. Use constants for values that are fixed for the life of the program, such as configuration values, mathematical constants, or buffer sizes.

\# Constants use the same declaration syntax but are named UPPER_SNAKE_CASE

\# by convention. The compiler enforces immutability.

int MAX_PLAYERS = 16;

float PI = 3.14159265358979;

string APP_VERSION = "0.1.0";

\# Attempting to reassign a constant is a compile error:

MAX_PLAYERS = 32; # COMPILER ERROR: Cannot reassign constant MAX_PLAYERS

**Note:** The compiler determines something is a constant when its name follows UPPER_SNAKE_CASE convention and it is declared at the global scope. You can explicitly mark a variable constant with the const keyword as well.

## **6.3 Null and the void Type**

void represents the absence of a value. A variable of type void can only hold null. This is used for optional values and for functions that do not return a result.

void result = null;

\# Check for null before using

if (result == null) {

print("No value present");

}

\# A function that returns nothing

func logMessage(string msg) -> void {

print(msg);

\# no return statement needed

}

## **6.4 Type Conversion**

Flux does not perform implicit type conversion. You must explicitly cast when converting between types. This prevents subtle bugs where values are silently mangled.

float score = 98.6;

\# Explicit cast: (target_type) value

int roundedScore = (int) score; # roundedScore = 98 (truncated)

string scoreText = (string) score; # scoreText = "98.6"

\# Casting a string to a number (will panic if string is not a valid number)

string input = "42";

int parsed = (int) input; # parsed = 42

\# Safe cast: returns null instead of panicking on failure

int? safeValue = (int?) input;

if (safeValue != null) {

print(safeValue);

}

  

# **7\. Operators & Logic**

Operators are symbols that perform operations on values. Flux includes all standard operators plus several unique ones that make common programming tasks more expressive.

## **7.1 Arithmetic Operators**

|     |     |     |
| --- | --- | --- |
| **Operator** | **Name** | **Example** |
| +   | Addition | int sum = 5 + 3; // 8 |
| \-  | Subtraction | int diff = 10 - 4; // 6 |
| \*  | Multiplication | float area = 3.0 \* 4.0; // 12.0 |
| /   | Division | float half = 10 / 4.0; // 2.5 |
| %   | Modulo (remainder) | int rem = 10 % 3; // 1 |
| ++  | Increment (add 1) | count++; // same as count = count + 1 |
| \-- | Decrement (subtract 1) | lives--; // same as lives = lives - 1 |

## **7.2 Assignment Operators**

int x = 10;

x += 5; # x = x + 5 --> 15

x -= 3; # x = x - 3 --> 12

x \*= 2; # x = x \* 2 --> 24

x /= 4; # x = x / 4 --> 6

x %= 4; # x = x % 4 --> 2

## **7.3 Comparison Operators**

|     |     |     |
| --- | --- | --- |
| **Operator** | **Meaning** | **Example** |
| \== | Equal to (identity) | 5 == 5 // true |
| !=  | Not equal to | 5 != 6 // true |
| <   | Less than | 3 < 5 // true |
| \>  | Greater than | 5 > 3 // true |
| <=  | Less than or equal to | 5 <= 5 // true |
| \>= | Greater than or equal to | 10 >= 9 // true |

## **7.4 Logical Operators**

bool a = true;

bool b = false;

a && b # AND: true only if BOTH are true --> false

a || b # OR: true if at LEAST ONE is true --> true

!a # NOT: flips the value --> false

## **7.5 The Exclusionary Operator: butnot**

butnot is a semantic operator unique to Flux. It is equivalent to && ! (AND NOT), but reads more naturally, especially when working with permission logic or filtering.

\# butnot: true when A is true AND B is false

\# Equivalent to: A && !B

bool isAdmin = true;

bool isBanned = false;

if (isAdmin butnot isBanned) {

print("Access granted.");

}

\# Truth table for A butnot B:

\# A=true, B=true --> false (Admin, but is also banned)

\# A=true, B=false --> true (Admin and NOT banned)

\# A=false, B=true --> false (Not admin)

\# A=false, B=false --> false (Not admin)

## **7.6 Semantic Comparators**

Standard equality (==) in many languages has hidden type-coercion rules that cause bugs. Flux solves this with three distinct equality operators, each with a clear, documented contract.

### **\== (Identity Equality)**

Compares values logically, allowing cross-type comparisons when the value is representable in both types. Use this for general equality checks.

string s = "10";

int i = 10;

bool b = true;

print(s == i); # true ("10" and 10 have the same value)

print(1 == b); # true (1 and true are identity-equal)

print("A" == "A"); # true

### **\=num= (Numeric Strict Equality)**

Returns true ONLY if both operands are valid numeric types (int, long, float, byte) and their values match. Strings that look like numbers do NOT pass this check.

print("50" =num= 50); # false - string vs int

print(50.0 =num= 50); # true - float vs int (both numeric)

print(50L =num= 50); # true - long vs int (both numeric)

print(true =num= 1); # false - bool is not numeric in =num= context

### **\=word= (String Strict Equality)**

Returns true ONLY if both operands are string types and their content matches exactly, including case.

print("hello" =word= "hello"); # true

print("Hello" =word= "hello"); # false - case sensitive

print("50" =word= 50); # false - int is not a string

print("50" =word= "50"); # true

**Note:** Use == for everyday comparisons. Use =num= when you need to guarantee you are comparing real numbers (e.g., financial calculations, physics). Use =word= when you need to guarantee you are comparing text (e.g., password checks, command parsing).

## **7.7 Operator Precedence**

When multiple operators appear in a single expression, Flux evaluates them in a fixed order. Higher rows in this table are evaluated first. Use parentheses to override this order.

|     |     |     |
| --- | --- | --- |
| **Level** | **Operators** | **Description** |
| 1 (highest) | . \[\] () | Accessors, subscript, call |
| 2   | ! - ++ -- .random | Unary operators |
| 3   | \* / % | Multiplicative |
| 4   | \+ - | Additive |
| 5   | &lt; &gt; &lt;= &gt;= | Relational |
| 6   | \=num= =word= == != | Equality / Semantic |
| 7   | && \| butnot | Logical |
| 8 (lowest) | \= += -= \*= /= new_type= | Assignment |

  

# **8\. Control Flow**

Control flow statements change the order in which code executes — they allow you to make decisions, repeat actions, and skip code based on conditions.

## **8.1 If / elif / else**

The if statement executes a block of code only if its condition is true. Use elif (else if) to check additional conditions. Use else as a fallback.

int score = 75;

if (score >= 90) {

print("Grade: A");

} elif (score >= 80) {

print("Grade: B");

} elif (score >= 70) {

print("Grade: C");

} else if (score >= 60) {

print("Grade: D"); # else if also works

} else {

print("Grade: F");

}

**Note:** Both elif and else if are valid syntax in Flux. They are identical in behavior. elif is preferred by convention for consistency.

## **8.2 Switch Statements**

A switch statement is a cleaner alternative to a long chain of if/elif when you are testing a single variable against multiple fixed values.

int statusCode = 404;

switch (statusCode) {

case 200:

print("200 OK - Success");

break;

case 301:

print("301 Moved Permanently");

break;

case 404:

print("404 Not Found");

break;

case 500:

print("500 Internal Server Error");

break;

default:

print("Unknown status code: $statusCode");

}

**Warning:** Every case block must end with break; unless you intentionally want fall-through behavior. Without break, execution continues into the next case.

### **Switching on Strings and Enums**

\# Switch works on strings

string command = "jump";

switch (command) {

case "jump":

player.jump();

break;

case "attack":

player.attack();

break;

default:

print("Unknown command");

}

\# Switch works on enums

Direction dir = Direction.NORTH;

switch (dir) {

case Direction.NORTH: moveUp(); break;

case Direction.SOUTH: moveDown(); break;

case Direction.EAST: moveRight(); break;

case Direction.WEST: moveLeft(); break;

}

## **8.3 For Loop**

The for loop repeats a block of code a controlled number of times. It has three parts: an initializer, a condition, and an update expression.

\# Standard for loop: count from 0 to 9

for (int i = 0; i < 10; i++) {

print(i);

}

\# Counting downward

for (int i = 10; i > 0; i--) {

print(i);

}

\# Iterating over a list

List&lt;string&gt; names = \["Alice", "Bob", "Carol"\];

for (int i = 0; i < names.length; i++) {

print("Hello, ${names\[i\]}!");

}

\# For-each style iteration

for (string name in names) {

print("Hello, $name!");

}

## **8.4 While Loop**

The while loop repeats as long as a condition is true. It checks the condition BEFORE executing the body, so if the condition starts as false, the body never runs.

int lives = 3;

while (lives > 0) {

print("Playing... lives left: $lives");

lives = lives - playRound(); # playRound() returns lives lost

}

print("Game Over!");

## **8.5 Do-While Loop**

The do-while loop is similar to while, but it checks the condition AFTER executing the body. This guarantees the body runs at least once.

bool connectionFailed = true;

int attempts = 0;

do {

attempts++;

print("Connection attempt #$attempts...");

connectionFailed = tryConnect();

} while (connectionFailed && attempts < 5);

if (connectionFailed) {

print("Could not connect after 5 attempts.");

} else {

print("Connected!");

}

## **8.6 Loop Control: break and continue**

Inside any loop, two keywords give you fine-grained control over execution flow:

\# break: exit the loop immediately

for (int i = 0; i < 100; i++) {

if (i == 50) {

print("Stopping at 50.");

break; # Loop ends here. i will be 50.

}

print(i);

}

\# continue: skip the rest of this iteration and go to the next

for (int i = 0; i < 10; i++) {

if (i % 2 == 0) {

continue; # Skip even numbers

}

print(i); # Only prints 1, 3, 5, 7, 9

}

  

# **9\. Functions, Methods & Closures**

Functions are named, reusable blocks of code. They are the primary way to organize and structure your programs in Flux.

## **9.1 Declaring Functions**

Functions are declared with the func keyword. The return type is specified after a -> arrow. If the return type is omitted, it defaults to int and returns 0 automatically.

\# Function with return type annotation

func add(int a, int b) -> int {

return a + b;

}

\# Function with no return value

func logError(string message) -> void {

print("\[ERROR\] $message");

}

\# Function with no return type annotation

\# (defaults to int, auto-returns 0)

func initialize() {

setupDisplay();

loadAssets();

\# implicitly returns 0 here

}

## **9.2 Calling Functions**

int sum = add(3, 7); # sum = 10

logError("File not found");

int status = initialize(); # status = 0

## **9.3 Named Arguments**

Flux supports named arguments, which allow you to pass arguments in any order and make your call sites more readable. Especially useful when a function has many parameters.

func createWindow(int width, int height, string title, bool fullscreen) {

\# ...

}

\# Positional call (must be in order)

createWindow(1920, 1080, "My App", false);

\# Named call (any order)

createWindow(title: "My App", fullscreen: false, width: 1920, height: 1080);

\# Mix of positional and named

createWindow(1920, 1080, title: "My App", fullscreen: true);

## **9.4 Default Parameter Values**

Parameters can have default values, making them optional in function calls.

func connect(string host, int port = 80, bool secure = false) -> bool {

\# ...

}

\# Using defaults

connect("google.com"); # port=80, secure=false

connect("google.com", 443, true); # All specified

connect("google.com", secure: true); # port=80 (default), secure=true

## **9.5 Access Modifiers on Functions**

Functions inside classes and exported from modules can have access modifiers. At the file level, functions are private by default.

public func visibleToEveryone() { ... }

private func onlyInsideThisFile() { ... }

## **9.6 Generics**

Generic functions can operate on any type. The type parameter is declared inside angle brackets &lt; &gt; before the argument list.

\# A generic function that works with any type T

func printItem&lt;T&gt;(T item) -> void {

print(item);

}

printItem&lt;int&gt;(42);

printItem&lt;string&gt;("Hello");

printItem&lt;float&gt;(3.14);

\# Generic function returning T

func getFirst&lt;T&gt;(List&lt;T&gt; list) -> T {

return list\[0\];

}

## **9.7 Anonymous Functions (Lambdas)**

Lambdas are functions without names. They are useful as short, inline callbacks — for sorting, event handling, or any case where you need to pass behavior as a value.

\# Lambda syntax: (params) => { body }

\# or for single expressions: (params) => expression

\# Sort a list of integers ascending

List&lt;int&gt; scores = \[88, 42, 95, 71, 56\];

scores.sort( (a, b) => { return a < b; } );

\# scores is now \[42, 56, 71, 88, 95\]

\# Short form (single expression)

scores.sort( (a, b) => a < b );

\# Store a lambda in a variable

func&lt;int, int, int&gt; multiply = (int x, int y) => x \* y;

int result = multiply(6, 7); # result = 42

## **9.8 Recursive Functions**

\# Functions can call themselves (recursion)

func factorial(int n) -> int {

if (n <= 1) return 1;

return n \* factorial(n - 1);

}

print(factorial(5)); # 120

  

# **10\. Object-Oriented Programming**

Flux is a multi-paradigm language with full support for Object-Oriented Programming (OOP). OOP helps you model real-world concepts by grouping related data and behavior into units called objects.

## **10.1 Classes & Objects**

A class is a blueprint. An object is a specific instance created from that blueprint. You create an object with the new keyword.

class Player {

\# Properties (data the player holds)

public string gamertag;

public int level;

private int score; # private: only accessible inside this class

private int health;

\# Constructor: called when 'new Player(...)' is used

func init(string name) {

gamertag = name;

level = 1;

score = 0;

health = 100;

}

\# Public method

public func addScore(int points) -> void {

score += points;

}

\# Getter for private property

public func getScore() -> int {

return score;

}

public func takeDamage(int dmg) -> void {

health -= dmg;

if (health < 0) health = 0;

}

public func isAlive() -> bool {

return health > 0;

}

}

\# Creating and using an object

Player hero = new Player("Atlas");

hero.addScore(500);

print("${hero.gamertag} has ${hero.getScore()} points.");

## **10.2 Inheritance**

A child class can extend a parent class, inheriting all its public and protected properties and methods. Use `extends` to declare inheritance. Flux supports single inheritance, including multi-level inheritance chains (e.g., `Child extends Middle extends Base`).

Within a method body, other methods on the same object (including inherited methods) can be called by name directly, without needing to qualify them through the object reference. Fields are also accessible by name.

```
class Base {
    public string tag;
    func init() { tag = "base"; }
    public func identify() -> string { return "I am $tag"; }
}

class Child extends Base {
    func init() { super.init(); tag = "child"; }
    public func info() -> string {
        return identify();  # Calls inherited method by name
    }
}
```

\# Parent class

class Character {

public string name;

protected int hp; # protected: accessible in this class AND subclasses

func init(string n, int startHp) {

name = n;

hp = startHp;

}

public func speak() -> void {

print("$name says hello.");

}

}

\# Child class inherits from Character

class Warrior extends Character {

private int armor;

func init(string n) {

super.init(n, 200); # Call parent constructor

armor = 50;

}

\# Override the parent method

public func speak() -> void {

print("$name roars: FOR GLORY!");

}

public func block() -> int {

return armor;

}

}

Warrior w = new Warrior("Thor");

w.speak(); # Prints: Thor roars: FOR GLORY!

## **10.3 Interfaces**

An interface defines a contract: a set of methods that a class must implement. Interfaces cannot have any implementation themselves — they only declare what methods must exist. Use implements to apply an interface to a class.

\# Define an interface

interface Serializable {

func toJson() -> string;

func toBytes() -> byte\[\];

}

interface Printable {

func print() -> void;

}

\# Implement multiple interfaces

class SaveState implements Serializable, Printable {

int level;

int score;

func init(int l, int s) { level = l; score = s; }

\# Must implement ALL methods from Serializable

public func toJson() -> string {

return "{\\"level\\": $level, \\"score\\": $score}";

}

public func toBytes() -> byte\[\] {

\# ... implementation

}

\# Must implement ALL methods from Printable

public func print() -> void {

print("Level $level | Score $score");

}

}

## **10.4 Structs**

Structs are lightweight data containers. Unlike classes, structs are passed by value (a copy is made when passed to a function), not by reference. They do not support inheritance or interfaces. Use structs for simple data groups like coordinates, colors, or sizes.

struct Vector3 {

float x;

float y;

float z;

}

struct Rect {

int x;

int y;

int width;

int height;

}

\# Creating a struct (no 'new' keyword needed)

Vector3 velocity = { x: 0.0, y: -9.81, z: 0.0 };

Rect viewport = { x: 0, y: 0, width: 1920, height: 1080 };

\# Accessing fields

velocity.y = -5.0;

print("Velocity Y: ${velocity.y}");

|     |     |     |
| --- | --- | --- |
| **Feature** | **Class** | **Struct** |
|     |     |
| Passed by | Reference | Value (copy) |
| Inheritance | Yes (extends) | No  |
| Interfaces | Yes (implements) | No  |
| Constructor | Yes (func init) | No (field initializer) |
| Best for | Complex entities with behavior | Simple data containers |

### **Struct Initialization Syntax**

Structs support named field initializer syntax using the struct type name followed by a braced list of `field: value` pairs. This is the preferred style when the struct type is not already specified by the variable declaration:

```flux
# With explicit type on the variable
Vector3 velocity = { x: 0.0, y: -9.81, z: 0.0 };

# With 'let' inference — struct name prefix required
let velocity = Vector3 { x: 0.0, y: -9.81, z: 0.0 };

# Multi-field struct
struct BootInfo {
    long framebuffer_addr;
    int width;
    int height;
    int pitch;
}

let info = BootInfo {
    framebuffer_addr: 0xFD000000,
    width: 1024,
    height: 768,
    pitch: 4096
};
```

All fields must be assigned at initialization. The order of fields in the initializer does not need to match the order in the struct definition.

  

# **11\. Error Handling & Safety**

Robust software must handle failure gracefully. Flux provides two complementary mechanisms: Exceptions (for recoverable errors) and Panics (for unrecoverable, fatal errors).

## **11.1 Try / Catch (Recoverable Errors)**

Use try/catch when a block of code might fail in ways you can handle — file not found, network timeout, invalid user input. The program continues running after a caught exception.

try {

\# Code that might fail

string content = fs.read("/config/settings.json");

HttpClient c = new HttpClient();

Response r = c.get("https://api.example.com/data");

} catch (FileSystemError e) {

\# Handle a specific error type

print("Could not read config: $e.message");

loadDefaultConfig(); # Fall back gracefully

} catch (NetworkError e) {

print("Network request failed: $e.message (code: $e.code)");

} catch (error e) {

\# Catch-all for any other error type

print("Unexpected error: $e");

} finally {

\# This block ALWAYS runs, whether or not an exception occurred

print("Cleanup complete.");

}

### **Error Object Properties**

All error objects have these standard properties:

- **e.message** — A human-readable description of the error.
- **e.code** — An integer error code (for network and system errors).
- **e.stack** — A string containing the call stack at the point of failure.

### **Custom Errors**

class GameError extends error {

string context;

func init(string msg, string ctx) {

super.message = msg;

context = ctx;

}

}

\# Throw a custom error

func loadLevel(int id) {

if (id < 0) {

throw new GameError("Invalid level ID", "loadLevel()");

}

\# ... load the level

}

## **11.2 panic (Unrecoverable Errors)**

panic is for situations where the program cannot safely continue. It is not caught by try/catch. When panic is called, the current thread halts immediately and an error message is printed. If called in kernel mode, the entire OS halts.

\# Use panic for situations that should NEVER happen in correct code

func allocateBuffer(int size) -> byte\[\] {

if (size <= 0) {

panic("allocateBuffer called with size <= 0. This is a programmer error.");

}

\# ...

}

\# Kernel-level panic (halts the OS)

if (memoryCorruption == true) {

panic("KERNEL FAULT: Memory corruption detected at 0xDEADBEEF. System halted.");

}

**Warning:** panic is a last resort, not a normal error handling tool. If a situation is recoverable at all, use try/catch instead.

## **11.3 Error Types Reference**

|     |     |
| --- | --- |
| **Error Type** | **Thrown When** |
| FileSystemError | File not found, permission denied, disk full |
| NetworkError | Connection refused, timeout, DNS failure |
| ParseError | Invalid JSON, malformed data, bad cast |
| MemoryError | Allocation failure, out-of-memory |
| TypeError | Type mismatch at runtime (rare in Flux) |
| IndexError | Array/list access out of bounds |
| error | Base type — catches any error not listed above |

  

# **12\. Concurrency**

Modern applications need to do multiple things at once — loading data, processing input, and rendering a frame simultaneously. Flux provides first-class support for concurrency through OS-level threads.

## **12.1 Threads**

A thread is an independent path of execution. Flux threads map 1:1 to OS threads, giving you direct control over the CPU.

import std.sys;

func downloadFile(string url) {

print("Downloading: $url");

\# ... network code

print("Done: $url");

}

func main() {

\# Launch a function in a new thread

thread t1 = thread.run(downloadFile, "https://example.com/file1.zip");

thread t2 = thread.run(downloadFile, "https://example.com/file2.zip");

\# Both downloads happen simultaneously.

\# Wait for both to finish before continuing.

t1.join();

t2.join();

print("All downloads complete.");

}

## **12.2 Mutex (Mutual Exclusion)**

When multiple threads access the same data, you can get race conditions — two threads reading and writing the same variable at the same time. A mutex (lock) ensures only one thread can access a protected section at a time.

import std.sys;

int sharedCounter = 0;

mutex counterLock;

func increment() {

for (int i = 0; i < 1000; i++) {

counterLock.lock();

sharedCounter++; # Protected section

counterLock.unlock();

}

}

func main() {

thread t1 = thread.run(increment);

thread t2 = thread.run(increment);

t1.join();

t2.join();

print(sharedCounter); # Always 2000, never a random value

}

## **12.3 Atomic Variables**

For simple integer counters and flags, atomic variables provide thread-safe operations without the overhead of a full mutex. An atomic variable's reads and writes are guaranteed to be indivisible — they can never be interrupted mid-operation.

\# Declare an atomic integer

atomic int requestCount = 0;

func handleRequest() {

requestCount++; # Thread-safe increment, no lock needed

\# ...process request

}

\# Atomic bool for flags

atomic bool isShuttingDown = false;

func workerLoop() {

while (isShuttingDown butnot false) {

doWork();

}

}

**Note:** Use atomic for simple counters and boolean flags. Use mutex for anything more complex — protecting multiple lines of code or complex data structures.

  

# **13\. Networking & I/O**

Flux includes a full networking stack in std.net and filesystem and console I/O in std.io.

## **13.1 Standard I/O**

import std.io;

\# Print to stdout

print("Hello, World!");

\# Print without newline

print_raw("Enter value: ");

\# Read a line from stdin

string name = input("What is your name? ");

print("Hello, $name!");

\# Read a number from stdin

int age = (int) input("Enter your age: ");

## **13.2 File System**

import std.io;

\# Read a file as a string

try {

string contents = fs.read("/home/user/notes.txt");

print(contents);

} catch (FileSystemError e) {

print(e.message);

}

\# Write a string to a file (overwrites if exists)

fs.write("/tmp/output.txt", "Hello from Flux!\\n");

\# Append to a file

fs.append("/var/log/app.log", "\[INFO\] Server started.\\n");

\# Check if a file exists

if (fs.exists("/config/settings.json")) {

string config = fs.read("/config/settings.json");

}

\# Delete a file

fs.delete("/tmp/tempfile.bin");

\# List directory contents

List&lt;string&gt; files = fs.list("/home/user/documents/");

for (string file in files) {

print(file);

}

## **13.3 HTTP Client**

import std.net;

func fetchUserData(int userId) -> string {

HttpClient client = new HttpClient();

\# GET request

Response res = client.get("https://api.example.com/users/$userId");

if (res.statusCode =num= 200) {

return res.body;

} else {

throw new NetworkError("API returned: ${res.statusCode}");

}

}

\# POST request with body

func createUser(string json) -> int {

HttpClient client = new HttpClient();

client.setHeader("Content-Type", "application/json");

client.setHeader("Authorization", "Bearer mytoken");

Response res = client.post("https://api.example.com/users", json);

return res.statusCode;

}

## **13.4 TCP Sockets**

For low-level network communication, Flux provides a Socket API with full TCP and UDP support.

import std.net;

\# TCP Client

func connectToServer() {

Socket s = new Socket(Protocol.TCP);

s.connect("192.168.1.100", 8080);

s.write("HELLO SERVER");

string response = s.readLine();

print(response);

s.close();

}

\# TCP Server

func runServer() {

Socket server = new Socket(Protocol.TCP);

server.bind(8080);

server.listen(10); # Max 10 queued connections

print("Server listening on port 8080...");

while (true) {

Socket client = server.accept();

thread t = thread.run(handleClient, client);

}

}

  

# **14\. Mathematics & Physics**

Flux has math deeply integrated into the language itself, not just as a library bolted on after the fact.

## **14.1 The std.math Library**

import std.math;

float a = math.sqrt(144); # 12.0

float b = math.pow(2, 10); # 1024.0

float c = math.abs(-42.5); # 42.5

float d = math.floor(3.9); # 3.0

float e = math.ceil(3.1); # 4.0

float f = math.round(3.5); # 4.0

float g = math.min(5.0, 3.0); # 3.0

float h = math.max(5.0, 3.0); # 5.0

float i = math.clamp(15, 0, 10); # 10.0 (clamp to range \[0,10\])

float j = math.lerp(0, 100, 0.5); # 50.0 (linear interpolation)

\# Trigonometry

float s = math.sin(math.PI / 2); # 1.0

float c2 = math.cos(0.0); # 1.0

float t = math.tan(math.PI / 4); # 1.0

float angle = math.atan2(1, 1); # PI/4

\# Inverse trigonometry

float as = math.asin(1.0); # PI/2

float ac = math.acos(1.0); # 0.0

\# Logarithms and exponentials

float ln = math.log(2.718); # ~1.0 (natural log)

float l2 = math.log2(8.0); # 3.0

float l10 = math.log10(100.0); # 2.0

float ex = math.exp(1.0); # ~2.718 (e^x)

\# Constants

float PI = math.PI; # 3.14159...

float E = math.E; # 2.71828...

float TAU = math.TAU; # 2 \* PI

float INF = math.INF; # Infinity

## **14.2 Intrinsic Randomness**

All primitive types have a built-in .random property that generates a random value of that type, seeded from OS entropy. No setup or seeding required.

\# float.random: generates a float between 0.0 (inclusive) and 1.0 (exclusive)

float roll = float.random;

\# int.random: generates any int in the full signed 32-bit range

int seed = int.random;

\# bool.random: generates true or false with 50/50 probability

bool coinFlip = bool.random;

\# Practical use: random number in a range \[min, max\]

func randomInRange(int min, int max) -> int {

return min + (int)(float.random \* (max - min + 1));

}

int dice = randomInRange(1, 6);

print("Rolled: $dice");

## **14.3 LaTeX Math Engine**

Flux can evaluate mathematical expressions written in LaTeX notation. Wrap a LaTeX expression in backticks and assign it to a float variable. The compiler translates the expression into optimized machine code — there is no string parsing at runtime.

\# Pythagorean theorem

float hyp = \`\\sqrt{a^2 + b^2}\`;

\# Kinematic equation: distance = v_i\*t + (1/2)\*a\*t^2

float d = \`v_i t + \\frac{1}{2} a t^2\`;

\# Quadratic formula

float x = \`\\frac{-b + \\sqrt{b^2 - 4ac}}{2a}\`;

\# Trigonometry

float angle = \`\\sin(x) + \\cos(y)\`;

\# Wave equation

float wave = \`A \\sin(\\omega t + \\phi)\`;

**Note:** Variables in LaTeX expressions refer to Flux variables in the current scope. Ensure the variables used in the LaTeX expression are declared and initialized before the expression is evaluated.

## **14.4 Vectors and Matrices**

vec2, vec3, and mat4 are primitive types in Flux with built-in operations. They use hardware-accelerated SIMD instructions automatically.

\# Vector operations

vec3 a = vec3(1.0, 2.0, 3.0);

vec3 b = vec3(4.0, 5.0, 6.0);

vec3 sum = a + b; # Component-wise addition

vec3 scaled = a \* 2.0; # Scalar multiplication

float dot = a.dot(b); # Dot product

vec3 cross = a.cross(b); # Cross product

float len = a.length(); # Magnitude

vec3 normal = a.normalize(); # Unit vector

\# Matrix operations

mat4 proj = mat4.perspective(60.0, 16.0/9.0, 0.1, 1000.0);

mat4 view = mat4.lookAt(cameraPos, targetPos, upVector);

mat4 mvp = proj \* view; # Matrix multiplication

\# Transform a point

vec3 worldPos = vec3(5, 0, -10);

vec3 screenPos = mvp.transform(worldPos);

  

# **15\. Memory Management & Pointers**

Flux gives you control over memory at multiple levels — from fully automatic management (the default) to raw pointer manipulation (for OS and driver development).

## **15.1 Automatic Reference Counting (ARC)**

By default, you do not manage memory in Flux. The compiler automatically inserts retain and release calls around your variables. When the last reference to a value goes out of scope, its memory is freed immediately.

func processData() {

\# The compiler automatically frees 'data' when processData() returns.

string data = fs.read("/tmp/huge_file.bin");

\# ... use data

} # <-- 'data' is freed here automatically

\# ARC tracks references, not just scope

func main() {

Player p = new Player("Atlas");

Player ref = p; # ARC count: 2 (p and ref both point to the same object)

p.delete; # ARC count: 1 (only ref remains)

} # <-- ref goes out of scope, ARC count: 0, object is freed

## **15.2 Manual Memory Control**

You can take manual control when you know you need to free something immediately (e.g., releasing a large texture before loading the next one).

\# .delete: Force immediate deallocation

\# This calls the destructor and frees all memory NOW.

Texture bigTexture = Texture.load("world_map.png");

\# ... use the texture ...

bigTexture.delete; # Free it NOW before loading the next one

\# cleanup: trigger a garbage collection sweep

\# This cleans up any ARC objects with zero references in the current scope.

func loadLevel(int id) {

\# Load lots of assets...

\# ...

cleanup; # Sweep and free all temporary objects right now

}

## **15.3 Unsafe Blocks & Raw Pointers**

When writing kernel code, device drivers, or working directly with hardware memory maps, you sometimes need to bypass ARC entirely and work with raw memory addresses. This is only possible inside an explicitly marked unsafe { } block.

\# Raw pointer arithmetic is ONLY allowed inside unsafe { }

unsafe {

\# Access the VGA text buffer at memory address 0xB8000

int\* ptr = 0xB8000;

\*ptr = 0x0F41; # Write white 'A' on black background

\# Pointer arithmetic

int\* next = ptr + 1;

\*next = 0x0F42; # Write 'B' in the next position

\# Direct memory allocation (no ARC)

void\* rawMem = mem.alloc(4096); # Allocate 4KB

\# ... use the memory ...

mem.free(rawMem); # Must be freed manually!

}

**Warning:** Code inside unsafe{} bypasses all of Flux's safety guarantees. Memory leaks, buffer overflows, and system crashes are all possible. Only use unsafe{} when absolutely necessary.

## **15.4 Memory Layout & Alignment**

|     |     |     |     |
| --- | --- | --- | --- |
| **Type** | **Size** | **Alignment** | **Notes** |
| bool | 1 byte | 1 byte | Stored as 0 or 1 |
| byte | 1 byte | 1 byte | Unsigned 0–255 |
| char | 1 byte | 1 byte | ASCII value |
| int | 4 bytes | 4 bytes | 32-bit signed |
| long | 8 bytes | 8 bytes | 64-bit signed |
| float | 8 bytes | 8 bytes | IEEE 754 double precision |
| pointer (\*) | 8 bytes | 8 bytes | 64-bit address (ELF-64) |
| vec3 | 24 bytes | 8 bytes | 3x float (SIMD-friendly) |
| mat4 | 128 bytes | 16 bytes | 16x float (16-byte aligned) |

## **15.5 Pointer Types**

For systems programming, Flux supports pointer type declarations using the `*` suffix on any type name. Pointer variables hold memory addresses and are primarily used inside `unsafe` blocks or in freestanding kernel code.

```flux
unsafe {
    byte* ptr = 0xB8000;        # Pointer to VGA text buffer
    int* counter = 0x1000;      # Pointer to an integer in memory
    BootInfo* info = 0x7E00;    # Pointer to a struct
}
```

### **Member Access on Pointers**

The transpiler tracks which variables in the current scope are pointers. When accessing a member of a pointer variable, the transpiler emits `->` (arrow operator) instead of `.` (dot operator) in the generated C++. In Flux source code, the dot syntax is used uniformly — the pointer-vs-value distinction is handled automatically:

```flux
unsafe {
    BootInfo* info = getBootInfo();

    # In Flux, use dot syntax regardless of pointer status
    int w = info.width;
    int h = info.height;

    # The transpiler emits: info->width, info->height
}
```

Pointer tracking is per-scope. A variable declared as a pointer in an outer scope is recognized as a pointer in nested scopes (function bodies, if-blocks, loops).

### **Pointer Arithmetic**

Standard pointer arithmetic is supported inside `unsafe` blocks:

```flux
unsafe {
    byte* base = 0xB8000;
    byte* next = base + 2;     # Advance by 2 bytes
    *next = 0x41;              # Write to the address
}
```

## **15.6 Inline Assembly**

Flux provides an `asm` statement for embedding raw assembly instructions directly in generated code. Inline assembly is only permitted inside `unsafe` blocks.

```flux
unsafe {
    asm("cli");                # Disable interrupts
    asm("hlt");                # Halt the CPU
}
```

### **Operands and Constraints**

The `asm` statement supports GAS-style (GNU Assembler) syntax with output operands, input operands, and register constraints:

```flux
unsafe {
    # Read from an I/O port
    byte result;
    asm("inb %1, %0" : "=a"(result) : "Nd"(port));

    # Write to an I/O port
    asm("outb %0, %1" : : "a"(value) : "Nd"(port));

    # Full syntax:
    # asm("instruction" : output_operands : input_operands);
    #   "=constraint"(variable)  — output operand
    #   "constraint"(expression) — input operand
}
```

Common constraints:

|     |     |
| --- | --- |
| **Constraint** | **Meaning** |
| `"a"` | The `%eax` / `%rax` register |
| `"b"` | The `%ebx` / `%rbx` register |
| `"c"` | The `%ecx` / `%rcx` register |
| `"d"` | The `%edx` / `%rdx` register |
| `"r"` | Any general-purpose register |
| `"m"` | A memory operand |
| `"i"` | An immediate integer operand |
| `"N"` | An 8-bit unsigned immediate (0–255), used for I/O port numbers |
| `"="` | Output operand (write-only) prefix |

### **Register Width Modifiers**

When an instruction requires a specific operand width, use register modifiers on the operand references:

|     |     |
| --- | --- |
| **Modifier** | **Effect** |
| `%0` | Default (full) register width for operand 0 |
| `%w0` | 16-bit register (e.g., `%ax` instead of `%eax`) |
| `%b0` | 8-bit register (e.g., `%al` instead of `%eax`) |

```flux
unsafe {
    int value = 0x0F41;

    # Use %w0 for 16-bit write to VGA buffer
    asm("movw %w0, (%1)" : : "r"(value) : "r"(vga_ptr));

    # Use %b0 for 8-bit I/O port write
    asm("outb %b0, %w1" : : "a"(data) : "Nd"(port));
}
```

These modifiers are essential when working with hardware interfaces that expect specific register widths (e.g., VGA text mode, I/O ports, interrupt descriptor tables).

  

# **16\. Graphics & StratOS Internals**

This chapter covers Flux's built-in graphics capabilities, which are unique to the language. These features are designed for StratOS's direct-framebuffer rendering model.

## **16.1 The StratOS Rendering Model**

StratOS has no traditional window manager. Instead, all applications render directly to a framebuffer. Flux provides native types and intrinsics to make this seamless. The Display object represents the active framebuffer.

import std.graphics;

\# Get the display dimensions

int screenW = Display.width; # e.g., 1920

int screenH = Display.height; # e.g., 1080

\# Plot a single pixel (direct framebuffer write)

\# color32 format: 0xRRGGBBAA

Display.plot(100, 200, 0xFF0000FF); # Red pixel at (100, 200)

\# Clear the screen to a color

Display.clear(0x1A1A2EFF); # Dark navy background

\# Present the buffer (swap front/back buffer)

Display.present();

## **16.2 Low-Level Framebuffer Access**

For maximum performance (e.g., in a game engine loop), you can access the framebuffer as a raw array and write pixels directly.

\# Direct buffer write: fastest possible pixel writing

func plotPixel(int x, int y, color32 color) {

Display.buffer\[y \* Display.width + x\] = color;

}

\# Draw a filled rectangle

func fillRect(int x, int y, int w, int h, color32 color) {

for (int row = y; row < y + h; row++) {

for (int col = x; col < x + w; col++) {

Display.buffer\[row \* Display.width + col\] = color;

}

}

}

\# Example: Game render loop

func renderLoop() {

while (true) {

Display.clear(0x000000FF); # Black background

updateGameState();

renderScene();

Display.present(); # Show the frame

}

}

## **16.3 The Entity System**

For 3D development, Flux provides a scene graph with built-in entity types. Entities are high-level objects that the Flux runtime knows how to render.

import std.graphics;

import std.scene;

\# Create primitive 3D objects

Entity cube = new Primitive.Cube(size: 1.0);

Entity sphere = new Primitive.Sphere(radius: 0.5, segments: 32);

Entity plane = new Primitive.Plane(width: 10.0, depth: 10.0);

\# Load and apply a texture (Warp = texture mapping)

Texture grass = Texture.load("assets/textures/grass.png");

cube.warp(grass);

\# Position, rotate, scale

cube.position = vec3(0, 1, -5);

cube.rotation = vec3(0, 45, 0); # Degrees

cube.scale = vec3(2, 2, 2);

\# Add to the scene

Scene.add(cube);

Scene.add(sphere);

Scene.add(plane);

\# Set up camera

Camera cam = new Camera();

cam.position = vec3(0, 3, 5);

cam.lookAt(vec3(0, 0, 0));

Scene.camera = cam;

\# Render loop

while (true) {

cube.rotation.y += 1.0; # Rotate cube each frame

Scene.render();

Display.present();

}

## **16.4 Lighting**

\# Add a directional light (like the sun)

Light sun = new Light(LightType.DIRECTIONAL);

sun.direction = vec3(-1, -1, -1);

sun.color = 0xFFFFE0FF; # Warm white

sun.intensity = 1.0;

Scene.addLight(sun);

\# Add a point light (like a lamp)

Light lamp = new Light(LightType.POINT);

lamp.position = vec3(0, 5, 0);

lamp.color = 0xFF4400FF; # Orange

lamp.intensity = 2.0;

lamp.range = 20.0;

Scene.addLight(lamp);

  

# **17\. Interoperability (C++ Bridge)**

StratOS must run legacy drivers, system libraries, and other C/C++ code. Flux provides a first-class C++ bridge that lets you call C++ code directly from Flux, and vice versa.

## **17.1 Importing C++ Files**

Pass a .cpp file to the import statement just like a .lx library. The Flux compiler (which includes a Clang-based C++ backend) will compile it alongside your Flux code.

\# wrapper.flux

import "drivers/audio_driver.cpp";

import "libs/openssl.h";

func initAudio() {

\# Call C++ functions directly by their namespace

audio_driver.initialize();

audio_driver.setVolume(80);

}

## **17.2 The Build Command**

When building a project with C++ files, list them all in the build command. The linker unifies them into a single binary.

\# Build a project mixing Flux and C++

fluxc build main.flux engine.cpp renderer.cpp audio.cpp -o game.bin

\# Build a kernel module

fluxc build kernel.flux driver.cpp pci.cpp -Oz -o kernel.bin

## **17.3 Calling Flux from C++**

You can also expose Flux functions to C++ using the export keyword with C linkage.

\# In your Flux file: expose a function with C-compatible ABI

export(c) func fluxCallback(int event_code) -> int {

handleEvent(event_code);

return 0;

}

// In your C++ file: declare the Flux function as extern C

extern "C" int fluxCallback(int event_code);

void registerCallbacks() {

event_system_register_callback(fluxCallback);

}

## **17.4 Type Correspondence**

When passing data between Flux and C++, use the following type mapping:

|     |     |     |
| --- | --- | --- |
| **Flux Type** | **C++ Type** | **Notes** |
| int | int32_t | Always 32-bit signed |
| long | int64_t | Always 64-bit signed |
| float | double | Flux float = C++ double |
| byte | uint8_t | Unsigned 8-bit |
| bool | bool | ABI-compatible |
| void\* | void\* | Raw pointer (unsafe only) |

  

# **18\. Standard Library Reference**

Flux ships with a standard library organized into modules. All modules must be imported before use.

## **18.1 std.io**

|     |     |
| --- | --- |
| **Function / Object** | **Description** |
| print(string s) | Print s to stdout with a newline. |
| print_raw(string s) | Print s without a trailing newline. |
| input(string prompt) -> string | Display prompt and read a line from stdin. |
| fs.read(string path) -> string | Read entire file as UTF-8 string. |
| fs.write(string path, string data) | Write data to file (overwrites). |
| fs.append(string path, string data) | Append data to end of file. |
| fs.exists(string path) -> bool | Return true if path exists. |
| fs.delete(string path) | Delete file at path. |
| fs.list(string dir) -> List&lt;string&gt; | List files in directory. |
| fs.mkdir(string path) | Create directory (and parents). |

## **18.2 std.net**

HTTP and socket networking. In interpreted mode, HTTP requests use libcurl (supports HTTP and HTTPS). In AOT-compiled mode, plain HTTP uses raw sockets, and HTTPS requests delegate to the system `curl` command for TLS support.

|     |     |
| --- | --- |
| **Class / Function** | **Description** |
| HttpClient | HTTP 1.1 / 2.0 client. Instantiate with `new HttpClient()`. |
| .get(string url) -> Response | Send an HTTP GET request. |
| .post(string url, string body) -> Response | Send an HTTP POST request. |
| .put(string url, string body) -> Response | Send an HTTP PUT request. |
| .delete(string url) -> Response | Send an HTTP DELETE request. |
| .setHeader(string key, string value) | Set a request header for subsequent requests. |
| .download(string url, string filePath) -> bool | Download content from `url` and save it to `filePath`. Returns true on success. |
| Response.statusCode -> int | HTTP status code (e.g. 200, 404). |
| Response.body -> string | Response body as a string. |
| Response.headers -> string | Response headers as a string. |
| Socket(Protocol.TCP\|UDP) | Raw TCP/UDP socket. |
| Socket.connect(host, port) | Connect to remote host. |
| Socket.bind(port) | Bind to a local port (for servers). |
| Socket.listen(backlog) | Listen for incoming connections. |
| Socket.accept() -> Socket | Accept one incoming connection. |
| Socket.write(string data) | Send data to remote. |
| Socket.readLine() -> string | Read one line from remote. |
| Socket.close() | Close the socket connection. |

## **18.3 std.math**

|     |     |
| --- | --- |
| **Function / Constant** | **Description** |
| math.sqrt(x) -> float | Square root of x. |
| math.pow(base, exp) -> float | base raised to exp. |
| math.abs(x) -> T | Absolute value (works for int and float). |
| math.floor(x) -> float | Round down to nearest integer. |
| math.ceil(x) -> float | Round up to nearest integer. |
| math.round(x) -> float | Round to nearest integer. |
| math.min(a, b) -> T | Returns the smaller of a and b. |
| math.max(a, b) -> T | Returns the larger of a and b. |
| math.clamp(v, min, max) -> T | Clamp v to the range \[min, max\]. |
| math.lerp(a, b, t) -> float | Linear interpolation from a to b by factor t. |
| math.sin/cos/tan(x) | Standard trigonometric functions (radians). |
| math.asin/acos(x) | Inverse trigonometric functions (radians). |
| math.atan2(y, x) -> float | Arc-tangent of y/x (handles all quadrants). |
| math.log(x) -> float | Natural logarithm (base e). |
| math.log2(x) -> float | Base-2 logarithm. |
| math.log10(x) -> float | Base-10 logarithm. |
| math.exp(x) -> float | Exponential function (e^x). |
| math.PI, math.E, math.TAU, math.INF | Mathematical constants. |

## **18.4 std.collections**

|     |     |
| --- | --- |
| **Type / Method** | **Description** |
| List&lt;T&gt; | Dynamic growable array. |
| .add(item) | Append item to end. |
| .removeAt(index) | Remove item at index. |
| .contains(item) -> bool | Returns true if item exists in list. |
| .sort(comparator) | Sort in-place using comparator lambda. |
| .length -> int | Number of elements. |
| .clear() | Remove all elements. |
| Map&lt;K, V&gt; | Hash table (key-value store). |
| .put(key, value) | Insert or update a key-value pair. |
| .get(key) -> V | Get value for key. Throws if not found. |
| .hasKey(key) -> bool | Returns true if key exists. |
| .remove(key) | Remove a key-value pair. |
| Stack&lt;T&gt; | LIFO (Last-In, First-Out) stack. |
| .push(item), .pop() -> T | Add/remove from top. |
| Queue&lt;T&gt; | FIFO (First-In, First-Out) queue. |
| .enqueue(item), .dequeue() -> T | Add to back, remove from front. |

## **18.5 std.sys**

|     |     |
| --- | --- |
| **Function / Type** | **Description** |
| thread.run(func, args...) -> thread | Launch function in a new OS thread. |
| thread.join() | Wait for thread to finish. |
| thread.sleep(int ms) | Suspend current thread for ms milliseconds. |
| mutex | Mutual exclusion lock. .lock() / .unlock() |
| atomic T | Thread-safe wrapper for any primitive type. |
| sys.time() -> long | Current Unix timestamp in milliseconds. |
| sys.env(string key) -> string | Get an environment variable. |
| sys.exit(int code) | Exit the process with the given code. |
| sys.platform -> string | Operating system name ("linux", "macos", "windows"). |
| sys.arch -> string | CPU architecture ("x86_64", "aarch64", "arm"). |
| sys.args -> List&lt;string&gt; | Command-line arguments. |

## **18.6 std.json**

Parse and generate JSON data.

|     |     |
| --- | --- |
| **Function** | **Description** |
| JSON.parse(string json) -> object | Parse a JSON string into a Flux object/list. |
| JSON.stringify(object, int indent) -> string | Convert a Flux value to a JSON string. Optional indent for pretty-printing. |

```flux
import std.json;

func main() {
    string data = "{\"name\":\"Flux\",\"version\":1}";
    object obj = JSON.parse(data);
    string pretty = JSON.stringify(obj, 2);
    print(pretty);
}
```

## **18.7 std.time**

Date, time, and timer utilities.

|     |     |
| --- | --- |
| **Function / Constructor** | **Description** |
| Time.now() -> float | Current Unix timestamp in seconds. |
| Time.nowMs() -> long | Current Unix timestamp in milliseconds. |
| Time.format(float ts, string fmt) -> string | Format a timestamp using strftime patterns (e.g. "%Y-%m-%d"). |
| Time.parse(string s, string fmt) -> float | Parse a date/time string into a timestamp. |
| Time.year(float ts) -> int | Extract the year from a timestamp. |
| Time.month(float ts) -> int | Extract the month (1-12). |
| Time.day(float ts) -> int | Extract the day of month (1-31). |
| Time.hour(float ts) -> int | Extract the hour (0-23). |
| Time.minute(float ts) -> int | Extract the minute (0-59). |
| Time.second(float ts) -> int | Extract the second (0-59). |
| Time.dayOfWeek(float ts) -> int | Day of week (0 = Sunday). |
| Time.elapsed(float start) -> float | Seconds elapsed since start timestamp. |
| Timer() | Constructor. Returns a timer object with .start(), .stop(), .elapsed() methods. |

## **18.8 std.crypto**

Cryptographic hashing and encoding. All implementations are pure Flux/C++ with no external dependencies.

|     |     |
| --- | --- |
| **Function** | **Description** |
| Crypto.sha256(string data) -> string | SHA-256 hash (64-character hex string). |
| Crypto.md5(string data) -> string | MD5 hash (32-character hex string). |
| Base64.encode(string data) -> string | Encode data to Base64. |
| Base64.decode(string b64) -> string | Decode a Base64 string. |

```flux
import std.crypto;

func main() {
    print(Crypto.sha256("hello"));
    print(Base64.encode("Hello, Flux!"));
}
```

## **18.9 std.os**

Operating system interaction and process management.

|     |     |
| --- | --- |
| **Function / Property** | **Description** |
| OS.exec(string command) -> string | Execute a shell command and return its stdout output. |
| OS.execStatus(string command) -> int | Execute a command and return its exit code. |
| OS.env(string key) -> string | Get an environment variable value. |
| OS.setEnv(string key, string val) | Set an environment variable. |
| OS.cwd() -> string | Get the current working directory. |
| OS.chdir(string path) | Change the current working directory. |
| OS.pid() -> int | Get the current process ID. |
| OS.hostname() -> string | Get the system hostname. |
| OS.username() -> string | Get the current username. |
| OS.tempDir() -> string | Get the system temporary directory path. |
| OS.platform -> string | Operating system name. |

## **18.10 std.regex**

Regular expression support using ECMAScript regex syntax.

|     |     |
| --- | --- |
| **Constructor / Method** | **Description** |
| Regex(string pattern) | Create a compiled regex from a pattern string. |
| .match(string s) -> bool | Test if the entire string matches the pattern. |
| .search(string s) -> string | Find the first match in the string. Returns empty string if none. |
| .findAll(string s) -> List&lt;string&gt; | Return all matches as a list of strings. |
| .replace(string s, string replacement) -> string | Replace all matches with the replacement string. |
| .split(string s) -> List&lt;string&gt; | Split the string at each match boundary. |
| .groups(string s) -> List&lt;string&gt; | Return capture groups from the first match. |

```flux
import std.regex;

func main() {
    object r = Regex("[0-9]+");
    print(r.match("12345"));        # true
    print(r.search("abc 42 def"));  # 42
    print(r.replace("a1b2c3", "X"));# aXbXcX
}
```

## **18.11 std.gpu**

GPU compute abstraction layer. Supports CUDA, ROCm, and CPU fallback backends. GPU support is compiled conditionally — if no GPU SDK is available, functions report CPU fallback.

|     |     |
| --- | --- |
| **Function** | **Description** |
| GPU.available() -> bool | Returns true if a GPU backend is available. |
| GPU.backend() -> string | Returns "CUDA", "ROCm", or "CPU Fallback". |
| GPU.deviceCount() -> int | Number of available GPU devices. |
| GPU.deviceName(int id) -> string | Name of GPU device at index. |
| GPU.allocate(int count) -> int | Allocate GPU memory for count floats. Returns a handle. |
| GPU.memcpyToDevice(int handle, List data) | Copy data from host to device. |
| GPU.memcpyToHost(int handle, int count) -> List | Copy data from device to host. |
| GPU.free(int handle) | Free GPU memory. |
| GPU.sync() | Synchronize all GPU streams. |

## **18.12 std.graphics**

Window creation, 2D drawing, text rendering, and image loading. Supports SDL2 (with SDL2_ttf for text, SDL2_image for images) and GLFW (for OpenGL 3D rendering) backends, compiled conditionally. When both SDL2 and GLFW are available, windows start in SDL2 mode for 2D and can switch to GLFW+OpenGL via `enable3D()`. If only one backend is available, it will be used exclusively. If neither is present, all window operations will report an error.

The `.backend` field reports the active configuration: `"sdl2+glfw"`, `"sdl2"`, `"glfw"`, or `"none"`.

### Window Lifecycle

|     |     |
| --- | --- |
| **Constructor / Function** | **Description** |
| Window(string title, int width, int height) | Create a new window. Returns a window object. |
| .isOpen() -> bool | Returns true if the window is still open. |
| .pollEvents() | Process pending window events. |
| .clear(int r, int g, int b) | Clear the window with an RGB color (0-255 each). |
| .present() | Swap buffers / display the current frame. |
| .close() | Close and destroy the window. |
| .setTitle(string title) | Change the window title. |
| .resize(int w, int h) | Resize the window. |
| .setBlendMode(string mode) | Set the blend mode ("none", "blend", "add", "mod"). |

### Basic Drawing Primitives

|     |     |
| --- | --- |
| **Method** | **Description** |
| .drawPixel(int x, int y, int r, int g, int b) | Draw a single pixel at (x,y) with RGB color. |
| .drawLine(int x1, int y1, int x2, int y2, int r, int g, int b) | Draw a line between two points. |
| .drawRect(int x, int y, int w, int h, int r, int g, int b) | Draw a rectangle outline. |
| .fillRect(int x, int y, int w, int h, int r, int g, int b) | Draw a filled rectangle. |
| .drawCircle(int cx, int cy, int radius, int r, int g, int b) | Draw a circle outline. |
| .fillCircle(int cx, int cy, int radius, int r, int g, int b) | Draw a filled circle. |
| .drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b) | Draw a triangle outline. |
| .fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a) | Draw a filled triangle. Alpha channel optional (default 255). |

### Extended Shapes

|     |     |
| --- | --- |
| **Method** | **Description** |
| .drawEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a) | Draw an ellipse outline. Alpha optional. |
| .fillEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a) | Draw a filled ellipse. Alpha optional. |
| .drawRoundedRect(int x, int y, int w, int h, int radius, int r, int g, int b, int a) | Draw a rounded rectangle outline. Alpha optional. |
| .fillRoundedRect(int x, int y, int w, int h, int radius, int r, int g, int b, int a) | Draw a filled rounded rectangle. Alpha optional. |

### Text Rendering (requires SDL2_ttf)

|     |     |
| --- | --- |
| **Method** | **Description** |
| .drawText(string text, int x, int y, string fontPath, int fontSize, int r, int g, int b, int a) | Render text at (x,y) using a TTF font file. Alpha optional. |
| .measureText(string text, string fontPath, int fontSize) -> list | Returns `[width, height]` of the rendered text without drawing it. |

### Image Loading (requires SDL2_image)

|     |     |
| --- | --- |
| **Method** | **Description** |
| .drawImage(string path, int x, int y) | Draw an image (PNG, JPG, etc.) at (x,y) at its native size. |
| .drawImageScaled(string path, int x, int y, int w, int h) | Draw an image scaled to the given width and height. |
| .getImageSize(string path) -> list | Returns `[width, height]` of the image without drawing it. |

### 3D Rendering (requires GLFW + OpenGL)

Calling `enable3D()` transitions the window from 2D (SDL2) to 3D (GLFW+OpenGL). Once in 3D mode, 2D drawing methods are unavailable. The transition destroys the SDL2 window and creates a GLFW window with an OpenGL context.

|     |     |
| --- | --- |
| **Method** | **Description** |
| .enable3D() | Switch window to 3D mode (GLFW + OpenGL). Enables depth testing and backface culling. |
| .disable3D() | Disable depth testing and culling (remains in GLFW mode). |
| .clearDepth() | Clear the depth buffer. Call after `clear()` each frame for correct depth ordering. |
| .setPerspective(float fov, float aspect, float near, float far) | Set a perspective projection matrix. Typical: `setPerspective(60.0, 800.0/600.0, 0.1, 100.0)`. |
| .setCamera(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) | Set the camera (view) matrix using eye position, look-at point, and up vector. |
| .pushMatrix() | Push the current model-view matrix onto the stack. |
| .popMatrix() | Pop the model-view matrix from the stack. |
| .translate(float x, float y, float z) | Translate (move) subsequent geometry. |
| .rotate(float angle, float x, float y, float z) | Rotate subsequent geometry by `angle` degrees around the axis (x, y, z). |
| .scale(float x, float y, float z) | Scale subsequent geometry. |
| .loadTexture(string path) -> int | Load an image file as an OpenGL texture. Returns a texture ID (0 on failure). |
| .bindTexture(int texId) | Bind a texture for subsequent draw calls. Pass 0 to unbind. |
| .drawTexturedCube(float size, int texId) | Draw a textured cube centered at the origin. |
| .setColor(float r, float g, float b, float a) | Set the current OpenGL color for untextured geometry. Values are 0.0-1.0. Alpha defaults to 1.0. |
| .drawQuad(x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4) | Draw a flat quad (four vertices). Useful for floors, walls, and procedural geometry. Uses the current color set by `setColor()`. |

### Window Input Methods

Window objects provide direct input polling methods in addition to the static `Input` namespace.

|     |     |
| --- | --- |
| **Method** | **Description** |
| .keyPressed(string key) -> bool | Returns true if the specified key is currently pressed. Works in both 2D and 3D mode. |
| .getMousePos() -> list | Returns `[x, y]` mouse position relative to the window. |
| .setMousePos(int x, int y) | Warp the mouse cursor to the given window coordinates. |
| .setCursorMode(string mode) | Set cursor mode: `"disabled"` hides and captures the cursor, `"normal"` restores it. |
| .mouseButtonPressed(int button) -> bool | Check mouse button state. 0 = left, 1 = middle, 2 = right. |

### Input

|     |     |
| --- | --- |
| **Function** | **Description** |
| Input.keyPressed(string key) -> bool | Returns true if the specified key is currently pressed. |
| Input.mouseX() -> int | Current mouse X position. |
| Input.mouseY() -> int | Current mouse Y position. |
| Input.mouseDown(int button) -> bool | Returns true if the specified mouse button is down. |

  

## **18.13 std.audio**

Audio playback and sound generation using SDL2_mixer. Supports loading audio files (WAV, OGG, MP3) and generating tones programmatically.

Requires SDL2_mixer to be installed on the system (`libsdl2-mixer-dev`). All functions are accessed through the `Audio` namespace object.

### Audio Lifecycle

|     |     |
| --- | --- |
| **Function** | **Description** |
| Audio.init() -> bool | Initialize the audio subsystem. Must be called before any other audio functions. Returns true on success. |
| Audio.quit() | Shut down the audio subsystem and free all loaded sounds and music. |

### Loading Audio

|     |     |
| --- | --- |
| **Function** | **Description** |
| Audio.loadSound(string path) -> int | Load a sound file (WAV, OGG). Returns a sound ID, or -1 on failure. |
| Audio.loadMusic(string path) -> int | Load a music file (WAV, OGG, MP3). Returns a music ID, or -1 on failure. |
| Audio.generateTone(int freq, int durationMs) -> int | Generate a sine wave tone at the given frequency and duration. Returns a sound ID. No external files needed. |

### Playback Control

|     |     |
| --- | --- |
| **Function** | **Description** |
| Audio.playSound(int id, int loops) -> int | Play a sound. `loops=0` plays once, `loops=1` plays twice, etc. Returns the channel number. |
| Audio.playMusic(int id, int loops) | Play music. `loops=-1` loops forever, `loops=0` plays once. |
| Audio.stopMusic() | Stop the currently playing music. |
| Audio.pauseMusic() | Pause the currently playing music. |
| Audio.resumeMusic() | Resume paused music. |
| Audio.isPlayingMusic() -> bool | Returns true if music is currently playing. |
| Audio.stopChannel(int channel) | Stop playback on a specific channel. |

### Volume Control

|     |     |
| --- | --- |
| **Function** | **Description** |
| Audio.setSoundVolume(int id, int vol) | Set volume for a loaded sound (0-128). |
| Audio.setMusicVolume(int vol) | Set the global music volume (0-128). |

### Example

```flux
import std.audio;

func main() {
    Audio.init();

    # Generate sound effects programmatically
    int beep = Audio.generateTone(440, 200);    # A4 note, 200ms
    int buzz = Audio.generateTone(220, 300);    # A3 note, 300ms

    # Play sounds
    Audio.playSound(beep, 0);                   # Play once

    # Load and play a music file
    int music = Audio.loadMusic("background.ogg");
    if (music >= 0) {
        Audio.setMusicVolume(64);               # Half volume
        Audio.playMusic(music, -1);             # Loop forever
    }

    Audio.quit();
}
```

  

## **18.14 std.video**

Video playback using FFmpeg for decoding and OpenGL for texture upload. Supports MP4, AVI, MKV, WebM, MOV, and any format that FFmpeg can decode. Video frames are decoded to RGB pixel data and can be uploaded as OpenGL textures for rendering on 3D geometry.

Requires FFmpeg development libraries (`libavcodec-dev`, `libavformat-dev`, `libswscale-dev`, `libswresample-dev`, `libavutil-dev`). Optional: SDL2_mixer for audio track playback, GLFW+OpenGL for texture upload.

### Video Constructor

|     |     |
| --- | --- |
| **Function** | **Description** |
| Video(string path) -> Video | Open a video file for playback. Returns a Video object with metadata and decode methods. |

### Video Metadata

|     |     |
| --- | --- |
| **Method** | **Description** |
| .isOpen() -> bool | Returns true if the video was opened successfully. |
| .width() -> int | Width of the video in pixels. |
| .height() -> int | Height of the video in pixels. |
| .fps() -> float | Frames per second of the video stream. |
| .duration() -> float | Duration of the video in seconds. |
| .isFinished() -> bool | Returns true if all frames have been decoded. |

### Frame Decoding

|     |     |
| --- | --- |
| **Method** | **Description** |
| .nextFrame() -> bool | Decode the next video frame. Returns true if a frame was decoded, false if the video has ended. |
| .getTextureId() -> int | Upload the current decoded frame to an OpenGL texture. Returns the GL texture ID. Subsequent calls update the same texture. |
| .seek(float seconds) | Seek to a specific time in the video. Resets the finished state. |
| .restart() | Seek back to the beginning of the video. |
| .close() | Close the video and free all FFmpeg resources and OpenGL textures. |

### Audio Track

|     |     |
| --- | --- |
| **Method** | **Description** |
| .playAudio() | Play the audio track extracted from the video. Requires buffered audio data (call nextFrame() first). |
| .stopAudio() | Stop audio track playback. |
| .setAudioVolume(int vol) | Set audio volume (0-128). |

### Example: Video Info

```flux
import std.video;

func main() {
    var vid = Video("clip.mp4");
    print("Size: " + toString(vid.width()) + "x" + toString(vid.height()));
    print("FPS: " + toString(vid.fps()));
    print("Duration: " + toString(vid.duration()) + "s");

    # Decode first 5 frames
    var count = 0;
    while (vid.nextFrame() && count < 5) {
        count = count + 1;
    }
    print("Decoded " + toString(count) + " frames");

    vid.close();
}
```

### Example: 3D Video Playback

```flux
import std.graphics;
import std.video;

func main() {
    var vid = Video("clip.mp4");
    var win = Window3D("Player", 800, 600);
    var input = Input();
    var interval = 1.0 / vid.fps();
    var lastTime = 0.0;

    while (win.isOpen()) {
        input.poll();
        if (input.isKeyDown("ESCAPE")) { break; }

        var now = win.getTime();
        if (now - lastTime >= interval) {
            if (!vid.nextFrame()) { break; }
            lastTime = now;
        }

        win.clear(0.0, 0.0, 0.0, 1.0);
        var tex = vid.getTextureId();
        if (tex > 0) {
            win.bindTexture(tex);
            win.drawQuad(-1.0, 1.0, 0.0, 1.0, 1.0, 0.0,
                          1.0, -1.0, 0.0, -1.0, -1.0, 0.0);
        }
        win.swap();
    }

    vid.close();
    win.close();
}
```

  

# **Appendix A: Operator Precedence Table**

Complete precedence reference. Higher level = evaluated first. Use parentheses to override.

|     |     |     |     |
| --- | --- | --- | --- |
| **Level** | **Operators** | **Description** | **Associativity** |
| 1   | . \[\] () | Member access, subscript, call | Left-to-right |
| 2   | ! - ++ -- .random (type) | Unary, cast | Right-to-left |
| 3   | \* / % | Multiplicative | Left-to-right |
| 4   | \+ - | Additive | Left-to-right |
| 5   | &lt; &gt; &lt;= &gt;= | Relational | Left-to-right |
| 6   | \=num= =word= == != | Equality and semantic | Left-to-right |
| 7   | && \| butnot | Logical | Left-to-right |
| 8   | \= += -= \*= /= new_type= | Assignment, re-type | Right-to-left |

# **Appendix B: Reserved Keywords**

The following 38 identifiers are reserved by the Flux language and cannot be used as variable, function, or class names.

|     |     |     |     |
| --- | --- | --- | --- |
|     |     |     |     |
| atomic | func | null | switch |
| bool | if  | panic | thread |
| break | implements | private | true |
| butnot | import | public | try |
| byte | int | return | unsafe |
| catch | interface | struct | void |
| char | long | string | while |
| class | mutex | super | new_type |
| cleanup | new | enum | do  |
| const | exec | export | extends |
| continue | else | elif | float |
| default | false | for | in  |
| asm | let | list | extern |

# **Appendix C: Primitive Type Reference**

Complete reference for all built-in primitive types.

|     |     |     |     |     |
| --- | --- | --- | --- | --- |
| **Type** | **Size** | **Default** | **Range** | **Example Literal** |
| void | 0   | null | null only | void x = null; |
| bool | 1 B | false | true / false | bool flag = true; |
| char | 1 B | \\0 | 0–127 (ASCII) | char c = 'A'; |
| byte | 1 B | 0   | 0 to 255 | byte b = 0xFF; |
| int | 4 B | 0   | \-2,147,483,648 to 2,147,483,647 | int n = 42; |
| long | 8 B | 0   | \-9.2e18 to 9.2e18 | long l = 9999999L; |
| float | 8 B | 0.0 | IEEE 754 double precision | float f = 3.14; |
| string | dynamic | ""  | UTF-8, any length | string s = "Hi"; |
| vec2 | 16 B | (0,0) | Two floats: x, y | vec2 v = vec2(1,2); |
| vec3 | 24 B | (0,0,0) | Three floats: x, y, z | vec3 v = vec3(1,2,3); |
| mat4 | 128 B | identity | 4x4 float matrix | mat4.identity() |
| color32 | 4 B | 0x000000FF | 32-bit RGBA value | color32 c = 0xFF0000FF; |

Flux Language Reference Manual v0.1

---

# **19\. StratOS System Services**

StratOS provides a set of system services implemented in Flux. These services form the runtime environment that the operating system exposes to the shell and user applications.

## **19.1 Virtual Filesystem (VFS)**

The VFS is a RAM-backed virtual filesystem. All files and directories reside in memory and do not persist across reboots. It uses a flat list of entries keyed by absolute path.

### Data Types

```flux
struct FSEntry {
    string path;
    int type;          // FS_FILE (0) or FS_DIRECTORY (1)
    string content;
    long size;
    int permissions;
    string owner;
}

struct FileInfo {
    string name;
    int type;
    long size;
    int permissions;
    string owner;
}
```

### API Reference

| Method | Signature | Description |
| --- | --- | --- |
| init | `VFS.init() -> void` | Seed default directories and config files |
| mkdir | `VFS.mkdir(string path) -> bool` | Create a single directory |
| mkdirp | `VFS.mkdirp(string path) -> bool` | Create directories recursively |
| touch | `VFS.touch(string path) -> bool` | Create an empty file |
| read | `VFS.read(string path) -> string` | Read file contents |
| write | `VFS.write(string path, string data) -> bool` | Write data to file (overwrite) |
| append | `VFS.append(string path, string data) -> bool` | Append data to file |
| exists | `VFS.exists(string path) -> bool` | Check if path exists |
| isDirectory | `VFS.isDirectory(string path) -> bool` | Check if path is a directory |
| isFile | `VFS.isFile(string path) -> bool` | Check if path is a file |
| stat | `VFS.stat(string path) -> FileInfo` | Get metadata for a path |
| listDir | `VFS.listDir(string path) -> list<FileInfo>` | List directory contents |
| remove | `VFS.remove(string path) -> bool` | Remove a single file or empty directory |
| removeRecursive | `VFS.removeRecursive(string path) -> bool` | Remove a path and all children |
| copy | `VFS.copy(string src, string dst) -> bool` | Copy a file |
| rename | `VFS.rename(string old, string new) -> bool` | Move or rename a path |
| normalizePath | `VFS.normalizePath(string path) -> string` | Clean up path separators |
| parentPath | `VFS.parentPath(string path) -> string` | Get the parent directory path |
| basename | `VFS.basename(string path) -> string` | Get the filename component |
| resolvePath | `VFS.resolvePath(string base, string rel) -> string` | Resolve a relative path against a base |
| getTotalUsage | `VFS.getTotalUsage() -> long` | Total bytes used by all files |
| getEntryCount | `VFS.getEntryCount() -> int` | Number of filesystem entries |

### Default Filesystem Layout

On boot, `VFS.init()` seeds the following structure:

```
/
├── bin/
├── boot/
├── dev/
├── etc/
│   ├── hostname          "stratos"
│   ├── os-release        OS metadata
│   └── motd              Message of the day
├── home/
│   └── void/
│       └── .bashrc       Shell configuration
├── proc/
├── system/
├── tmp/
├── usr/
│   ├── bin/
│   └── lib/
└── var/
    ├── log/
    │   └── boot.log      Boot log
    └── lib/
        └── quantum/      Package manager data
```

### Example

```flux
import "system/fs/vfs.lx";

VFS.init();
VFS.mkdirp("/home/void/documents");
VFS.write("/home/void/documents/hello.txt", "Hello from Flux!");
string content = VFS.read("/home/void/documents/hello.txt");
Console.log(content);  // "Hello from Flux!"

list<FileInfo> entries = VFS.listDir("/home/void/documents");
for (FileInfo fi in entries) {
    Console.log("${fi.name} (${fi.size} bytes)");
}
```

## **19.2 Quantum Package Manager**

Quantum is the StratOS package manager. It maintains a registry of available packages and tracks installed packages using the VFS.

### Shell Commands

| Command | Description |
| --- | --- |
| `quantum` | Show usage information |
| `quantum install <pkg>` | Install a package from the registry |
| `quantum remove <pkg>` | Remove an installed package (core packages are protected) |
| `quantum list` | List all installed packages with version and description |
| `quantum search <query>` | Search the registry for matching packages |
| `quantum info <pkg>` | Show detailed information about a package |
| `quantum update` | Synchronize the package registry |

### Package Registry

The registry is stored at `/var/lib/quantum/registry/index` as a pipe-delimited text file:

```
name|version|description|category
```

Categories: `core`, `system`, `app`, `lib`.

Installed package manifests are stored as individual files under `/var/lib/quantum/installed/<name>`:

```
version|description|category
```

### Default Registry

| Package | Version | Description | Category |
| --- | --- | --- | --- |
| flux | 0.1.0 | Flux language runtime | core |
| nova | 0.1.0 | Text editor | app |
| drift | 0.1.0 | File explorer | app |
| nebula | 0.1.0 | Web browser | app |
| pulsar | 0.1.0 | Music player | app |
| cosmos | 0.1.0 | Desktop environment | system |
| stellar | 0.1.0 | Window compositor | system |
| orbit | 0.1.0 | Task manager | app |
| photon | 0.1.0 | Image viewer | app |
| echo | 0.1.0 | Audio framework | lib |
| void-utils | 0.1.0 | Core system utilities | core |
| libcrypt | 0.1.0 | Cryptography library | lib |
| netstack | 0.1.0 | Network stack | lib |

### Example Session

```
void@stratos ~ $ quantum list
Installed packages:

  flux            0.1.0   Flux language runtime
  void-utils      0.1.0   Core system utilities

void@stratos ~ $ quantum install nova
:: Installing nova 0.1.0...
   Resolving dependencies...
   Downloading nova-0.1.0.qpkg...
   Verifying integrity...
   Installing to /usr/lib/nova/...
   nova 0.1.0 installed successfully.

void@stratos ~ $ quantum search editor
Search results for 'editor':

  nova            0.1.0   Text editor [installed]
```

## **19.3 Shell Built-in Commands**

The StratOS shell provides a Bash-like command-line interface. All commands operate on the VFS.

### Navigation

| Command | Description |
| --- | --- |
| `cd [dir]` | Change working directory. Supports `..`, `~`, `-`, absolute and relative paths. |
| `ls [-la] [dir]` | List directory contents. `-l` for long format, `-a` to include hidden files. Directories shown in color. |
| `pwd` | Print the current working directory. |

### File Operations

| Command | Description |
| --- | --- |
| `cat <file> [file2...]` | Display file contents. Supports multiple files. |
| `head [-nN] <file>` | Show the first N lines of a file (default 10). |
| `wc <file>` | Print line, word, and byte count for a file. |
| `touch <file>` | Create an empty file if it does not exist. |
| `write <file> <text...>` | Write text content to a file (create or overwrite). |
| `mkdir [-p] <dir>` | Create a directory. `-p` creates parent directories. |
| `rm [-rf] <path>` | Remove a file or directory. `-r` for recursive, `-f` to suppress errors. |
| `cp [-r] <src> <dst>` | Copy a file or directory. |
| `mv <src> <dst>` | Move or rename a file or directory. |
| `df` | Show filesystem usage summary. |

### System Commands

| Command | Description |
| --- | --- |
| `echo <text>` | Print text. Expands `$VAR` environment variables. |
| `clear` | Clear the screen. |
| `history` | Show command history. |
| `export KEY=VALUE` | Set an environment variable. |
| `env` | Show all environment variables. |
| `whoami` | Print the current username. |
| `hostname` | Print the system hostname. |
| `uname [-a]` | Print system name. `-a` for full info. |
| `ps` | List running processes. |
| `kill <pid>` | Terminate a process by PID. |
| `free` | Show memory usage (total, used, free, heap). |
| `uptime` | Show system uptime. |
| `date` | Show the current date and time. |
| `neofetch` | Display system info with ASCII art. |

### Tools

| Command | Description |
| --- | --- |
| `flux` | Enter the Flux REPL. |
| `quantum [cmd]` | Invoke the Quantum package manager. |
| `reboot` | Restart the system via keyboard controller reset. |
| `shutdown` | Power off via ACPI (QEMU port 0x604). |
| `help` | Show all available commands. |

## **19.4 Desktop Compositor**

StratOS boots directly into a graphical desktop managed by the Stellar Compositor. The compositor handles window management, input dispatch, and frame rendering.

### Architecture

The compositor runs a 60 FPS render loop in kernel context:

1. **Process Input** — poll keyboard and mouse events, dispatch to windows or handle shortcuts
2. **Update Windows** — remove closed windows, update states
3. **Render Frame** — compose the desktop background, windows, taskbar, and cursor onto a back buffer, then present to the hardware framebuffer

### Double Buffering

All rendering targets a back buffer allocated from the kernel heap. At the end of each frame, the back buffer is copied to the hardware framebuffer in a single pass, eliminating visual tearing and flicker.

The wallpaper is cached in a separate buffer. The expensive per-pixel wallpaper generation runs only once (or when the wallpaper changes). Each frame restores the cached wallpaper to the back buffer before drawing windows on top.

| Method | Description |
| --- | --- |
| `Framebuffer.enableDoubleBuffering()` | Allocate back buffer and wallpaper cache from kernel heap. |
| `Framebuffer.present()` | Copy back buffer to hardware framebuffer. |
| `Framebuffer.restoreWallpaper()` | Copy wallpaper cache to back buffer (fast frame start). |
| `Framebuffer.cacheWallpaper()` | Save current back buffer as wallpaper cache. |
| `Framebuffer.invalidateWallpaper()` | Mark wallpaper cache as stale (triggers re-render). |

### Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Alt + Left/Right` | Snap focused window to left/right half. |
| `Alt + Up` | Maximize focused window. |
| `Alt + Down` | Restore focused window. |
| `Alt + F4` | Close focused window. |
| `Alt + Tab` | Cycle focus between windows. |
| `Ctrl + Q` | Close focused window (alternative). |
| `Ctrl + W` | Cycle wallpaper. |
| `Ctrl + T` | Open a new terminal window. |
| `Escape` | Exit desktop and drop to shell. |

### Mouse Interaction

Click on a window title bar to drag it. Click on a window body to focus it. Drag a window to the screen edge and release to snap:

| Action | Description |
| --- | --- |
| Left edge drag | Snap to left half. |
| Right edge drag | Snap to right half. |
| Top edge drag | Maximize. |
| Double-click title bar | Toggle maximize/restore. |
| Right-click desktop | Open context menu (new terminal, wallpaper, settings). |
| Edge-drag borders | Resize windows from any edge or corner. |

Click the close button (red, right side of title bar), minimize button (yellow), or maximize button (green) to manage windows.

## **19.5 PS/2 Mouse Driver**

The mouse driver handles standard PS/2 mouse input via IRQ12, delivering 3-byte movement/button packets.

### Initialization

`Mouse.init(screenW, screenH)` enables the PS/2 auxiliary device on the 8042 controller, configures data reporting, and registers the IRQ12 interrupt handler (vector 44). The PIC slave mask is updated to unmask IRQ12.

### API

| Method | Description |
| --- | --- |
| `Mouse.getX() -> int` | Current absolute X position (clamped to screen). |
| `Mouse.getY() -> int` | Current absolute Y position (clamped to screen). |
| `Mouse.isLeftDown() -> bool` | Left button state. |
| `Mouse.isRightDown() -> bool` | Right button state. |
| `Mouse.isMiddleDown() -> bool` | Middle button state. |
| `Mouse.hasEvent() -> bool` | Check if mouse events are pending. |
| `Mouse.pollEvent() -> MouseEvent` | Dequeue next mouse event (non-blocking). |

### MouseEvent Struct

```flux
struct MouseEvent {
    int x;           # Absolute X position
    int y;           # Absolute Y position
    int deltaX;      # Relative X movement
    int deltaY;      # Relative Y movement
    bool leftBtn;    # Left button pressed
    bool rightBtn;   # Right button pressed
    bool middleBtn;  # Middle button pressed
}
```

### Packet Format

Each PS/2 mouse report consists of 3 bytes:

| Byte | Bits | Description |
| --- | --- | --- |
| 0 | 0 | Left button |
| 0 | 1 | Right button |
| 0 | 2 | Middle button |
| 0 | 3 | Always 1 (sync bit) |
| 0 | 4 | X sign |
| 0 | 5 | Y sign |
| 0 | 6-7 | X/Y overflow |
| 1 | 0-7 | X movement (unsigned, sign in byte 0) |
| 2 | 0-7 | Y movement (unsigned, sign in byte 0) |

The Y axis is inverted by the driver (PS/2 positive = up, screen positive = down).

## **19.6 Wallpapers & Icons**

Four procedural wallpapers are generated at runtime:

| Index | Name | Description |
| --- | --- | --- |
| 0 | Void | Deep-space gradient with scattered stars. |
| 1 | Nebula | Magenta-to-blue cosmic nebula with glowing center. |
| 2 | Aurora | Green-to-teal aurora borealis bands. |
| 3 | Mountain | Layered mountain silhouettes at sunset. |

Cycle wallpapers with `Ctrl+W` or via shell: `wallpaper next`, `wallpaper set <N>`.

Desktop icons are arranged in a grid and include: Terminal, Editor, Files, Browser, Calculator, Music, Notes, Settings.

## **19.7 TUI Installer**

The StratOS Text User Interface installer provides an 8-step guided setup:

1. **Welcome** — language selection
2. **Keyboard Layout** — US QWERTY, UK, DE, FR, ES
3. **Timezone** — Americas, Europe, Asia, Other
4. **User Account** — username and hostname
5. **Installation Profile** — Minimal, Standard, Full, Server, Developer
6. **Disk Setup** — partition information
7. **Confirmation** — review all selections
8. **Installation** — animated progress with component listing

Navigate with arrow keys and Enter. All rendering uses hardware framebuffer with the built-in system font.

---

## **19.8 Built-in Applications**

StratOS ships with a set of built-in applications accessible from the desktop icons, taskbar, or the app launcher (Super key).

### **Terminal**
A full terminal emulator window with:
- Text buffer with line history and scrolling (Page Up/Down)
- Command prompt showing `user@hostname dir $`
- Input line editing with cursor movement (arrows, Home, End)
- Shell command execution via `Console.beginCapture()` / `Shell.executeCommand()` / `Console.endCapture()`
- Ctrl+L (clear), Ctrl+C (cancel input)
- Tab completion for file and directory paths
- Command history (Up/Down arrow keys to recall previous commands)
- Opens by default on desktop startup

### **Editor**
A text editor with syntax highlighting and search:
- Line-based editing with configurable scroll and cursor
- Syntax highlighting for Flux source files: keywords, types, strings, numbers, comments, operators, function names
- Ctrl+F opens a find bar with wrapping search and match highlighting
- Ctrl+S saves the current file to VFS
- Can be launched from Explorer to open files directly
- New file/open file support via compositor integration

### **Explorer**
A dual-pane file browser for the VFS:
- Sidebar with bookmarks: Home, Documents, Desktop, Root
- Main file listing with name, size, and type columns
- Navigate directories with Enter or click
- Delete key removes selected file/directory (with recursive support)
- Ctrl+N creates a new folder via dialog overlay
- Enter on a file opens it in the Editor
- Status bar showing operation feedback

### **Browser**
An internal page browser with bookmarks:
- URL bar with address input and navigation
- Back/Forward history navigation
- Bookmark bar (Ctrl+B to toggle, Ctrl+D to add current page)
- Up to 8 bookmarks with 3 defaults (Home, Help, About)
- Multiple built-in pages with hyperlink navigation
- Page content rendering with styled headers and text

### **Calculator**
An iOS-style calculator with history:
- 5×4 button grid: AC, +/-, %, ÷, 7-9×, 4-6−, 1-3+, 0.=
- Right-aligned 2x pixel-doubled display text
- Arithmetic operations: +, -, *, /, % with chained evaluation
- Keyboard support: digits, operators, Enter (=), Backspace, Escape (clear)
- History panel (H key to toggle) showing last 6 calculations
- Custom `parseFloat()` and `formatFloat()` for freestanding math

### **Music**
A music player application with playlist and transport controls:
- Play/Pause, Previous, Next, Stop buttons
- Progress bar with seek support
- Volume slider control
- Playlist view with track listing and selection
- Track metadata display (title, artist, album)

### **Notes**
A sticky-notes application:
- Multiple color-coded notes (yellow, green, blue, pink, purple, orange)
- Add, delete, and edit notes
- Notes stored as VFS entries under `/home/void/notes/`
- Compact card view showing note previews
- Full edit mode for selected notes

### **Settings**
System preferences with tabbed sidebar:
- **Wallpaper** tab — browse and apply built-in wallpapers (Void, Nebula, Aurora, Mountain) with arrow keys and Enter
- **About** tab — system info (version, arch, memory, display, uptime)
- Tab switching via sidebar navigation or number keys 1/2

### **App Launcher**
Full-screen overlay triggered by the Super/Windows key or Start button:
- Grid of available apps with colored icons and labels
- Arrow key navigation with visual selection highlight
- Enter to launch; Escape or Super to close
- Mouse click support

---

## **19.9 Window Management**

The Stellar compositor provides desktop-class window management:

### **Mouse Interaction**
PS/2 mouse driver on IRQ12 with 3-byte packet assembly:
- Click desktop icons to launch apps
- Click window title bar to drag/move windows
- Double-click title bar to toggle maximize/restore
- Close (red), Maximize (green), Minimize (amber) buttons in title bar
- Click taskbar buttons to focus/restore windows
- Drag to screen edges for snap zones (left/right half, maximize)
- Edge-drag window borders to resize from any edge or corner
- Right-click desktop for context menu (new terminal, wallpaper, settings)
- Active/focused window highlighted on taskbar with accent bar

### **Keyboard Shortcuts**
| Shortcut | Action |
|----------|--------|
| Super | Toggle app launcher |
| Alt+Arrow | Snap window left/right/maximize/restore |
| Alt+F4 | Close focused window |
| Alt+Tab | Cycle window focus |
| Ctrl+Q | Close focused window |
| Ctrl+T | Open new terminal window |
| Ctrl+W | Cycle wallpaper |
| Escape | Exit desktop to shell |

### **Window Controls**
Each window has three control buttons (macOS-style circles):
- **Close** (red) — removes the window
- **Maximize** (green) — toggles fullscreen/restore with saved position
- **Minimize** (amber) — toggles visibility; focus moves to next window
- Multi-layer drop shadow (deeper for focused windows)
- Windows can be restored from the taskbar

### **Taskbar**
Bottom panel with:
- Start button (launches app launcher)
- Running app buttons with icon dots and text labels
- Active app indicator bar (accent-colored underline on focused window)
- System tray with volume/network icons and uptime clock (HH:MM)

### **Toast Notifications**
Transient messages slide in at the top-center of the screen:
- Auto-dismiss after a configurable duration
- Used for system feedback (app launched, file saved, etc.)

### **Onboarding**
First-boot welcome overlay introducing StratOS features and keyboard shortcuts.

_"From the Void, Structure."_