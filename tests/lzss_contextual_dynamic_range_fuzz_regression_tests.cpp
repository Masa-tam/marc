#include <marc/marc.h>

#include "dictionary/lzss_typed_token.hpp"
#include "context/lzss_field_context_format.hpp"
#include "frame/lzss_typed_context_frame_decoder.hpp"
#include "frame/typed_context_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using Token = marc::dictionary::internal::LzssTypedToken;

constexpr std::array raw{
    std::uint8_t{'A'}, std::uint8_t{'B'}, std::uint8_t{'A'},
    std::uint8_t{'B'}, std::uint8_t{'X'}};
constexpr std::size_t maximum_payload = raw.size() * 14 + 5;
constexpr std::size_t maximum_encoded_frame = 64 + 16 + maximum_payload;
constexpr std::size_t maximum_internal = 8192;

struct Workspace {
    marc_workspace_requirements requirements{};
    std::vector<std::uint8_t> primary;
    std::vector<std::uint8_t> secondary;
    std::vector<std::max_align_t> views;
    std::size_t views_bytes{};

    [[nodiscard]] marc_buffer views_buffer() noexcept {
        return {reinterpret_cast<std::uint8_t*>(views.data()), views_bytes};
    }
};

[[nodiscard]] marc_lzss_contextual_dynamic_range_config settings(
    const marc_direction direction,
    const marc_lzss_contextual_profile profile =
        MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
    marc_lzss_contextual_dynamic_range_config result{};
    EXPECT_EQ(marc_lzss_contextual_dynamic_range_config_init(
                  direction, &result),
              MARC_STATUS_OK);
    result.original_size = raw.size();
    result.frame_size = raw.size();
    result.window_size = profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
        ? UINT32_C(1) << 24
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
            ? UINT32_C(1) << 22
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
            ? UINT32_C(1) << 20 : UINT32_C(1) << 16;
    result.max_total_output_size = 32;
    result.max_frame_size = raw.size();
    result.max_block_size = raw.size();
    result.max_compressed_payload_size = maximum_payload;
    result.max_internal_buffered_bytes = maximum_internal;
    result.max_lz_distance = UINT64_C(1) << 24;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v4;
    result.max_range_model_total = UINT64_C(1) << 24;
    result.profile = profile;
    return result;
}

