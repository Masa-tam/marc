# Silesia Corpus input directory

The Silesia Corpus is external benchmark data. It is not part of marc, is not
redistributed by this repository, and is not downloaded by the build,
tests, or benchmark tools.

## Obtain the corpus

1. Open the
   [official Silesia Corpus page](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia).
2. Download the complete `silesia.zip` archive through the `Total` link, or
   download the twelve individual bzip2 files. The ignored `downloads/`
   directory may be used for these local archives.
3. Place the twelve uncompressed files directly in the ignored `corpus/`
   directory beside this README.

The resulting layout must be:

```text
benchmarks/data/silesia/
    README.md
    downloads/             # optional local archives; ignored
    corpus/
        dickens
        mozilla
        mr
        nci
        ooffice
        osdb
        reymont
        samba
        sao
        webster
        xml
        x-ray
    results/               # optional local reports; ignored
```

Do not commit the archive, compressed members, extracted files, or locally
generated benchmark results. The `downloads/`, `corpus/`, and `results/`
directories are excluded by the repository `.gitignore`.

## Expected files

The sizes and MD5 values below are the values published by the official
Corpus page. MD5 is used only to identify the published files; it is not
treated as a security or authenticity guarantee.

| File | Uncompressed bytes | Published MD5 |
| --- | ---: | --- |
| `dickens` | 10,192,446 | `88334708559f6db57d79096bc0aca07e` |
| `mozilla` | 51,220,480 | `c7789a2097f1ff944b0c737430a339b3` |
| `mr` | 9,970,564 | `38e623e3093b7bf2003ca4b1bbc19927` |
| `nci` | 33,553,445 | `31f85bc8706f3c921104e7c169e2e2e1` |
| `ooffice` | 6,152,192 | `573c4ae915e36631d8f2dcffb9b9b66d` |
| `osdb` | 10,085,684 | `e734b0c48e6a982adfb5802da3032ecd` |
| `reymont` | 6,627,202 | `d8f54d78105079775f32d76dc55fc671` |
| `samba` | 21,606,400 | `154eaea7ea70e89f6339ff0abf4112ca` |
| `sao` | 7,251,944 | `79e95a22e18cd82b7e42bf91b380d30b` |
| `webster` | 41,458,703 | `474931ad907ac27bf962c75ded46c069` |
| `xml` | 5,345,280 | `9b09c0c80104adb8aae910b7d7db003e` |
| `x-ray` | 8,474,240 | `9baec32ad14ec3eff487d254382cb91c` |
| **Total** | **211,938,580** | - |

Run the local-only verifier from the repository root before measurement:

```console
py -3 tools/verify_silesia_corpus.py
```

On platforms where Python is exposed as `python3`, use:

```console
python3 tools/verify_silesia_corpus.py
```

An alternative Corpus directory may be supplied as the sole argument. The
verifier checks the exact names, sizes, and published MD5 values, then prints
SHA-256 values for experiment records. It performs no network access.

The verifier requires Python 3.9 or later. Its fixture-only unit test is
registered with CTest when CMake can discover a suitable Python interpreter;
Corpus presence is never a condition for registering or passing that test.

After verification, an individual member can be measured without loading the
complete file into memory. For example, compare 64 KiB and 1 MiB windows with
the same one MiB raw-frame boundary:

```console
marc_lzss_match_finder_benchmark --frames hash-chain-exact benchmarks/data/silesia/corpus/dickens 1 1048576 65536
marc_lzss_match_finder_benchmark --frames hash-chain-exact benchmarks/data/silesia/corpus/dickens 1 1048576 1048576
```

The private HashTree experiment requires an additional finite promotion
threshold and is currently run manually rather than through the v1 Corpus
runner:

```console
marc_lzss_match_finder_benchmark --frames hash-tree-exact benchmarks/data/silesia/corpus/dickens 1 1048576 1048576 32
```

After building the benchmark, the complete private HashTree threshold matrix
can be run without network access:

```console
py -3.14 tools/run_silesia_hash_tree_threshold_benchmark.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/hash-tree-threshold-msvc.json --compiler "MSVC 19.50" --generator "Visual Studio 18 2026"
```

Use `python3` instead of `py -3.14` where appropriate. The runner first
verifies all twelve local members, then measures one HashChain baseline and
HashTree thresholds 16, 64, 256, and 1,024 at each configured window. Results
remain local under the ignored `results/` directory.

