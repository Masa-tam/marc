#include "dictionary/lzss_typed_encoder.hpp"

#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace marc::dictionary::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] bool equal_token(
    const LzssTypedToken& left, const LzssTypedToken& right) noexcept {
    return left.kind == right.kind && left.literal == right.literal
        && left.distance == right.distance && left.length == right.length;
}

[[nodiscard]] LzssTypedToken convert(const LzssToken& token) noexcept {
    return token.tag == LzssTokenTag::literal
        ? LzssTypedToken{LzssTypedTokenKind::literal, token.literal, 0, 0}
        : LzssTypedToken{LzssTypedTokenKind::match, 0, token.distance,
                         token.length};
}

[[nodiscard]] std::vector<std::byte> serialize_typed_tokens(
    const std::span<const LzssTypedToken> tokens) {
    std::vector<std::byte> result{};
    for (const auto& token : tokens) {
        const auto raw = token.kind == LzssTypedTokenKind::literal
            ? LzssToken{LzssTokenTag::literal, 0, 0, token.literal}
            : LzssToken{LzssTokenTag::match, token.distance,
                        token.length, 0};
        std::array<std::byte, lzss_match_size> buffer{};
        std::size_t written{};
        EXPECT_EQ(serialize_lzss_token(raw, buffer, written),
                  LzssFormatError::none);
        result.insert(result.end(), buffer.begin(), buffer.begin() + written);
    }
    return result;
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

void expect_binary_tree_typed_equals_exact(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {},
    const LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) {
    const auto reference_plan = plan_lzss_typed_tokens(
        input, parameters, {}, variant);
    ASSERT_EQ(reference_plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> reference(reference_plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(
                  input, parameters, {}, reference, variant).error,
              LzssTypedEncodeError::none);

    const auto hash_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(hash_required.error, LzssHashChainError::none);
    AlignedWorkspace hash_owner(hash_required.workspace_size);
    auto hash_workspace = hash_owner.bytes(hash_required.workspace_size);
    std::vector<LzssTypedToken> hash_tokens(input.size());
    const auto hash_result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, parameters, {}, hash_tokens, hash_workspace, nullptr, variant);
    ASSERT_EQ(hash_result.error, LzssTypedEncodeError::none);
    hash_tokens.resize(hash_result.token_count);

    const auto binary_required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(binary_required.error, LzssBinaryTreeError::none);
    AlignedWorkspace binary_owner(binary_required.workspace_size);
    auto binary_workspace = binary_owner.bytes(binary_required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, UINT32_C(0xdeadbeef),
        UINT32_C(0xcafebabe)};
    std::vector<LzssTypedToken> binary_tokens(input.size(), sentinel);
    LzssMatchFinderStatistics binary_statistics{};
    const auto binary_result =
        encode_lzss_typed_tokens_binary_tree_single_pass(
            input, parameters, {}, binary_tokens, binary_workspace,
            &binary_statistics, variant);
    ASSERT_EQ(binary_result.error, LzssTypedEncodeError::none);
    EXPECT_EQ(binary_result.binary_tree_match_finder_error,
              LzssBinaryTreeError::none);
    EXPECT_EQ(binary_result.token_count, reference.size());
    EXPECT_EQ(binary_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(binary_statistics.query_count, binary_result.token_count);
    EXPECT_FALSE(binary_statistics.overflowed);
    ASSERT_EQ(hash_tokens.size(), reference.size());
    for (std::size_t index = 0; index < reference.size(); ++index) {
        EXPECT_TRUE(equal_token(hash_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(binary_tokens[index], reference[index]));
    }
    for (std::size_t index = reference.size(); index < binary_tokens.size();
         ++index) {
        EXPECT_TRUE(equal_token(binary_tokens[index], sentinel));
    }

    const auto byte_plan = plan_lzss_token_stream(input, parameters, {});
    ASSERT_EQ(byte_plan.error, LzssEncodeError::none);
    std::vector<std::byte> canonical(byte_plan.output_size);
    ASSERT_EQ(encode_lzss_token_stream(
                  input, parameters, {}, canonical).error,
              LzssEncodeError::none);
    EXPECT_EQ(serialize_typed_tokens(
                  std::span{binary_tokens}.first(binary_result.token_count)),
              canonical);
}

} // namespace

TEST(LzssTypedEncoder, PlansEmptyAndOneLiteralExactly) {
    auto plan = plan_lzss_typed_tokens({}, {}, {});
    EXPECT_EQ(plan.error, LzssTypedEncodeError::none);
    EXPECT_EQ(plan.input_size, 0U);
    EXPECT_EQ(plan.token_count, 0U);
    EXPECT_EQ(plan.token_storage_size, 0U);
    EXPECT_EQ(encode_lzss_typed_tokens({}, {}, {}, {}).error,
              LzssTypedEncodeError::none);

    const auto input = bytes("A");
    plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.token_storage_size, sizeof(LzssTypedToken));
    std::array<LzssTypedToken, 2> output{};
    output[1] = {LzssTypedTokenKind::match, 0, 123, 456};
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, output).error,
              LzssTypedEncodeError::none);
    EXPECT_TRUE(equal_token(
        output[0], {LzssTypedTokenKind::literal, 'A', 0, 0}));
    EXPECT_TRUE(equal_token(
        output[1], {LzssTypedTokenKind::match, 0, 123, 456}));
}

