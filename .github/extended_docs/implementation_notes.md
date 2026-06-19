# Flux Implementation Notes

## Current Implementation Status

This document tracks the current state of the Flux interpreter implementation.

### Fully Implemented Features

- **Lexer**: Tokenizes all Flux syntax including `=num=`, `=word=`, `butnot`, hex/binary/scientific literals, string escape sequences, character literals, `#` / `//` / `/* */` comments
- **Parser**: Recursive descent parser with full expression precedence chain, type redefinition detection, lambda detection, for-each loops, generic type parsing
- **Interpreter**: Tree-walking interpreter with:
  - Mutable static typing with type redefinition (`x = string = "hello"`)
  - String interpolation (`$var` and `${expr}`)
  - All arithmetic, comparison, logical, `butnot`, `=num=`, `=word=` operators
  - Control flow: `if`/`elif`/`else`, `for`, `for-each`, `while`, `do-while`, `switch`, `break`, `continue`
  - Functions with return types, default parameters, named arguments, recursion
  - Lambdas: `(params) => expr`
  - Classes with constructors, public/private fields, methods, inheritance (`extends`), `super.init()`
  - Structs (value types)
  - Enums with auto and explicit integer values
  - Lists with `add`, `removeAt`, `contains`, `clear`, `sort`, `length`
  - Index access and assignment on lists
  - Type casting: `(type) value`
  - Error handling: `try`/`catch`/`finally`, `throw`, `panic`
  - Math builtins: `math.sqrt`, `math.pow`, `math.abs`, `math.floor`, `math.ceil`, `math.round`, `math.sin`, `math.cos`, `math.tan`, `math.min`, `math.max`, `math.clamp`, `math.lerp`, `math.PI`, `math.E`, `math.TAU`, `math.INF`
  - Random: `int.random`, `float.random`, `bool.random`
  - Builtin functions: `print`, `print_raw`, `input`, `typeof`, `len`, `toString`
  - `unsafe` blocks
  - REPL mode

### Not Yet Implemented

- Concurrency (threads, mutex, atomic)
- Networking & I/O (std.io, std.net, fs)
- Graphics (StratOS framebuffer, entity system)
- C++ Bridge (import .cpp)
- Generics (function/class templates)
- Interfaces
- Map, Stack, Queue collections
- Safe casting (`(type?) value`)
- Module/import system (partially stubbed)
- AOT/JIT compilation (currently interpreter-only)

### Known Limitations

- String interpolation `${expr}` cannot contain string literals with quotes (workaround: extract to a variable first)
- Type redefinition with identifier types (`x = MyClass;`) requires the full form (`x = MyClass = value`)
- The `this` keyword inside methods works implicitly through field binding, not as an explicit variable
