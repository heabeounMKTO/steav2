# CLI, exit codes, and build options

## `steav`

```sh
./build/steav path/to/script.sts
./build/steav --version   # or -v
```

With no arguments, or more than one, it prints a usage line to stderr and
exits `64`.

### Exit codes

Classic BSD `sysexits.h` convention (`examples/steav.cpp`):

| code | meaning |
|---|---|
| `0` | ran to completion (or `--version`) |
| `64` | usage error — wrong number of arguments |
| `65` | compile error — scan/parse/type-check failure, including a missing `bongSlanhOun` opening statement |
| `70` | runtime error — e.g. an out-of-bounds array index |
| `74` | couldn't open the given file |

## `steav2-lsp`

A language server for `.sts` files:

- live errors from the real scanner, parser and type checker — the same
  checks `steav` runs, so the editor and the CLI never disagree
- hover docs for keywords (`somhab` → `for`), base types, modules and their
  members (`vec.dot`, `v.length`, `PI`)
- completion for keywords, types, imported names, names declared in the
  current file, and module members after `vec.` (follows `jea` aliases)

Split across three files: `lsp/steav2_lsp.cpp` is just the JSON-RPC/stdio
transport and `main()`; `lsp/lsp_core.cpp` (declared in `lsp/lsp_core.h`)
is the three actual features — `diagnostics()` / `hover()` / `completion()`
— as plain functions over a source string, with no LSP-protocol or stdio
code in them; `lsp/json.h` is a tiny dependency-free JSON reader/writer
both use. `lsp_core.cpp` is compiled into `steav-wasm` too (see below), so
the playground shows exactly what an editor running the real language
server would.

It also ships a Vim syntax file, an ftplugin so `gc` comments lines with
`//`, and a Lua snippet that registers the `.sts` filetype and starts the
server — see `editor/nvim/` and the top-level `README.md`'s "Editor
support" section for setup.

## Building

```sh
cmake -S . -B build
cmake --build build
```

The build type defaults to `Debug`; pass `-DCMAKE_BUILD_TYPE=Release` for
an optimized build (or run `./install.sh` / `make install`, which builds
Release and installs `steav` + `steav2-lsp` to `~/.local/bin`).

### CMake build options

| option | default | what |
|---|---|---|
| `BUILD_STEAV_EXAMPLES` | `ON` | build the `steav` CLI |
| `BUILD_STEAV_LSP` | `ON` | build `steav2-lsp` |
| `BUILD_STEAV_WASM` | `OFF` | build `steav-wasm.js`/`.wasm` (needs the Emscripten toolchain) |
| `BUILD_STEAV_TESTS` | `OFF` | register `tests/` with ctest |
| `STEAV_DEBUG_TRACE` | `OFF` | print disassembly per executed op (with the live stack) |
| `STEAV_DEBUG_PRINT` | `OFF` | disassemble whole chunks right after compiling |
| `STEAV_ASAN` | `OFF` | build with AddressSanitizer + UBSan |
| `STEAV_DEBUG_STRESS_GC` | `OFF` | collect garbage on every allocation |

Warnings (`-Wall -Wextra`) are on for everything.

### WASM

```sh
emcmake cmake -B build-wasm -DBUILD_STEAV_WASM=ON \
  -DBUILD_STEAV_EXAMPLES=OFF -DBUILD_STEAV_LSP=OFF
cmake --build build-wasm
```

Produces `steav-wasm.mjs` + `steav-wasm.wasm` (`examples/steav_wasm.cpp` +
`lsp/lsp_core.cpp`), an ES6 module (`EXPORT_ES6=1`, `MODULARIZE=1`,
`EXPORT_NAME=createSteav`) built for a Web Worker or Node
(`ENVIRONMENT=worker,node`), with no filesystem (`FILESYSTEM=0`) — no host
filesystem to point a script path at, so every entry point takes the
source directly as a JS string. `jongyeytha` output (and everything else
on stdout/stderr) goes through Emscripten's default console forwarding;
`Module.print`/`Module.printErr` can still override it as usual.

Five exported C functions, all `extern "C"`, all `.mjs`/`.wasm` — no
`ccall`/`cwrap` (not in `EXPORTED_RUNTIME_METHODS`, to keep the calling
convention explicit); marshal strings yourself with `stringToNewUTF8` /
`UTF8ToString`, remembering to `_free` what you allocated:

| function | returns | what |
|---|---|---|
| `steav_run(const char *source)` | `int` | runs the script; same exit-code family as `steav` (0/65/70). Flushes stdout/stderr before returning, and records the stats `steav_last_stats()` reports |
| `steav_check(const char *source)` | `const char *` | parse + type-check only (no run) — JSON array of `{"line","col","length","message"}`, `col` -1 when only the line is known; valid until the next call |
| `steav_hover(const char *source, int line, int character)` | `const char *` | `steav2-lsp`'s `hover()` (0-based position) as JSON — `null`, or `{"contents":{"kind":"markdown","value":"..."}}`; valid until the next call |
| `steav_last_stats()` | `const char *` | JSON about the last `steav_run`: `source_bytes`, `source_lines`, `tokens`, `frontend_ms`, `interpret_ms`, `functions`, `natives`, `code_bytes`, `constants`, `heap_bytes`, `gc_threshold`, `object_bytes`, and `objects` (counts by `string`/`function`/`class`/`instance`/`array`/`struct_type`) — from walking `heap.objects` right after `interpret()`, before the VM is freed |
| `steav_version()` | `const char *` | `"<version> (<commit>)"`, from `PROJECT_VERSION` / `STEAV_GIT_COMMIT` at configure time |

```js
import createSteav from "./steav-wasm.mjs";

const mod = await createSteav();

function run(source) {
  const p = mod.stringToNewUTF8(source);
  const code = mod._steav_run(p);
  mod._free(p);
  return code;
}
function check(source) {
  const p = mod.stringToNewUTF8(source);
  const json = mod.UTF8ToString(mod._steav_check(p)); // don't free: owned by steav_check
  mod._free(p);
  return JSON.parse(json);
}

run(`bongSlanhOun "hi"; jongyeytha "n = " + 5;`); // n = 5
check("akthe x = 1;"); // [{"line":1,"col":0,...,"message":"na `bongSlanhOun`?!!! ..."}]
```

A fresh `createSteav()` instance means a fresh process-wide `random` seed
(the shared RNG in `src/stdlib/random.cpp` is a static, reset by
re-instantiating rather than by any explicit call).

`BUILD_STEAV_WASM` requires configuring with `emcmake cmake` — CMake
errors out at configure time if it's turned on without the Emscripten
toolchain. The `steav` / `steav2-lsp` targets also cross-compile fine
under `emcmake` if left on, but they read a file path from `argv` and open
it from the real filesystem, which needs Emscripten's `NODERAWFS` or
`--preload-file` to work — `steav-wasm` is the one built for embedding.

## Tests

`.sts` files under `tests/` paired with a golden `.expected` stdout, or a
first line `// expect: <message>` for a script that must fail to compile
or run:

```sh
cmake -S . -B build -DBUILD_STEAV_TESTS=ON && cmake --build build
ctest --test-dir build          # or:
tests/run.sh build/steav
```

`tests/run.sh` is the faster loop while iterating — it runs every `.sts`
file directly against a given `steav` binary and diffs stdout, without
going through ctest.
