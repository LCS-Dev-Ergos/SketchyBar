# Testing

Run from the repository root on macOS with CMake 3.25 or newer, Ninja and a
clang toolchain. Builds stay under `build/<preset>`.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
```

`release` uses the same tests with the makefile's optimised flags
(`-O3 -ffast-math`, assertions kept). Configure with
`-DSKETCHYBAR_LINK_COMPILE_COMMANDS=OFF` to leave the editor's root
`compile_commands.json` symlink alone. The upstream `makefile` still builds
the same sources into `bin/`.

The CMake binary is universal like the makefile's, but both architectures
target macOS 11; the makefile targets 10.13 for x86_64. The sanitizer and
fuzz presets target macOS 14, the target of the Nix clang runtimes.

## CTest and sanitizers

None of the tests opens a window, talks to WindowServer or contacts the
running bar.

- `version` runs `sketchybar --version`, which exits before the client or
  daemon starts.
- `display_layout_tests` checks the display reconciliation decisions in
  `src/display_layout.h`, including the sequence recorded while two external
  displays woke from sleep.
- `display_reconcile_tests` runs `src/display_reconcile.c` on the main dispatch
  queue against counting stubs of the bar manager and simulated displays:
  coalesced bursts, the quiet period, the wake settle period, the lock screen,
  unlock and sleep. The build shortens the quiet period to 50 ms and the
  settle period to 1 s. The checks wait for each reconciliation instead of
  fixed times, so a slow runner passes them.
- `bar_level_tests` checks the bar background level against the item,
  desktop, normal window and menu bar levels for every `topmost` setting.
- `sanitize` instruments sketchybar and the tests with ASan and UBSan.
- `thread-sanitize` uses TSan and UBSan instead. Run it separately from ASan.

```sh
cmake --preset sanitize
cmake --build --preset sanitize --parallel
ctest --preset sanitize
```

Passing these tests does not verify live display, wake or window ordering
behaviour.

## Static analyzer

`tools/analyze.sh` runs the clang static analyzer over every translation unit
and fails when a finding is new or more frequent than in
`tools/analyzer-baseline.txt`, which records the findings of upstream 2.24.0
by file, message and checker. After fixing a finding, lower the baseline with
`tools/analyze.sh --update-baseline`. The findings depend on the analyzer:
the baseline uses Apple clang, as CI does. Newer LLVM releases find more, such
as the uninitialised `memcpy` sources in the [audit](audit.md):
`CLANG=/path/to/clang tools/analyze.sh` reports them as new findings, and
the baseline stays with Apple clang.

## Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request to `dev`: the
universal release build with CTest and the upstream makefile build, the
`sanitize` and `thread-sanitize` presets, 60 seconds of each fuzz target with
Homebrew LLVM, and the analyzer. Fuzz failure inputs are uploaded as an
artifact. Tags build, sign and publish the release described in
[fork.md](fork.md).

## Fuzzing

The `fuzz` preset needs a clang that ships libFuzzer, such as the Nix clang
toolchain or Homebrew LLVM; Apple clang does not. Select both C and
Objective-C compilers on the first configure when the default compiler lacks
it:

```sh
CC=/path/to/clang OBJC=/path/to/clang cmake --preset fuzz -DSKETCHYBAR_FUZZ_SECONDS=60
cmake --build --preset fuzz --parallel
ctest --preset fuzz
```

CMake caches the compiler: use a fresh build directory when switching
toolchains. The targets use libFuzzer, ASan and UBSan:

- `fuzz_tokens`: the tokenizer every command goes through
  (`src/misc/helpers.h`), on inputs framed like client messages: tokens,
  key-value pairs as `--set` packs them, comma separated lists, numbers and
  boolean states.

Each target runs for `SKETCHYBAR_FUZZ_SECONDS` (default 30) with a 10-second
limit per input. Seeds live in `tests/fuzz/corpus/`; generated inputs go to
`build/fuzz/fuzz/corpus/` and failure inputs to `build/fuzz/fuzz/`. Replay one
with its executable, for example
`build/fuzz/fuzz/fuzz_tokens build/fuzz/fuzz/crash-<hash>`.

## Live checks

`tools/window_order` (built with `SKETCHYBAR_BUILD_TOOLS`, the default) reads
the on-screen window list and exits with 1 when an item window of the running
bar sits behind the bar background, 0 when none does and 2 when the check is
inconclusive. Run it after clicking an application and then an empty area of
the bar.

Launching a local build replaces the running bar; the VS Code launch
configurations stop and restart the Home Manager agent around it. Agree live
measurements and service restarts with the user first.
