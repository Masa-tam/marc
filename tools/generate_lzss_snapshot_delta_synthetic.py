#!/usr/bin/env python3
"""Generate deterministic fixtures for the Sparse snapshot delta experiment."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sys
from typing import Iterable, Optional, Sequence


PROFILE = "marc-lzss-snapshot-delta-synthetic-v1"
DEFAULT_SIZE = 67_108_864
DEFAULT_CHUNK_SIZE = 1_048_576
PSEUDORANDOM_SEED = 0x74D13A8E59C620BF
PSEUDORANDOM_MULTIPLIER = 2_685_821_657_736_338_717
MASK64 = (1 << 64) - 1
SHARED_PREFIX = bytes((index * 29 + 17) % 251 for index in range(56))
PREFIX_COLLISION_UNIT = b"AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh"
PERIODIC_251 = bytes(range(251))
PHASE_BLOCK_SIZE = 4_096
PHASE_PERIOD = b"".join(
    bytes((index + phase) % 251 for index in range(PHASE_BLOCK_SIZE))
    for phase in range(251)
)
CASE_NAMES = (
    "zeros",
    "periodic-251",
    "shared-prefix-records",
    "prefix-collision-runs",
    "phase-shifted-periodic",
    "fixed-seed-pseudorandom",
)
EXPECTED_SHA256 = {
    "zeros": "3b6a07d0d404fab4e23b6d34bc6696a6a312dd92821332385e5af7c01c421351",
    "periodic-251": "98dc891b284e4d84ac25b0c0a24fdbe39a7f0dbd643ad5e8aa06e02fc6258254",
    "shared-prefix-records": "4b5fabcab9e6f2990300e76a7f1ce88517e6e1d0ceef0be488de2f1a0d97f351",
    "prefix-collision-runs": "482e02b39e59e67c193b2946d3d4263db28b41a73b76dea1a1829709a725086e",
    "phase-shifted-periodic": "4e7aff6549651523e3f399c53dda706dac328c0a76cac7a472effe83f2004ac8",
    "fixed-seed-pseudorandom": "ee9b03a4842b059719865ac8b591a9546c5a326e22c89a7d078e817b2ff5163f",
}


class GenerationError(ValueError):
    """The requested deterministic fixture cannot be generated."""


def _repeat_pattern(
    pattern: bytes, size: int, chunk_size: int,
) -> Iterable[bytes]:
    offset = 0
    remaining = size
    while remaining:
        count = min(remaining, chunk_size)
        repetitions = (offset + count + len(pattern) - 1) // len(pattern) + 1
        expanded = pattern * repetitions
        yield expanded[offset:offset + count]
        offset = (offset + count) % len(pattern)
        remaining -= count


def _shared_prefix_chunks(size: int, chunk_size: int) -> Iterable[bytes]:
    position = 0
    while position < size:
        count = min(size - position, chunk_size)
        end = position + count
        chunk = bytearray()
        while position < end:
            record_index, record_offset = divmod(position, 64)
            record = SHARED_PREFIX + record_index.to_bytes(8, "little")
            take = min(end - position, 64 - record_offset)
            chunk.extend(record[record_offset:record_offset + take])
            position += take
        yield bytes(chunk)


def _pseudorandom_chunks(size: int, chunk_size: int) -> Iterable[bytes]:
    state = PSEUDORANDOM_SEED
    pending = b""
    remaining = size
    while remaining:
        count = min(remaining, chunk_size)
        chunk = bytearray()
        if pending:
            take = min(count, len(pending))
            chunk.extend(pending[:take])
            pending = pending[take:]
        while len(chunk) < count:
            state ^= state >> 12
            state ^= (state << 25) & MASK64
            state ^= state >> 27
            word = ((state * PSEUDORANDOM_MULTIPLIER) & MASK64).to_bytes(
                8, "little",
            )
            take = min(count - len(chunk), len(word))
            chunk.extend(word[:take])
            pending = word[take:]
        yield bytes(chunk)
        remaining -= count


def fixture_chunks(
    case_name: str, size: int,
    chunk_size: int = DEFAULT_CHUNK_SIZE,
) -> Iterable[bytes]:
    """Yield one deterministic fixture without retaining it in memory."""
    if case_name not in CASE_NAMES:
        raise GenerationError(f"unknown fixture: {case_name}")
    if isinstance(size, bool) or size <= 0:
        raise GenerationError("fixture size must be positive")
    if isinstance(chunk_size, bool) or chunk_size <= 0:
        raise GenerationError("chunk size must be positive")
    if case_name == "zeros":
        remaining = size
        block = bytes(min(size, chunk_size))
        while remaining:
            count = min(remaining, len(block))
            yield block[:count]
            remaining -= count
    elif case_name == "periodic-251":
        yield from _repeat_pattern(PERIODIC_251, size, chunk_size)
    elif case_name == "shared-prefix-records":
        yield from _shared_prefix_chunks(size, chunk_size)
    elif case_name == "prefix-collision-runs":
        yield from _repeat_pattern(PREFIX_COLLISION_UNIT, size, chunk_size)
    elif case_name == "phase-shifted-periodic":
        yield from _repeat_pattern(PHASE_PERIOD, size, chunk_size)
    else:
        yield from _pseudorandom_chunks(size, chunk_size)


def fixture_sha256(
    case_name: str, size: int,
    chunk_size: int = DEFAULT_CHUNK_SIZE,
) -> str:
    digest = hashlib.sha256()
    produced = 0
    for chunk in fixture_chunks(case_name, size, chunk_size):
        produced += len(chunk)
        digest.update(chunk)
    if produced != size:
        raise GenerationError("fixture generator produced the wrong size")
    return digest.hexdigest()


def write_fixture(
    case_name: str, size: int, output: Path,
    chunk_size: int = DEFAULT_CHUNK_SIZE,
) -> str:
    """Atomically write and return the SHA-256 of one fixture."""
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    digest = hashlib.sha256()
    produced = 0
    try:
        with temporary.open("wb") as stream:
            for chunk in fixture_chunks(case_name, size, chunk_size):
                stream.write(chunk)
                digest.update(chunk)
                produced += len(chunk)
            stream.flush()
            os.fsync(stream.fileno())
        if produced != size:
            raise GenerationError("fixture generator produced the wrong size")
        os.replace(temporary, output)
    except BaseException:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise
    return digest.hexdigest()


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Generate repository-defined Sparse snapshot delta fixtures; "
            "performs no network or external-data access."
        ),
    )
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE)
    parser.add_argument("--case", choices=CASE_NAMES, action="append")
    parsed = parser.parse_args(arguments)
    if parsed.size <= 0:
        parser.error("size must be positive")
    selected = tuple(parsed.case) if parsed.case else CASE_NAMES
    if len(set(selected)) != len(selected):
        parser.error("fixture names must be unique")
    try:
        for case_name in selected:
            output = parsed.output_directory / f"{case_name}.bin"
            digest = write_fixture(case_name, parsed.size, output)
            print(f"{case_name} {parsed.size} {digest} {output}")
    except (OSError, GenerationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
