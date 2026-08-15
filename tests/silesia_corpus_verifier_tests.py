from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from verify_silesia_corpus import (  # noqa: E402
    ManifestEntry,
    VerificationError,
    main,
    verify_directory,
)


def manifest_entry(name: str, contents: bytes) -> ManifestEntry:
    return ManifestEntry(
        name,
        len(contents),
        hashlib.md5(contents, usedforsecurity=False).hexdigest(),
    )


class SilesiaCorpusVerifierTests(unittest.TestCase):
    def test_accepts_exact_regular_files_and_preserves_manifest_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            contents = {"second": b"second fixture", "first": b"first fixture"}
            for name, value in contents.items():
                (root / name).write_bytes(value)
            manifest = tuple(
                manifest_entry(name, contents[name]) for name in ("first", "second")
            )

            results = verify_directory(root, manifest)

            self.assertEqual([result.name for result in results], ["first", "second"])
            self.assertEqual(
                results[0].sha256, hashlib.sha256(contents["first"]).hexdigest()
            )

    def test_rejects_missing_and_unexpected_entries_together(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "unexpected").write_bytes(b"data")
            manifest = (manifest_entry("missing", b"expected"),)

            with self.assertRaises(VerificationError) as caught:
                verify_directory(root, manifest)

            self.assertEqual(
                caught.exception.messages,
                ("missing file: missing", "unexpected entry: unexpected"),
            )

    def test_rejects_wrong_size_before_hashing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "sample").write_bytes(b"short")
            manifest = (manifest_entry("sample", b"expected contents"),)

            with self.assertRaisesRegex(VerificationError, "size mismatch"):
                verify_directory(root, manifest)

    def test_rejects_wrong_md5_with_the_same_size(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "sample").write_bytes(b"wrong")
            manifest = (manifest_entry("sample", b"right"),)

            with self.assertRaisesRegex(VerificationError, "MD5 mismatch"):
                verify_directory(root, manifest)

    def test_rejects_directory_in_place_of_expected_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "sample").mkdir()
            manifest = (manifest_entry("sample", b"value"),)

            with self.assertRaisesRegex(VerificationError, "not a regular file"):
                verify_directory(root, manifest)

    def test_rejects_missing_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            missing = Path(temporary_directory) / "missing"

            with self.assertRaisesRegex(VerificationError, "not a directory"):
                verify_directory(missing, ())

    def test_rejects_duplicate_manifest_names(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            duplicate = manifest_entry("sample", b"value")

            with self.assertRaisesRegex(ValueError, "duplicate names"):
                verify_directory(Path(temporary_directory), (duplicate, duplicate))

    def test_cli_failure_emits_no_partial_success_table(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            standard_output = io.StringIO()
            standard_error = io.StringIO()

            with redirect_stdout(standard_output), redirect_stderr(standard_error):
                result = main((temporary_directory,))

            self.assertEqual(result, 1)
            self.assertEqual(standard_output.getvalue(), "")
            self.assertIn("error: missing file: dickens", standard_error.getvalue())


if __name__ == "__main__":
    unittest.main()