[[nodiscard]] Workspace workspace_for(
    const marc_lzss_contextual_dynamic_range_config& config) {
    Workspace result{};
    EXPECT_EQ(marc_lzss_contextual_dynamic_range_workspace_requirements(
                  &config, &result.requirements),
              MARC_STATUS_OK);
    EXPECT_LE(result.requirements.views_alignment,
              alignof(std::max_align_t));
    result.primary.resize(result.requirements.primary_bytes);
    result.secondary.resize(result.requirements.secondary_bytes);
    result.views_bytes = result.requirements.views_bytes;
    result.views.resize(
        (result.views_bytes + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t));
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> canonical_stream(
    const marc_lzss_contextual_profile profile =
        MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
    auto config = settings(MARC_DIRECTION_ENCODE, profile);
    auto workspace = workspace_for(config);
    marc_transform* encoder{};
    EXPECT_EQ(marc_lzss_contextual_dynamic_range_create(
                  &config,
                  {workspace.primary.data(), workspace.primary.size()},
                  {workspace.secondary.data(), workspace.secondary.size()},
                  workspace.views_buffer(), &encoder),
              MARC_STATUS_OK);
    std::vector<std::uint8_t> encoded(
        marc::frame::internal::typed_context_stream_header_size
        + maximum_encoded_frame);
    const auto result = marc_transform_process(
        encoder, {raw.data(), raw.size()}, {encoded.data(), encoded.size()},
        MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status, MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(encoder);
    encoded.resize(result.output_produced);
    return encoded;
}

void expect_private_atomic_failure(
    const std::span<const std::uint8_t> input) {
    auto config = settings(MARC_DIRECTION_DECODE);
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = config.max_total_output_size;
    limits.max_frame_size = config.max_frame_size;
    limits.max_block_size = config.max_block_size;
    limits.max_compressed_payload_size =
        config.max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config.max_internal_buffered_bytes;
    limits.max_lz_distance = config.max_lz_distance;
    limits.max_lz_match_length = config.max_lz_match_length;
    limits.max_entropy_table_entries = config.max_entropy_table_entries;
    limits.max_range_model_total = config.max_range_model_total;
    const auto bytes = std::as_bytes(input);
    marc::frame::internal::TypedContextStreamHeader stream{};
    std::size_t consumed{};
    if (marc::frame::internal::parse_typed_context_stream_header(
            bytes, limits, stream, consumed)
        != marc::frame::internal::TypedContextStreamHeaderError::none) {
        return;
    }
    std::array<Token, raw.size()> tokens{};
    std::array<std::byte, raw.size()> private_raw{};
    private_raw.fill(std::byte{0xa5});
    const marc::frame::internal::TypedContextFrameValidationContext context{
        stream, limits, 0, 0};
    const auto result =
        marc::frame::internal::decode_lzss_typed_context_frame(
            bytes.subspan(consumed), context, tokens, private_raw);
    EXPECT_NE(result.error,
              marc::frame::internal::LzssTypedContextFrameDecodeError::none);
    EXPECT_TRUE(std::ranges::all_of(
        private_raw, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

void expect_public_atomic_failure(
    const std::span<const std::uint8_t> input,
    const marc_lzss_contextual_profile profile =
        MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
    auto config = settings(MARC_DIRECTION_DECODE, profile);
    auto workspace = workspace_for(config);
    marc_transform* decoder{};
    ASSERT_EQ(marc_lzss_contextual_dynamic_range_create(
                  &config,
                  {workspace.primary.data(), workspace.primary.size()},
                  {workspace.secondary.data(), workspace.secondary.size()},
                  workspace.views_buffer(), &decoder),
              MARC_STATUS_OK);
    std::array<std::uint8_t, raw.size()> output{};
    output.fill(UINT8_C(0xa5));
    const auto result = marc_transform_process(
        decoder, {input.data(), input.size()}, {output.data(), output.size()},
        MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status, MARC_STATUS_MALFORMED_STREAM);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::uint8_t value) {
            return value == UINT8_C(0xa5);
        }));
    const auto repeated = marc_transform_process(
        decoder, {nullptr, 0}, {nullptr, 0}, MARC_PROCESS_NONE);
    EXPECT_EQ(repeated.status, result.status);
    EXPECT_EQ(repeated.error_byte_position, result.error_byte_position);
    EXPECT_EQ(repeated.error_bit_position, result.error_bit_position);
    EXPECT_EQ(repeated.input_consumed, 0U);
    EXPECT_EQ(repeated.output_produced, 0U);
    marc_transform_destroy(decoder);
}

void expect_dual_atomic_failure(
    const std::span<const std::uint8_t> input,
    const marc_lzss_contextual_profile profile =
        MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
    expect_private_atomic_failure(input);
    expect_public_atomic_failure(input, profile);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size));
    }
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     EveryExtendedCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size),
            MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    }
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     EveryFourMiBCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size),
            MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    }
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     EverySixteenMiBCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size),
            MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    }
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     CrossProfilePublicDecodersRejectAtomically) {
    const auto frozen = canonical_stream();
    expect_public_atomic_failure(
        frozen, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    const auto extended = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    expect_public_atomic_failure(
        extended, MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    const auto four_mib = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    expect_public_atomic_failure(
        frozen, MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    expect_public_atomic_failure(
        extended, MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    expect_public_atomic_failure(
        four_mib, MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    expect_public_atomic_failure(
        four_mib, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    const auto sixteen_mib =
        canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    expect_public_atomic_failure(
        frozen, MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    expect_public_atomic_failure(
        extended, MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    expect_public_atomic_failure(
        four_mib, MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    expect_public_atomic_failure(
        sixteen_mib, MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    expect_public_atomic_failure(
        sixteen_mib, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    expect_public_atomic_failure(
        sixteen_mib, MARC_LZSS_CONTEXTUAL_PROFILE_4M);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream();
    constexpr auto frame_offset =
        marc::frame::internal::typed_context_stream_header_size;
    std::fill(encoded.begin() + frame_offset + 16,
              encoded.begin() + frame_offset + 48,
              UINT8_C(0xff));
    expect_dual_atomic_failure(encoded);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     NonzeroDescriptorReservedByteIsAtomic) {
    auto encoded = canonical_stream();
    constexpr auto descriptor_reserved =
        marc::frame::internal::typed_context_stream_header_size
        + marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size - 1;
    encoded[descriptor_reserved] = 1;
    expect_dual_atomic_failure(encoded);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     ExtendedNonzeroDescriptorReservedByteIsAtomic) {
    auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    constexpr auto descriptor_reserved =
        marc::frame::internal::typed_context_stream_header_size
        + marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size - 1;
    encoded[descriptor_reserved] = 1;
    expect_dual_atomic_failure(
        encoded, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     FourMiBNonzeroDescriptorReservedByteIsAtomic) {
    auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    constexpr auto descriptor_reserved =
        marc::frame::internal::typed_context_stream_header_size
        + marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size - 1;
    encoded[descriptor_reserved] = 1;
    expect_dual_atomic_failure(
        encoded, MARC_LZSS_CONTEXTUAL_PROFILE_4M);
}

TEST(LzssContextualDynamicRangeFuzzRegression,
     SixteenMiBNonzeroDescriptorReservedByteIsAtomic) {
    auto encoded = canonical_stream(MARC_LZSS_CONTEXTUAL_PROFILE_16M);
    constexpr auto descriptor_reserved =
        marc::frame::internal::typed_context_stream_header_size
        + marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size - 1;
    encoded[descriptor_reserved] = 1;
    expect_dual_atomic_failure(
        encoded, MARC_LZSS_CONTEXTUAL_PROFILE_16M);
}

} // namespace
