#include <marc/marc.h>

#include "test_assert.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(marc_lzss_contextual_tans_config) == 112,
               "contextual tANS ABI-1 configuration extent changed");
_Static_assert(offsetof(marc_lzss_contextual_tans_config,
                        match_finder_strategy) == 12,
               "match-finder selector must reuse the ABI v1 reserved slot");

static marc_buffer allocate(size_t size) {
    marc_buffer result = {size == 0 ? NULL : (uint8_t*)malloc(size), size};
    assert(size == 0 || result.data != NULL);
    return result;
}

static void release(marc_buffer buffer) {
    free(buffer.data);
}

static void set_small_limits(marc_lzss_contextual_tans_config* config) {
    config->max_total_output_size = 1024;
    config->max_frame_size = 2;
    config->max_block_size = 4096;
    config->max_compressed_payload_size = 64;
    config->max_internal_buffered_bytes = UINT64_C(2) << 20;
    config->max_lz_distance = 65536;
    config->max_lz_match_length = 258;
    config->max_entropy_table_entries = UINT64_C(1) << 20;
}

static void expect_apply_failure(
    marc_lzss_contextual_tans_config config,
    marc_lzss_contextual_profile profile) {
    const marc_lzss_contextual_tans_config snapshot = config;
    assert(marc_lzss_contextual_tans_config_apply_profile(&config, profile)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&config, &snapshot, sizeof(config)) == 0);
}

