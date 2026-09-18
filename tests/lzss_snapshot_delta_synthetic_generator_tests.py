#!/usr/bin/env python3
"""Tests for deterministic Sparse snapshot delta fixture generation."""

from __future__ import annotations

import hashlib
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import generate_lzss_snapshot_delta_synthetic as generator  # noqa: E402


def _bytes(case_name: str, size: int, chunk_size: int = 17) -> bytes:
    return b"".join(generator.fixture_chunks(case_name, size, chunk_size))


class SnapshotDeltaSyntheticGeneratorTests(unittest.TestCase):
    def test_fixed_case_order_and_hand_checkable_prefixes(self) -> None:
        self.assertEqual(len(generator.CASE_NAMES), 6)
        self.assertEqual(_bytes("zeros", 9), bytes(9))
        self.assertEqual(
            _bytes("periodic-251", 255),
            bytes(range(251)) + bytes(range(4)),
        )
        shared = _bytes("shared-prefix-records", 128)
        self.assertEqual(shared[:56], generator.SHARED_PREFIX)
        self.assertEqual(shared[56:64], bytes(8))
        self.assertEqual(shared[64:120], generator.SHARED_PREFIX)
        self.assertEqual(shared[120:128], (1).to_bytes(8, "little"))
        self.assertEqual(
            _bytes("prefix-collision-runs", 61),
            (generator.PREFIX_COLLISION_UNIT * 2)[:61],
        )
        phased = _bytes(
            "phase-shifted-periodic", generator.PHASE_BLOCK_SIZE + 4,
        )
        self.assertEqual(phased[:4], bytes(range(4)))
        self.assertEqual(phased[-4:], bytes((1, 2, 3, 4)))

    def test_chunking_does_not_change_bytes_or_digest(self) -> None:
        for case_name in generator.CASE_NAMES:
            expected = _bytes(case_name, 12_345, 12_345)
            expected_digest = hashlib.sha256(expected).hexdigest()
            for chunk_size in (1, 7, 1_024, 4_097):
                self.assertEqual(
                    _bytes(case_name, 12_345, chunk_size), expected,
                )
                self.assertEqual(
                    generator.fixture_sha256(
                        case_name, 12_345, chunk_size,
                    ),
                    expected_digest,
                )

    def test_pseudorandom_prefix_is_fixed(self) -> None:
        self.assertEqual(
            _bytes("fixed-seed-pseudorandom", 24).hex(),
            "84ad9b36c2f74596158c35b7bb25add991b4663b5f0b5dbe",
        )

    def test_full_fixture_digests_are_fixed(self) -> None:
        self.assertEqual(set(generator.EXPECTED_SHA256), set(generator.CASE_NAMES))
        for case_name in generator.CASE_NAMES:
            self.assertEqual(
                generator.fixture_sha256(case_name, generator.DEFAULT_SIZE),
                generator.EXPECTED_SHA256[case_name],
            )

    def test_atomic_write_matches_stream_digest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "nested" / "fixture.bin"
            expected = generator.fixture_sha256("shared-prefix-records", 4099)
            actual = generator.write_fixture(
                "shared-prefix-records", 4099, output, 31,
            )
            self.assertEqual(actual, expected)
            self.assertEqual(output.stat().st_size, 4099)
            self.assertFalse(output.with_name(output.name + ".tmp").exists())
            self.assertEqual(
                hashlib.sha256(output.read_bytes()).hexdigest(), expected,
            )

    def test_invalid_request_is_rejected(self) -> None:
        for case_name, size, chunk_size in (
            ("unknown", 1, 1),
            ("zeros", 0, 1),
            ("zeros", 1, 0),
            ("zeros", True, 1),
            ("zeros", 1, True),
        ):
            with self.assertRaises(generator.GenerationError):
                list(generator.fixture_chunks(case_name, size, chunk_size))


if __name__ == "__main__":
    unittest.main()
