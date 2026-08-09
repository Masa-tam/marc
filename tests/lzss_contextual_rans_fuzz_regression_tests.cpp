#include <marc/marc.h>

#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_rans_compact_format.hpp"
#include "entropy/contextual_rans_format.hpp"
#include "entropy/rans_decode_table.hpp"
#include "frame/lzss_contextual_rans_compact_frame_decoder.hpp"
#include "frame/lzss_contextual_rans_frame_decoder.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using RansDecodeEntry = marc::entropy::internal::RansDecodeEntry;
using Token = marc::dictionary::internal::LzssTypedToken;

enum class Representation { fixed, compact };

constexpr std::array raw{
    std::uint8_t{'A'}, std::uint8_t{'B'}, std::uint8_t{'A'},
    std::uint8_t{'B'}, std::uint8_t{'X'}};
constexpr std::size_t maximum_decisions = raw.size() * 6;
constexpr std::size_t maximum_payload = raw.size() * 12 + 8;
constexpr std::size_t maximum_internal = 2U << 20;

[[nodiscard]] constexpr std::size_t maximum_encoded_frame(
    const Representation representation) noexcept {
    const auto descriptor_size = representation == Representation::fixed
        ? marc::entropy::internal::contextual_rans_descriptor_size
        : marc::entropy::internal::contextual_rans_compact_max_descriptor_size;
    return marc::frame::internal::lzss_contextual_rans_frame_header_size
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

struct Config {
    Representation representation{};
    marc_lzss_contextual_rans_config fixed{};
    marc_lzss_contextual_rans_compact_config compact{};
};

template <class T>
void configure_fields(T& result) {
    result.original_size = raw.size();
    result.frame_size = raw.size();
    result.max_total_output_size = 32;
    result.max_frame_size = raw.size();
    result.max_block_size = maximum_decisions;
    result.max_compressed_payload_size = maximum_payload;
    result.max_internal_buffered_bytes = maximum_internal;
    result.max_lz_distance = UINT64_C(1) << 16;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries =
        marc::entropy::internal::contextual_rans_decode_table_entries;
}

[[nodiscard]] Config settings(const Representation representation,
                              const marc_direction direction) {
    Config result{};
    result.representation = representation;
    if (representation == Representation::fixed) {
        EXPECT_EQ(marc_lzss_contextual_rans_config_init(
                      direction, &result.fixed),
                  MARC_STATUS_OK);
        configure_fields(result.fixed);
    } else {
        EXPECT_EQ(marc_lzss_contextual_rans_compact_config_init(
                      direction, &result.compact),
                  MARC_STATUS_OK);
        configure_fields(result.compact);
    }
    return result;
}

[[nodiscard]] Workspace workspace_for(const Config& config) {
    Workspace result{};
    const auto status = config.representation == Representation::fixed
        ? marc_lzss_contextual_rans_workspace_requirements(
              &config.fixed, &result.requirements)
        : marc_lzss_contextual_rans_compact_workspace_requirements(
              &config.compact, &result.requirements);
    EXPECT_EQ(status, MARC_STATUS_OK);
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
    const Representation representation) {
    auto config = settings(representation, MARC_DIRECTION_ENCODE);
    auto workspace = workspace_for(config);
    marc_transform* encoder{};
    const marc_buffer primary{workspace.primary.data(),
                              workspace.primary.size()};
    const marc_buffer secondary{workspace.secondary.data(),
                                workspace.secondary.size()};
    const auto views = workspace.views_buffer();
    const auto status = representation == Representation::fixed
        ? marc_lzss_contextual_rans_create(
              &config.fixed, primary, secondary, views, &encoder)
        : marc_lzss_contextual_rans_compact_create(
              &config.compact, primary, secondary, views, &encoder);
    EXPECT_EQ(status, MARC_STATUS_OK);
    std::vector<std::uint8_t> encoded(
        marc::frame::internal::lzss_contextual_rans_stream_header_size
        + maximum_encoded_frame(representation));
    const auto result = marc_transform_process(
        encoder, {raw.data(), raw.size()}, {encoded.data(), encoded.size()},
        MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status, MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(encoder);
    encoded.resize(result.output_produced);
    return encoded;
}

class LzssContextualRansFuzzRegression
    : public testing::TestWithParam<Representation> {
protected:
    LzssContextualRansFuzzRegression()
        : representation_(GetParam()),
          decode_config_(settings(representation_, MARC_DIRECTION_DECODE)),
          public_workspace_(workspace_for(decode_config_)),
          tables_(marc::entropy::internal::
                      contextual_rans_decode_table_entries) {}

    void expect_private_atomic_failure(
        const std::span<const std::uint8_t> input) {
        marc::core::DecoderLimits limits{};
        limits.max_total_output_size = 32;
        limits.max_frame_size = raw.size();
        limits.max_block_size = maximum_decisions;
        limits.max_compressed_payload_size = maximum_payload;
        limits.max_internal_buffered_bytes = maximum_internal;
        limits.max_lz_distance = UINT64_C(1) << 16;
        limits.max_lz_match_length = 258;
        limits.max_entropy_table_entries =
            marc::entropy::internal::contextual_rans_decode_table_entries;
        const auto bytes = std::as_bytes(input);
        marc::frame::internal::LzssContextualRansStreamHeader stream{};
        std::size_t consumed{};
        const auto parsed = representation_ == Representation::fixed
            ? marc::frame::internal::parse_lzss_contextual_rans_stream_header(
                  bytes, limits, stream, consumed)
            : marc::frame::internal::
                  parse_lzss_contextual_rans_compact_stream_header(
                      bytes, limits, stream, consumed);
        if (parsed != marc::frame::internal::
                          LzssContextualRansStreamHeaderError::none) {
            return;
        }
        private_raw_.fill(std::byte{0xa5});
        const marc::frame::internal::
            LzssContextualRansFrameValidationContext context{
                stream, limits, 0, 0};
        bool failed{};
        if (representation_ == Representation::fixed) {
            const auto result =
                marc::frame::internal::decode_lzss_contextual_rans_frame(
                    bytes.subspan(consumed), context, tables_, tokens_,
                    private_raw_);
            failed = result.error != marc::frame::internal::
                                         LzssContextualRansFrameDecodeError::none;
        } else {
            const auto result = marc::frame::internal::
                decode_lzss_contextual_rans_compact_frame(
                    bytes.subspan(consumed), context, tables_, tokens_,
                    private_raw_);
            failed = result.error
                != marc::frame::internal::
                       LzssContextualRansFrameDecodeError::none;
        }
        EXPECT_TRUE(failed);
        EXPECT_TRUE(std::ranges::all_of(
            private_raw_, [](const std::byte value) {
                return value == std::byte{0xa5};
            }));
    }

    void expect_public_atomic_failure(
        const std::span<const std::uint8_t> input) {
        marc_transform* decoder{};
        const marc_buffer primary{public_workspace_.primary.data(),
                                  public_workspace_.primary.size()};
        const marc_buffer secondary{public_workspace_.secondary.data(),
                                    public_workspace_.secondary.size()};
        const auto views = public_workspace_.views_buffer();
        const auto status = representation_ == Representation::fixed
            ? marc_lzss_contextual_rans_create(
                  &decode_config_.fixed, primary, secondary, views, &decoder)
            : marc_lzss_contextual_rans_compact_create(
                  &decode_config_.compact, primary, secondary, views,
                  &decoder);
        ASSERT_EQ(status, MARC_STATUS_OK);
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
    Representation representation_{};
    Config decode_config_{};
    Workspace public_workspace_{};
    std::vector<RansDecodeEntry> tables_;
    std::array<Token, raw.size()> tokens_{};
    std::array<std::byte, raw.size()> private_raw_{};
};

TEST_P(LzssContextualRansFuzzRegression,
       EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream(GetParam());
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_dual_atomic_failure(
            std::span<const std::uint8_t>{encoded}.first(size));
    }
}

TEST_P(LzssContextualRansFuzzRegression,
       ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream(GetParam());
    constexpr auto frame_offset =
        marc::frame::internal::lzss_contextual_rans_stream_header_size;
    std::fill(encoded.begin() + frame_offset + 16,
              encoded.begin() + frame_offset + 48, UINT8_C(0xff));
    expect_dual_atomic_failure(encoded);
}

TEST_P(LzssContextualRansFuzzRegression,
       NonzeroDescriptorFlagsAreAtomic) {
    auto encoded = canonical_stream(GetParam());
    constexpr auto descriptor_flags =
        marc::frame::internal::lzss_contextual_rans_stream_header_size
        + marc::frame::internal::lzss_contextual_rans_frame_header_size + 9;
    encoded[descriptor_flags] = 1;
    expect_dual_atomic_failure(encoded);
}

INSTANTIATE_TEST_SUITE_P(
    FixedAndCompact, LzssContextualRansFuzzRegression,
    testing::Values(Representation::fixed, Representation::compact),
    [](const testing::TestParamInfo<Representation>& info) {
        return info.param == Representation::fixed ? "Fixed" : "Compact";
    });

} // namespace