TEST(LzssTypedEncoder, RetainsStrictCostBoundaryAndOverlapMatch) {
    auto input = bytes("AAAAA");
    auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    EXPECT_EQ(plan.token_count, 5U);

    input = bytes("AAAAAA");
    plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_EQ(plan.token_count, 2U);
    std::array<LzssTypedToken, 2> output{};
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, output).error,
              LzssTypedEncodeError::none);
    EXPECT_TRUE(equal_token(
        output[0], {LzssTypedTokenKind::literal, 'A', 0, 0}));
    EXPECT_TRUE(equal_token(
        output[1], {LzssTypedTokenKind::match, 0, 1, 5}));
}

TEST(LzssTypedEncoder, MatchesCanonicalByteTokenParser) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto typed_plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(typed_plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> typed(typed_plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, typed).error,
              LzssTypedEncodeError::none);

    const auto byte_plan = plan_lzss_token_stream(input, {}, {});
    ASSERT_EQ(byte_plan.error, LzssEncodeError::none);
    std::vector<std::byte> serialized(byte_plan.output_size);
    ASSERT_EQ(encode_lzss_token_stream(input, {}, {}, serialized).error,
              LzssEncodeError::none);
    std::size_t offset{};
    std::size_t index{};
    while (offset < serialized.size()) {
        LzssToken parsed{};
        std::size_t consumed{};
        ASSERT_EQ(parse_lzss_token(
                      std::span<const std::byte>{serialized}.subspan(offset),
                      parsed, consumed),
                  LzssFormatError::none);
        ASSERT_LT(index, typed.size());
        EXPECT_TRUE(equal_token(typed[index], convert(parsed)));
        offset += consumed;
        ++index;
    }
    EXPECT_EQ(index, typed.size());
    const auto last_match = std::find_if(
        typed.rbegin(), typed.rend(),
        [](const LzssTypedToken& token) noexcept {
            return token.kind == LzssTypedTokenKind::match;
        });
    ASSERT_NE(last_match, typed.rend());
    EXPECT_EQ(last_match->distance, 6U);
    EXPECT_EQ(last_match->length, 5U);
}

