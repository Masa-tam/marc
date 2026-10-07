# Private full-literal position-distance rANS, 4MiB

## DD-1532 exact alternative format

This private candidate preserves DD-1530 unchanged and uses a distinct tuple:
dictionary2/10, context1/16, entropy4/7. It retains the fixed-five dictionary
encoder, grammar lengths3..258, 4194304-byte window/frame and all DD-1530
scalar rANS state, frequency normalization, byte ordering, count bounds,
known-size framing and failed-frame quarantine rules.

The literal model has17 contexts: initial literal context3, subsequent
literal context4+(previous literal >>4). Matches preserve literal history.
Kind contexts0..2 are unchanged. Length contexts20..22 select by preceding
kind; the nine-class short-length escape is unchanged. Distance-class
contexts23..31 select by length class and have alphabet23. Distance extra
bits p use context32+p for p=0..21. Length extra bits remain uniform binary.
Thus there are54 contexts,4636 frequencies and alphabet sequence
[2]*3+[256]*17+[9]*3+[23]*9+[2]*22. Offsets[32]=4592; offsets[54]=4636.

Descriptor metadata remains decisions u32,payload u32,table_log u8=12,
flags u8=0,contexts u16=54,frequency entries u32=4636. A SEVEN-byte active
mask occupies16..22. Bits0..53 name active contexts; bits6/7 of byte22 are
reserved zero. Records begin at23. Existing single/dense/sparse canonical
record rules and total4096 are unchanged. Descriptor extent23..9241:
23+2*4636-54. Empty isolated entropy data uses23-byte descriptor and8-byte
payload. No reinterpretation of DD-1530's six-byte mask is permitted.

Stream header112 bytes: replace entropy variant at18 with7, context variant
at98 with16, context count at82 with54 and frequency entries at84 with4636.
Every other field follows DD-1530. Frame64 layout is unchanged except
minimum/maximum descriptor extent23/9241. Serialized worst-case workspace
is18F+9313; decisions<=min(33T,9F), payload<=18F+8 remain unchanged. Reserve128KiB for fixed working state, conservatively covering the larger
model, descriptor and nested planning/validation temporaries. All count,
size and aggregate bounds are checked before buffering or allocation.

This is a diagnostic alternative, not public CLI/C ABI/exchange admission.
The name lzss-position-distance-rans-4m remains a proposed public selector.
Compare exact archives with the retained reduced-literal candidate and
contextual rANS4MiB on every corpus member; determine actual sizes before
choosing the representation. Arithmetic/model changes are not proposed.