static void test_apply_profile(void) {
    static const marc_lzss_contextual_profile profiles[] = {
        MARC_LZSS_CONTEXTUAL_PROFILE_64K,
        MARC_LZSS_CONTEXTUAL_PROFILE_1M,
        MARC_LZSS_CONTEXTUAL_PROFILE_4M,
        MARC_LZSS_CONTEXTUAL_PROFILE_16M};
    static const uint32_t extents[] = {
        UINT32_C(1) << 16, UINT32_C(1) << 20, UINT32_C(1) << 22,
        UINT32_C(1) << 24};
    static const uint64_t blocks[] = {
        UINT64_C(393216), UINT64_C(6291456), UINT64_C(29360128),
        UINT64_C(117440512)};
    static const uint64_t payloads[] = {
        UINT64_C(589826), UINT64_C(9437186), UINT64_C(44040194),
        UINT64_C(176160770)};
    static const uint64_t aggregates[] = {
        UINT64_C(8) << 20, UINT64_C(128) << 20, UINT64_C(128) << 20,
        UINT64_C(512) << 20};
    static const marc_direction directions[] = {
        MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE};

    for (size_t direction_index = 0;
         direction_index < sizeof(directions) / sizeof(directions[0]);
         ++direction_index) {
        marc_lzss_contextual_tans_config config;
        assert(marc_lzss_contextual_tans_config_init(
                   directions[direction_index], &config) == MARC_STATUS_OK);
        config.original_size = UINT64_C(1234567);
        config.max_total_output_size = UINT64_C(7654321);
        for (size_t index = 0;
             index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
            assert(marc_lzss_contextual_tans_config_apply_profile(
                       &config, profiles[index]) == MARC_STATUS_OK);
            assert(config.direction == directions[direction_index]);
            assert(config.original_size == UINT64_C(1234567));
            assert(config.max_total_output_size == UINT64_C(7654321));
            assert(config.frame_size == extents[index]);
            assert(config.window_size == extents[index]);
            assert(config.min_match_length == 5);
            assert(config.max_match_length == 258);
            assert(config.max_frame_size == extents[index]);
            assert(config.max_block_size == blocks[index]);
            assert(config.max_compressed_payload_size == payloads[index]);
            assert(config.max_internal_buffered_bytes == aggregates[index]);
            assert(config.max_lz_distance == extents[index]);
            assert(config.max_lz_match_length == 258);
            assert(config.max_entropy_table_entries == UINT64_C(131072));
            assert(config.profile == profiles[index]);
            marc_workspace_requirements requirements;
            const marc_status query_status =
                marc_lzss_contextual_tans_workspace_requirements(
                    &config, &requirements);
            assert(query_status
                   == (profiles[index] == MARC_LZSS_CONTEXTUAL_PROFILE_16M
                           ? MARC_STATUS_INVALID_ARGUMENT
                           : MARC_STATUS_OK));
            const marc_lzss_contextual_tans_config snapshot = config;
            assert(marc_lzss_contextual_tans_config_apply_profile(
                       &config, profiles[index]) == MARC_STATUS_OK);
            assert(memcmp(&config, &snapshot, sizeof(config)) == 0);
        }
    }

    marc_lzss_contextual_tans_config invalid;
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    assert(invalid.match_finder_strategy
           == MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT);
    invalid.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_OK);
    assert(invalid.match_finder_strategy
           == MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT);
    invalid.original_size = 4096;
    marc_workspace_requirements binary_tree_requirements;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &invalid, &binary_tree_requirements) == MARC_STATUS_OK);
    invalid.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
    marc_workspace_requirements hash_chain_requirements;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &invalid, &hash_chain_requirements) == MARC_STATUS_OK);
    assert(binary_tree_requirements.primary_bytes
           == hash_chain_requirements.primary_bytes);
    assert(binary_tree_requirements.secondary_bytes
           == hash_chain_requirements.secondary_bytes);
    assert(binary_tree_requirements.views_bytes != 0);
    assert(hash_chain_requirements.views_bytes != 0);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    --invalid.struct_size;
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    ++invalid.abi_version;
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    invalid.direction = (marc_direction)99;
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    invalid.match_finder_strategy = UINT32_C(255);
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    invalid.reserved2 = 1;
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &invalid) == MARC_STATUS_OK);
    expect_apply_failure(invalid, MARC_LZSS_CONTEXTUAL_PROFILE_64M);
    assert(marc_lzss_contextual_tans_config_apply_profile(
               NULL, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
}

static void test_sixteen_mib_public_boundary(void) {
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[40000];
    uint8_t baseline_encoded[40000];
    uint8_t decoded[sizeof(input)];
    marc_lzss_contextual_tans_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    config.max_lz_distance = UINT32_C(1) << 24;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(encoded[14] == 5 && encoded[15] == 0);
    assert(encoded[98] == 4 && encoded[99] == 0);
    const size_t encoded_size = result.output_produced;
    memcpy(baseline_encoded, encoded, encoded_size);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    config.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == encoded_size);
    assert(memcmp(encoded, baseline_encoded, encoded_size) == 0);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
    set_small_limits(&config);
    config.max_lz_distance = UINT32_C(1) << 24;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_4M)
           == MARC_STATUS_OK);
    set_small_limits(&config);
    config.max_lz_distance = UINT32_C(1) << 24;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, sizeof(decoded));
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0);
    for (size_t index = 0; index < sizeof(decoded); ++index) {
        assert(decoded[index] == 0xcc);
    }
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = UINT32_C(1) << 24;
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(462169094);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(462169095);
#endif
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    assert(needed.primary_bytes == UINT64_C(16777216));
    assert(needed.secondary_bytes == UINT64_C(176169991));
    assert(needed.views_bytes == UINT64_C(269221888));
#endif

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    assert(marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(394798086);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(394798087);
#endif
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    assert(needed.primary_bytes == UINT64_C(176169991));
    assert(needed.secondary_bytes == UINT64_C(16777216));
    assert(needed.views_bytes == UINT64_C(201850880));
#endif
}

