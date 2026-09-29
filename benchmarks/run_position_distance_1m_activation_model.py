"""Freeze prefix-only predictions, then challenge them with forced replays.

Requires the existing observation and lazy-six benchmark executables. Always
creates a new artifact directory. No timing is fed back into model selection.
"""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

from position_distance_1m_activation_model import CAPACITY, predict, read_report

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from verify_silesia_corpus import verify_directory


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def generated(seed, size, structured):
    state = seed
    result = bytearray()
    while len(result) < size:
        state = (1664525 * state + 1013904223) & 0xffffffff
        byte = state >> 24
        if structured and len(result) % 8 < 5:
            byte = (byte & 63) if len(result) % 8 == 0 else b"ABCD"[len(result) % 8 - 1]
        result.append(byte)
    return bytes(result)


def save(path, value):
    with path.open("x", encoding="utf-8") as file:
        json.dump(value, file, indent=2)
        file.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--observation", type=Path, required=True)
    parser.add_argument("--forced", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    members = list(verify_directory(args.corpus))
    args.output.mkdir(parents=True, exist_ok=False)
    fixtures = args.output / "fixtures"
    fixtures.mkdir()
    sources = [Path(__file__), Path(__file__).with_name("position_distance_1m_activation_model.py")]
    identity = {str(p): digest(p) for p in sources + [args.observation, args.forced]}
    cases = {}
    for name in ("ooffice", "osdb", "sao"):
        with (args.corpus / name).open("rb") as file:
            cases[name + "-first-frame"] = file.read(CAPACITY)
    high = generated(1709, CAPACITY, True)
    low = generated(2909, CAPACITY, False)
    half = CAPACITY // 2
    cases.update(high=high, low=low, high_low=high[:half] + low[:half], low_high=low[:half] + high[:half])
    for name, raw in cases.items():
        assert len(raw) == CAPACITY
        with (fixtures / name).open("xb") as file:
            file.write(raw)
    save(args.output / "protocol.json", {
        "identity": identity, "corpus_manifest": [vars(m) for m in members],
        "inputs": {name: hashlib.sha256(raw).hexdigest() for name, raw in cases.items()},
        "structured_seed": 1709, "random_seed": 2909,
        "saving_quarters": [1, 2, 3], "cost_scales": [1, 4, 16],
        "checkpoints": ["quarter", "half"], "processes": 3,
        "timing_scope": "forced replay only; observation and decisions excluded",
    })

    def run(executable, name, label, verify_only=False):
        command = [str(executable.resolve()), str((fixtures / name).resolve())]
        if verify_only:
            command.append("--verify-only")
        result = subprocess.run(command, capture_output=True, text=True, timeout=600)
        with (args.output / (label + ".log")).open("x", encoding="utf-8") as file:
            file.write(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{label}: exit {result.returncode}")
        report = read_report(result.stdout)
        assert report["frame_identity"] == "1"
        assert report["verified_iterations"] == ("0" if verify_only else "3")
        return report

    observations, predictions = {}, {}
    for name in cases:
        report = run(args.observation, name, "observe-" + name, True)
        assert report["observations_verified"] == "1"
        observations[name] = report
        predictions[name] = predict(report)
        print("predicted", name, {v["mode"] for v in predictions[name].values()}, flush=True)
    # Persist every prediction before launching the first timing process.
    save(args.output / "predictions.json", predictions)
    save(args.output / "observations.json", observations)
    rows = []
    for iteration in range(3):
        order = list(cases)
        if iteration % 2:
            order.reverse()
        for name in order:
            report = run(args.forced, name, f"timed-{iteration}-{name}")
            assert report["boundary_verified"] == "1"
            for key in ("input_bytes", "tokens", "frame_bytes"):
                assert report[key] == observations[name][key]
            for index, mode in enumerate(("quarter", "half"), 1):
                assert report[f"frame_0_{mode}_checkpoint"] == observations[name][f"frame_0_sample_{index}_position"]
            medians = {mode: statistics.median(float(report[f"iteration_{i}_{mode}_seconds"]) for i in range(3))
                       for mode in ("bounded_five", "original_six", "never", "zero", "quarter", "half", "late")}
            row = {"case": name, "process": iteration, "report": report, "medians": medians}
            rows.append(row)
            with (args.output / "progress.jsonl").open("a", encoding="utf-8") as file:
                file.write(json.dumps(row) + "\n")
            print("timed", iteration, name, medians, flush=True)
    assert all(digest(path) == value for path, value in identity.items())
    save(args.output / "results.json", rows)
    summary = {}
    for name in cases:
        selected = [r for r in rows if r["case"] == name]
        summary[name] = {"median_seconds": {mode: statistics.median(r["medians"][mode] for r in selected)
                                           for mode in selected[0]["medians"]}, "models": {}}
        for key, prediction in predictions[name].items():
            mode = prediction["mode"]
            summary[name]["models"][key] = {
                "mode": mode, "checkpoint": prediction["checkpoint"],
                "forced_vs_never_percent": [(r["medians"][mode] / r["medians"]["never"] - 1) * 100 for r in selected],
            }
    save(args.output / "summary.json", summary)
    print("complete", flush=True)


if __name__ == "__main__":
    main()
