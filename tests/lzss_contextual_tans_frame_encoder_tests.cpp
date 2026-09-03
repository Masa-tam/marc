#include "frame/lzss_contextual_tans_frame_encoder.hpp"

#include "frame/lzss_contextual_tans_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::entropy::internal::TansDecodeEntry;
using marc::entropy::internal::contextual_tans_decode_table_entries;
using marc::entropy::internal::contextual_tans_encode_table_entries;

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

[[nodiscard]] LzssContextualTansStreamHeader stream_for(
    const std::uint64_t original_size) {
    LzssContextualTansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_for_1m(
    const std::uint64_t original_size) {
    auto stream = stream_for(original_size);
    stream.frame_size = static_cast<std::uint32_t>(original_size);
    stream.dictionary.window_size = 1'048'576;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    stream.frequency_entry_count = 4550;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_for_4m(
    const std::uint64_t original_size) {
    auto stream = stream_for(original_size);
    stream.frame_size = static_cast<std::uint32_t>(original_size);
    stream.dictionary.window_size = 4'194'304;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    stream.frequency_entry_count = 4566;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_for_16m(
    const std::uint64_t original_size) {
    auto stream = stream_for(original_size);
    stream.frame_size = static_cast<std::uint32_t>(original_size);
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    stream.frequency_entry_count = 4582;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_for_64m(
    const std::uint64_t original_size) {
    auto stream = stream_for(original_size);
    stream.frame_size = static_cast<std::uint32_t>(original_size);
    stream.dictionary.window_size = UINT32_C(1) << 26;
    stream.dictionary_variant = 6;
    stream.context_variant = 5;
    stream.frequency_entry_count = 4598;
    return stream;
}

[[nodiscard]] std::vector<std::byte> documented_literal_frame() {
    std::vector<std::byte> bytes(96);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01}; bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02}; bytes[32] = std::byte{0x02};
    bytes[36] = std::byte{0x1e};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

[[nodiscard]] std::vector<std::uint16_t> encode_tables() {
    return std::vector<std::uint16_t>(
        contextual_tans_encode_table_entries);
}

[[nodiscard]] std::vector<TansDecodeEntry> decode_tables() {
    return std::vector<TansDecodeEntry>(
        contextual_tans_decode_table_entries);
}

} // namespace

TEST(LzssContextualTansFrameEncoder, PlansAndEmitsDocumentedLiteralFrame) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xcc;
    auto tables = encode_tables();
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(result.serialized_size, 96U);
    EXPECT_EQ(result.descriptor_size, 30U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(result.required_table_entries,
              contextual_tans_encode_table_entries);

    std::vector<std::byte> output(result.serialized_size + 1,
                                  std::byte{0xcc});
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, output);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        documented_literal_frame(),
        std::span<const std::byte>{output}.first(result.serialized_size)));
    EXPECT_EQ(output.back(), std::byte{0xcc});
    EXPECT_EQ(tokens[1].literal, 0xcc);
}

