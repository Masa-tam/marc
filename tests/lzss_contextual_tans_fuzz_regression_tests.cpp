#include <marc/marc.h>

#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_tans_decode_tables.hpp"
#include "entropy/contextual_tans_format.hpp"
#include "frame/lzss_contextual_tans_frame_decoder.hpp"
#include "frame/lzss_contextual_tans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using TansDecodeEntry = marc::entropy::internal::TansDecodeEntry;
using Token = marc::dictionary::internal::LzssTypedToken;

constexpr std::array raw{
    std::uint8_t{'A'}, std::uint8_t{'B'}, std::uint8_t{'A'},
    std::uint8_t{'B'}, std::uint8_t{'X'}};
constexpr std::size_t maximum_decisions = raw.size() * 6;
constexpr std::size_t maximum_payload = raw.size() * 9 + 2;
constexpr std::size_t maximum_internal = 2U << 20;

[[nodiscard]] constexpr std::size_t maximum_encoded_frame(
    const marc_lzss_contextual_window_profile window_profile) noexcept {
    const auto descriptor_size = window_profile
            == MARC_LZSS_CONTEXTUAL_WINDOW_1M
        ? marc::entropy::internal::contextual_tans_max_descriptor_size_v2
        : marc::entropy::internal::contextual_tans_max_descriptor_size_v1;
    return marc::frame::internal::lzss_contextual_tans_frame_header_size
        + descriptor_size + maximum_payload;
}

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

[[nodiscard]] marc_lzss_contextual_tans_config settings(
    const marc_direction direction,
    const marc_lzss_contextual_window_profile window_profile =
        MARC_LZSS_CONTEXTUAL_WINDOW_64K) {
    marc_lzss_contextual_tans_config result{};
    EXPECT_EQ(marc_lzss_contextual_tans_config_init(direction, &result),
              MARC_STATUS_OK);
    result.original_size = raw.size();
    result.frame_size = raw.size();
    result.window_size = window_profile == MARC_LZSS_CONTEXTUAL_WINDOW_1M
        ? UINT32_C(1) << 20 : UINT32_C(1) << 16;
    result.max_total_output_size = 32;
    result.max_frame_size = raw.size();
    result.max_block_size = maximum_decisions;
    result.max_compressed_payload_size = maximum_payload;
    result.max_internal_buffered_bytes = maximum_internal;
    result.max_lz_distance = UINT64_C(1) << 20;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries =
        marc::entropy::internal::contextual_tans_decode_table_entries;
    result.window_profile = window_profile;
    return result;
}

