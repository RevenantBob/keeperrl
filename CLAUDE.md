# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

KeeperRL — a dungeon-management roguelike game written in C++14. Source at
http://keeperrl.com / https://store.steampowered.com/app/329970 / https://miki151.itch.io/keeperrl.
The repo is a flat directory of ~250 `.cpp`/~370 `.h` files at the root (no `src/` subdirectory), plus:

- `extern/` — vendored third-party code (cereal serialization, ProgramOptions, cxxopts, variant/optional
  polyfills, lodepng).
- `data_free/`, `data_contrib/` — runtime game content and assets (see "Content is data" below).
- `server/` — PHP scripts for the online highscore/mod server (separate from the game client).
- `cmake/`, `mac/` — platform build helpers.

## Building

There is no single canonical build; pick based on platform. All build configs compile every `*.cpp` in the
root plus `extern/*.cpp` (no explicit source list to maintain — dropping a new `.cpp` file in the root is
enough).

**Linux/macOS (GNU Make, the primary build):**
```
make -j 8 OPT=true RELEASE=true   # release build; add OSX=true on macOS
make -j 8                          # debug build (default), uses clang++ and a precompiled stdafx.h header
./keeper
```
Key Make flags: `RELEASE=true` (optimized, no debug asserts, defines `RELEASE`), `OPT=true` (`-O3`),
`DEBUG=true` (debug symbols), `NO_STEAMWORKS=true` (build without Steam integration — Steamworks SDK is
normally expected under `extern/steamworks/`), `SANITIZE=true` (ASan), `GCC=g++` (override compiler, default
`clang++`).

**Windows (legacy):** `Makefile-win` (mingw/clang toolchain) — see
http://keeperrl.com/compiling-keeperrl-on-windows/. Superseded on this checkout by the CMake path below;
only relevant if actually working with mingw.

**Windows (current, CMake + Visual Studio 2026 + ClangCL):** this checkout has a working native build —
no MinGW involved. `CMakeLists.txt` on `WIN32`:
- vendors SDL3 from source via `add_subdirectory` — expects the SDL3 source tree at
  `SDL-release-3.4.4/` in the repo root (not committed; drop it in yourself). SDL3's own Vulkan render
  backend (`src/render/vulkan`) builds in and is what the game actually renders through (see Rendering
  under Architecture) — requested explicitly via `SDL_CreateRenderer(window, "vulkan")`, with an
  automatic fallback to SDL's default driver if that fails (no Vulkan runtime present, etc.).
- pulls OpenAL/libvorbis/libogg/libtheora/cURL/zlib from vcpkg via `$ENV{VCPKG_ROOT}`'s toolchain file
  (classic mode, `x64-windows` triplet) — set `VCPKG_ROOT` before configuring.
- excludes `steam_*.cpp` except `steam_input.cpp`/`steam_achievements.cpp`, which compile without the
  Steamworks SDK (their SDK-dependent code is behind `#ifdef USE_STEAMWORKS`, with no-op fallbacks
  otherwise) — the SDK itself isn't vendored in this checkout.