TEST(LzssContextualTansFrameEncoder,
     FourMiBIdentityRoundTripsCanonicalLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for_4m(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto encode_table_storage = encode_tables();
    const auto plan = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, encode_table_storage);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.event_count, 2U);
    EXPECT_EQ(plan.decision_count, 2U);
    EXPECT_EQ(plan.payload_size, 2U);

    std::vector<std::byte> output(plan.serialized_size, std::byte{0xa5});
    const auto encoded = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, encode_table_storage, output);
    ASSERT_EQ(encoded.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_GT(encoded.descriptor_size, 17U);
    EXPECT_EQ(output[lzss_contextual_tans_frame_header_size + 16],
              std::byte{0xd6});
    EXPECT_EQ(output[lzss_contextual_tans_frame_header_size + 17],
              std::byte{0x11});

    auto decode_table_storage = decode_tables();
    std::array<LzssTypedToken, 1> decoded_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        output, {stream, {}, 0, 0}, decode_table_storage, decoded_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     SixteenMiBIdentityRoundTripsCanonicalLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for_16m(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto encode_table_storage = encode_tables();
    const auto plan = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, encode_table_storage);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.event_count, 2U);
    EXPECT_EQ(plan.decision_count, 2U);
    EXPECT_EQ(plan.payload_size, 2U);

    std::vector<std::byte> output(plan.serialized_size, std::byte{0xa5});
    const auto encoded = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, encode_table_storage, output);
    ASSERT_EQ(encoded.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_GT(encoded.descriptor_size, 17U);
    EXPECT_EQ(output[lzss_contextual_tans_frame_header_size + 16],
              std::byte{0xe6});
    EXPECT_EQ(output[lzss_contextual_tans_frame_header_size + 17],
              std::byte{0x11});

    auto decode_table_storage = decode_tables();
    std::array<LzssTypedToken, 1> decoded_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        output, {stream, {}, 0, 0}, decode_table_storage, decoded_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     SixtyFourMiBCompleteFrameEncodingRemainsClosed) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for_64m(raw.size());
    auto limits = marc::core::DecoderLimits{};
    limits.max_lz_distance = UINT64_C(1) << 26;
    std::array<LzssTypedToken, 1> tokens{};
    auto encode_table_storage = encode_tables();

    const auto plan = plan_lzss_contextual_tans_frame(
        stream, limits, 0, 0, raw, tokens, encode_table_storage);
    EXPECT_EQ(plan.error, LzssContextualTansFrameEncodeError::invalid_stream);
    EXPECT_EQ(tokens[0].literal, 0U);
    EXPECT_TRUE(std::ranges::all_of(
        encode_table_storage,
        [](const auto value) { return value == 0; }));
}

