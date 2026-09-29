"""Independently verify online diagnostic traces using rational arithmetic."""
from fractions import Fraction
from pathlib import Path
import sys

MODES = ["monitor"] + [f"s{s}k{k}" for s in (1, 2, 3) for k in (1, 4, 16)]


def verify(report):
    size = int(report["input_bytes"])
    assert size > 0 and report["online_verified"] == report["frame_identity"] == "1"
    totals = {mode: [0, 0] for mode in MODES}
    for frame, offset in enumerate(range(0, size, 1048576)):
        n = min(1048576, size - offset)
        for mode in MODES:
            saving, scale = (2, 1) if mode == "monitor" else tuple(map(int, mode[1:].split("k")))
            prefix = f"frame_{frame}_{mode}_"
            events = report[prefix + "events"]
            previous, target, selected = 0, max(65536, (n + 3) // 4), False
            activation = n
            rows = events.split(";") if events else []
            assert len(rows) <= 9
            for row in rows:
                p, width, visits, queries, lhs, rhs, choice = map(int, row.split(","))
                assert not selected and target <= 3 * n // 4
                assert target <= p <= target + 257 and p < n
                assert width == p - previous and width > 0
                assert 0 <= queries <= width and 0 <= visits <= n * n
                remaining = n - p
                assert remaining >= max(65536, (n + 7) // 8)
                benefit = Fraction(saving * visits * remaining, 4 * width) if queries else 0
                cost = scale * (65536 + n + 2 * p + remaining)
                assert Fraction(lhs, 4 * width) == benefit
                assert Fraction(rhs, 4 * width) == cost
                assert max(lhs, rhs) < 1 << 64
                selected = mode != "monitor" and benefit > cost
                assert choice == int(selected)
                if selected:
                    activation = p
                previous, target = p, target + 65536
            if not selected and target <= 3 * n // 4:
                # A remaining scheduled check can be suppressed only by the
                # guard, allowing at most one maximum-match boundary overshoot.
                assert n - min(n, target + 257) < max(65536, (n + 7) // 8)
            assert int(report[prefix + "activation"]) == activation
            totals[mode][0] += int(selected)
            totals[mode][1] += len(rows)
    for mode, (transitions, checks) in totals.items():
        assert int(report[mode + "_transitions"]) == transitions
        assert int(report[mode + "_checks"]) == checks


if __name__ == "__main__":
    for name in sys.argv[1:]:
        lines = Path(name).read_text().splitlines()
        report = dict(line.split("=", 1) for line in lines)
        assert len(report) == len(lines)
        verify(report)
        print(name, "verified")
