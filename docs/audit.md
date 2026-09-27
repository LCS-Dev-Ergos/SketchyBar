# Audit of SketchyBar 2.24.0

Phase one of the [plan](plan.md), carried out on 2026-09-27 on upstream
`5f358ec` (v2.24.0 and four later commits). Line references are to that
commit; fork commits have since moved some of them.

Method: a full read of `src/` (about 12,500 lines), the upstream makefile
build, the clang static analyzer, read-only review of open and recently
closed upstream issues and pull requests, and the two patches Dotfiles
applies to 2.24.0. No live measurement was taken: the only access to the
running bar was a passive process listing.

Legend: **[V]** verified from code together with the analyzer or a local
check; **[P]** plausible from code, not confirmed at runtime.

## Security

- **S1 [V] The Mach server trusts the received message.**
  `mach_message_callback` copies a fixed-size structure without checking the
  message size, the complex bit, the descriptor count or type
  (`src/mach.c:127-133`). `handle_message_mach` then parses
  `descriptor.address` (`src/message.c:596-598`) with a tokenizer that relies
  on a double NUL terminator nobody checks (`src/misc/helpers.h:260-276`). A
  malformed message can crash the bar or corrupt its memory. Fix: validate the
  header, the descriptor and the termination before parsing; fuzz the framing.
- **S2 [V] No sender check.** Any process in the user's session that can look
  up `git.felix.<name>` can make the bar run scripts, reload an arbitrary file
  as its configuration (`src/message.c:734-750`) or exit, with SketchyBar's
  TCC grants such as Screen Recording. This is the scripting model by design,
  as with yabai's socket. Fix direction: check the sender's audit token
  (user id, optionally a code signing requirement).
- **S3 [V] Unbounded numeric and size inputs.**
  - Token conversions allocate a stack array as long as the token on the main
    thread (`src/misc/helpers.h:239-258`).
  - Shifts by user values are undefined for indices of 32 and more or a
    display of 0 (`src/bar_item.c:1151`, `src/bar_item.c:1173`,
    `src/message.c:427`, `src/bar_item.c:100`).
  - `--reorder` with repeated names writes past a stack array
    (`src/message.c:573-590`).
  - A graph of width 0 divides by zero on `--push` (`src/graph.c:16-33`).
  - More than 64 custom events shift past 64 bits
    (`src/custom_events.c:61-66`).
- **S4 [V] Child processes.** `fork_exec` calls `vfork` and then `setenv`,
  `alarm` and `exit` in the child (`src/misc/helpers.h:441-461`), which POSIX
  does not allow; the analyzer reports it. `setenv` changes the parent's
  environment, so variables passed with `--trigger` persist into every later
  script, and `exit` flushes the parent's stdio buffers. The configuration
  path reaches `sh -c` as a command string, so a path with spaces fails
  (`src/hotload.c:58-69`). Scripts inherit SIGCHLD set to SIG_IGN
  (`src/sketchybar.c:139`); a local check confirmed the inheritance but also
  that `waitpid` still works on macOS 27.2, so it does not explain upstream
  #807.
- **S5 [V] Predictable lock file in /tmp** without `O_NOFOLLOW` or an owner
  check (`src/sketchybar.c:110-127`): another local user can prevent the bar
  from starting. Low severity.
- **S6 [V] Minor.** `event_port` looks up any bootstrap name and never
  releases the send right (`src/bar_item.c:378-381`); JSON in query output and
  `INFO` does not escape names (`src/app_windows.c:162`); every error is
  printed to stdout, so the service log grows without bound. Running as root
  is refused (`src/sketchybar.c:205`). The send-right lifetime is fixed on
  `dev`, including clones; the other findings remain.
- **S7 [P] Ad-hoc signature.** Each Nix rebuild changes the code directory
  hash, which can void the Screen Recording grant. A stable signature from CI,
  as for yabai, avoids that.

## Robustness