TEST(LzssContextualTansFrameEncoder,
     OneMiBHashChainRoundTripExercisesExtendedDistance) {
    constexpr std::size_t gap = 65536;
    std::vector<std::byte> raw(5 + gap + 5, std::byte{'Z'});
    constexpr std::array marker{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    std::ranges::copy(marker, raw.begin());
    std::ranges::copy(marker, raw.end() - marker.size());
    const auto stream = stream_for_1m(raw.size());
    std::vector<LzssTypedToken> encode_tokens(raw.size());
    auto encode_table_storage = encode_tables();
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            raw.size(), stream.dictionary, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);

    const auto plan = plan_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, encode_tokens, encode_table_storage,
        workspace);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_TRUE(std::ranges::any_of(
        std::span<const LzssTypedToken>{encode_tokens}.first(plan.token_count),
        [](const LzssTypedToken& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance > 65536;
        }));

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame_hash_chain(
                  stream, {}, 0, 0, raw, encode_tokens,
                  encode_table_storage, workspace, encoded).error,
              LzssContextualTansFrameEncodeError::none);
    auto decode_table_storage = decode_tables();
    std::vector<LzssTypedToken> decode_tokens(plan.token_count);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzss_contextual_tans_frame(
        encoded, {stream, {}, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);

    auto crossed = stream;
    crossed.dictionary.window_size = 65536;
    crossed.dictionary_variant = 2;
    crossed.context_variant = 1;
    crossed.frequency_entry_count = 4518;
    std::ranges::fill(decoded, std::byte{0xcc});
    const auto rejected = decode_lzss_contextual_tans_frame(
        encoded, {crossed, {}, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    EXPECT_EQ(rejected.error,
              LzssContextualTansFrameDecodeError::preflight_error);
    EXPECT_TRUE(std::ranges::all_of(decoded, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualTansFrameEncoder,
     FourMiBHashChainRoundTripExercisesDistanceBeyondOneMiB) {
    constexpr std::size_t gap = UINT32_C(1) << 20;
    std::vector<std::byte> raw(5 + gap + 5, std::byte{'Z'});
    constexpr std::array marker{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    std::ranges::copy(marker, raw.begin());
    std::ranges::copy(marker, raw.end() - marker.size());
    const auto stream = stream_for_4m(raw.size());
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    std::vector<LzssTypedToken> encode_tokens(raw.size());
    auto encode_table_storage = encode_tables();
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            raw.size(), stream.dictionary, limits);
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);

    const auto plan = plan_lzss_contextual_tans_frame_hash_chain(
        stream, limits, 0, 0, raw, encode_tokens, encode_table_storage,
        workspace);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_TRUE(std::ranges::any_of(
        std::span<const LzssTypedToken>{encode_tokens}.first(plan.token_count),
        [](const LzssTypedToken& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance > (UINT32_C(1) << 20);
        }));

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame_hash_chain(
                  stream, limits, 0, 0, raw, encode_tokens,
                  encode_table_storage, workspace, encoded).error,
              LzssContextualTansFrameEncodeError::none);
    auto decode_table_storage = decode_tables();
    std::vector<LzssTypedToken> decode_tokens(plan.token_count);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzss_contextual_tans_frame(
        encoded, {stream, limits, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);

    auto crossed = stream;
    crossed.dictionary.window_size = UINT32_C(1) << 20;
    crossed.dictionary_variant = 3;
    crossed.context_variant = 2;
    crossed.frequency_entry_count = 4550;
    std::ranges::fill(decoded, std::byte{0xcc});
    const auto rejected = decode_lzss_contextual_tans_frame(
        encoded, {crossed, limits, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    EXPECT_EQ(rejected.error,
              LzssContextualTansFrameDecodeError::preflight_error);
    EXPECT_TRUE(std::ranges::all_of(decoded, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualTansFrameEncoder, CompleteDecoderRecoversLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> encode_tokens{};
    auto encode_table_storage = encode_tables();
    std::vector<std::byte> frame(96);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, encode_tokens,
                  encode_table_storage, frame).error,
              LzssContextualTansFrameEncodeError::none);

    auto decode_table_storage = decode_tables();
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     RoundTripsMixedRawFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens_a{};
    auto tables_a = encode_tables();
    const auto plan = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens_a, tables_a);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    std::vector<std::byte> first(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, tokens_a, tables_a, first).error,
              LzssContextualTansFrameEncodeError::none);

    std::array<LzssTypedToken, raw.size()> tokens_b{};
    auto tables_b = encode_tables();
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, tokens_b, tables_b, second).error,
              LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(second, first);

    auto decoder_tables = decode_tables();
    std::array<LzssTypedToken, raw.size()> decode_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        first, {stream, {}, 0, 0}, decoder_tables, decode_tokens, decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     CapacityFailuresPreserveSerializedOutput) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();
    std::vector<std::byte> output(96, std::byte{0xcc});

    auto result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw,
        std::span<LzssTypedToken>{tokens}.first(0), tables, output);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::token_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens,
        std::span<std::uint16_t>{tables}.first(tables.size() - 1), output);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::table_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables,
        std::span<std::byte>{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualTansFrameEncoder, RejectsWorkspaceAliasingBeforeWrites) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    table_bytes[0] = std::byte{'A'};
    const auto table_marker = table_bytes[0];
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{table_bytes}.first(1), tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(table_bytes[0], table_marker);

    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    token_bytes[0] = std::byte{'A'};
    const auto token_marker = token_bytes[0];
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{token_bytes}.first(1), tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(token_bytes[0], token_marker);

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, table_bytes.first(96));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, token_bytes);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);

    std::array<std::byte, 96> serialized_raw{};
    serialized_raw[0] = std::byte{'A'};
    const auto raw_marker = serialized_raw[0];
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{serialized_raw}.first(1), tokens, tables,
        serialized_raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(serialized_raw[0], raw_marker);
}

TEST(LzssContextualTansFrameEncoder,
     RejectsStreamInputAndAggregateWorkspaceLimit) {
    constexpr std::array raw{std::byte{'A'}};
    auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();
    stream.state_count = 2;
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::invalid_stream);

    stream = stream_for(2);
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::input_size_mismatch);

    stream = stream_for(raw.size());
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        raw.size() + result.token_encode.token_storage_size
        + contextual_tans_encode_table_entries * sizeof(std::uint16_t)
        + result.serialized_size - 1;
    result = plan_lzss_contextual_tans_frame(
        stream, limits, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::workspace_limit);
}

