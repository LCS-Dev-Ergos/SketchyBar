# Reload investigation

Focused measurements on 2026-09-27, macOS 27.2, with the same immutable
Dotfiles configuration and 126 bar items. This is not the phase 6 Desktop,
wake, or steady-state CPU baseline.

The timer starts before `sketchybar --reload` and stops when `--query bar`
first reports all 126 items. The command itself replies in about 0.1 s, before
the configuration has finished loading.

| Daemon | Launch context | Reload to 126 items |
| --- | --- | ---: |
| Installed, signed `v2.24.0-lcs.1` | Home Manager LaunchAgent | 5.6–7.3 s |
| The same signed binary | Direct child process | 1.05 s |
| `v2.24.0-lcs.1`, local makefile build | Direct child process | 1.05 s |
| Current fork, local CMake Release build | Direct child process | 0.84 s |
| `master` at `5f358ec`, local makefile build | Direct child process | 0.76 s |

The direct runs booted out the LaunchAgent, used its config path, then
restored the signed LaunchAgent in a `finally` block. The signed service was
verified running afterward. Direct runs differ in environment, parent
process, and possible macOS privacy attribution; their times are not an
equivalent control for the LaunchAgent. In particular, the same signed binary
was fast when run directly, so the source commit and CMake build alone do not
explain the observed delay.

A 12-second sample of the installed service during a 7.3-second reload put
488 of 1,014 main-thread samples under QuartzCore transaction commits from
`__CGSWindowReloadCABackingStoreLayer_block_invoke`. Another 37 samples
were in `bar_manager_destroy`, mostly removing windows through SkyLight;
one sample reached `vfork` while starting the config. The measured delay is
therefore in the window and backing-store work of the full reload, rather
than the script launcher. The exact reason this work costs more under the
LaunchAgent remains open; a controlled comparison with equivalent macOS
privacy grants is needed before attributing the difference to the fork.

Separately, the fork's new Mach validator rejected native provider messages
that ended with one NUL. CPU, network, and Brew processes were alive, but
their widgets stayed at their placeholder values. The receiver now appends
the missing empty token before parsing legacy messages; the Dotfiles
formatter also sends two NULs. A corrected one-shot CPU event changed the
live label from `cpu ??%` to `cpu 42%`.