TEST(LzssTypedEncoder, ReconstructsArbitraryBinaryInput) {
    std::vector<std::byte> input;
    for (std::uint32_t value = 0; value < 256; ++value) {
        input.push_back(static_cast<std::byte>(value));
    }
    input.insert(input.end(), input.begin(), input.end());
    const auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> tokens(plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, tokens).error,
              LzssTypedEncodeError::none);
    std::vector<std::byte> reconstructed(input.size());
    const LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(input.size()), 0};
    ASSERT_EQ(reconstruct_lzss_typed_frame(
                  tokens, {}, context, {}, reconstructed).error,
              LzssTypedReconstructError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedEncoder, ShortAndAliasedOutputAreAtomic) {
    const auto input = bytes("ABCABCABCX");
    const auto plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    ASSERT_GT(plan.token_count, 1U);
    std::vector<LzssTypedToken> short_output(plan.token_count - 1);
    for (auto& token : short_output) {
        token = {LzssTypedTokenKind::match, 0, 123, 456};
    }
    const auto before = short_output;
    auto result = encode_lzss_typed_tokens(
        input, {}, {}, short_output);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    ASSERT_EQ(short_output.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        EXPECT_TRUE(equal_token(short_output[index], before[index]));
    }

    std::array<LzssTypedToken, 4> aliased_storage{};
    auto storage_bytes = std::as_writable_bytes(std::span{aliased_storage});
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::ranges::copy(raw, storage_bytes.begin());
    const std::vector<std::byte> snapshot(
        storage_bytes.begin(), storage_bytes.end());
    result = encode_lzss_typed_tokens(
        std::span<const std::byte>{storage_bytes}.first(raw.size()), {}, {},
        aliased_storage);
    EXPECT_EQ(result.error, LzssTypedEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(snapshot, storage_bytes));
}

TEST(LzssTypedEncoder, EnforcesVariantAndWorkspaceLimits) {
    const auto input = bytes("ABCABCABCX");
    LzssParameters parameters{};
    parameters.max_match_length = 259;
    auto result = plan_lzss_typed_tokens(input, parameters, {});
    EXPECT_EQ(result.error, LzssTypedEncodeError::invalid_parameters);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = input.size() - 1;
    result = plan_lzss_typed_tokens(input, {}, limits);
    EXPECT_EQ(result.error, LzssTypedEncodeError::input_limit_exceeded);

    limits = {};
    const auto plan = plan_lzss_typed_tokens(input, {}, limits);
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes =
        input.size() + plan.token_storage_size - 1;
    result = plan_lzss_typed_tokens(input, {}, limits);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
}

TEST(LzssTypedEncoder, HashChainEmitsMatchBeyond64KiBForExtendedVariant) {
    constexpr std::size_t distance = 65537;
    std::vector<std::byte> input(distance + 5, std::byte{0});
    for (std::size_t index = 5; index < distance; ++index) {
        input[index] = static_cast<std::byte>(1 + ((index - 5) % 255));
    }
    marc::dictionary::internal::LzssParameters parameters{};
    parameters.window_size = 1048576;
    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(requirements.error, LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    std::vector<LzssTypedToken> tokens(input.size());
    LzssMatchFinderStatistics statistics{};
    const auto result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, parameters, {}, tokens, workspace, &statistics,
        LzssTypedTokenVariant::field_context_1m);
    ASSERT_EQ(result.error, LzssTypedEncodeError::none);
    tokens.resize(result.token_count);
    const auto match = std::ranges::find_if(
        tokens, [](const LzssTypedToken& token) {
            return token.kind == LzssTypedTokenKind::match
                && token.distance == 65537;
        });
    ASSERT_NE(match, tokens.end());
    EXPECT_EQ(match->length, 5U);

    EXPECT_EQ(encode_lzss_typed_tokens_hash_chain_single_pass(
                  input, parameters, {}, tokens, workspace).error,
              LzssTypedEncodeError::invalid_parameters);
}

