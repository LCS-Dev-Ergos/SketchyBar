# Fork workflow

`LCS-Dev-Ergos/SketchyBar` carries fixes and tooling on top of
`FelixKratz/SketchyBar`, following the same process as the LCS-Dev-Ergos yabai
fork.

## Branches

- `master` mirrors upstream `master` and only fast-forwards. Nothing is
  committed to it directly.
- `dev` is the default branch and holds all fork work. Changes land as atomic
  commits; changes to upstream files stay small and fork additions live in
  their own files (`src/display_*`, `src/bar_level.h`, `tests/`, `tools/`,
  `docs/`, `.vscode/`, CMake files).
- The local clone names the fork `origin` and upstream `upstream`, with
  pushing to upstream disabled.

## Upstream releases

`.github/workflows/sync-upstream.yml` runs every Monday and on demand. When
upstream publishes a release that `master` does not contain yet, it
fast-forwards `master` to upstream, copies the upstream tags and opens a pull
request from `master` into `dev`. Resolve conflicts with fork changes in that
pull request. The pull request step needs "Allow GitHub Actions to create and
approve pull requests" in the repository's Actions settings.

Upstream tags are mirrored into the fork. Local clones with
`fetch.pruneTags` enabled delete tags the fork lacks, so push new upstream
tags to the fork before fetching it.

## Releases

Fork releases are annotated tags named `v<upstream version>-lcs.<n>` on `dev`,
for example `v2.24.0-lcs.1`, counting from 1 again after each upstream
release. The release workflow, which builds and signs tagged releases, and
the `~/Dotfiles` package file with its update script are phase 4 of the
[plan](plan.md); until then Dotfiles builds upstream 2.24.0 with its own
patches.

## Documents

- [audit.md](audit.md): security, robustness and performance audit of
  upstream 2.24.0.
- [plan.md](plan.md): phased plan and its status.
- [testing.md](testing.md): presets, tests, fuzzing and live checks.
