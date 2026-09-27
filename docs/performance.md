# Reload investigation

Focused measurements on 2026-09-27, macOS 27.2, with the same immutable
Dotfiles configuration and 126 bar items. This is not the phase 6 Desktop,
wake, or steady-state CPU baseline.

The timer starts before `sketchybar --reload` and stops when `--query bar`
first reports all 126 items. The command itself replies in about 0.1 s, before
the configuration has finished loading.

| Daemon | Launch context | Reload to 126 items |
| --- | --- | ---: |
| Installed, signed `v2.24.0-lcs.1` | Home Manager LaunchAgent | 0.85–7.51 s |
| `master` at `5f358ec`, local makefile build | Temporary Home Manager-style LaunchAgent | 0.70–6.41 s |
| Current fork, local CMake Release build | Temporary Home Manager-style LaunchAgent | 1.00–1.24 s |
| The same signed binary | Direct child process | 1.05 s |
| `v2.24.0-lcs.1`, local makefile build | Direct child process | 1.05 s |
| Current fork, local CMake Release build | Direct child process | 0.84 s |
| `master` at `5f358ec`, local makefile build | Direct child process | 0.76 s |

The comparisons booted out the installed LaunchAgent, used the same immutable
config path, then restored the signed LaunchAgent in a `finally` block. The
temporary agents copied the Home Manager plist and startup script, replacing
only the executable path. The signed service was verified running afterward.
Direct runs differ in environment, parent process, and possible macOS privacy
attribution; their times are not an equivalent control for the LaunchAgent.

The installed agent's first three reloads took 5.6–7.3 s, but later reloads
of that same process alternated between roughly 1 and 7.5 s. With 8-second
idle gaps, `master` also alternated: 1.07, 6.41, then 0.70 s. Thus a slow
reload is reproducible on upstream `master` in the same launch context. The
two local fork-agent reloads were fast, but that small sample does not show a
fork improvement or rule out its own slow runs. The signed binary also had a
fast direct reload, so the source commit and CMake build alone do not explain
the observed variation.

A 12-second sample of the installed service during a 7.3-second reload put
488 of 1,014 main-thread samples under QuartzCore transaction commits from
`__CGSWindowReloadCABackingStoreLayer_block_invoke`. Another 37 samples
were in `bar_manager_destroy`, mostly removing windows through SkyLight;
one sample reached `vfork` while starting the config. The measured slow run
spent its time in the window and backing-store work of the full reload,
rather than the script launcher. Both master and the fork destroy and recreate
all bar windows on reload. The condition that makes a given WindowServer
transaction slow remains open; these observations do not establish a
fork-specific regression. Repeated interleaved runs, including WindowServer
diagnostics, would be needed before changing rendering code.

## Burst reloads and Lua helpers

The active signed `lcs.2` service showed a separate, reproducible problem.
Three `--reload` commands sent over 0.402 s each acknowledged in about
0.11 s, but the bar took 12.551 s to first report 126 items. The service
had three persistent SbarLua processes afterward, later four, duplicate
Brew providers and four new `Item not found` errors. A service restart
restored one Lua process, one each CPU, network and Brew provider, and
126 items. A six-reload sequential probe included fast and slow rebuilds,
but one query missed its all-items interval; it does not establish a
monotonic slowdown from sequential reloads.

The daemon starts the configuration asynchronously. Before SbarLua's batched
`mach_helper` setting reaches an item, the daemon has no event port to which
`bar_manager_destroy` can send its stop message. A second reload in that
window leaves the first Lua process running; late configuration batches and
provider startup commands then overlap. The Dotfiles Lua configuration
starts CPU, Brew and network providers with asynchronous `pkill; provider &`
commands during module loading. That startup pattern amplifies overlapping
configurations. Its ordinary space-change callback does not invoke yabai;
yabai commands occur in the click handlers. Contention with yabai is an
inference from the extra processes and WindowServer work, not a measured
causal timing result.

