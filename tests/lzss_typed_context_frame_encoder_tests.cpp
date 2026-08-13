#include "frame/lzss_typed_context_frame_encoder.hpp"

#include "frame/lzss_typed_context_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] TypedContextStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    TypedContextStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    return stream;
}

[[nodiscard]] TypedContextStreamHeader extended_stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    auto stream = stream_config(frame_size, original_size);
    stream.dictionary.window_size = 1048576;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    return stream;
}

[[nodiscard]] constexpr std::array<std::byte, 86> one_literal_frame() {
    std::array<std::byte, 86> encoded{};
    encoded[0] = std::byte{0x4D};
    encoded[1] = std::byte{0x52};
    encoded[2] = std::byte{0x46};
    encoded[3] = std::byte{0x32};
    encoded[4] = std::byte{0x40};
    encoded[16] = std::byte{0x01};
    encoded[20] = std::byte{0x01};
    encoded[24] = std::byte{0x02};
    encoded[28] = std::byte{0x02};
    encoded[32] = std::byte{0x06};
    encoded[36] = std::byte{0x10};
    encoded[64] = std::byte{0x02};
    encoded[68] = std::byte{0x06};
    encoded[72] = std::byte{0x1F};
    encoded[80] = std::byte{0x00};
    encoded[81] = std::byte{0x20};
    encoded[82] = std::byte{0x7F};
    encoded[83] = std::byte{0xFF};
    encoded[84] = std::byte{0xBF};
    encoded[85] = std::byte{0x00};
    return encoded;
}

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

} // namespace

TEST(LzssTypedContextFrameEncoder, EmitsSpecifiedOneLiteralFrame) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = stream_config(64, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 2> tokens{};
    std::array<marc::context::internal::ModeledOperation, 3> operations{};
    const auto plan = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(plan.serialized_size, 86U);
    EXPECT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.operation_count, 2U);
    EXPECT_EQ(plan.decision_count, 2U);
    EXPECT_EQ(plan.payload_size, 6U);

    std::array<std::byte, 87> output{};
    output.back() = std::byte{0xCC};
    const auto encoded = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations, output);
    ASSERT_EQ(encoded.error, LzssTypedContextFrameEncodeError::none);
    constexpr auto expected = one_literal_frame();
    EXPECT_TRUE(std::ranges::equal(
        expected, std::span<const std::byte>{output}.first(expected.size())));
    EXPECT_EQ(output.back(), std::byte{0xCC});
}

TEST(LzssTypedContextFrameEncoder,
     ExtendedVariantRetainsOneLiteralFrameBytes) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = extended_stream_config(1048576, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    std::array<std::byte, 86> output{};
    const auto encoded = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations, output);
    ASSERT_EQ(encoded.error, LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(output, one_literal_frame());
}

TEST(LzssTypedContextFrameEncoder, RoundTripsMatchBearingFrame) {
    const auto input = bytes("ABCABCABCX");
    const auto stream = stream_config(
        static_cast<std::uint32_t>(input.size()), input.size());
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(
        input.size());
    std::vector<marc::context::internal::ModeledOperation> operations(
        input.size() * 5);
    const auto plan = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
    std::vector<std::byte> serialized(plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, {}, 0, 0, input, tokens, operations, serialized)
                  .error,
              LzssTypedContextFrameEncodeError::none);

    std::vector<marc::dictionary::internal::LzssTypedToken> decoded_tokens(
        plan.token_count);
    std::vector<std::byte> reconstructed(input.size());
    const auto decoded = decode_lzss_typed_context_frame(
        serialized, {stream, {}, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, serialized.size());
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedContextFrameEncoder, RejectsInvalidStreamAndFrameExtent) {
    constexpr std::array input{std::byte{'A'}};
    auto stream = stream_config(64, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    stream.range_model_total = 0;
    auto result = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    EXPECT_EQ(result.error, LzssTypedContextFrameEncodeError::invalid_stream);

    stream = stream_config(64, 2);
    result = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::input_size_mismatch);

    stream = stream_config(1, 2);
    result = plan_lzss_typed_context_frame(
        stream, {}, 1, 1, input, tokens, operations);
    EXPECT_EQ(result.error, LzssTypedContextFrameEncodeError::none);
}

