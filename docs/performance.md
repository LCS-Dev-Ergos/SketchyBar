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