[[nodiscard]] Workspace workspace_for(
    const marc_lzss_contextual_tans_config& config) {
    Workspace result{};
    EXPECT_EQ(marc_lzss_contextual_tans_workspace_requirements(
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
    const marc_lzss_contextual_window_profile window_profile) {
    auto config = settings(MARC_DIRECTION_ENCODE, window_profile);
    auto workspace = workspace_for(config);
    marc_transform* encoder{};
    EXPECT_EQ(marc_lzss_contextual_tans_create(
                  &config,
                  {workspace.primary.data(), workspace.primary.size()},
                  {workspace.secondary.data(), workspace.secondary.size()},
                  workspace.views_buffer(), &encoder),
              MARC_STATUS_OK);
    std::vector<std::uint8_t> encoded(
        marc::frame::internal::lzss_contextual_tans_stream_header_size
        + maximum_encoded_frame(window_profile));
    const auto result = marc_transform_process(
        encoder, {raw.data(), raw.size()}, {encoded.data(), encoded.size()},
        MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status, MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(encoder);
    encoded.resize(result.output_produced);
    return encoded;
}

class LzssContextualTansFuzzRegression
    : public testing::TestWithParam<
          marc_lzss_contextual_window_profile> {
protected:
    LzssContextualTansFuzzRegression()
        : decode_config_(settings(MARC_DIRECTION_DECODE, GetParam())),
          public_workspace_(workspace_for(decode_config_)),
          tables_(marc::entropy::internal::
                      contextual_tans_decode_table_entries) {}

    void expect_private_atomic_failure(
        const std::span<const std::uint8_t> input) {
        marc::core::DecoderLimits limits{};
        limits.max_total_output_size = 32;
        limits.max_frame_size = raw.size();
        limits.max_block_size = maximum_decisions;
        limits.max_compressed_payload_size = maximum_payload;
        limits.max_internal_buffered_bytes = maximum_internal;
        limits.max_lz_distance = UINT64_C(1) << 20;
        limits.max_lz_match_length = 258;
        limits.max_entropy_table_entries =
            marc::entropy::internal::contextual_tans_decode_table_entries;
        const auto bytes = std::as_bytes(input);
        marc::frame::internal::LzssContextualTansStreamHeader stream{};
        std::size_t consumed{};
        if (marc::frame::internal::parse_lzss_contextual_tans_stream_header(
                bytes, limits, stream, consumed)
            != marc::frame::internal::
                   LzssContextualTansStreamHeaderError::none) {
            return;
        }
        private_raw_.fill(std::byte{0xa5});
        const marc::frame::internal::
            LzssContextualTansFrameValidationContext context{
                stream, limits, 0, 0};
        const auto result =
            marc::frame::internal::decode_lzss_contextual_tans_frame(
                bytes.subspan(consumed), context, tables_, tokens_,
                private_raw_);
        EXPECT_NE(result.error, marc::frame::internal::
                                    LzssContextualTansFrameDecodeError::none);
        EXPECT_TRUE(std::ranges::all_of(
            private_raw_, [](const std::byte value) {
                return value == std::byte{0xa5};
            }));
    }

    void expect_public_atomic_failure(
        const std::span<const std::uint8_t> input) {
        marc_transform* decoder{};
        ASSERT_EQ(marc_lzss_contextual_tans_create(
                      &decode_config_,
                      {public_workspace_.primary.data(),
                       public_workspace_.primary.size()},
                      {public_workspace_.secondary.data(),
                       public_workspace_.secondary.size()},
                      public_workspace_.views_buffer(), &decoder),
                  MARC_STATUS_OK);
        std::array<std::uint8_t, raw.size()> output{};
        output.fill(UINT8_C(0xa5));
        const auto result = marc_transform_process(
            decoder, {input.data(), input.size()},
            {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
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
        const std::span<const std::uint8_t> input) {
        expect_private_atomic_failure(input);
        expect_public_atomic_failure(input);
    }

private:
    marc_lzss_contextual_tans_config decode_config_{};
    Workspace public_workspace_{};
    std::vector<TansDecodeEntry> tables_;
    std::array<Token, raw.size()> tokens_{};
    std::array<std::byte, raw.size()> private_raw_{};
};

TEST_P(LzssContextualTansFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream(GetParam());
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size));
    }
}

TEST_P(LzssContextualTansFuzzRegression, ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream(GetParam());
    constexpr auto frame_offset =
        marc::frame::internal::lzss_contextual_tans_stream_header_size;
    std::fill(encoded.begin() + frame_offset + 16,
              encoded.begin() + frame_offset + 48, UINT8_C(0xff));
    expect_dual_atomic_failure(encoded);
}

TEST_P(LzssContextualTansFuzzRegression,
       NonzeroDescriptorReservedBytesAreAtomic) {
    auto encoded = canonical_stream(GetParam());
    constexpr auto descriptor_reserved =
        marc::frame::internal::lzss_contextual_tans_stream_header_size
        + marc::frame::internal::lzss_contextual_tans_frame_header_size + 9;
    encoded[descriptor_reserved] = 1;
    expect_dual_atomic_failure(encoded);
}

TEST(LzssContextualTansFuzzRegression,
     CrossProfilePublicDecodersRejectAtomically) {
    const auto expect_rejection = [](
        const std::span<const std::uint8_t> input,
        const marc_lzss_contextual_window_profile decoder_profile) {
        auto config = settings(MARC_DIRECTION_DECODE, decoder_profile);
        auto decoder_workspace = workspace_for(config);
        marc_transform* decoder{};
        ASSERT_EQ(marc_lzss_contextual_tans_create(
                      &config,
                      {decoder_workspace.primary.data(),
                       decoder_workspace.primary.size()},
                      {decoder_workspace.secondary.data(),
                       decoder_workspace.secondary.size()},
                      decoder_workspace.views_buffer(), &decoder),
                  MARC_STATUS_OK);
        std::array<std::uint8_t, raw.size()> output{};
        output.fill(UINT8_C(0xa5));
        const auto result = marc_transform_process(
            decoder, {input.data(), input.size()},
            {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
        EXPECT_EQ(result.status, MARC_STATUS_MALFORMED_STREAM);
        EXPECT_EQ(result.output_produced, 0U);
        EXPECT_TRUE(std::ranges::all_of(
            output, [](const std::uint8_t value) {
                return value == UINT8_C(0xa5);
            }));
        const auto repeated = marc_transform_process(
            decoder, {nullptr, 0}, {nullptr, 0}, MARC_PROCESS_NONE);
        EXPECT_EQ(repeated.status, result.status);
        EXPECT_EQ(repeated.error_byte_position,
                  result.error_byte_position);
        EXPECT_EQ(repeated.error_bit_position,
                  result.error_bit_position);
        EXPECT_EQ(repeated.input_consumed, 0U);
        EXPECT_EQ(repeated.output_produced, 0U);
        marc_transform_destroy(decoder);
    };

    const auto frozen = canonical_stream(MARC_LZSS_CONTEXTUAL_WINDOW_64K);
    expect_rejection(frozen, MARC_LZSS_CONTEXTUAL_WINDOW_1M);
    const auto extended = canonical_stream(MARC_LZSS_CONTEXTUAL_WINDOW_1M);
    expect_rejection(extended, MARC_LZSS_CONTEXTUAL_WINDOW_64K);
}

INSTANTIATE_TEST_SUITE_P(
    Profiles, LzssContextualTansFuzzRegression,
    testing::Values(
        MARC_LZSS_CONTEXTUAL_WINDOW_64K,
        MARC_LZSS_CONTEXTUAL_WINDOW_1M));

} // namespace
