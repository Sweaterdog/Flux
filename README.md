# Flux

Flux is a systems-oriented programming language and runtime for building native applications and StratOS components. It includes an interpreter, parser, transpiler, standard library modules, documentation, and a test suite for the current stable package.

## What Is Included

- `src/` - core lexer, parser, interpreter, transpiler, AST, values, and runtime environment code.
- `standard/` - standard library modules for I/O, networking, collections, system utilities, JSON, time, crypto, OS helpers, regex, GPU, graphics, audio, and video.
- `test_suite/` - Flux language, standard library, graphics, audio, video, stress, and transpiler tests.
- `Flux_Language_Manual.md` - language reference manual.
- `Makefile` - build, run, REPL, test, and dependency-info targets.
- `install.sh` - local install helper.

StratOS is published separately at `https://github.com/Sweaterdog/StratOS`.

## Build

```sh
make release
```

The binary is written to:

```text
build/flux
```

For a debug build:

```sh
make debug
```

## Run

Run a Flux source file:

```sh
make run FILE=test_suite/basics/hello.flux
```

Start the REPL:

```sh
make repl
```

## Test

```sh
make test
```

Current validation from the first package publish:

- `45` tests passed.
- `3` tests failed.
- Known failures:
  - `test_suite/basics/operators.flux` exits `139`.
  - `test_suite/video/video_3d_playback.flux` exits `1`.
  - `test_suite/video/video_info_test.flux` exits `1`.

## Optional Dependencies

The Makefile detects optional system libraries through `pkg-config`:

- libcurl for HTTP support.
- SDL2, SDL2_ttf, SDL2_image, and SDL2_mixer for graphics, text, image, and audio support.
- GLFW plus OpenGL for 3D rendering.
- FFmpeg libraries for video playback.

Inspect detected support with:

```sh
make info
```

## Repository Notes

Generated outputs are intentionally ignored, including `build/` and generated test binaries. Keep source, tests, docs, and package scripts in git; rebuild artifacts locally as needed.
