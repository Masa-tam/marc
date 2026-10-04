# Eight-MiB command-line implementation and qualification

DD-1472 / IR-1231 / TVG-1339 / CR-1643 implements DD-1471's explicit
command-line connection. Select lzss-position-distance-dynamic-range-8m.
No codec core, C ABI, existing profile default or schema-60 inventory changes.
The default remains LZ77. General header inference and a separate reader ABI
are not introduced.

tools/marc_cli.cpp adds an early dedicated branch, enum, usage entry and exact
name parsing. tools/lzss_position_distance_8m_cli.hpp owns the configuration,
queried opaque buffers and bounded file loop. It uses only the public C API.
Encode uses the existing prepared owning factory and INITIAL_ONLY query.
Decode uses the separate CAPACITY_ONLY query and factory, never the old common
workspace representation. The existing outer run() file transaction is intact.

Both directions use frame/block/window ceiling 8388608, maximum match 258,
payload ceiling 150994949, total output ceiling 1099511627776, logical internal
policy 1073741824, entropy entries 2599, range total 32768, expansion ratio 1024,
slack 1048576 and two 65536-byte call buffers. Concrete encoder original size
comes from the regular input file; decode validates its own bounded header.
Smaller legal frame/window/match headers remain accepted within these limits.

The checked external charge sums the complete codec Context, Loop, handle,
input/output stream objects and conservative helper-local control extents.
Full declared I/O and all allocated workspace tails remain separately charged
by the unchanged public/private resource contracts, without duplicate discounts.
On the qualified routes that caller charge is 1740 bytes. The five decoder
allocations are 150995029, 100663296, 100663296, 8388608 and 8388608 bytes,
sum 369098837. Token size/alignment come only from the public query, not these
diagnostic numbers. This is source-bound codec logical accounting; ordinary
file-library internals, allocator and instrumentation overhead are not a
process-wide memory guarantee. The one-GiB policy succeeded on the executed
two-full-frame repetitive and incompressible recipes, not all possible inputs.

Raw operator new allocations preserve exact requested extents and use the
matching ordinary/aligned delete. All returned capacities, alignment and sums
are validated before any decoder workspace allocation. Factory-started token
lifetimes end with transform destruction before borrowed owners are freed.
Partial allocation and creation failures use the same RAII order. No allocation
pool, reserve subtraction, retry, budget increase or alternate encoder strategy.
Initial encoder admission remains distinct from later generation admission.

The file loop preserves EndInput on final suffixes, consumes and produces
independent counts, drains verified pending output after source exhaustion,
checks count bounds and writes only committed bytes before interpreting error
status. A failed frame never reaches that loop as raw output. Prior verified
frames may have entered the invocation's temporary file; the outer transaction
removes that file on failure and commits a destination only after complete EOS
and successful close. Pre-existing destination and temporary files are refused.

Final source-bound matrix: two optimized routes and one address/undefined
instrumented route each passed 67 actual command-line invocations, 201 total,
with identical wire/raw digests and exit records. Recipes include empty, one byte,
all byte values, F-1/F/F+1, two contrasted full frames and two independently
generated SHAKE256 full frames. Decode and deterministic re-encode passed.
Limits, expansion, smaller valid headers, cross-profile, truncation, late
prefix/range/finish, trailing data, exact/near-miss names, override rejection
and existing output preservation were executed. All earlier trial artifacts
remain separately retained and are excluded from this final count.

A separate test executable includes the actual command-line translation unit
and injects fallible global allocations. Each route passed nine decoder and
eleven owning encoder refusals, including deferred generation failure, with
no destination or leftover invocation temporary and no live allocation receipt.
It proves borrowed owners release after both factory objects and explicitly
exercises the aligned allocation/delete branch. This isolated seam is absent
from the production executable; its receipt cleanup is not general leak
detection. A separately compiled private operation encoder matches the
256-byte all-byte recipe's full wire on all three actual tools; its unregistered
private dependencies are compiled explicitly. The first attempted reference
link omitted those dependencies and is retained as excluded diagnostic evidence.

A fresh complete static-library build passed eight selected registered CTests,
including the new allocation and boundary targets, old position-distance
round trips/selection/boundaries and default LZ77. This is not the full repository
suite, a throughput measurement, coverage-guided fuzz, hosted CI or external
exchange qualification. Authored by Codex from repository-owned contracts;
no external implementation source consulted. Schema 61 and revision-specific
71-archive CI/external exchange remain DD-1473 and later evidence gates.
