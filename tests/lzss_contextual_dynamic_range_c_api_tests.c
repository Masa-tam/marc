#include <marc/marc.h>

#include "test_assert.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(
    sizeof(marc_lzss_contextual_dynamic_range_config) == 120,
    "contextual Dynamic Range ABI-1 configuration extent changed");
_Static_assert(
    offsetof(marc_lzss_contextual_dynamic_range_config,
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

static void set_small_limits(
    marc_lzss_contextual_dynamic_range_config* config) {
    config->max_total_output_size = 1024;
    config->max_frame_size = 2;
    config->max_block_size = 2;
    config->max_compressed_payload_size = 64;
    config->max_internal_buffered_bytes = 8192;
    config->max_lz_distance = 65536;
    config->max_lz_match_length = 258;
    config->max_entropy_table_entries = UINT64_C(1) << 20;
    config->max_range_model_total = UINT64_C(1) << 24;
}

static void test_apply_profile(void) {
    static const marc_lzss_contextual_profile profiles[] = {
        MARC_LZSS_CONTEXTUAL_PROFILE_64K,
        MARC_LZSS_CONTEXTUAL_PROFILE_1M,
        MARC_LZSS_CONTEXTUAL_PROFILE_4M,
        MARC_LZSS_CONTEXTUAL_PROFILE_16M,
        MARC_LZSS_CONTEXTUAL_PROFILE_64M};
    static const uint32_t extents[] = {
        UINT32_C(1) << 16, UINT32_C(1) << 20, UINT32_C(1) << 22,
        UINT32_C(1) << 24, UINT32_C(1) << 26};
    static const uint64_t payloads[] = {
        UINT64_C(786437), UINT64_C(12582917), UINT64_C(58720261),
        UINT64_C(234881029), UINT64_C(1073741829)};
    static const uint64_t aggregates[] = {
        UINT64_C(8) << 20, UINT64_C(128) << 20, UINT64_C(256) << 20,
        UINT64_C(1) << 30, UINT64_C(8) << 30};
    static const uint64_t model_entries[] = {4518, 4550, 4566, 4582, 4598};
    static const marc_direction directions[] = {
        MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE};

    for (size_t direction_index = 0;
         direction_index < sizeof(directions) / sizeof(directions[0]);
         ++direction_index) {
        marc_lzss_contextual_dynamic_range_config config;
        assert(marc_lzss_contextual_dynamic_range_config_init(
                   directions[direction_index], &config) == MARC_STATUS_OK);
        assert(config.match_finder_strategy
               == MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT);
        config.match_finder_strategy =
            MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
        config.original_size = UINT64_C(1234567);
        config.max_total_output_size = UINT64_C(7654321);
        for (size_t index = 0;
             index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
            assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
                       &config, profiles[index]) == MARC_STATUS_OK);
            assert(config.direction == directions[direction_index]);
            assert(config.match_finder_strategy
                   == MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT);
            assert(config.original_size == UINT64_C(1234567));
            assert(config.max_total_output_size == UINT64_C(7654321));
            assert(config.frame_size == extents[index]);
            assert(config.window_size == extents[index]);
            assert(config.min_match_length == 5);
            assert(config.max_match_length == 258);
            assert(config.max_frame_size == extents[index]);
            assert(config.max_block_size == extents[index]);
            assert(config.max_compressed_payload_size == payloads[index]);
            assert(config.max_internal_buffered_bytes == aggregates[index]);
            assert(config.max_lz_distance == extents[index]);
            assert(config.max_lz_match_length == 258);
            assert(config.max_entropy_table_entries == model_entries[index]);
            assert(config.max_range_model_total == (UINT64_C(1) << 15));
            assert(config.profile == profiles[index]);
            const marc_lzss_contextual_dynamic_range_config snapshot = config;
            assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
                       &config, profiles[index]) == MARC_STATUS_OK);
            assert(memcmp(&config, &snapshot, sizeof(config)) == 0);
        }
    }

    marc_lzss_contextual_dynamic_range_config config;
    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    marc_lzss_contextual_dynamic_range_config invalid = config;
    --invalid.struct_size;
    marc_lzss_contextual_dynamic_range_config snapshot = invalid;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&invalid, &snapshot, sizeof(invalid)) == 0);
    invalid = config;
    ++invalid.abi_version;
    snapshot = invalid;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&invalid, &snapshot, sizeof(invalid)) == 0);
    invalid = config;
    invalid.direction = (marc_direction)99;
    snapshot = invalid;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&invalid, &snapshot, sizeof(invalid)) == 0);
    invalid = config;
    invalid.match_finder_strategy = UINT32_C(255);
    snapshot = invalid;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&invalid, &snapshot, sizeof(invalid)) == 0);
    invalid = config;
    invalid.reserved2 = 1;
    snapshot = invalid;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &invalid, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&invalid, &snapshot, sizeof(invalid)) == 0);
    snapshot = config;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &config, (marc_lzss_contextual_profile)5)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(memcmp(&config, &snapshot, sizeof(config)) == 0);
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               NULL, MARC_LZSS_CONTEXTUAL_PROFILE_1M)
           == MARC_STATUS_INVALID_ARGUMENT);
}

