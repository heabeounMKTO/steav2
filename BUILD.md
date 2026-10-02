# Building steav2

steav2 has **no external dependencies** — no vcpkg, no Conan, nothing to
fetch. The whole thing (scanner, parser, type checker, compiler, VM, and
even `steav2-lsp`'s JSON reader/writer) is plain C++11 against the
standard library, built with CMake. All you need is CMake 3.12+ and a
C++11 compiler.

For what each CMake option does, the WASM embedding API, exit codes, and
running the test suite, see [`docs/cli.md`](docs/cli.md) — this page is
just "how do I get a working build on my platform."

## Quick start (any platform)

```sh
cmake -S . -B build
cmake --build build
./build/steav path/to/script.sts   # build/Debug/steav.exe on a multi-config Windows generator
```

The build type defaults to `Debug` for single-config generators (Makefiles,
Ninja); pass `-DCMAKE_BUILD_TYPE=Release` for an optimized build, or
`--config Release` at build time for a multi-config generator (Visual
Studio, Xcode).

## macOS

Needs the Xcode Command Line Tools (for `clang`/`clang++`) and CMake:

```sh
xcode-select --install   # if you haven't already
brew install cmake
```

Then the quick start above works as-is (AppleClang is what `STEAV_ASAN`
and the warnings flags were written against). Or use the one-shot script,
which builds Release and installs `steav` + `steav2-lsp` to
`~/.local/bin`, plus the Neovim syntax/LSP files:

```sh
./install.sh                     # --prefix DIR, --vim-dir DIR, --no-editor
```

**Known caveat:** `STEAV_ASAN` hangs at startup in some macOS dev setups
(see `TODO.md`) — UBSan + `STEAV_DEBUG_STRESS_GC` work fine there instead.

## Linux

Any recent GCC or Clang plus CMake:

```sh
# Debian/Ubuntu
sudo apt install build-essential cmake
# Fedora
sudo dnf install gcc-c++ cmake make
# Arch
sudo pacman -S base-devel cmake
```

Then the quick start above, or `./install.sh` (same script as macOS, pure
bash). `-lm` is linked automatically on non-Windows (see
`CMakeLists.txt`'s `if(NOT WIN32)` guard) — nothing extra to install for
`math.h`.

## Windows

No POSIX-only code paths — the only platform-conditional bits in
`CMakeLists.txt` are the warnings flags (`/W4` under MSVC, `-Wall -Wextra`
otherwise) and skipping the explicit `-lm` link (Windows' CRT already has
the math functions). Two ways to build:

### MSVC (Visual Studio)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\steav.exe path\to\script.sts
```

(Or open the generated `.sln` in `build/` directly.) Swap the generator
string for whatever Visual Studio version you have; `cmake --help` lists
the generators CMake detected.

### MinGW-w64 (MSYS2)

```sh
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/steav.exe path/to/script.sts
```

`install.sh` is bash and won't run under `cmd`/PowerShell directly — run
it from Git Bash, MSYS2, or WSL, or just do the generic
`cmake --install build --prefix <dir>` (below) and copy the Neovim files
from `editor/nvim/` yourself.

## WASM (Emscripten)

```sh
emcmake cmake -B build-wasm -DBUILD_STEAV_WASM=ON \
  -DBUILD_STEAV_EXAMPLES=OFF -DBUILD_STEAV_LSP=OFF
cmake --build build-wasm
```

Produces `steav-wasm.mjs` + `steav-wasm.wasm`, an ES6 module exporting
five C functions for embedding in a browser/Node — see
[`docs/cli.md`](docs/cli.md#wasm) for the full list, the calling
convention (`stringToNewUTF8`/`UTF8ToString`, no `ccall`/`cwrap`), and a
usage snippet. `BUILD_STEAV_WASM` refuses to configure without the
Emscripten toolchain (`emcmake`), so there's no silent half-built state.

## Build options

The full table (debug tracing, GC stress-testing, ASan/UBSan, the WASM
build) is in [`docs/cli.md`](docs/cli.md#cmake-build-options) — same
options on every platform, passed the same way:

```sh
cmake -S . -B build -DBUILD_STEAV_TESTS=ON -DSTEAV_DEBUG_TRACE=ON
```

## Installing anywhere

Beyond `install.sh` (macOS/Linux only), the plain CMake install target
works on any platform once you've built:

```sh
cmake --install build --prefix <dir>
```

Installs the `steav2` static library + headers, and `steav` /
`steav2-lsp` if those were built (`BUILD_STEAV_EXAMPLES` /
`BUILD_STEAV_LSP`, both default `ON`).

## Running the tests

```sh
cmake -S . -B build -DBUILD_STEAV_TESTS=ON
cmake --build build
ctest --test-dir build
```

`tests/run.sh build/steav` is a faster loop while iterating (diffs every
`.sts` file's stdout against its `.expected` directly, skipping ctest) —
bash, so it needs Git Bash/MSYS2/WSL on Windows, same as `install.sh`.
