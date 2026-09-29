"""Arithmetic and causal-input checks for the offline diagnostic model."""
import importlib.util
from fractions import Fraction
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location(
    "activation_model", Path(__file__).resolve().parents[1] / "benchmarks/position_distance_1m_activation_model.py")
model = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = model
spec.loader.exec_module(model)


def report(first=0, second=0):
    result = {"input_bytes": str(model.CAPACITY)}
    for index, visits in enumerate((first, second), 1):
        for key, value in zip(("position",) + model.FIELDS,
                              (model.CAPACITY * index // 4, index * 100, visits, 0, 0)):
            result[f"frame_0_sample_{index}_{key}"] = str(value)
    return result


class ActivationModelTests(unittest.TestCase):
    def test_rational_reference_and_integer_bounds(self):
        for position in (model.CAPACITY // 4, model.CAPACITY // 2 + 257):
            for visits in (0, 1, 1 << 20, model.CAPACITY**2):
                for saving in (1, 2, 3):
                    for scale in (1, 4, 16):
                        a = model.Sample(0, 0, 0, 0, 0)
                        b = model.Sample(position, 100, visits, 0, 0)
                        lhs, rhs = model.evidence(a, b, saving, scale)
                        remaining = model.CAPACITY - position
                        benefit = Fraction(saving * visits * remaining, 4 * position)
                        cost = scale * (model.HEADS + model.CAPACITY + 2 * position + remaining)
                        self.assertEqual(lhs > rhs, benefit > cost)
                        self.assertLess(max(lhs, rhs), 1 << 64)

    def test_zero_and_high_pressure(self):
        self.assertEqual({d["mode"] for d in model.predict(report()).values()}, {"never"})
        self.assertEqual({d["mode"] for d in model.predict(report(1 << 30, 1 << 30)).values()}, {"quarter"})

    def test_second_interval_and_future_independence(self):
        source = report(0, 1 << 30)
        decisions = model.predict(source)
        self.assertEqual({d["mode"] for d in decisions.values()}, {"half"})
        source["frame_0_sample_3_long_visits"] = "999999999999999999999"
        source["iteration_0_quarter_seconds"] = "0.000001"
        self.assertEqual(model.predict(source), decisions)

    def test_first_decision_ignores_later_pressure(self):
        first = model.predict(report(1 << 30, 1 << 30))
        second = model.predict(report(1 << 30, 1 << 40))
        self.assertEqual(first, second)

    def test_strict_tie_stays_five(self):
        # At half, second interval width=N/4 and remaining=N/2.
        work = model.HEADS + 2 * model.CAPACITY + model.CAPACITY // 2
        visits = 2 * work
        decision = model.predict(report(0, visits))["saving_1_of_4_cost_1"]
        self.assertEqual(decision["mode"], "never")

    def test_invalid_reports(self):
        for key, value in (("input_bytes", "1"), ("frame_0_sample_1_queries", "-1"),
                           ("frame_0_sample_2_long_visits", "-1"),
                           ("frame_0_sample_1_position", "0")):
            source = report()
            source[key] = value
            with self.assertRaises(ValueError):
                model.predict(source)
        with self.assertRaises(ValueError):
            model.read_report("a=1\na=2")


if __name__ == "__main__":
    unittest.main()