TEST(LzssTypedEncoder, HashChainExactMatchesExhaustiveTokens) {
    auto input = bytes("ABCDE1ABCDE2ABCDE3");
    for (std::uint32_t value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());

    const auto reference_plan = plan_lzss_typed_tokens(input, {}, {});
    ASSERT_EQ(reference_plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> reference(reference_plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(input, {}, {}, reference).error,
              LzssTypedEncodeError::none);

    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(requirements.error, LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    const auto plan = plan_lzss_typed_tokens_hash_chain(
        input, {}, {}, workspace);
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    EXPECT_EQ(plan.token_count, reference_plan.token_count);
    EXPECT_EQ(plan.token_storage_size, reference_plan.token_storage_size);
    std::vector<LzssTypedToken> encoded(plan.token_count);
    const auto result = encode_lzss_typed_tokens_hash_chain(
        input, {}, {}, encoded, workspace);
    ASSERT_EQ(result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(encoded.size(), reference.size());
    for (std::size_t index = 0; index < encoded.size(); ++index)
        EXPECT_TRUE(equal_token(encoded[index], reference[index]));
}

TEST(LzssTypedEncoder, HashChainFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(requirements.error, LzssHashChainError::none);
    ASSERT_GT(requirements.workspace_size, 0U);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    const auto plan = plan_lzss_typed_tokens_hash_chain(
        input, {}, {}, workspace);
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);

    std::vector<LzssTypedToken> output(
        plan.token_count,
        {LzssTypedTokenKind::match, 0, 123, 456});
    const auto before = output;
    auto result = encode_lzss_typed_tokens_hash_chain(
        input, {}, {}, output,
        workspace.first(requirements.workspace_size - 1));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.match_finder_error,
              LzssHashChainError::workspace_too_small);
    for (std::size_t index = 0; index < output.size(); ++index)
        EXPECT_TRUE(equal_token(output[index], before[index]));

    std::vector<LzssTypedToken> aliased_storage(
        (requirements.workspace_size + sizeof(LzssTypedToken) - 1)
        / sizeof(LzssTypedToken));
    auto aliased_workspace = std::as_writable_bytes(
        std::span{aliased_storage}).first(requirements.workspace_size);
    ASSERT_GE(aliased_storage.size(), plan.token_count);
    const auto snapshot = std::vector<std::byte>(
        aliased_workspace.begin(), aliased_workspace.end());
    result = encode_lzss_typed_tokens_hash_chain(
        input, {}, {}, std::span{aliased_storage}.first(plan.token_count),
        aliased_workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(snapshot, aliased_workspace));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = input.size();
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes = input.size()
        + requirements.workspace_size + plan.token_storage_size - 1;
    result = plan_lzss_typed_tokens_hash_chain(
        input, {}, limits, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
}

TEST(LzssTypedEncoder, HashChainSinglePassMatchesAndQueriesOnce) {
    auto input = bytes("ABCDE1ABCDE2ABCDE3");
    for (std::uint32_t value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());
    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(requirements.error, LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);

    const auto plan = plan_lzss_typed_tokens_hash_chain(
        input, {}, {}, workspace);
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> reference(plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens_hash_chain(
                  input, {}, {}, reference, workspace).error,
              LzssTypedEncodeError::none);

    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};
    std::vector<LzssTypedToken> encoded(input.size(), sentinel);
    LzssMatchFinderStatistics statistics{};
    const auto result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, {}, {}, encoded, workspace, &statistics);
    ASSERT_EQ(result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(result.token_count, reference.size());
    EXPECT_EQ(result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(statistics.query_count, result.token_count);
    for (std::size_t index = 0; index < reference.size(); ++index)
        EXPECT_TRUE(equal_token(encoded[index], reference[index]));
    for (std::size_t index = reference.size(); index < encoded.size(); ++index)
        EXPECT_TRUE(equal_token(encoded[index], sentinel));
}

TEST(LzssTypedEncoder, HashChainSinglePassReservesWorstCaseAtomically) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(requirements.error, LzssHashChainError::none);
    AlignedWorkspace owner(requirements.workspace_size);
    auto workspace = owner.bytes(requirements.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};
    std::vector<LzssTypedToken> short_output(input.size() - 1, sentinel);
    auto result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, {}, {}, short_output, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_EQ(result.token_count, input.size());
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const LzssTypedToken& token) {
            return equal_token(token, sentinel);
        }));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = input.size();
    limits.max_block_size = input.size();
    const auto maximum_storage = input.size() * sizeof(LzssTypedToken);
    limits.max_internal_buffered_bytes =
        input.size() + requirements.workspace_size + maximum_storage - 1;
    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, {}, limits, output, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const LzssTypedToken& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder, BinaryTreePrivateEntryMatchesExactTokensAndBytes) {
    expect_binary_tree_typed_equals_exact(bytes(""));
    expect_binary_tree_typed_equals_exact(bytes("A"));
    expect_binary_tree_typed_equals_exact(bytes("AAAAAAAAAAAAAAAA"));
    expect_binary_tree_typed_equals_exact(bytes("ABCDE1ABCDE2ABCDE3"));

    std::vector<std::byte> all_values{};
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_binary_tree_typed_equals_exact(all_values);

    std::vector<std::byte> pseudorandom(1024);
    std::uint32_t state = UINT32_C(0x5d2a19c7);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_binary_tree_typed_equals_exact(pseudorandom);

    std::vector<std::byte> mixed{};
    for (std::size_t index = 0; index < 512; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 29 == 0 ? index & 0xffU : index % 7));
    }
    expect_binary_tree_typed_equals_exact(mixed);

    LzssParameters extended{};
    extended.window_size = 1U << 20;
    expect_binary_tree_typed_equals_exact(
        all_values, extended, LzssTypedTokenVariant::field_context_1m);
}

