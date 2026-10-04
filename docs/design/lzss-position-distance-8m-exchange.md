# Eight-MiB position-distance exchange admission

DD-1473 extends the repository-owned exchange inventory after the separately
qualified DD-1472 command-line connection. This changes bundle metadata and
qualification scripts; it changes no codec grammar, public ABI or default.

The producer emits schema 61 and codec set marc-cli-v61 with exactly 71 archives.
The ordered first 70 entries are the frozen schema-60 inventory. Entry 71 is
lzss-position-distance-dynamic-range-8m. The independently defined 8193-byte
fixture recipe remains unchanged. The stable first 42 profiles remain unchanged.
The verifier retains schemas 1 through 60 and their exact historical lists.
A real schema-60 downgrade removes the new entry; relabeling all 71 entries as
schema 60 is invalid.

The eight-MiB archive retains Format 2.0, dictionary 2/11, context 1/12 and
entropy 3/2. Little-endian 16-bit header checks are exactly:

| Offset | Value |
| --- | --- |
| 4 | 2 |
| 6 | 0 |
| 12 | 2 |
| 14 | 11 |
| 16 | 3 |
| 18 | 2 |
| 96 | 1 |
| 98 | 12 |

Both bytes of each field must match. Read the bounded 112-byte header before
launching the codec. Workspace ownership, admission budgets and platform labels
are not serialized codec fields.

The verifier first admits the complete manifest: schema, codec set, revision,
input size/hash, exact count and order, uniqueness, leaf paths, archive size/hash
and the existing four-MiB and new eight-MiB exact identities. Only after this
pass does it decode and independently compare raw fixture bytes, then re-encode
and compare complete archive bytes. A late invalid manifest entry therefore
launches no codec and creates no decoded or re-encoded output. The output
directory may already have been created. Codec payload failures still use the
unchanged CLI file transaction and failed-frame nonpublication contract.

The registered schema compatibility test now generates schema 61, verifies it,
runs 27 directed rejection cases and converts to schema 60 before continuing
all historical schema tests. Cases include relabeled downgrade, wrong codec
set, missing entry, duplicate, order, archive hash/size, input hash, crossed
dictionary/context identities, rehashed truncated header and all 16 identity
bytes individually changed and rehashed. Each case requires its expected error
category and an empty output directory. Historical identity/order negatives
remain intact.

Local qualification uses actual source-bound tools and records script, CLI and
library hashes. Three local producer/verifier routes must produce identical
71-entry archive metadata and bytes. An unchanged prior producer must reproduce
the frozen 70-entry prefix. These checks are local interoperability qualification;
they do not establish hosted CI or external operating-system exchange.

Revision-specific external admission requires successful hosted CI and four
reported roles: the two hosted producer bundles consumed on the external
consumer, then one independently produced external bundle consumed there and on
the other consumer. Two reports for that last producer are two consumer roles,
not another producer. Earlier 70-archive receipts do not qualify schema 61.
The small fixture establishes inventory, identity, determinism and exchange;
DD-1469 through DD-1472 retain separate maximum-window and failure evidence.

No external implementation source was consulted. No timing, new fuzz campaign,
universal memory-fit assertion or release similarity review is introduced.
