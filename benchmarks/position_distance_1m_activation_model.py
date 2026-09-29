"""Offline sensitivity model, never a runtime activation policy.

Only prefix observations choose a forced-replay checkpoint. Timings are used
afterwards to challenge the assumptions, never to choose the checkpoint.
"""

from dataclasses import dataclass

CAPACITY = 1 << 20
HEADS = 1 << 16
FIELDS = ("queries", "long_visits", "probe_passes", "prefix_passes")


@dataclass(frozen=True)
class Sample:
    position: int
    queries: int
    long_visits: int
    probe_passes: int
    prefix_passes: int


def read_report(text):
    result = {}
    for line in text.splitlines():
        key, value = line.split("=", 1)
        if key in result:
            raise ValueError("duplicate report field")
        result[key] = value
    return result


def prefix_samples(report):
    if int(report["input_bytes"]) != CAPACITY:
        raise ValueError("model evaluation requires exactly one full frame")
    result = []
    previous = Sample(0, 0, 0, 0, 0)
    for index in (1, 2):
        prefix = f"frame_0_sample_{index}_"
        sample = Sample(*(int(report[prefix + key]) for key in ("position",) + FIELDS))
        threshold = CAPACITY * index // 4
        if not threshold <= sample.position <= threshold + 257:
            raise ValueError("checkpoint is not a quarter token boundary")
        if not 0 < sample.queries <= sample.position:
            raise ValueError("invalid query count")
        if not 0 <= sample.prefix_passes <= sample.probe_passes <= sample.long_visits <= CAPACITY**2:
            raise ValueError("invalid candidate counts")
        if any(getattr(sample, key) < getattr(previous, key) for key in FIELDS):
            raise ValueError("non-monotone observations")
        result.append(sample)
        previous = sample
    return result


def evidence(previous, current, saving_quarters, cost_scale):
    """Return integer benefit numerator and cost in the same denominator.

    Saving fractions 1/4, 1/2, 3/4 and work scales 1, 4, 16 are a frozen
    sensitivity grid. They are assumptions, not fitted CPU-time coefficients.
    """
    if saving_quarters not in (1, 2, 3) or cost_scale not in (1, 4, 16):
        raise ValueError("unsupported sensitivity assumption")
    width = current.position - previous.position
    visits = current.long_visits - previous.long_visits
    queries = current.queries - previous.queries
    if width <= 0 or visits < 0 or queries < 0 or current.position >= CAPACITY:
        raise ValueError("invalid observation interval")
    remaining = CAPACITY - current.position
    # Initialization touches HEADS+N entries; catch-up gets two units/position,
    # future sixth-index maintenance one unit/position. Units are hypothetical.
    work = HEADS + CAPACITY + 2 * current.position + remaining
    benefit = saving_quarters * visits * remaining if queries else 0
    cost = 4 * width * cost_scale * work
    return benefit, cost


def predict(report):
    samples = prefix_samples(report)
    result = {}
    for saving in (1, 2, 3):
        for scale in (1, 4, 16):
            previous = Sample(0, 0, 0, 0, 0)
            decision = {"mode": "never", "checkpoint": CAPACITY, "evaluated": []}
            for mode, sample in zip(("quarter", "half"), samples):
                benefit, cost = evidence(previous, sample, saving, scale)
                decision["evaluated"].append({"mode": mode, "benefit": benefit, "cost": cost})
                if benefit > cost:  # A tie stays Five.
                    decision.update(mode=mode, checkpoint=sample.position)
                    break
                previous = sample
            result[f"saving_{saving}_of_4_cost_{scale}"] = decision
    return result
