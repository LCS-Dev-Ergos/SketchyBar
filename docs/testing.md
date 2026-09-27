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
the same sources into `bin/`. Its default target runs cleanup before the
universal build, including when invoked with `make -j`.

The CMake binary is universal like the makefile's, but both architectures
target macOS 11; the makefile targets 10.13 for x86_64. The sanitizer and
fuzz presets target macOS 14, the target of the Nix clang runtimes.

## CTest and sanitizers

None of the tests opens a window or contacts the running bar. The message
tests reach WindowServer only through read-only CoreGraphics queries, and the
scripts the tests run write only to a temporary directory.

- `version` runs `sketchybar --version`, which exits before the client or
  daemon starts.
- `display_layout_tests` checks the display reconciliation decisions in
  `src/display_layout.h`, including the sequence recorded while two external
  displays woke from sleep.
- `display_reconcile_tests` runs `src/display_reconcile.c` on the main dispatch
  queue against counting stubs of the bar manager and simulated displays:
  coalesced bursts, the quiet period, the wake settle period, the lock screen,
  unlock and sleep. The build shortens the quiet period to 50 ms and the
  settle period to 2 s. The checks wait for each reconciliation instead of
  fixed times, so a slow runner passes them.
- `bar_level_tests` checks the bar background level against the item,
  desktop, normal window and menu bar levels for every `topmost` setting.
- `window_deferred_tests` replays a captured post-decode callback after an
  embedded window is cleared and freed, including storage reuse and a heap
  window removed while a callback is pending. ASan reports a use after free
  without the callback guard; WindowServer calls are stubbed.
- `mach_server_tests` sends client, native-provider, inline, unterminated and
  descriptorless messages to the receive callback of `src/mach.c` on a private
  port. The native-provider and empty SbarLua transaction cases check that
  one trailing NUL gains an empty token before parsing.
- `mach_send_tests` sends events to a full queue, which must time out and
  release the rights of the undelivered messages.
- `script_tests` runs scripts through `src/script.c`: the environment, spaced
  configuration paths and the alarm, shortened to one second.
- `message_*_tests` send commands to the daemon's message handlers through
  `tests/message/harness.h`, which links every source but `main` and creates
  no bar, so items never get windows. Without a WindowServer connection every
  SkyLight query fails, as it may during a wake. They cover property
  animations, out-of-range numbers and indices, WindowServer failures, item
  memory and removal during animations.
- `mask_bounds_tests` exercises display and space associations at indices 31
  and 32 through the bar and bar manager; UBSan catches signed and
  out-of-range shifts before the guarded-mask fix.
- `space_snapshot_tests` stubs the managed-space and display lists to check
  one copy per space-item update, two-display association, unchanged explicit
  associations, safe fallback when WindowServer has no answer, and reuse of
  the space snapshot across two bars. The earlier paths fail the call-count
  assertions.
- `space_notifications_tests` posts three `SPACE_CHANGED` events without a
  bar or WindowServer connection, waits for one update, then verifies that a
  later event and a forced update still run. It fails before the 16 ms burst
  coalescer.
- `sanitize` instruments sketchybar and the tests with ASan and UBSan.
- `thread-sanitize` uses TSan and UBSan instead. Run it separately from ASan.
  Both stop at the first undefined behaviour.

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
- `fuzz_message`: the checks of `src/mach_validate.h` on messages with any
  complex bit, descriptor count and type, followed by the tokenizer on every
  accepted descriptor.
- `fuzz_domains`: the daemon's `--add item`, `--set` and safe `--query`
  handlers through `tests/message/harness.c`. Each input creates and destroys
  an isolated bar manager without bar windows. Script execution and Mach
  bootstrap lookups are stubbed; the selected domain cannot become `--exit`.
  It fuzzes item names and selected property values with printable ASCII.

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
