# Plan

Phases that take the fork from upstream 2.24.0 to an implementation that is
safer, faster and closer to the yabai fork. Each phase states its goal, risks,
checks and exit criterion. Findings are referenced by their ids in the
[audit](audit.md).

## Status

| Phase | State |
| --- | --- |
| 0. Fork and synchronization | Done |
| 1. Dotfiles patches as commits | Done on `dev` |
| 2. CMake and presets | Done on `dev` |
| 3. VS Code workspace | Done on `dev` |
| 4. CI and releases | Done: signed `v2.24.0-lcs.2` is active through Dotfiles |
| 5. Targeted hardening | Done, except the items listed in its section |
| 6. Live baseline | In progress: Desktop, CPU, unlock and display wake checked; full system sleep unverified |
| 7. Restructuring | Not started |
| 8. Repository maintenance | Planned after safety, robustness and performance work |

Phase 2 was committed before phase 1 so that the patches' tests run under
CTest from their first commit.

Priority: finish the phase 6 measurements, close the remaining safety and
robustness findings from phase 5, then make measured phase 7 performance and
protocol changes. Keep repository layout, formatting and presentation work in
separate changes after those behavior changes are stable.

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
- Risks: overlap with upstream #847, #851 and #855. With `topmost=on` the
  background first shared the menu bar's level; the items now sit one level
  above the status level instead.
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
- Done: S1 (`fix(mach)`, with `fuzz_message`), S3 (`fix(message)`), S4
  (`fix(script)`), R1 on the daemon side (`fix(mach)`: event and reply sends
  time out after 100 ms), R3 (`fix(display)`) and the verified R5 findings
  (`fix(items)`; `fix(animation)` for removal during animations, which the
  message harness confirmed). The analyzer baseline fell from 52 to 19
  findings.
- Deviations: scripts still start with `vfork`, because the child must set
  the alarm that SbarLua and long-running helpers cancel with `alarm(0)`,
  which `posix_spawn` cannot do; the child now only sets the alarm and execs.
  SbarLua is unchanged: the daemon's send timeout already breaks the
  deadlock of #794.
- Open: windows embedded in bars and popups may still be freed before a
  deferred frame update (R5, plausible, needs WindowServer to confirm); space
  indices of 32 and more are still shifted in `bar.c` and `bar_manager.c`
  (phase 7); S2 is the scripting model (phase 7); fuzzing the message domains
  through the harness needs `fork_exec` and bootstrap lookups stubbed first,
  so that fuzzed commands cannot run scripts or reach the running bar.

## 6. Live baseline

- Goal: SketchyBar and WindowServer CPU and latency per Desktop switch with
  yabai; wake with both displays; lock and unlock.
- Risks: changes the screen in use; agree each run with the user.
- Progress: the user confirmed that `lcs.2` removed the visible space lag.
  Six controlled Desktop switches, a one-minute CPU sample, lock/unlock,
  display power recovery and window ordering are recorded. The sleep request
  produced no system `Sleep`/`Wake` event, so full sleep recovery remains open.
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
- authentication of message senders;
- investigate the intermittent full reload cost with WindowServer evidence
  before changing the window creation or rendering path.

## 8. Repository maintenance

Do these as separate, behavior-preserving changes after the higher-priority
security, robustness and performance work:

- Assess a thematic `src/` layout for entry points and IPC, bar and items,
  display and workspace, rendering, integrations, and shared helpers. The 81
  source files are mostly flat today. Confirm boundaries from the include and
  call graph before moving modules; phase 7 may change those boundaries.
  Move one coherent group at a time, updating CMake, the upstream makefile,
  tests, tooling and analyzer path baselines. Run both builds and the relevant
  tests after each move, with no behavior edits in the same commit.
- Establish one formatting policy for C and Objective-C after sampling the
  existing conventions. Apply it in isolated format-only commits, avoiding
  broad churn across files still changing for fixes.
- Delegate concise module headers to the external agent after paths settle:
  follow the `List.hpp` file-header style with `@file`, a module-specific
  `@brief` and only essential ownership or invariant notes.
- Review sample assets and documents before removal. Candidates are the
  repository example `sketchybarrc`, `images/default.png`,
  `images/example.png`, and any unreferenced images or sample plugins. Keep
  the runtime `sketchybarrc` filename and its tests. Rewrite the fork README
  around the maintained release and link a `CHANGELOG.md` that records the
  fixes in each fork release; retain upstream attribution and documentation
  links. Check references and build/release packaging after cleanup.