int main(void) {
    test_apply_profile();
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[1024];
    uint8_t hash_chain_encoded[1024];
    uint8_t decoded[sizeof(input)];
    marc_lzss_contextual_dynamic_range_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.abi_version == MARC_ABI_VERSION);
    assert(config.frame_size == 65536);
    assert(config.window_size == 65536);
    assert(config.profile == MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    assert(config.min_match_length == 5);
    assert(config.max_match_length == 258);
    config.original_size = UINT32_C(1) << 16;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    const size_t default_hash_chain_views_bytes = needed.views_bytes;
    config.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.views_bytes != default_hash_chain_views_bytes);
    config.match_finder_strategy = MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes == 109);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
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
    const size_t encoded_size = result.output_produced;
    memcpy(hash_chain_encoded, encoded, encoded_size);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == encoded_size);
    assert(memcmp(encoded, hash_chain_encoded, encoded_size) == 0);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    set_small_limits(&config);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 144);
    assert(needed.secondary_bytes == 2);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    const marc_workspace_requirements hash_chain_decode_needed = needed;
    config.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(memcmp(&needed, &hash_chain_decode_needed, sizeof(needed)) == 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
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

    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    config.window_size = UINT32_C(1) << 20;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    config.max_entropy_table_entries = 4549;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_entropy_table_entries = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(encoded[14] == 3 && encoded[15] == 0);
    assert(encoded[98] == 2 && encoded[99] == 0);
    const size_t extended_encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, extended_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    set_small_limits(&config);
    config.max_lz_distance = UINT64_C(1) << 20;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, extended_encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_contextual_dynamic_range_create(
               &config,
               (marc_buffer){primary.data, needed.primary_bytes - 1},
               secondary, views, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               views, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary,
               (marc_buffer){views.data, needed.views_bytes - 1},
               &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary,
               (marc_buffer){primary.data, needed.views_bytes},
               &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    if (needed.views_alignment > 1) {
        marc_buffer storage = allocate(needed.views_bytes + 1);
        const marc_buffer misaligned = {
            storage.data + 1, needed.views_bytes};
        assert(marc_lzss_contextual_dynamic_range_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        assert(transform == NULL);
        release(storage);
    }
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, primary, secondary, views, NULL)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.original_size = UINT32_C(1) << 22;
    config.frame_size = UINT32_C(1) << 22;
    config.window_size = UINT32_C(1) << 22;
    config.max_block_size = UINT32_C(1) << 22;
    config.max_lz_distance = UINT32_C(1) << 22;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(264765524);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(264765525);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 4194304);
    assert(needed.secondary_bytes == 58720341);
    assert(needed.views_bytes == 201850880);
#endif

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.max_block_size = UINT32_C(1) << 22;
    config.max_lz_distance = UINT32_C(1) << 22;
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_4M;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    assert(needed.primary_bytes == 67108944);
    assert(needed.secondary_bytes == 4194304);
    assert(needed.views_bytes == 50331648);
#endif

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.original_size = UINT32_C(1) << 24;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(1057488980);
    needed.primary_bytes = 1;
    needed.secondary_bytes = 1;
    needed.views_bytes = 1;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    assert(needed.primary_bytes == 0);
    assert(needed.secondary_bytes == 0);
    assert(needed.views_bytes == 0);
    config.max_internal_buffered_bytes = UINT64_C(1057488981);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 16777216);
    assert(needed.secondary_bytes == 234881109);
    assert(needed.views_bytes == 805830656);
#endif

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(452984916);
    needed.primary_bytes = 1;
    needed.secondary_bytes = 1;
    needed.views_bytes = 1;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    assert(needed.primary_bytes == 0);
    assert(needed.secondary_bytes == 0);
    assert(needed.views_bytes == 0);
    config.max_internal_buffered_bytes = UINT64_C(452984917);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 234881109);
    assert(needed.secondary_bytes == 16777216);
    assert(needed.views_bytes == 201326592);
#endif

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.original_size = UINT32_C(1) << 26;
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_64M)
           == MARC_STATUS_OK);
#if SIZE_MAX > UINT32_MAX
    config.max_internal_buffered_bytes = UINT64_C(4362600532);
    needed.primary_bytes = 1;
    needed.secondary_bytes = 1;
    needed.views_bytes = 1;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    assert(needed.primary_bytes == 0);
    assert(needed.secondary_bytes == 0);
    assert(needed.views_bytes == 0);
    config.max_internal_buffered_bytes = UINT64_C(4362600533);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == UINT64_C(67108864));
    assert(needed.secondary_bytes == UINT64_C(1073741909));
    assert(needed.views_bytes == UINT64_C(3221749760));

    config.match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
    config.max_internal_buffered_bytes = UINT64_C(6039797844);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(6039797845);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == UINT64_C(67108864));
    assert(needed.secondary_bytes == UINT64_C(1073741909));
    assert(needed.views_bytes == UINT64_C(4898947072));

    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    assert(marc_lzss_contextual_dynamic_range_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_64M)
           == MARC_STATUS_OK);
    config.max_internal_buffered_bytes = UINT64_C(1946157140);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_LIMIT_EXCEEDED);
    config.max_internal_buffered_bytes = UINT64_C(1946157141);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == UINT64_C(1073741909));
    assert(needed.secondary_bytes == UINT64_C(67108864));
    assert(needed.views_bytes == UINT64_C(805306368));
#endif

    config.profile = UINT32_C(5);
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    config.profile = MARC_LZSS_CONTEXTUAL_PROFILE_1M;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    config.match_finder_strategy = UINT32_C(255);
    needed.primary_bytes = 1;
    needed.secondary_bytes = 1;
    needed.views_bytes = 1;
    assert(marc_lzss_contextual_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(needed.primary_bytes == 0 && needed.secondary_bytes == 0
           && needed.views_bytes == 0);
    transform = (marc_transform*)(uintptr_t)1;
    assert(marc_lzss_contextual_dynamic_range_create(
               &config, (marc_buffer){0}, (marc_buffer){0},
               (marc_buffer){0}, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_dynamic_range_config_init(0, &config)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, NULL)
           == MARC_STATUS_INVALID_ARGUMENT);

    release(primary);
    release(secondary);
    release(views);
    return 0;
}
