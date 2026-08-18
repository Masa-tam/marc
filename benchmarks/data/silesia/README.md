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

Run each of the twelve members independently. The existing Corpus-wide v1
runner orchestrates HashChain/BinaryTree only; HashTree threshold-sweep
orchestration will use a later versioned report contract.

## Usage policy

- Corpus measurements are opt-in developer benchmarks, not CTest pass gates.
- Measure each file independently and report both per-file and aggregate
  results. Do not silently concatenate the files.
- Record the marc revision, compiler, build type, CPU, command line, window
  size, match-finder strategy, search effort, and iteration count.
- Consult the official page and each listed source for applicable rights and
  usage conditions. This repository makes no redistribution claim for the
  Corpus or its members.