TEST(LzssContextualTansFrameEncoder,
     HashChainFrameMatchesExhaustiveBytes) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'3'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> reference_tokens{};
    auto reference_tables = encode_tables();
    const auto reference_plan = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, reference_tokens, reference_tables);
    ASSERT_EQ(reference_plan.error,
              LzssContextualTansFrameEncodeError::none);
    std::vector<std::byte> reference(reference_plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, reference_tokens,
                  reference_tables, reference).error,
              LzssContextualTansFrameEncodeError::none);

    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(raw.size(), {}, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    std::array<LzssTypedToken, raw.size()> tokens{};
    auto tables = encode_tables();
    marc::dictionary::internal::LzssMatchFinderStatistics statistics{};
    const auto plan = plan_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, tokens, tables, workspace, &statistics);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(plan.serialized_size, reference_plan.serialized_size);
    EXPECT_EQ(plan.descriptor_size, reference_plan.descriptor_size);
    EXPECT_EQ(plan.token_count, reference_plan.token_count);
    EXPECT_EQ(plan.event_count, reference_plan.event_count);
    EXPECT_EQ(plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(plan.payload_size, reference_plan.payload_size);
    EXPECT_EQ(statistics.query_count, plan.token_count);

    statistics = {};
    std::vector<std::byte> encoded(plan.serialized_size);
    const auto result = encode_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, tokens, tables, workspace, encoded,
        &statistics);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(statistics.query_count, result.token_count);
    EXPECT_EQ(encoded, reference);

    auto decoder_tables = decode_tables();
    std::array<LzssTypedToken, raw.size()> decoded_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decode_lzss_contextual_tans_frame(
        encoded, {stream, {}, 0, 0}, decoder_tables, decoded_tokens, decoded);
    ASSERT_EQ(decoded_result.error,
              LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     BinaryTreeFrameMatchesHashChainBytes) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'3'}};
    const auto stream = stream_for(raw.size());
    const auto chain_requirements = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            marc::dictionary::internal::
                LzssMatchFinderStrategy::hash_chain_exact,
            raw.size(), {}, {});
    const auto tree_requirements = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            marc::dictionary::internal::
                LzssMatchFinderStrategy::binary_tree_exact,
            raw.size(), {}, {});
    ASSERT_EQ(chain_requirements.error, marc::dictionary::internal::
                                            LzssMatchFinderWorkspaceError::none);
    ASSERT_EQ(tree_requirements.error, marc::dictionary::internal::
                                           LzssMatchFinderWorkspaceError::none);
    AlignedWorkspace chain_owner(chain_requirements.workspace_size);
    AlignedWorkspace tree_owner(tree_requirements.workspace_size);
    auto chain_workspace = chain_owner.bytes(
        chain_requirements.workspace_size);
    auto tree_workspace = tree_owner.bytes(tree_requirements.workspace_size);
    std::array<LzssTypedToken, raw.size()> chain_tokens{};
    std::array<LzssTypedToken, raw.size()> tree_tokens{};
    auto chain_tables = encode_tables();
    auto tree_tables = encode_tables();

    const auto chain_plan =
        plan_lzss_contextual_tans_frame_with_match_finder(
            stream, {}, 0, 0, raw, chain_tokens, chain_tables,
            marc::dictionary::internal::
                LzssMatchFinderStrategy::hash_chain_exact,
            chain_workspace);
    const auto tree_plan =
        plan_lzss_contextual_tans_frame_with_match_finder(
            stream, {}, 0, 0, raw, tree_tokens, tree_tables,
            marc::dictionary::internal::
                LzssMatchFinderStrategy::binary_tree_exact,
            tree_workspace);
    ASSERT_EQ(chain_plan.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_EQ(tree_plan.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_EQ(tree_plan.serialized_size, chain_plan.serialized_size);

    std::vector<std::byte> chain(chain_plan.serialized_size);
    std::vector<std::byte> tree(tree_plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame_with_match_finder(
                  stream, {}, 0, 0, raw, chain_tokens, chain_tables,
                  marc::dictionary::internal::
                      LzssMatchFinderStrategy::hash_chain_exact,
                  chain_workspace, chain).error,
              LzssContextualTansFrameEncodeError::none);
    ASSERT_EQ(encode_lzss_contextual_tans_frame_with_match_finder(
                  stream, {}, 0, 0, raw, tree_tokens, tree_tables,
                  marc::dictionary::internal::
                      LzssMatchFinderStrategy::binary_tree_exact,
                  tree_workspace, tree).error,
              LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(tree, chain);
}

TEST(LzssContextualTansFrameEncoder,
     SixteenMiBHashChainRoundTripExercisesDistanceBeyondFourMiB) {
    constexpr std::size_t gap = UINT32_C(1) << 22;
    std::vector<std::byte> raw(5 + gap + 5, std::byte{'Z'});
    constexpr std::array marker{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    std::ranges::copy(marker, raw.begin());
    std::ranges::copy(marker, raw.end() - marker.size());
    const auto stream = stream_for_16m(raw.size());
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    std::vector<LzssTypedToken> encode_tokens(raw.size());
    auto encode_table_storage = encode_tables();
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            raw.size(), stream.dictionary, limits);
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);

    const auto plan = plan_lzss_contextual_tans_frame_hash_chain(
        stream, limits, 0, 0, raw, encode_tokens, encode_table_storage,
        workspace);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    ASSERT_TRUE(std::ranges::any_of(
        std::span<const LzssTypedToken>{encode_tokens}.first(plan.token_count),
        [](const LzssTypedToken& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance > (UINT32_C(1) << 22);
        }));

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame_hash_chain(
                  stream, limits, 0, 0, raw, encode_tokens,
                  encode_table_storage, workspace, encoded).error,
              LzssContextualTansFrameEncodeError::none);
    auto decode_table_storage = decode_tables();
    std::vector<LzssTypedToken> decode_tokens(plan.token_count);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzss_contextual_tans_frame(
        encoded, {stream, limits, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);

    auto crossed = stream;
    crossed.dictionary.window_size = UINT32_C(1) << 22;
    crossed.dictionary_variant = 4;
    crossed.context_variant = 3;
    crossed.frequency_entry_count = 4566;
    std::ranges::fill(decoded, std::byte{0xcc});
    const auto rejected = decode_lzss_contextual_tans_frame(
        encoded, {crossed, limits, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    EXPECT_EQ(rejected.error,
              LzssContextualTansFrameDecodeError::preflight_error);
    EXPECT_TRUE(std::ranges::all_of(decoded, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualTansFrameEncoder,
     HashChainWorkspaceFailuresAreAtomic) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens{};
    auto tables = encode_tables();
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(raw.size(), {}, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    ASSERT_GT(requirements.workspace_size, 0U);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    std::vector<std::byte> output(16'384, std::byte{0xcc});

    auto result = encode_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, tokens, tables,
        workspace.first(workspace.size() - 1), output);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::token_encode_error);
    EXPECT_EQ(result.token_encode.match_finder_error,
              marc::dictionary::internal::
                  LzssHashChainError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    const auto table_snapshot = std::vector<std::byte>(
        table_bytes.begin(), table_bytes.end());
    result = plan_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, tokens, tables,
        table_bytes.first(requirements.workspace_size));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(table_snapshot, table_bytes));

    const auto snapshot = std::vector<std::byte>(
        workspace.begin(), workspace.end());
    result = encode_lzss_contextual_tans_frame_hash_chain(
        stream, {}, 0, 0, raw, tokens, tables, workspace,
        workspace.first(workspace.size()));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(snapshot, workspace));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = stream.frame_size;
    limits.max_block_size = stream.frame_size;
    const auto baseline = plan_lzss_contextual_tans_frame_hash_chain(
        stream, limits, 0, 0, raw, tokens, tables, workspace);
    ASSERT_EQ(baseline.error, LzssContextualTansFrameEncodeError::none);
    limits.max_internal_buffered_bytes = raw.size()
        + raw.size() * sizeof(LzssTypedToken)
        + contextual_tans_encode_table_entries * sizeof(std::uint16_t)
        + requirements.workspace_size + baseline.serialized_size - 1;
    result = plan_lzss_contextual_tans_frame_hash_chain(
        stream, limits, 0, 0, raw, tokens, tables, workspace);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::workspace_limit);
}
