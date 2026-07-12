# Architecture

## Baseline

marc is a C++20 library of bounded, stateful byte-stream transforms. MSVC is
the reference toolchain, but the implementation avoids compiler extensions and
keeps portable C++ as a design constraint. CMake is the canonical build
description.

The same source list builds both a static library and a shared library. The C
ABI is the binary boundary; C++ implementation types are never exported as ABI.

## Layers

Encoding flows from raw input through an optional dictionary transform, an
entropy transform, and the frame controller to a sink. Decoding reverses that
order. Direction is immutable for a transform instance.

Entropy blocks cannot cross frame boundaries. A frame may contain multiple
entropy blocks, and its last entropy block may be shorter than the configured
entropy block size. A frame boundary finalizes and resets every layer whose
state is scoped to that frame.

## Error and ownership policy

Normal control flow does not use exceptions. Public C++ processing functions
are `noexcept`, and no exception may cross the C ABI. Allocation failures and
unexpected internal exceptions at ABI adapters are converted to stable status
codes. Compiler exception support remains enabled so standard-library failures
can be contained safely.

Opaque C handles are created and destroyed by marc. The initial ABI does not
accept allocator callbacks and does not return variable-sized library-owned
buffers. Caller-provided input, output, and diagnostic buffers retain caller
ownership.

## Initial stream-size policy

The baseline framed format requires the original uncompressed size to be known
when encoding begins. Unknown-size streams are a future format capability, not
a baseline requirement. The transform API remains incremental; known size does
not imply that the complete input must be buffered.