- defines `WINDOWS` (the project's own portability macro, same one `Makefile-win` set) and undefines
  `NDEBUG`, since `fx_base.h`'s `PASSERT`/`ASSERT` macros key off `!defined(NDEBUG)` and some call sites
  chain `<<` onto them assuming they always expand to something.

Configure/build:
```
cmake -S . -B build-vs -G "Visual Studio 18 2026" -A x64 -T ClangCL
cmake --build build-vs --target keeper --config RelWithDebInfo -- /m
```
CMake `GLOB`-based source discovery (see above) doesn't notice added/removed `.cpp` files without a fresh
configure — re-run the `cmake -S ... -B build-vs ...` line (not just `cmake --build`) after adding or
deleting a root-level source file.

The exe lands at `build-vs/<Config>/keeper.exe` (e.g. `build-vs/RelWithDebInfo/keeper.exe`). It needs
`SDL3.dll` and the vcpkg DLLs (`OpenAL32.dll`, `libcurl.dll`, `zlib1.dll`, `vorbis*.dll`, `ogg.dll`,
`theoradec.dll`) sitting next to it — DLLs are always resolved relative to the exe's own location,
regardless of working directory. **There is no CMake step that copies these** — on this checkout they were
copied in once by hand into `build-vs/RelWithDebInfo/` when the build was first set up (from
`build-vs/SDL-release-3.4.4/RelWithDebInfo/` and `$VCPKG_ROOT/installed/x64-windows/bin/`), and they simply
survive incremental rebuilds since `cmake --build` only rebuilds changed targets rather than cleaning the
output directory. After a from-scratch `build-vs` (deleted directory, different `Config`, etc.) they need
copying in again by hand the same way.

Game *data* (`data_free/`, `data_contrib/`, saves, mod files, `appconfig.txt`) is resolved relative to the
process's **working directory**, not the exe's location — separately from the DLL lookup above. Running
`keeper.exe` from the repo root uses this checkout's own `data_free/`. `run-against-steam.ps1` (repo root,
untracked/local — not part of the committed build) instead launches the checkout-built exe with its working
directory pointed at a local Steam install of the game, so it runs against the full shipped asset set rather
than whatever subset is checked into this repo; useful when tracking down a rendering/asset bug, since dev
data can be thinner than the real game's. It also auto-copies `appconfig-dev.txt` into the Steam dir the
first time it's needed — non-`RELEASE` builds (`RelWithDebInfo`/`Debug`) read that filename instead of the
shipped `appconfig.txt` (see `main.cpp`'s `#ifdef RELEASE`), and a bare Steam install won't have it.

**CMake on Linux** is a separate, mostly-untouched path; it hardcodes some author-specific paths
(`/home/michal/...`) so expect to need overrides. Not affected by the WIN32-only changes above.

`make clean` removes `obj*/`, the binary, and generated files (`version.h` is regenerated by
`gen_version.sh` at build time from git describe output).

## Tests

There is no separate test framework/binary target that's normally used — tests live in `test.cpp` inside a
`class Test` guarded by `#ifndef RELEASE`, and `testAll()` iterates over its methods. `test.h` declares
`testAll()`, which is called from `main.cpp` when the game binary itself is invoked with the `--run_tests`
CLI flag:
```
make            # debug (non-RELEASE) build so Test:: methods are compiled in
./keeper --run_tests
```
`CHECK(...)` (from `debug.h`) is the assertion macro used throughout tests and runtime invariant checks.
Test cases are added as new methods on `Test` in `test.cpp` and called from `testAll()` at the bottom of
that file — there's no auto-discovery.

`check_serial.sh` (invoked via `make check_serial`, and wired into the CMake build) is a static consistency
check, not a unit test: it verifies every field declared with `SERIAL(...)`/`HASH(...)` is actually listed
in the corresponding `SERIALIZE_ALL`/`HASH_ALL` (or `ar(...)`) call, catching the classic "added a field,
forgot to serialize/hash it" bug via grep/diff rather than compilation.

## Architecture

**Data flow / object ownership.** `Game` (`game.h`) owns the overall game session (campaign, players,
turns); it holds one or more `Model`s (`model.h`), each representing a single dungeon/overworld level graph
built via `ModelBuilder`/`LevelBuilder`. A `Model` owns `Level`s, which hold the `Square`/`Creature`/`Item`
grid content for a map. `Collective` (`collective.h`) tracks a controlled group of creatures (the player's
dungeon population or an enemy faction) — most "dungeon management" gameplay logic (build orders, workshops,
immigration, warnings) hangs off `Collective` and its `CollectiveConfig`/`CollectiveControl`.

**View layer is abstracted from game logic.** `View` (`view.h`) is the interface the game engine talks to;
`WindowView` (`window_view.h`) is the real renderer. `DummyView` (`dummy_view.h`) is a
headless/no-op implementation used for tests and batch/automated runs (e.g. `MainLoop::modelGenTest`,
`battleTest`, `launchQuickGame` in `main_loop.h` — these run full games without a display, useful for
balance/regression testing of generated content). `Renderer` (`renderer.h`) and the `fx_*` files handle
low-level drawing and particle effects; `layout_renderer.h` handles UI layout.

**Rendering (Windows build): SDL3's `SDL_Renderer` 2D API, Vulkan backend.** `renderer.cpp` draws
exclusively through `SDL_RenderGeometry`/`SDL_RenderTexture`/etc — no raw OpenGL calls anywhere in this
checkout (the old `opengl.cpp`/`framebuffer.cpp` fixed-function/FBO path was fully removed). A few things
that are easy to get wrong if you touch this code:
  - `SDL_Renderer` has no depth buffer, so there's no way to make something "always draw on top" by giving
    it a different z like the old GL code did. `Renderer::setTopLayer()`/`popLayer()` (used for tooltips
    and similar always-on-top UI) instead defer every draw call issued while a top layer is active into a
    command queue (`Renderer::runOrDefer`), replayed once at the very end of the frame in
    `drawAndClearBuffer()` — after everything else. If you add a new kind of draw call to `Renderer`, route
    it through `runOrDefer` too or it'll ignore top-layer nesting.
  - `SDL_RenderGeometry`/`SDL_RenderGeometryRaw` with `texture == nullptr` (solid-color fills — see
    `Renderer`'s `fillQuad` helper) blend using the *renderer's own* draw blend mode
    (`SDL_SetRenderDrawBlendMode`), not per-vertex alpha or anything texture-related. It's set to
    `SDL_BLENDMODE_BLEND` once, in the `Renderer` constructor — don't assume alpha "just works" for
    untextured geometry the way it does for sprites.
  - `SDL_CreateTextureFromSurface` copies the *source `SDL_Surface`'s own* blend mode onto the resulting
    texture (in `SDL_UpdateTextureFromSurface`), silently overriding its own format-based auto-detection.
    `TileSet::loadTilesFromDir` builds its sprite atlas by `SDL_BlitSurface`-ing individual tile PNGs into
    one big surface with `SDL_SetSurfaceBlendMode(..., SDL_BLENDMODE_NONE)` (deliberately, so tiles get
    copied verbatim rather than alpha-composited into each other during that CPU-side step) — `Texture`'s
    loading path explicitly force-sets `SDL_BLENDMODE_BLEND` on the GPU texture afterward to counteract
    this. Any new code path that creates a `Texture` from a hand-built `SDL_Surface` needs the same
    explicit override, or sprites drawn from it will silently render fully opaque.
  - Textures default to `SDL_TEXTURE_ADDRESS_CLAMP` (`Texture::Wrapping::clamp`), not `WRAP` — `WRAP`
    addressing is only well-defined for power-of-two-sized textures on some backends
    (`SDL_PROP_RENDERER_TEXTURE_WRAPPING_BOOLEAN`), and nothing in this codebase actually needs true
    tiling for sprite/tile-atlas sub-rect sampling.

**`MainLoop`** (`main_loop.h`/`.cpp`) is the top-level orchestrator invoked from `main.cpp`: menu flow, save/
load, campaign setup, mod downloading, and the various headless test/battle-simulation entry points all live
here rather than in `Game` itself.

**Content is data, not code.** Game content — creatures, items, spells, buildings, technology, biomes,
enemies, immigration rules, world maps, etc. — is defined in plain-text config files under
`data_free/game_config/*.txt`, not hardcoded in C++. `ContentFactory` (`content_factory.h`) and
`GameConfig` load and validate this data at startup (`KeyVerifier` cross-checks referenced IDs exist).
This is also the basis of the mod system: mods are alternate/additional sets of these config files, managed
via `MainLoop`'s `getGameConfig`/`getVanillaConfig`/download/upload mod methods. When a task involves game
balance, new monsters/items/spells, or campaign content, look in `data_free/game_config/` before adding C++
code.

**Serialization.** Save games use `cereal` (vendored in `extern/cereal/`) via macros in `serialization.h`.
Fields intended to persist are wrapped with `SERIAL(x)`, then every such field must be listed in a
`SERIALIZE_ALL(...)`/`SERIALIZE_ALL_NO_VERSION(...)` (or a manual `ar(...)` call) in that class — enforced by
`check_serial.sh`, not the compiler, so a mismatch only shows up if that script is run. The same
declare/list-together pattern applies to `HASH(...)`/`HASH_ALL(...)` (used for detecting content/save
compatibility drift).

**Precompiled header.** `stdafx.h` is precompiled (`stdafx.h.gch`) and included first in most translation
units in debug builds; if you add a new commonly-used standard/extern header, consider whether it belongs
there rather than in individual files.

**Style.** Formatting follows `.clang-format` (LLVM base, 2-space indent, 120 col, left-aligned pointers, no
comment reflow) — run `clang-format` on touched files rather than hand-matching style. `Wno-*` flags in the
Makefiles indicate the codebase intentionally tolerates sign-compare/unused-variable warnings; don't treat
those as bugs to fix opportunistically.

## Known issues (Windows/CMake/SDL3 build)

- **Crash handler assumes gdb/Linux.** `main.cpp`'s `keeperMain` install a `FatalLog` output that on a
  `FATAL`/`CHECK` failure shells out to `rungdb.bat` to capture a backtrace. That script doesn't exist on
  Windows, so a failed `CHECK` crashes a second time (access violation) instead of printing a clean
  diagnostic — the original assertion message still prints first, so it's not silent, just noisy afterward.
  (Harmless in the test suite: `test.cpp` has an intentional `FATAL` case to verify error-message formatting,
  which triggers this same fallback.)

`./keeper --run_tests` (run from the repo root, so `data_free/` resolves) passes cleanly.
