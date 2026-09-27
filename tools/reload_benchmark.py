#!/usr/bin/env python3
"""Measure live reloads until the original item count returns.

Run against an already configured GUI service: reload_benchmark.py [runs] [limit_s].
The limit is a local performance gate, not a CI assertion across machines.
"""

import json
import subprocess
import sys
import time


def items():
    start = time.monotonic()
    try:
        result = subprocess.run(
            ["sketchybar", "--query", "bar"],
            capture_output=True, text=True, check=True, timeout=3,
        )
        count = len(json.loads(result.stdout)["items"])
    except (subprocess.SubprocessError, ValueError, KeyError):
        count = None
    return count, time.monotonic() - start


def once(expected, number, limit):
    start = time.monotonic()
    subprocess.run(["sketchybar", "--reload"], capture_output=True,
                   check=True, timeout=5)
    ack = time.monotonic() - start
    first_unready = None
    ready = None
    max_query = 0
    samples = 0
    changes = []
    previous = expected

    while time.monotonic() - start < 15:
        count, query = items()
        samples += 1
        max_query = max(query, max_query)
        elapsed = time.monotonic() - start
        if count != previous:
            changes.append([round(elapsed, 3), count])
            previous = count
        if count != expected and first_unready is None:
            first_unready = elapsed
        if first_unready is not None and count == expected:
            ready = elapsed
            break
        time.sleep(.04)

    row = dict(reload=number, ack_s=round(ack, 3),
               unready_s=round(first_unready, 3) if first_unready else None,
               ready_s=round(ready, 3) if ready else None,
               max_query_s=round(max_query, 3), samples=samples,
               changes=changes[:12] + changes[-6:] if len(changes) > 18 else changes,
               change_count=len(changes),
               slow=ready is not None and ready > limit)
    print(json.dumps(row), flush=True)
    return row


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    limit = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
    expected, _ = items()
    if not expected:
        raise SystemExit("active bar did not answer")
    print(json.dumps(dict(baseline_items=expected)), flush=True)
    rows = []
    for number in range(1, runs + 1):
        row = once(expected, number, limit)
        rows.append(row)
        if row["ready_s"] is None:
            break
        time.sleep(1)
    print(json.dumps(dict(slow_count=sum(row["slow"] for row in rows),
                          inconclusive_count=sum(row["ready_s"] is None
                                                 for row in rows))), flush=True)
    return int(any(row["slow"] for row in rows))


if __name__ == "__main__":
    sys.exit(main())