- **R1 [V] IPC deadlock.** Sends have no timeout, both in SketchyBar
  (`src/mach.c:96-102`) and in SbarLua, so a full queue on each side blocks
  both processes (upstream #794). The reply to a client has no timeout either
  (`src/message.c:771-774`), while the client waits only 100 ms for it
  (`src/mach.c:30-37`) and returns an empty answer without an error.
- **R2 [P] Synchronous dispatch to the main thread.** `event_post` uses
  `dispatch_sync` from other threads (`src/event.c:386-392`), for example the
  CoreAudio listener (`src/volume.c:84-88`); if the main thread waits on those
  subsystems, both block.
- **R3 [V] Unchecked SkyLight and CoreGraphics results**, which can crash
  during wake and reconfiguration: `SLSCopyManagedDisplaySpaces`
  (`src/misc/helpers.h:464-518`), `SLSCopyManagedDisplays`
  (`src/display.c:182-213`), `SLSHWCaptureSpace` (`src/misc/helpers.h:520-532`),
  `IOPSGetProvidingPowerSourceType` (`src/power.c:7-10`), an uninitialised
  window id when creation fails (`src/window.c:62-79`) and an uninitialised
  display count (`src/display.c:270-274`). Upstream #691 reports a crash on
  wake.
- **R4 [V] Sleep, wake and displays.** Every display callback rebuilds every
  bar and item window synchronously inside the CoreGraphics callback
  (`src/display.c:24-44`, `src/bar_manager.c:754-781`). A wake rebuilds twice,
  the second time 500 ms later (`src/bar_manager.c:1024-1043`), and a screen
  unlock is handled as a wake (`src/workspace.m:145-148`). Before macOS 26 the
  window updates go through `SLSDisableUpdate` (`src/window.c:138-156`),
  consistent with the "enable_updates_common timed out" logs in #776, a hang
  on Mac Studio and Mac mini machines with external displays. Fixed in the
  fork by `fix(display)`.
- **R5 Memory safety.**
  - [V] `--move` of an item relative to itself, or to an item that is not
    listed, copies an uninitialised pointer into the item list
    (`src/bar_manager.c:120-146`, analyzer at line 141).
  - [V] An image path starting with `~` is read after `resolve_path` freed it
    (`src/image.c:44`, `src/image.c:70`, analyzer).
  - [V] `--clone` shares the graph samples and the alias strings of its
    parent, so removing both frees them twice (`src/bar_item.c:753-801`).
  - [P] Animations keep writing to items that `--remove` freed;
    `bar_manager_remove_item` does not cancel them
    (`src/bar_manager.c:148-181`).
  - [V] A deferred frame update reads an embedded window after its owner is
    freed. An isolated post-decode callback replay reproduces the use after
    free under ASan (`tests/window/deferred.c`); the fork now cancels pending
    updates on close. Live WindowServer timing remains untested.
- **R6 [P] Threads and deprecated APIs.** `CVDisplayLink`, deprecated since
  macOS 15, queues one block per frame on the main queue without coalescing
  (`src/animation.c:4-11`). `NSScreen` is read without an `NSApplication`,
  which may return stale screens after a change.
- **R7 [V] macOS 27.** A click on an empty area of the bar raises the
  background above the items (#850); fixed in the fork by `fix(bar)`. The
  MediaRemote framework is locked (`src/media.m:4`), so `media_change` never
  fires (#708).
- **R8 [P] `-ffast-math`** in the makefile (`makefile:1`) meets floats parsed
  from user input without NaN or infinity handling.

## Performance and WindowServer load

The cost descriptions are based on code inspection and isolated tests. The
focused live baseline is in [performance.md](performance.md).

- **P1** Everything runs on the main thread: parsing, `vfork` (which suspends
  the thread until `exec`), SkyLight calls, CoreText layout and captures.
- **P2 Desktop switch, partly addressed.** `SPACE_CHANGED` arrives from three
  sources (`src/sketchybar.c`, `src/workspace.m`). The space-item update used
  to copy the managed space list once per item and repeatedly resolve each
  display. With 16 items, the isolated test failed its single-copy assertion
  before the fix and observed one space-list plus one display-list copy
  afterward. It also checks two displays and missing WindowServer answers.
  This is a call-count result, not a measured live latency improvement. A
  later change shares that same space-list copy with the bar loop, replacing
  its per-bar copies, and
  coalesces `SPACE_CHANGED` notifications arriving within 16 ms. Later
  duplicates may still run. `space_windows_change` reads every Desktop and
  window twice per switch
  (`src/app_windows.c:269-285`).
- **P3 Lua.** Each event sends one message per subscribed item, and each
  `:set` is a separate message with its own parse, refresh and transaction
  commit (`src/event.c:378-384`): about 16 events and 32 messages per
  `space_change` with the Dotfiles configuration.
- **P4 Rendering.** Every window has a 2x backing store, also on 1x displays
  (`src/window.c:81`, `src/surface.c:37-47`); every redraw copies the bitmap
  into a new image (`src/surface.c:67-75`); image comparison copies all pixels
  (`src/image.c:156`); a missing alias reads the whole system window list every
  second (`src/alias.c:71-74`).
- **P5 Timers and polling.** A 1 Hz timer always runs without tolerance
  (`src/bar_manager.c:65-78`). Without separate spaces per display, the active
  display is polled on every event, including animation frames
  (`src/event.c:379-381`).
- **P6 Rebuilds.** A rebuild destroys and recreates every item's window on
  every bar, about 100 windows with two displays and the Dotfiles
  configuration.

## yabai integration

- Desktop indices come from Mission Control across all displays, as in yabai
  (`src/misc/helpers.h:464-490`).
- Space and display masks are 32-bit, so Desktops beyond 31 cannot be
  represented (`src/bar.c:19`). Sixteen items suffice today; three displays
  may not.
- `hidden=current` indexes bars by arrangement id
  (`src/bar_manager.c:270-272`), which is wrong when not every display has a
  bar; related to #856 and to popups on the wrong display (#742).

## Upstream issues and pull requests

| Problem | Issues | Pull requests |
| --- | --- | --- |
| Hang or crash on wake with external displays | #776, #691, #783 | #847 (draft, similar to `fix(display)`) |
| IPC deadlock with Lua callbacks | #794 | #841 (100 ms), #838 (5 s) |
| Click raises the bar over its items on macOS 27 | #850 | #851, #855 (reorder after the click) |
| `media_change` never fires | #708, #731, #735 | none |
| Blur and flicker since 2.24.0 | #827, #839, #834 | none |
| Menu bar aliases on macOS 26 and 27 | #849 | #789 |
| Scripts and `waitpid` | #807 | none |
| Silent invalid values, hidden main bar, notch gap, popup display | #860, #856, #858, #742 | #857, #859, #861 |

Upstream merged six pull requests in 2026, the last four on 2026-09-16, so
the fork will carry its fixes for a while.

## Static analysis

The clang static analyzer reports 101 warnings over all translation units,
many repeated through shared headers: `vfork` misuse, the two use-after-free
paths in R5, uninitialised `memcpy` sources in `bar_manager.c`, `popup.c`,
`group.c`, `animation.c` and `env_vars.h`, and Core Foundation ownership
warnings. Phase 4 records them as a baseline.