The private sparse HashTree pool/threshold matrix uses a separate report and
keeps the existing runners unchanged:

```console
py -3.14 tools/run_silesia_sparse_hash_tree_matrix.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/sparse-hash-tree-msvc.json --compiler "MSVC 19.51" --generator "Visual Studio 18 2026"
```

The default grid uses pool capacities 4,096, 16,384, and 65,536 nodes with
promotion thresholds 16, 64, 256, and 1,024 at each standard window. Capacity
zero remains an optional explicit chain-only control (`--pool-capacities 0`),
but is not repeated across the default threshold grid. For
a bounded development smoke, add for example `--members dickens --windows
65536 --pool-capacities 65536 --thresholds 64`. The complete twelve-member
manifest is still verified before the selected members run. Output remains
local under the ignored `results/` directory.

Long runs should also specify an ignored checkpoint:

```console
py -3.14 tools/run_silesia_sparse_hash_tree_matrix.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --checkpoint benchmarks/data/silesia/results/sparse-hash-tree-msvc.checkpoint.json --output benchmarks/data/silesia/results/sparse-hash-tree-msvc.json --compiler "MSVC 19.51" --generator "Visual Studio 18 2026"
```

The runner saves after every completed baseline or sparse point and resumes
automatically when the checkpoint identity matches exactly. Do not edit the
checkpoint. A different revision, benchmark or runner-source content or path,
Corpus, selected grid, member set, or recorded platform/build environment is rejected rather
than mixed with earlier measurements. A completed checkpoint is retained for
audit and fast deterministic report regeneration.

To run a bounded batch, omit `--output` and add for example
`--max-new-points 12`. The limit counts newly launched HashChain baselines and
sparse candidates together; restored points do not consume the budget. The
runner exits successfully at a validated checkpoint boundary and prints
`progress=completed/total`. Repeat the same command until complete, then remove
`--max-new-points`, add `--output`, and run once more to publish the final report
without relaunching completed points. A value of zero performs a status-only
resume validation.

Run each of the twelve members independently. The existing Corpus-wide v1
runner orchestrates HashChain/BinaryTree only; HashTree threshold-sweep
orchestration will use a later versioned report contract.

The fixed private four-MiB experiment uses a separate runner and report
contract. After building the benchmark, run on Windows:

```console
py -3.14 tools/run_silesia_hash_tree_4m_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/hash-tree-4m-msvc.json --compiler "MSVC 19.51" --generator "Visual Studio 18 2026"
```

Use `python3` in place of `py -3.14` on other platforms. The runner performs
no download or network access. It verifies all twelve members, then measures
the fixed one-MiB HashChain control, four-MiB HashChain oracle, and four-MiB
HashTree threshold-1,024 candidate. A four-MiB Exact fingerprint mismatch is
fatal; failed performance or parse-opportunity gates remain recorded as valid
negative evidence. Output remains ignored under `results/`.

The fixed 64-MiB global BinaryTree experiment also uses a separate runner and
independent checkpoint/result schemas. Run a small bounded batch on a 64-bit
host with sufficient virtual-memory and commit headroom:

```console
py -3.14 tools/run_silesia_binary_tree_64m_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/binary-tree-64m-msvc.checkpoint.json --max-new-points 1 --compiler "MSVC 19.51" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

The runner verifies all twelve members before starting, executes only one
record at a time, and may require about 1.9 GiB of BinaryTree workspace plus
the 64-MiB input. Repeat the same command until it reports `48/48`; then remove
`--max-new-points` and add `--output` under the ignored `results/` directory.
Do not edit, combine, reorder, or move a checkpoint between builds. Allocation
failure is a failed point and must not be worked around by silently changing
the fixed frame, window, strategy, or memory policy.

## Usage policy

- Corpus measurements are opt-in developer benchmarks, not CTest pass gates.
- Measure each file independently and report both per-file and aggregate
  results. Do not silently concatenate the files.
- Record the marc revision, compiler, build type, CPU, command line, window
  size, match-finder strategy, search effort, and iteration count.
- Consult the official page and each listed source for applicable rights and
  usage conditions. This repository makes no redistribution claim for the
  Corpus or its members.