The local `dev` change coalesces reload requests for 200 ms, waits for the
new configuration to register a Mach port or for a short-lived config
process to exit, and starts at most one queued reload afterward. The isolated
test failed before the gate and passes after it. The same change sends one
stop per distinct event port and releases send-right references on replacement,
clone and destruction; a Mach-port test verifies the ownership and single
stop. The active signed release has not been replaced, so the live burst
improvement remains to be measured after the next deployment. The separate
0.85–7.51 s single-reload variation above remains open; its slow sample was
dominated by QuartzCore and also occurred on upstream master.

## Earlier native provider framing

Separately, the fork's new Mach validator rejected native provider messages
that ended with one NUL. CPU, network, and Brew processes were alive, but
their widgets stayed at their placeholder values. The receiver now appends
the missing empty token before parsing legacy messages; the Dotfiles
formatter also sends two NULs. A corrected one-shot CPU event changed the
live label from `cpu ??%` to `cpu 42%`.

## Space selection after the Dotfiles switch

After the user activated the corrected helpers, `v2.24.0-lcs.1` still showed
space selection several seconds late. In an isolated switch and return, yabai
reported the target in 0.34 and 0.28 s while the bar's highlight took 3.19
and 6.51 s. Another switch and return took 0.31 and 0.50 s in yabai, versus
3.41 and 5.59 s in the bar. Bar queries generally continued to reply within
0.1-0.2 s. A separate event item confirmed that `space_change` delivery was
also late. These timings measure query state, not the visible end of macOS's
Desktop animation.

The active release's validator requires two terminal NULs and silently drops
other frames. The pinned SbarLua creates a transaction for every callback,
including callbacks that make no changes. An empty transaction sends exactly
one NUL and waits up to 1 s for a reply that the daemon never sends. All 16
personal space items subscribe to `space_change`; only the items changing
selection need an update, so a switch creates many empty transactions. In a
15-second sample covering a switch and return, SbarLua's main thread spent
10,393 samples waiting in a callback's Mach send/response path and another
2,649 in a delayed callback's path. SketchyBar's main thread was mostly idle.

The current `dev` receiver accepts a one-NUL frame and appends the missing
empty token before parsing. Run as a temporary LaunchAgent with the same
immutable personal config, the local `dev` binary updated the highlight in
0.54 and 0.23 s during a switch and return. That A/B used spaces 1 and 2,
while the signed-release samples used spaces 5-7, so the absolute times are
not a fixed-workload benchmark. The framing defect, the sampled wait, and the
large improvement with the corrected receiver identify the installed release
as the cause of the multi-second bar lag; the 16 callbacks amplify it. The
signed release was restored after the comparison. `v2.24.0-lcs.2` now contains
the receiver fix; its tagged CI run built, tested, and signed the archive.
Dotfiles commit `2634b85` pins that archive, and the complete Darwin build
passed. The installed service remains on `lcs.1` until the user switches.

The subsequent `dev` fuzz run found that its test target passed an accepted
single-NUL descriptor directly to `get_token`, whereas the real receiver
appends the missing NUL first. Commit `5190788` mirrors that normalization in
the target and adds the empty frame to its corpus. The local fuzz run and all
four jobs in the branch CI passed; no release binary change was needed.

## After activating lcs.2

On 2026-09-27 the user reported that the visible space-selection delay was
resolved after switching to `lcs.2`. A read-only check found the signed
`lcs.2` Nix binary in the running Home Manager LaunchAgent, with bar and
display queries responding. The `Item not found` entries in the service log
precede the current process; its subsequent entries report display refreshes
without new item errors. That initial check did not establish a controlled
post-switch baseline; the focused checks follow.

## Phase 6 live checks on lcs.2

The Home Manager LaunchAgent kept the signed `lcs.2` process running through
these checks. Bar and display queries answered with 126 items and two displays.

- Steady use, 16:12:44-16:13:53 CEST: 60 one-second `top` samples after its
  initial sample, with space 5 focused at both ends and no new service-log
  entries. SketchyBar CPU averaged 0.59% (median 0.55%, maximum 1.9%);
  WindowServer averaged 37.08% (median 36.9%, maximum 44.6%). This host had
  other active load, so WindowServer CPU cannot be attributed to the bar.
