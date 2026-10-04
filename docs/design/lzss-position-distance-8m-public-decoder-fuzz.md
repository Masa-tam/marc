# Bounded public decoder mutation campaign

DD-1470 specifies a reproducible finite mutation campaign for the existing
eight-MiB public five-buffer decoder. IR-1229, TVG-1337 and CR-1641 bind its
executed scope. No stream representation, production default, C ABI or codec
implementation changes are intended.

Use frame size 64, total output ceiling 192, supplied input ceiling 4096,
declared output capacity 4160, and an explicit eight-MiB internal policy with
four-MiB external retained reserve. Generate seeds before allocating decoder
workspaces. Retain and charge all five queried extents; close each decoder and
typed lifetime before sequential reuse. Logical reservation is not resident
memory or allocator overhead. Maximum-frame qualification remains DD-1469.

Generate public encoder seeds at lengths 0, 1, 63, 64, 65, 128 and 192.
The multi-frame recipe uses distinct first and later raw values. A fixed
unsigned xorshift32 generator (shifts 13, 17, 5; initial state 0x14701229)
selects seeds, byte flips, truncation, insertion, deletion, four-byte overwrites,
random byte inputs, trailing bytes and unchanged replay. Record case index,
input digest and schedule seed. Bound each schedule to 4096 calls, force
positive capacities between starvation calls, vary input widths 1..47 and
output widths 1..31, and alternate nonterminal Flush. Compare unsplit and split
committed bytes, terminal category, consumed extent, error byte/bit position
and full publication slot. Compare each call against the unchanged private
stream decoder using the same schedule. Verify buffer tails and sticky terminal
results. On failure print a complete hexadecimal input and reproducible seed.

Separately traverse finite frames to determine the committed frontier and full
last validated raw slot, including untouched sentinel tails. This oracle shares
the finite range codec and validates coordination and publication, not independent
range arithmetic. Known raw recipes and deliberately malformed later prefixes
provide independent expectations. In particular, submit a complete valid frame
with zero output capacity, require NeedOutput with all input consumed and no
downstream output, repeat starvation, then drain the verified frame unchanged.

The registered test runs 512 mutations by default; an explicitly bounded command
argument permits up to 16384. Qualify the same source on two optimized routes
and an address/undefined instrumented route. This is deterministic mutation
fuzzing, not a coverage-guided engine or an exhaustive malformed-stream proof.
Keep immutable seeds, commands, source/library hashes, logs and failed artifacts.
No throughput, leak-detection, command-line integration or external verification
claim follows from this gate. Authored by Codex using repository-owned contracts;
no external implementation consulted.
