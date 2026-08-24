# Contextual profile application

Status: accepted design for staged implementation.

This design standardizes the opt-in configuration helpers used by the five
LZSS contextual codec families. It changes public configuration names during
the pre-release period, but it does not change any decoder-visible stream
representation.

## Scope

The following families have multiple named resource profiles and therefore
expose a profile-application helper:

- LZSS Contextual Dynamic Range;
- LZSS Contextual rANS;
- LZSS Contextual tANS;
- LZSS Contextual Blocked Huffman;
- LZSS Contextual Adaptive Huffman.

Other codec configurations do not gain a ceremonial single-profile helper.
For those codecs, `marc_<codec>_config_init()` returns a complete usable default
configuration. Callers may override individual documented fields before the
workspace query.

## Public naming

The common selector is named for the complete contextual resource profile,
not only its dictionary window:

```c
typedef uint32_t marc_lzss_contextual_profile;

#define MARC_LZSS_CONTEXTUAL_PROFILE_64K ((marc_lzss_contextual_profile)0u)
#define MARC_LZSS_CONTEXTUAL_PROFILE_1M  ((marc_lzss_contextual_profile)1u)
#define MARC_LZSS_CONTEXTUAL_PROFILE_4M  ((marc_lzss_contextual_profile)2u)
#define MARC_LZSS_CONTEXTUAL_PROFILE_16M ((marc_lzss_contextual_profile)3u)
```

The selector namespace is shared, but support is family-specific. A helper
accepts only the profiles documented for that codec and leaves the
configuration unchanged when given another known or unknown selector value.

Each profile-bearing configuration stores the selector in `profile`, and each
family exposes this shape:

```c
marc_status
marc_<codec>_config_apply_profile(
    marc_<codec>_config* config,
    marc_lzss_contextual_profile profile);
```

The former `window_profile`, `MARC_LZSS_CONTEXTUAL_WINDOW_*`, and Adaptive
Huffman `config_apply_window_profile` names are replaced without compatibility
aliases. Their ABI widths, integer selector values, initializer defaults, and
stream identities remain unchanged.

## Atomic helper contract

Every profile helper follows the same contract:

1. Validate the pointer, the full size-tagged ABI shell, and the profile value
   before changing any byte of the configuration.
2. On failure, leave the configuration byte-for-byte unchanged.
3. Preserve caller-specific fields: `direction`, `original_size`,
   `max_total_output_size`, ABI size/version fields, and reserved fields.
4. Set the profile-owned frame, window, match, entropy-block, payload, model,
   aggregate, and LZ hard limits as applicable to that family. A backend may
   additionally set its own model limit, such as the Dynamic Range total.
5. Perform no allocation. Build a private candidate and publish it only after
   all checked calculations and validation succeed.
6. Reapplying the same profile is idempotent.
7. Allow callers to make hard limits stricter after successful application.
8. Never infer or enlarge a profile from encoded stream data.

The helper expresses one coherent supported resource envelope. It is not a
substitute for the workspace query and does not make arbitrary combinations of
the profile-owned fields valid.

## Workspace contract

After initialization, profile application, or any caller override, the caller
must run the matching workspace-requirements query. The query is authoritative:
it returns the actual required primary, secondary, and views extents for the
selected direction and configuration.

For each non-zero extent, a buffer one byte smaller than the reported value
must be rejected with `MARC_STATUS_LIMIT_EXCEEDED` before a handle is published.
The query and create path must use the same checked layout calculation rather
than independent estimates.

## Single source of policy

Each family owns one private profile table or equivalent function used by its
initializer, public helper, workspace validation, command-line tool, benchmark,
and tests. The command-line tool and benchmark must not duplicate numeric
profile limits.

The public ABI remains strongly typed per codec. A generic exported `void*`
helper is not introduced merely to reduce implementation repetition.

## Validation matrix

The implementation must cover all five families, every profile supported by
each family, and both directions. Tests must prove:

- every profile-owned value and every preserved caller value;
- byte-atomic failure for null, short, wrong-version, invalid-direction,
  non-zero-reserved, and unknown-profile inputs;
- idempotent reapplication;
- stricter caller hard-limit overrides;
- exact workspace extents and one-byte-short rejection for each used region;
- command-line and benchmark configurations equal the public-helper result;
- unchanged stream bytes, algorithm identities, ABI extents, and
  interoperability-schema entries.

## Staged implementation

1. Establish this contract and its provenance records.
2. Normalize the common selector, configuration field, and existing Adaptive
   Huffman helper names.
3. Add Dynamic Range and rANS helpers.
4. Add tANS and Blocked Huffman helpers.
5. Migrate the command-line and benchmark policy to the helpers and remove
   duplicated constants.
6. Document the no-helper `config_init()` rule in examples, complete the full
   test matrix, and verify frozen-schema compatibility.
