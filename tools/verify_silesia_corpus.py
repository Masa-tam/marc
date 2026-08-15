#!/usr/bin/env python3
"""Verify a locally supplied Silesia Corpus without network access."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import sys
from typing import Iterable, Optional, Sequence


READ_CHUNK_SIZE = 1024 * 1024


@dataclass(frozen=True)
class ManifestEntry:
    name: str
    size: int
    md5: str


@dataclass(frozen=True)
class VerificationResult:
    name: str
    size: int
    md5: str
    sha256: str


class VerificationError(Exception):
    def __init__(self, messages: Iterable[str]) -> None:
        self.messages = tuple(messages)
        super().__init__("; ".join(self.messages))


SILESIA_MANIFEST = (
    ManifestEntry("dickens", 10_192_446, "88334708559f6db57d79096bc0aca07e"),
    ManifestEntry("mozilla", 51_220_480, "c7789a2097f1ff944b0c737430a339b3"),
    ManifestEntry("mr", 9_970_564, "38e623e3093b7bf2003ca4b1bbc19927"),
    ManifestEntry("nci", 33_553_445, "31f85bc8706f3c921104e7c169e2e2e1"),
    ManifestEntry("ooffice", 6_152_192, "573c4ae915e36631d8f2dcffb9b9b66d"),
    ManifestEntry("osdb", 10_085_684, "e734b0c48e6a982adfb5802da3032ecd"),
    ManifestEntry("reymont", 6_627_202, "d8f54d78105079775f32d76dc55fc671"),
    ManifestEntry("samba", 21_606_400, "154eaea7ea70e89f6339ff0abf4112ca"),
    ManifestEntry("sao", 7_251_944, "79e95a22e18cd82b7e42bf91b380d30b"),
    ManifestEntry("webster", 41_458_703, "474931ad907ac27bf962c75ded46c069"),
    ManifestEntry("xml", 5_345_280, "9b09c0c80104adb8aae910b7d7db003e"),
    ManifestEntry("x-ray", 8_474_240, "9baec32ad14ec3eff487d254382cb91c"),
)


def _new_md5():
    try:
        return hashlib.md5(usedforsecurity=False)
    except TypeError:
        return hashlib.md5()


def _hash_file(path: Path) -> tuple[str, str]:
    md5 = _new_md5()
    sha256 = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(READ_CHUNK_SIZE):
            md5.update(chunk)
            sha256.update(chunk)
    return md5.hexdigest(), sha256.hexdigest()


def verify_directory(
    directory: Path,
    manifest: Sequence[ManifestEntry] = SILESIA_MANIFEST,
) -> tuple[VerificationResult, ...]:
    root = Path(directory)
    if not root.is_dir():
        raise VerificationError((f"not a directory: {root}",))

    expected_names = [entry.name for entry in manifest]
    if len(set(expected_names)) != len(expected_names):
        raise ValueError("manifest contains duplicate names")

    actual_entries = {entry.name: entry for entry in root.iterdir()}
    expected_name_set = set(expected_names)
    errors: list[str] = []

    for name in sorted(expected_name_set - actual_entries.keys()):
        errors.append(f"missing file: {name}")
    for name in sorted(actual_entries.keys() - expected_name_set):
        errors.append(f"unexpected entry: {name}")

    results: list[VerificationResult] = []
    for expected in manifest:
        path = actual_entries.get(expected.name)
        if path is None:
            continue
        if path.is_symlink() or not path.is_file():
            errors.append(f"not a regular file: {expected.name}")
            continue

        actual_size = path.stat().st_size
        if actual_size != expected.size:
            errors.append(
                f"size mismatch for {expected.name}: "
                f"expected {expected.size}, found {actual_size}"
            )
            continue

        actual_md5, actual_sha256 = _hash_file(path)
        if actual_md5 != expected.md5:
            errors.append(
                f"MD5 mismatch for {expected.name}: "
                f"expected {expected.md5}, found {actual_md5}"
            )
            continue

        results.append(
            VerificationResult(
                name=expected.name,
                size=actual_size,
                md5=actual_md5,
                sha256=actual_sha256,
            )
        )

    if errors:
        raise VerificationError(errors)
    return tuple(results)


def _default_corpus_directory() -> Path:
    return Path(__file__).resolve().parents[1] / "benchmarks" / "data" / "silesia" / "corpus"


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify a local Silesia Corpus; performs no network access."
    )
    parser.add_argument(
        "directory",
        nargs="?",
        type=Path,
        default=_default_corpus_directory(),
        help="directory containing the twelve uncompressed files",
    )
    parsed = parser.parse_args(arguments)

    try:
        results = verify_directory(parsed.directory)
    except (OSError, VerificationError) as error:
        if isinstance(error, VerificationError):
            for message in error.messages:
                print(f"error: {message}", file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print("name\tbytes\tmd5\tsha256")
    for result in results:
        print(f"{result.name}\t{result.size}\t{result.md5}\t{result.sha256}")
    total_size = sum(result.size for result in results)
    print(f"Verified {len(results)} Silesia files, {total_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
