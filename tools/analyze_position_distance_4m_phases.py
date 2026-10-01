"""Reconcile archived four-MiB diagnostic observations; never run a benchmark."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from statistics import median


PHASES = ("create_seconds", "prepare_seconds", "collect_seconds", "drain_seconds",
          "decode_process_seconds", "destroy_seconds")


def number(row, key):
    value = float(row[key])
    if not math.isfinite(value) or value < 0:
        raise ValueError(f"invalid nonnegative finite field: {key}")
    return value


def load(path, profiles, phase_harness=False):
    with path.open("rb") as stream:
        data = stream.read(16 * 1024 * 1024 + 1)
    if len(data) > 16 * 1024 * 1024:
        raise ValueError("measurement file exceeds 16 MiB")
    rows = json.loads(data)
    if not isinstance(rows, list) or len(rows) != 36 * len(profiles):
        raise ValueError("incomplete observation count")
    seen = set()
    sizes = {}
    for row in rows:
        profile = row["finder"] if phase_harness else (row["mode"], row["profile"])
        key = (row["member"], int(row["pass_index"]), profile)
        if key in seen or key[1] not in range(3) or profile not in profiles:
            raise ValueError("duplicate or unsupported observation key")
        seen.add(key)
        if str(row["verified"]) != "1":
            raise ValueError("unverified observation")
        size = (int(row["input_bytes"]), int(row["archive_bytes"]))
        if min(size) <= 0 or sizes.setdefault(key[0], size) != size:
            raise ValueError("contradictory member sizes")
        if phase_harness:
            if row["mode"] != "measure" or number(row, "selection_seconds") + number(row, "frame_seconds") <= 0:
                raise ValueError("not a positive timed phase observation")
            for metric, suffix in (("selection_seconds", "selection_seconds"),
                                   ("frame_seconds", "coding_seconds")):
                total = number(row, metric)
                frames = int(row["frames"])
                if frames != (size[0] + 4194303) // 4194304:
                    raise ValueError("contradictory frame count")
                summed = sum(number(row, f"frame_{i}_{suffix}") for i in range(frames))
                if not math.isclose(total, summed, abs_tol=1e-5, rel_tol=1e-9):
                    raise ValueError("non-additive frame phases")
        else:
            total = number(row, "seconds")
            summed = sum(number(row, field) for field in PHASES)
            if total <= 0 or not math.isclose(total, summed, abs_tol=1e-5, rel_tol=1e-9):
                raise ValueError("non-additive owner phases")
    if len(sizes) != 12 or len(seen) != 12 * 3 * len(profiles):
        raise ValueError("incomplete member/pass/profile set")
    return rows, sizes, hashlib.sha256(data).hexdigest()


def owner_summary(rows):
    selected = [r for r in rows if r["mode"] == "encode" and r["profile"] == "five-prefix"]
    passes = []
    for index in range(3):
        group = [r for r in selected if int(r["pass_index"]) == index]
        totals = {field: sum(number(r, field) for r in group) for field in (*PHASES, "seconds")}
        passes.append({"pass": index, "totals": totals,
                       "preparation_share_percent": 100 * totals["prepare_seconds"] / totals["seconds"]})
    ranking = []
    for member in sorted({r["member"] for r in selected}):
        group = [r for r in selected if r["member"] == member]
        ranking.append({"member": member, "median_seconds": median(number(r, "seconds") for r in group),
                        "median_preparation_seconds": median(number(r, "prepare_seconds") for r in group)})
    total = sum(r["median_seconds"] for r in ranking)
    for row in ranking:
        row["median_cost_share_percent"] = 100 * row["median_seconds"] / total
    ranking.sort(key=lambda r: r["median_seconds"], reverse=True)
    return {"passes": passes, "member_ranking": ranking,
            "top_three_median_cost_share_percent": sum(r["median_cost_share_percent"] for r in ranking[:3])}


def selection_summary(rows):
    result = []
    for index in range(3):
        group = [r for r in rows if r["finder"] == "five-prefix" and int(r["pass_index"]) == index]
        selection = sum(number(r, "selection_seconds") for r in group)
        coding = sum(number(r, "frame_seconds") for r in group)
        result.append({"pass": index, "selection_seconds": selection, "coding_seconds": coding,
                       "selection_share_percent": 100 * selection / (selection + coding)})
    return result


def scratch_summary(rows):
    comparisons = []
    for index in range(3):
        for member in sorted({r["member"] for r in rows}):
            group = {r["profile"]: r for r in rows if r["mode"] == "encode"
                     and r["member"] == member and int(r["pass_index"]) == index}
            a, b, trial = (number(group[p], "seconds") for p in ("reference-0", "reference-1", "finder-scratch"))
            comparisons.append({"pass": index, "member": member,
                "controls_seconds": [a, b], "trial_seconds": trial,
                "classification": "win-both" if trial < min(a, b) else "loss-both" if trial > max(a, b) else "between",
                "control_spread_percent": 100 * (max(a, b) / min(a, b) - 1)})
    counts = {label: sum(r["classification"] == label for r in comparisons)
              for label in ("win-both", "loss-both", "between")}
    return {"counts": counts, "exceptions": [r for r in comparisons if r["classification"] != "win-both"],
            "largest_control_spread": max(comparisons, key=lambda r: r["control_spread_percent"])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("owner", "selection", "scratch", "scratch-repeat", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("output already exists")
    owner_profiles = {("encode", p) for p in ("reference-0", "reference-1", "five-prefix")} | {("decode", "five-prefix")}
    scratch_profiles = {("encode", p) for p in ("reference-0", "reference-1", "finder-scratch")} | {("decode", "finder-scratch")}
    owner, sizes, owner_hash = load(args.owner, owner_profiles)
    selection, selection_sizes, selection_hash = load(args.selection, {"reference-0", "reference-1", "five-prefix"}, True)
    scratch, scratch_sizes, scratch_hash = load(args.scratch, scratch_profiles)
    repeat, repeat_sizes, repeat_hash = load(args.scratch_repeat, scratch_profiles)
    if not sizes == selection_sizes == scratch_sizes == repeat_sizes:
        raise ValueError("datasets disagree on corpus members or archive sizes")
    report = {"method": "archived observations only; campaigns analysed separately, no new timing or causal claim",
        "input_sha256": {"owner": owner_hash, "selection": selection_hash, "scratch": scratch_hash, "scratch_repeat": repeat_hash},
        "owner": owner_summary(owner), "selection": selection_summary(selection),
        "scratch_original": scratch_summary(scratch), "scratch_repeat": scratch_summary(repeat),
        "scratch_admission": "not admitted; original exception and unresolved control spread retained",
        "next_diagnostic": "separate finder initialization, lookup traversal/extension, insertion and frame coding; prove identical complete tokens/streams before timing"}
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print("Reconciled 540 archived observations; no benchmark executed")


if __name__ == "__main__":
    main()