TEST(LzssTypedContextFrameEncoder, CapacityFailuresPreserveSerializedOutput) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = stream_config(64, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    std::array<std::byte, 86> output{};
    output.fill(std::byte{0xCC});

    auto result = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input,
        std::span<marc::dictionary::internal::LzssTypedToken>{tokens}.first(0),
        operations, output);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::token_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));

    result = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens,
        std::span<marc::context::internal::ModeledOperation>{operations}
            .first(1),
        output);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::operation_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));

    result = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations,
        std::span<std::byte>{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));
}

TEST(LzssTypedContextFrameEncoder, EnforcesCompleteWorkspaceAggregate) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = stream_config(64, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    const auto baseline = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(baseline.error, LzssTypedContextFrameEncodeError::none);
    const auto complete_workspace = input.size()
        + sizeof(tokens) + sizeof(operations) + baseline.serialized_size;
    ASSERT_GT(complete_workspace, typed_context_stream_header_size);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = complete_workspace - 1;
    const auto limited = plan_lzss_typed_context_frame(
        stream, limits, 0, 0, input, tokens, operations);
    EXPECT_EQ(limited.error,
              LzssTypedContextFrameEncodeError::workspace_limit);
}

TEST(LzssTypedContextFrameEncoder, AliasingFailsBeforeCallerStorageChanges) {
    std::array<std::byte, 96> shared{};
    shared.fill(std::byte{0xCC});
    shared[0] = std::byte{'A'};
    const auto before = shared;
    const auto stream = stream_config(64, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};

    const auto result = encode_lzss_typed_context_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{shared}.first(1), tokens, operations,
        std::span<std::byte>{shared}.first(86));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(shared, before);

    constexpr std::array input{std::byte{'A'}};
    std::array<marc::dictionary::internal::LzssTypedToken, 8>
        token_output_storage{};
    auto token_bytes =
        std::as_writable_bytes(std::span{token_output_storage});
    std::ranges::fill(token_bytes, std::byte{0xCC});
    const std::vector<std::byte> token_snapshot(
        token_bytes.begin(), token_bytes.end());
    const auto token_alias = encode_lzss_typed_context_frame(
        stream, {}, 0, 0, input, token_output_storage, operations,
        token_bytes.first(86));
    EXPECT_EQ(token_alias.error,
              LzssTypedContextFrameEncodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(token_snapshot, token_bytes));
}