TEST(LzssTypedEncoder, BinaryTreePrivateEntryFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    auto workspace = owner.bytes(required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};

    std::vector<LzssTypedToken> short_output(input.size() - 1U, sentinel);
    auto result = encode_lzss_typed_tokens_binary_tree_single_pass(
        input, {}, {}, short_output, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result = encode_lzss_typed_tokens_binary_tree_single_pass(
        input, {}, {}, output, workspace.first(required.workspace_size - 1U));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.binary_tree_match_finder_error,
              LzssBinaryTreeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> input_alias(input.size(), sentinel);
    auto input_alias_bytes = std::as_writable_bytes(
        std::span{input_alias});
    std::ranges::copy(input, input_alias_bytes.begin());
    const auto input_alias_snapshot = input_alias;
    result = encode_lzss_typed_tokens_binary_tree_single_pass(
        std::span<const std::byte>{input_alias_bytes}.first(input.size()),
        {}, {}, input_alias, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        input_alias, input_alias_snapshot, equal_token));

    AlignedWorkspace input_workspace_owner(
        required.workspace_size + input.size());
    auto input_workspace = input_workspace_owner.bytes(
        required.workspace_size + input.size());
    std::ranges::copy(input, input_workspace.begin());
    std::ranges::fill(output, sentinel);
    result = encode_lzss_typed_tokens_binary_tree_single_pass(
        std::span<const std::byte>{input_workspace}.first(input.size()),
        {}, {}, output, input_workspace.first(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.binary_tree_match_finder_error,
              LzssBinaryTreeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    const auto aliased_token_count = std::max(
        input.size(),
        (required.workspace_size + sizeof(LzssTypedToken) - 1U)
            / sizeof(LzssTypedToken));
    std::vector<LzssTypedToken> output_workspace(
        aliased_token_count, sentinel);
    const auto output_workspace_snapshot = output_workspace;
    result = encode_lzss_typed_tokens_binary_tree_single_pass(
        input, {}, {}, std::span{output_workspace}.first(input.size()),
        std::as_writable_bytes(std::span{output_workspace})
            .first(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        output_workspace, output_workspace_snapshot, equal_token));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = input.size();
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes = input.size()
        + required.workspace_size
        + input.size() * sizeof(LzssTypedToken) - 1U;
    std::ranges::fill(output, sentinel);
    result = encode_lzss_typed_tokens_binary_tree_single_pass(
        input, {}, limits, output, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));
}
