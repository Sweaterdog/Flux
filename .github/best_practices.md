# Flux Development Best Practices & Pre-Completion Checklist

This document establishes binding rules for developing Flux. Every change MUST pass the checklist at the bottom before being considered "done."

---

## Code Organization Rules

1. **One concern per file.** Standard library modules are split into `std_<name>.cpp` / `std_<name>.h`. Do not merge modules.
2. **JIT and AOT must have feature parity.** Every function registered in the JIT interpreter (`standard/std_*.cpp`) MUST have a matching implementation in the AOT transpiler stubs (`src/transpiler.cpp`). No exceptions.
3. **Method names must match exactly between JIT and AOT.** If the JIT registers `httpDelete`, the AOT struct must expose `httpDelete`. If C++ keywords collide, rename consistently in BOTH layers — never rename in only one place.
4. **AOT stubs must generate valid C++17.** Do NOT use `auto` in function parameters (that requires C++20 `-fconcepts`). Use concrete types or templates instead.
5. **All non-void functions must have a return statement.** Functions defaulting to `int32_t` return type must end with `return 0;` to prevent undefined behavior.
6. **Standard library types must be passed by reference in AOT.** `Window`, `HttpClient`, `Socket`, `Timer`, `Map`, `Stack`, `Queue`, and user classes are passed as `T&` — never `auto` copies.
7. **Empty collections must respect their declared type.** `[]` assigned to `list<Post>` must emit `std::vector<Post>{}`, not `std::vector<int32_t>{}`.
8. **Comments in English, always.** Every function, struct, and non-obvious block must have a short comment explaining its purpose.
9. **No dead code.** If a feature is removed or replaced, delete the old code entirely. Do not comment it out.

---

## Type System Rules

1. **`object` must map to a usable type in AOT.** Use `std::map<std::string, std::string>` or a dedicated `FluxValue` variant — never `std::string`.
2. **`JSON.parse()` must return a structured type.** In AOT, parsed JSON must support property access (e.g., `data["key"]` or `data.key`). Returning raw string defeats the purpose.
3. **`new ClassName()` must work WITHOUT `.init()` on stdlib types.** The transpiler emits `_obj.init()` for all `new` expressions. Stdlib structs (HttpClient, Socket, etc.) that have no `init()` method must still work. Either add `void init() {}` to the struct or fix the transpiler to handle stdlib types differently.
4. **All class-typed fields must initialize with the correct type.** `list<Post> posts = []` must emit `std::vector<Post>{}`, not `std::vector<int32_t>{}`.

---

## Graphics Library Requirements

The graphics library (`std.graphics`) MUST support the following to be considered usable for application development:

### Drawing Primitives (existing)
- [x] `drawPixel`, `drawLine`, `drawRect`, `fillRect`
- [x] `drawCircle`, `fillCircle`, `drawTriangle`
- [x] `setBlendMode`, `clear`, `present`

### Text Rendering (required)
- [ ] `drawText(x, y, text, size, r, g, b)` — render text at position
- [ ] `measureText(text, size) -> int` — return pixel width of text string
- [ ] Use SDL_ttf with a bundled default font; allow `loadFont(path, size)`

### Image Support (required)
- [ ] `loadImage(path) -> Image` — load PNG/JPG from disk
- [ ] `drawImage(image, x, y)` — draw at position
- [ ] `drawImageScaled(image, x, y, w, h)` — draw scaled
- [ ] Use SDL2_image for format support

### Preset Shapes (required)
- [ ] `fillTriangle(x1, y1, x2, y2, x3, y3, r, g, b)` — filled triangle
- [ ] `drawRoundedRect(x, y, w, h, radius, r, g, b)` — rounded corners
- [ ] `fillRoundedRect(x, y, w, h, radius, r, g, b)`
- [ ] `drawEllipse(cx, cy, rx, ry, r, g, b)` — ellipse
- [ ] `fillEllipse(cx, cy, rx, ry, r, g, b)`

### UI Widgets (future — not blocking)
- [ ] Text input fields
- [ ] Buttons with click handlers
- [ ] Sliders, switches
- [ ] Scrollable containers

---

## Network Library Requirements

- [ ] `HttpClient` must work in BOTH JIT and AOT with identical method names
- [ ] HTTP methods: `get`, `post`, `put`, `httpDelete`, `setHeader`
- [ ] AOT HttpClient struct must have `void init() {}` or be constructible without `.init()`
- [ ] `download(url, filePath)` — download binary content to a file
- [ ] Response object must expose `statusCode`, `body`, `headers` in both modes

---

## Transpiler / Compiler Rules

