# marc
![marc logo](.github/assets/images/marc.png)

`marc` is a C++20 framework for independently designed, streaming lossless
compression components. The currently implemented public profiles are the
version 1 framed Blocked Huffman, Adaptive Huffman, Dynamic Range, rANS, tANS,
LZ77, and LZSS codecs, exposed through a small C ABI. The format and API are
still under development and version 0.x streams are not yet promised long-term
compatibility.

## Build

Initialize the compiler environment, then use the repository presets:

```console
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Windows uses the Visual Studio generator and MSBuild. The portable presets use
Ninja on non-Windows hosts. GoogleTest is needed only when `MARC_BUILD_TESTS` is
enabled; initialize the pinned submodule with:

```console
git submodule update --init --recursive
```

## Command-line tool

Top-level builds produce a minimal `marc` executable that exercises the public
C ABI with bounded streaming buffers. It currently writes the version 1 LZ77
profile with no entropy layer:

```console
marc encode input.bin output.marc
marc decode output.marc restored.bin
```

The destination and its `.tmp` staging path must not already exist. A successful
operation renames the staging file; a failed operation removes it, so malformed
input does not leave a partially decoded destination.

## CMake consumption

An installed package exports whichever libraries were enabled when marc was
built:

```cmake
find_package(marc CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE marc::shared) # or marc::static
```

The standalone project in `examples/` demonstrates installed-package use. See
[`docs/c-api.md`](docs/c-api.md) for the C transform lifecycle and
[`docs/format.md`](docs/format.md) for the current byte representation.

## License and provenance

The repository is MIT licensed. Its implementation process is described as a
specification-driven independent implementation; provenance and intentionally
unconsulted implementation sources are recorded in
[`docs/clean-room-record.md`](docs/clean-room-record.md). This process is not a
legal guarantee of non-infringement.