TEST(LzssTypedContextFrameEncoder, HashChainFrameMatchesExhaustiveBytes) {
    auto input = bytes("ABCDE1ABCDE2ABCDE3");
    for (std::uint32_t value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());
    const auto stream = stream_config(
        static_cast<std::uint32_t>(input.size()), input.size());
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(
        input.size());
    std::vector<marc::context::internal::ModeledOperation> operations(
        input.size() * 5);
    const auto reference_plan = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(reference_plan.error, LzssTypedContextFrameEncodeError::none);
    std::vector<std::byte> reference(reference_plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, {}, 0, 0, input, tokens, operations, reference).error,
              LzssTypedContextFrameEncodeError::none);

    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(input.size(), {}, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    marc::dictionary::internal::LzssMatchFinderStatistics statistics{};
    const auto plan = plan_lzss_typed_context_frame_hash_chain(
        stream, {}, 0, 0, input, tokens, operations, workspace, &statistics);
    ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(plan.serialized_size, reference_plan.serialized_size);
    EXPECT_EQ(plan.token_count, reference_plan.token_count);
    EXPECT_EQ(plan.operation_count, reference_plan.operation_count);
    EXPECT_EQ(statistics.query_count, plan.token_count);

    statistics = {};
    std::vector<std::byte> encoded(plan.serialized_size);
    const auto result = encode_lzss_typed_context_frame_hash_chain(
        stream, {}, 0, 0, input, tokens, operations, workspace, encoded,
        &statistics);
    ASSERT_EQ(result.error, LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(statistics.query_count, result.token_count);
    EXPECT_EQ(encoded, reference);

    std::vector<marc::dictionary::internal::LzssTypedToken> decoded_tokens(
        result.token_count);
    std::vector<std::byte> reconstructed(input.size());
    const auto decoded = decode_lzss_typed_context_frame(
        encoded, {stream, {}, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedContextFrameEncoder,
     ExtendedHashChainFrameUsesAndRoundTripsDistantMatch) {
    constexpr std::size_t distance = 65537;
    std::vector<std::byte> input(distance + 5, std::byte{0});
    for (std::size_t index = 5; index < distance; ++index) {
        input[index] = static_cast<std::byte>(1 + ((index - 5) % 255));
    }
    const auto stream = extended_stream_config(
        static_cast<std::uint32_t>(input.size()), input.size());
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(
        input.size());
    std::vector<marc::context::internal::ModeledOperation> operations(
        input.size() * 5);
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            input.size(), stream.dictionary, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    const auto plan = plan_lzss_typed_context_frame_hash_chain(
        stream, {}, 0, 0, input, tokens, operations, workspace);
    ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
    const auto distant = std::ranges::find_if(
        std::span{tokens}.first(plan.token_count),
        [](const marc::dictionary::internal::LzssTypedToken& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance == 65537;
        });
    ASSERT_NE(distant, std::span{tokens}.first(plan.token_count).end());
    EXPECT_EQ(distant->length, 5U);

    std::vector<std::byte> serialized(plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame_hash_chain(
                  stream, {}, 0, 0, input, tokens, operations, workspace,
                  serialized).error,
              LzssTypedContextFrameEncodeError::none);
    std::vector<marc::dictionary::internal::LzssTypedToken> decoded_tokens(
        plan.token_count);
    std::vector<std::byte> reconstructed(input.size());
    const auto decoded = decode_lzss_typed_context_frame(
        serialized, {stream, {}, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedContextFrameEncoder, HashChainWorkspaceFailuresAreAtomic) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto stream = stream_config(
        static_cast<std::uint32_t>(input.size()), input.size());
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(
        input.size());
    std::vector<marc::context::internal::ModeledOperation> operations(
        input.size() * 5);
    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(input.size(), {}, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    ASSERT_GT(requirements.workspace_size, 0U);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    std::array<std::byte, 256> output{};
    output.fill(std::byte{0xCC});

    auto result = encode_lzss_typed_context_frame_hash_chain(
        stream, {}, 0, 0, input, tokens, operations,
        workspace.first(workspace.size() - 1), output);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::token_encode_error);
    EXPECT_EQ(result.token_encode.match_finder_error,
              marc::dictionary::internal::LzssHashChainError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));

    const auto snapshot = std::vector<std::byte>(
        workspace.begin(), workspace.end());
    result = encode_lzss_typed_context_frame_hash_chain(
        stream, {}, 0, 0, input, tokens, operations, workspace,
        workspace.first(128));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameEncodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(snapshot, workspace));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = input.size();
    limits.max_block_size = input.size();
    const auto baseline = plan_lzss_typed_context_frame_hash_chain(
        stream, limits, 0, 0, input, tokens, operations, workspace);
    ASSERT_EQ(baseline.error, LzssTypedContextFrameEncodeError::none);
    const auto complete_workspace = input.size()
        + input.size()
            * sizeof(marc::dictionary::internal::LzssTypedToken)
        + baseline.operation_count
            * sizeof(marc::context::internal::ModeledOperation)
        + requirements.workspace_size + baseline.serialized_size;
    limits.max_internal_buffered_bytes = complete_workspace - 1;
    result = plan_lzss_typed_context_frame_hash_chain(
        stream, limits, 0, 0, input, tokens, operations, workspace);
    EXPECT_EQ(result.error, LzssTypedContextFrameEncodeError::workspace_limit);
}
