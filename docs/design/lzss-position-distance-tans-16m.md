# Sixteen-MiB position-distance tANS

## DD-1603: exact sixteen-MiB specialization

This specialization uses the native position-distance tANS family with
`F = 16,777,216` and `N = 24`. Its distinct identity is dictionary `2/12`,
context `1/25`, and entropy `5/7`. The configured dictionary window and
maximum frame are both 16,777,216 bytes; grammar lengths remain 3 through
258. The encoder, when admitted, uses fixed-five eligibility, exact longest
matching, and nearest-distance tie breaking.

There are 56 contexts and 4,658 frequencies. The alphabet sequence is
`[2]*3 + [256]*17 + [9]*3 + [25]*9 + [2]*24`. The descriptor follows the
family 24-byte prefix and has an eight-byte active mask. Its maximum extent is
9,291 bytes. States are 4096 through 8191, table log is 12, and the payload
starts with the little-endian final-state delta followed by prepended LSB-first
bits. Empty entropy is the zero-mask descriptor and `00 00` payload.

Each nonempty frame resets the dictionary and entropy state. With token count
T and raw extent R no greater than F, decision count D satisfies
`2T <= D <= min(34T, 10R)`. Unknown identities, noncanonical models, invalid
state/counts, truncation, padding, or trailing bytes reject without publishing
a failed frame. No public factory, CLI selector, benchmark, or bundle entry is
admitted by this definition alone.

`tests/position_tans_16m_vectors.py` independently derives 232 descriptor and
payload vectors, including boundary lengths and distances through 16 MiB. Its
successful generation is evidence for this definition only; implementation and
public qualification remain incomplete.