int main(void) {
    test_apply_profile();
    test_sixteen_mib_public_boundary();
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[40000];
    uint8_t baseline_encoded[40000];
    uint8_t decoded[sizeof(input)];
    marc_lzss_contextual_tans_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.abi_version == MARC_ABI_VERSION);
    assert(config.frame_size == 65536);
    assert(config.window_size == 65536);
    assert(config.profile == MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    assert(config.min_match_length == 5);
    assert(config.max_match_length == 258);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes == 9113);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(result.output_produced > 112);
    assert(encoded[0] == 0x4d && encoded[1] == 0x41
           && encoded[2] == 0x52 && encoded[3] == 0x43);
    assert(encoded[4] == 2 && encoded[5] == 0);
    assert(encoded[14] == 2 && encoded[15] == 0);
    assert(encoded[16] == 5 && encoded[17] == 0);
    assert(encoded[18] == 2 && encoded[19] == 0);
    const size_t encoded_size = result.output_produced;
    memcpy(baseline_encoded, encoded, encoded_size);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    set_small_limits(&config);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 9113);
    assert(needed.secondary_bytes == 2);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == encoded_size);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_contextual_tans_create(
               &config,
               (marc_buffer){primary.data, needed.primary_bytes - 1},
               secondary, views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_tans_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary,
               (marc_buffer){views.data, needed.views_bytes - 1},
               &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    const size_t shared_size = needed.views_bytes > needed.primary_bytes
        ? needed.views_bytes : needed.primary_bytes;
    marc_buffer shared = allocate(shared_size);
    assert(marc_lzss_contextual_tans_create(
               &config,
               (marc_buffer){shared.data, needed.primary_bytes}, secondary,
               (marc_buffer){shared.data, needed.views_bytes}, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    release(shared);
    if (needed.views_alignment > 1) {
        marc_buffer storage = allocate(needed.views_bytes + 1);
        const marc_buffer misaligned = {
            storage.data + 1, needed.views_bytes};
        assert(marc_lzss_contextual_tans_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        assert(transform == NULL);
        release(storage);
    }
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, NULL)
           == MARC_STATUS_INVALID_ARGUMENT);

    config.match_finder_strategy = UINT32_C(255);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.match_finder_strategy = MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    --config.struct_size;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    ++config.struct_size;
    ++config.abi_version;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    --config.abi_version;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               NULL, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, NULL) == MARC_STATUS_INVALID_ARGUMENT);
    config.direction = (marc_direction)99;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_config_init(
               (marc_direction)0, &config)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, NULL) == MARC_STATUS_INVALID_ARGUMENT);

    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    config.window_size = UINT32_C(1) << 20;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes == 9177);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(encoded[14] == 3 && encoded[15] == 0);
    assert(encoded[84] == 0xc6 && encoded[85] == 0x11);
    assert(encoded[98] == 2 && encoded[99] == 0);
    const size_t extended_encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 9113);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, sizeof(decoded));
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, extended_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0);
    for (size_t index = 0; index < sizeof(decoded); ++index) {
        assert(decoded[index] == 0xcc);
    }
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 9177);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, extended_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, sizeof(decoded));
    result = marc_transform_process(
        transform, (marc_const_buffer){baseline_encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0);
    for (size_t index = 0; index < sizeof(decoded); ++index) {
        assert(decoded[index] == 0xcc);
    }
    marc_transform_destroy(transform);

    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    config.window_size = UINT32_C(1) << 22;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 22;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes == 9212);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(encoded[14] == 4 && encoded[15] == 0);
    assert(encoded[84] == 0xd6 && encoded[85] == 0x11);
    assert(encoded[98] == 3 && encoded[99] == 0);
    const size_t four_mib_encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 22;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, sizeof(decoded));
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, four_mib_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0);
    for (size_t index = 0; index < sizeof(decoded); ++index) {
        assert(decoded[index] == 0xcc);
    }
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 22;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 9212);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, four_mib_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = UINT32_C(1) << 22;
    config.frame_size = UINT32_C(1) << 22;
    config.window_size = UINT32_C(1) << 22;
    config.max_frame_size = UINT32_C(1) << 22;
    config.max_block_size = UINT64_C(7) << 22;
    config.max_lz_distance = UINT32_C(1) << 22;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(116138982);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(116138983);
#endif
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    assert(needed.primary_bytes == 4194304);
    assert(needed.secondary_bytes == 44049383);
    assert(needed.views_bytes == 67895296);
#endif

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    config.max_frame_size = UINT32_C(1) << 22;
    config.max_block_size = UINT64_C(7) << 22;
    config.max_lz_distance = UINT32_C(1) << 22;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    assert(needed.primary_bytes == 44049383);
    assert(needed.secondary_bytes == 4194304);
    assert(needed.views_bytes == 50855936);
    config.max_internal_buffered_bytes = UINT64_C(99099622);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(99099623);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
#endif

    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_64M;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);

    return 0;
}
