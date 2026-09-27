# Plan

Phases that take the fork from upstream 2.24.0 to an implementation that is
safer, faster and closer to the yabai fork. Each phase states its goal, risks,
checks and exit criterion. Findings are referenced by their ids in the
[audit](audit.md).

## Status

| Phase | State |
|---|---|
| 0. Fork and synchronization | Done, except the Actions setting that lets the sync workflow open pull requests |
| 1. Dotfiles patches as commits | Done on `dev` |
| 2. CMake and presets | Done on `dev` |
| 3. VS Code workspace | Done on `dev` |
| 4. CI and releases | Not started |
| 5. Targeted hardening | Not started |
| 6. Live baseline | Not started; needs the user's agreement |
| 7. Restructuring | Not started |

Phase 2 was committed before phase 1 so that the patches' tests run under
CTest from their first commit.

## 0. Fork and synchronization

- Goal: `LCS-Dev-Ergos/SketchyBar` with `master` as a fast-forward-only mirror,
  `dev` as the default branch, upstream tags mirrored, and a weekly workflow
  that opens a pull request for each upstream release.
- Risks: scheduled workflows only run from the default branch; creating pull
  requests with `GITHUB_TOKEN` needs a repository setting.
- Checks: a manual run without a new release does nothing.
- Exit: the same structure as the yabai fork.

## 1. Dotfiles patches as commits

- Goal: `fix(display)` for the display reconciliation and `fix(bar)` for the
  background window level, each with its own tests, replacing the text
  extraction of the Python test with C tests.
- Risks: overlap with upstream #847, #851 and #855; the background shares the
  menu bar's level with `topmost=on`.
- Checks: unit tests under ASan, UBSan and TSan, a mutation check of the
  reconciliation test, the upstream makefile build, and the `window_order`
  tool on the running bar.
- Exit: the Dotfiles package no longer needs its patches (effective in
  phase 4).

## 2. CMake and presets

- Goal: debug, release, sanitize (ASan/UBSan), thread-sanitize and fuzz
  presets; a linked `compile_commands.json`; the makefile unchanged in role;
  universal x86_64 and arm64 binaries.
- Risks: flags diverging from the makefile's.
- Checks: same dylibs and architectures as the makefile binary; `ctest` green
  in every preset.
- Exit: all presets configure, build and pass.

## 3. VS Code workspace

- Goal: clangd as the only language server, CMake Tools, CodeLLDB with tasks
  that stop and restart the Home Manager agent.
- Risks: a debug launch replaces the running bar, so it runs only with the
  user.
- Exit: configure, build, test and attach work from the editor.

## 4. CI and releases

- Goal: build and test on `macos-26`; ASan/UBSan and TSan jobs; a static
  analyzer run with a baseline of the current warnings; libFuzzer on the
  framing and tokenizer, then on the `--set`, `--add` and `--query` domains
  without WindowServer; annotated `v2.24.0-lcs.N` tags signed in CI with the
  Nix hash in the release notes; `sketchybar-package.nix` and
  `scripts/update-sketchybar.sh` in Dotfiles.
- Risks: fuzzing the domains needs the parser separated from SkyLight calls.
- Checks: branch and release CI, archive signature and hash.
- Exit: a signed release that Dotfiles consumes after the user switches.

## 5. Targeted hardening

- Goal: S1, S3, S4 (`posix_spawn` with an isolated environment), R1 (send
  timeouts, with the matching SbarLua change), R3 and R5, each with a
  regression test that fails before and passes after the fix.
- Risks: changed semantics, such as messages dropped under load.
- Exit: fuzzers and sanitizers clean; analyzer below the baseline.

## 6. Live baseline

- Goal: SketchyBar and WindowServer CPU and latency per Desktop switch with
  yabai; wake with both displays; lock and unlock.
- Risks: changes the screen in use; agree each run with the user.
- Exit: measurements recorded in `docs/performance.md`.

## 7. Restructuring

Separate sprints, each verified by tests, fuzzers, TSan and comparison with
the phase 6 baseline:

- a typed and validated protocol with explicit framing;
- an event bus on one serial queue, without `dispatch_sync`, that coalesces
  refreshes and frames;
- a display and Desktop model: one snapshot per notification, deduplicated
  notifications, indices not limited to 32;
- rendering at each display's scale, without copying backing stores, on a
  current display link;
- authentication of message senders.