- Three 5-to-6-to-5 round trips on one display, starting the timer before
  each `yabai space --focus` command: yabai reported focus in 0.115-0.301 s
  (median 0.196 s), and `sketchybar --query item` reported the target's icon
  highlighted in 0.211-0.551 s (median 0.316 s). The six bar-minus-yabai
  gaps were 0.056-0.250 s. Queries ran sequentially after the focus command,
  so these are polling bounds, not visual animation-completion times. The
  script restored space 5; the service log stayed at 379 lines.
- User-performed lock and unlock: the same daemon PID remained, queries found
  126 items and two displays, and the only new service-log entry was
  `displays unchanged, refreshed (2 active) in 443 ms`.
- User-performed sleep request and wake with two external displays: the two
  display UUIDs and frames matched before and after, the daemon PID remained,
  and the bar still had 126 items. The service logged two unchanged-layout
  refreshes, in 241 and 428 ms, with no rebuild. `pmset -g log` recorded
  `Display is turned off` at 16:19:27 and on at 16:19:58, but no full system
  `Sleep`/`Wake` transition. Its sleep-notification record included a 30 s
  `AntelopeAudioSer` timeout. This verifies display recovery, not a full
  system-sleep recovery; the timeout's effect on sleep is unresolved.
- After the user clicked an application and an empty bar area,
  `window_order` found 58 visible item windows and zero covered by the bar
  background (exit 0).

## After activating lcs.3

On 2026-09-27 the user switched the full Darwin generation, including
`v2.24.0-lcs.3` and yabai `lcs.18`. The signed SketchyBar LaunchAgent kept
the same PID through the checks below, reported 126 items and returned to the
initial Desktop 11 after each controlled run. One SbarLua configuration process
and one each CPU, Brew and network provider remained after the probes. The
duplicate-item errors in the persistent service log predate this LaunchAgent.

- Two bursts of three `--reload` requests each returned to 126 items with no
  new missing-item errors and no persistent duplicate helpers. The first
  burst first reported all items after 6.644 s and was stable after 7.660 s;
  the second was stable after 2.326 s. This verifies the overlap fix but also
  reproduces the intermittent slow rebuild on `lcs.3`. A process trace in the
  second burst showed one Lua child of SketchyBar. Other Lua PIDs lasted less
  than 0.1 s and were children of that Lua process, not independent
  configuration instances.
- Six controlled 5-to-6-to-5 Desktop transitions, with the same polling
  method as the `lcs.2` baseline, gave median yabai focus 0.195 s and median
  bar highlight 0.315 s. The bar-minus-yabai gaps were 0.010-0.182 s.
  The corresponding `lcs.2` medians were 0.196 s and 0.316 s, so this
  fixed-workload check does not show a navigation regression. A separate
  cross-display entry into space 5 was slower and is not part of that
  comparison.
- From 20:36:22 to 20:37:31 CEST, 60 non-initial `top` samples found
  SketchyBar CPU mean 0.58%, median 0.6%, maximum 1.3%. WindowServer mean
  was 46.08%, median 46.0%, maximum 51.7%. The earlier `lcs.2` SketchyBar
  mean was 0.59%. WindowServer's higher figure includes unrelated host load
  and cannot be assigned to SketchyBar.

An eight-second sample spanning five Desktop changes showed
`update_all_spaces` repeatedly walking the 11 Desktop spaces. Its call tree
included `SLSRequestNotificationsForWindows` after each space scan, including
many synchronous SkyLight replies. The next development change keeps the
per-space event registration for individual window changes but registers once
after a full scan. A focused 11-space regression fails on the previous code
and passes with this change. Release, ASan/UBSan and TSan/UBSan suites each
pass 19/19. This change is not in the active release; a live latency gain has
not been measured.
The slow individual reload still needs a separate WindowServer investigation.
