#!/usr/bin/env bash
#
# Runs the clang static analyzer over every translation unit of sketchybar.
#
# Upstream sources carry warnings, several of them real (see docs/audit.md),
# so the findings are compared with tools/analyzer-baseline.txt: a warning
# that is not in the baseline, or appears more often than recorded, fails the
# run. Fixing a finding lowers its count; refresh the baseline afterwards.
#
# The findings depend on the analyzer version: record the baseline with the
# Apple clang of the CI runner image.
#
# usage: tools/analyze.sh [--update-baseline]

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
baseline="$root/tools/analyzer-baseline.txt"
clang=${CLANG:-/usr/bin/clang}
flags=(--analyze -std=c99 -arch arm64 -F/System/Library/PrivateFrameworks -o /dev/null)

cd "$root"

# Line numbers shift with every edit, so findings are keyed by file, message
# and checker, and counted per key. A header is counted once per translation
# unit that reports it.
output=$(for source in src/*.c src/*.m; do
	"$clang" "${flags[@]}" "$source" 2>&1 || echo "error: cannot analyze $source"
done)

if grep -q '^error: ' <<<"$output"; then
	grep '^error: ' <<<"$output" >&2
	exit 1
fi

current=$(sed -nE 's/^([^:]+):[0-9]+:[0-9]+: warning: (.*)$/\1: \2/p' <<<"$output" |
	sort | uniq -c | sed -E 's/^ +//')

if [[ ${1:-} == --update-baseline ]]; then
	printf '%s\n' "$current" >"$baseline"
	echo "updated $baseline"
	exit 0
fi

# Each line is "<count> <file>: <message> [<checker>]".
regressions=$(printf '%s\n' "$current" | awk '
    NR == FNR { count = $1; sub(/^[0-9]+ /, ""); allowed[$0] = count; next }
    { count = $1; sub(/^[0-9]+ /, ""); if (count > allowed[$0] + 0) print count " (baseline " allowed[$0] + 0 ") " $0 }
' "$baseline" -)

if [[ -n $regressions ]]; then
	echo "error: new analyzer warnings:" >&2
	printf '%s\n' "$regressions" >&2
	exit 1
fi

echo "analyzer: $(wc -l <<<"$current" | tr -d ' ') finding kinds, none above the baseline"
