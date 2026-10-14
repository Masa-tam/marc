# Sixteen-MiB position-distance tANS

## DD-1606: exact sixteen-MiB specialization

This specialization uses the native position-distance tANS family with
`F = 16,777,216` and `N = 24`. Its distinct identity is dictionary `2/12`,
context `1/25`, and entropy `5/7`. The configured dictionary window and
maximum frame are both 16,777,216 bytes; grammar lengths remain 3 through
258. The encoder, when admitted, uses fixed-five eligibility, exact longest
matching, and nearest-distance tie breaking.

There are 56 contexts and 4,658 frequencies. The alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [25]*9 + [2]*24`. The descriptor follows the
family 24-byte prefix and has a seven-byte active mask with all 56 bits used. Its maximum extent is
9,291 bytes. States are 4096 through 8191, table log is 12, and the payload
starts with the little-endian final-state delta followed by prepended LSB-first
bits. Empty entropy is the zero-mask descriptor and `00 00` payload.

Each nonempty frame resets the dictionary and entropy state. With token count
T and raw extent R no greater than F, decision count D satisfies
`2T <= D <= min(34T, 10R)`. Unknown identities, noncanonical models, invalid
state/counts, truncation, padding, or trailing bytes reject without publishing
a failed frame. No public factory, CLI selector, benchmark, or bundle entry is
admitted by this definition alone.

`tests/position_tans_16m_vectors.py` independently derives 234 descriptor and
payload vectors, including boundary lengths and distances through 16 MiB. Its
successful generation is evidence for this definition only; implementation and
public qualification remain incomplete.

The internal entropy and token implementations pass their dedicated core and
token tests. These check canonical descriptor rejection, terminal state and
padding, uniform-bank separation, overlap and budget rejection, discarded
private token prefixes, and retention of ordinary destinations on failure.
The C++ core also agrees with 236 independent oracle fixtures. The oracle
was corrected to emit bits from the pre-transition state. The inherited
eight-MiB oracle was separately corrected and passed 234 core fixtures. Frame, public API/CLI,
benchmark, fuzz and exchange completion remain unproven.

The bounded stream/frame-header validator is implemented. It verifies the
exact identity, fixed stream layout, known-size partition, sequence, descriptor
canonicality before payload buffering, and decision/payload ceilings. Tests
check the singleton literal header bytes, truncations, malformed fields,
ordinary-output retention, exact private-footprint admission and rejection one
byte below it. The 233,472 decode-table entries include the uniform bank.
Metadata-only ceiling tests supply explicit limits and allocate no full frame;
their limits do not establish public defaults or measured resource admission.
Frame entropy completion, raw reconstruction, streaming quarantine and public
connection remain separate unfinished gates.

The private 16 MiB frame layer is now implemented. It admits a planned token
sequence only after bounded validation, writes descriptor and payload bytes to
private scratch, and publishes the serialized frame only after every stage
succeeds. The decoder validates the complete descriptor before payload
buffering, decodes into private tokens, reconstructs into private raw storage,
and exposes raw bytes only while draining a validated frame. A truncated
frame, invalid history, or entropy failure therefore leaves ordinary frame and
raw destinations unchanged. The first frame and streaming split-boundary
tests pass; public API/CLI, measurements, fuzzing, exchange, and full-frame
corpus qualification remain unfinished.

The public C API now exposes the distinct `2/12 + 1/25 + 5/7` profile and
reports its bounded ownership and private-admission requirements before
allocation. The CLI selector is
`lzss-position-distance-tans-16m`. Fine-grained API processing, a two-frame
failure-publication regression, and a CLI encode/decode smoke round trip pass
with the updated implementation. Benchmark, fuzz, exchange, and corpus
qualification are still open.

The 16 MiB benchmark executable and smoke test are present. Its contextual
control is the published profile 16 configured to a 16 MiB window and frame;
the measurement harness uses an explicit 512 MiB compressed-payload admission
limit because the public workspace query measured 176,169,991 bytes for the
contextual encoded frame. The full twelve-member corpus comparison was
started but remains pending the process-idle audit; no partial corpus result is
treated as a qualification receipt.

The stream sanitizer harness is also present and was built with the updated
Clang toolchain. A 2,048-input bounded run completed without a crash, hang, or
sanitizer finding; it checks split versus whole input, terminal-state
stability, output-tail retention, and the failed-frame publication boundary.
This is a fuzz pilot receipt, not the full public fuzz campaign.