1. **Never save generated `.cpp` files by default.** Only save when `--dev` flag is passed, or on compilation failure.
2. **Always use `-std=c++17`.** No C++20 features in generated code.
3. **Link flags must be added automatically** based on imports: `-lSDL2`, `-lSDL2_ttf`, `-lSDL2_image`, `-lpthread`, `-lstdc++fs`, `-lcurl`.
4. **Sanitize error output.** Replace temp file paths with the original `.flux` filename in error messages.
5. **Pointer dereference assignments must be emitted in `emitExpr`.** The `DEREF_ASSIGN` node type must be handled in BOTH `emitNode` (statement context) AND `emitExpr` (expression context, e.g. inside `EXPRESSION_STMT`). Missing the `emitExpr` case causes `*ptr = value` to silently emit a no-op.

---

## Struct-in-List Mutation Rules

When a struct is stored in a `FluxList` (vector), the transpiler generates `StructType var = list[i]` as a VALUE COPY — **not a reference**. Any mutations to `var` are silently lost and do not affect the original element.

**NEVER do this:**
```flux
AppWindow win = windows[i];
win.x = 100;         // LOST — mutating a copy!
win.isFocused = true; // LOST — mutating a copy!
```

**ALWAYS do this instead:**
```flux
windows[i].x = 100;         // Correct — mutates the actual element
windows[i].isFocused = true; // Correct — mutates the actual element
```

This applies to ALL struct types stored in lists. The pattern `let var = list[i]; var.field = ...` WILL compile without errors but the mutation is silently discarded. To read a struct from a list without mutation, using a copy is fine.

---

## String Method Requirements

The following string methods must exist and work in both JIT and AOT:

- [ ] `string.substring(start, length)` or `string.slice(start, end)`
- [ ] `string.indexOf(substr)` → int (-1 if not found)
- [ ] `string.split(delimiter)` → list of strings
- [ ] `string.trim()`, `string.toUpper()`, `string.toLower()`
- [ ] `string.replace(old, new)`, `string.contains(substr)`
- [ ] `string.startsWith(prefix)`, `string.endsWith(suffix)`
- [ ] `string.charAt(index)` → char

---

## StratOS Kernel Development Rules

1. **`func` is a reserved keyword.** Never use `func` as a variable or parameter name. The Flux parser will fail with "Expected function name" errors. Use `fn`, `fnIdx`, or similar alternatives.
2. **Hex literal maximum is `0x7FFFFFFFFFFFFFFF`.** The Flux parser uses `std::stoll` for hex literals, which overflows on values ≥ 0x8000000000000000. Use bitwise complement `~((long) mask)` or `~((long) 0x0F)` instead of raw hex bitmasks like `0xFFFFFFF0`.
3. **Sign-extension on int-to-long cast.** When casting negative `int` values to `long`, C++ sign-extends the result. For PCI BAR values or other unsigned 32-bit values, zero-extend manually: `long val64 = ((long) val) & mask32;` where `mask32 = (((long) 1) << 32) - 1;`.
4. **Struct literal initialization is not supported.** `MyStruct s = { field: value, ... };` does not parse. Declare the struct first, then assign fields: `MyStruct s; s.field = value;`.
5. **Class member variables cannot be accessed from static functions.** The transpiler emits class members as C++ instance members, but static functions have no `this` pointer. Move shared state to file scope (before the class definition).
6. **`init()` return type is forced to void.** The transpiler treats `init()` specially. If you need a bool-returning initialization function, name it something else (e.g., `mount()`, `setup()`).
7. **MMIO addresses must be identity-mapped before access.** Use `MemoryManager.mapPage(addr, addr, flags)` for each 4KB page in the MMIO region. The initial page tables only cover 0-4GB with 2MB pages.
8. **Always validate PCI device presence before driver init.** Use `PCI.findDevice(vendor, device)` and check for -1 before accessing device fields.

---

## Pre-Completion Checklist

**EVERY change must pass ALL of these before the response ends:**

### Testing
- [ ] `make release` compiles without errors
- [ ] `bash test_suite/run_all.sh` — ALL tests pass (0 failures)
- [ ] New features have test files in `test_suite/` with expected behavior
- [ ] AOT compilation of new features tested (compile and run the binary)
- [ ] JIT execution of new features tested (`flux run`)

### Documentation
- [ ] `Flux_Language_Manual.md` updated for all new/changed features
- [ ] New API methods documented with examples
- [ ] No "I added" or "new feature" language — plain, reference-style documentation

### Consistency
- [ ] JIT behavior matches AOT behavior for the same code
- [ ] Method names identical between JIT registration and AOT struct
- [ ] All types handled in `fluxTypeToC()` for new types
- [ ] Standard library types added to `refTypes` set for by-reference passing

### Regression
- [ ] Existing tests still pass (no regressions)
- [ ] Skinnertopia app's error categories addressed if relevant
- [ ] Empty list initialization respects declared element type
