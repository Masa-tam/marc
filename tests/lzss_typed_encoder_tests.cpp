#include "dictionary/lzss_typed_encoder.hpp"

#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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

void expect_private_match_finders_typed_equal_exact(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {},
    const LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k,
    const std::uint8_t sparse_reuse_threshold = 1) {
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

    const auto wavl_required = calculate_lzss_wavl_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(wavl_required.error, LzssWavlTreeError::none);
    AlignedWorkspace wavl_owner(wavl_required.workspace_size);
    auto wavl_workspace = wavl_owner.bytes(wavl_required.workspace_size);
    std::vector<LzssTypedToken> wavl_tokens(input.size(), sentinel);
    LzssMatchFinderStatistics wavl_statistics{};
    const auto wavl_result =
        encode_lzss_typed_tokens_wavl_tree_single_pass(
            input, parameters, {}, wavl_tokens, wavl_workspace,
            &wavl_statistics, variant);
    ASSERT_EQ(wavl_result.error, LzssTypedEncodeError::none);
    EXPECT_EQ(wavl_result.wavl_tree_match_finder_error,
              LzssWavlTreeError::none);
    EXPECT_EQ(wavl_result.token_count, reference.size());
    EXPECT_EQ(wavl_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(wavl_statistics.query_count, wavl_result.token_count);
    EXPECT_EQ(wavl_statistics.wavl_tree_insertion_count,
              input.size() < lzss_wavl_tree_prefix_size
                  ? 0U : input.size() - 4U);
    EXPECT_FALSE(wavl_statistics.overflowed);

    const auto red_black_required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(red_black_required.error, LzssRedBlackTreeError::none);
    AlignedWorkspace red_black_owner(red_black_required.workspace_size);
    auto red_black_workspace = red_black_owner.bytes(
        red_black_required.workspace_size);
    std::vector<LzssTypedToken> red_black_tokens(input.size(), sentinel);
    LzssMatchFinderStatistics red_black_statistics{};
    const auto red_black_result =
        encode_lzss_typed_tokens_red_black_tree_single_pass(
            input, parameters, {}, red_black_tokens, red_black_workspace,
            &red_black_statistics, variant);
    ASSERT_EQ(red_black_result.error, LzssTypedEncodeError::none);
    EXPECT_EQ(red_black_result.red_black_tree_match_finder_error,
              LzssRedBlackTreeError::none);
    EXPECT_EQ(red_black_result.token_count, reference.size());
    EXPECT_EQ(red_black_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(red_black_statistics.query_count,
              red_black_result.token_count);
    EXPECT_EQ(red_black_statistics.red_black_tree_insertion_count,
              input.size() < lzss_red_black_tree_prefix_size
                  ? 0U : input.size() - 4U);
    EXPECT_FALSE(red_black_statistics.overflowed);

    const auto scapegoat_required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(scapegoat_required.error, LzssScapegoatTreeError::none);
    AlignedWorkspace scapegoat_owner(scapegoat_required.workspace_size);
    auto scapegoat_workspace = scapegoat_owner.bytes(
        scapegoat_required.workspace_size);
    std::vector<LzssTypedToken> scapegoat_tokens(input.size(), sentinel);
    const auto scapegoat_result =
        encode_lzss_typed_tokens_scapegoat_tree_single_pass(
            input, parameters, {}, scapegoat_tokens, scapegoat_workspace,
            nullptr, variant);
    ASSERT_EQ(scapegoat_result.error, LzssTypedEncodeError::none);
    EXPECT_EQ(scapegoat_result.scapegoat_tree_match_finder_error,
              LzssScapegoatTreeError::none);
    EXPECT_EQ(scapegoat_result.token_count, reference.size());
    EXPECT_EQ(scapegoat_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));

    const auto sparse_capacity =
        input.size() < lzss_match_finder_prefix_size
        ? 0U
        : std::min<std::size_t>(input.size(), parameters.window_size);
    const LzssSparseHashTreeMatchFinderOptions sparse_options{
        sparse_capacity, 0, sparse_reuse_threshold};
    const auto sparse_required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, sparse_capacity,
        sparse_reuse_threshold);
    ASSERT_EQ(sparse_required.error, LzssSparseHashTreeError::none);
    AlignedWorkspace sparse_owner(sparse_required.workspace_size);
    auto sparse_workspace = sparse_owner.bytes(sparse_required.workspace_size);
    std::vector<LzssTypedToken> sparse_tokens(input.size(), sentinel);
    LzssMatchFinderStatistics sparse_statistics{};
    const auto sparse_result =
        encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
            input, parameters, {}, sparse_tokens, sparse_workspace,
            sparse_options, &sparse_statistics, variant);
    ASSERT_EQ(sparse_result.error, LzssTypedEncodeError::none);
    EXPECT_EQ(sparse_result.sparse_hash_tree_match_finder_error,
              LzssSparseHashTreeMatchFinderError::none);
    EXPECT_EQ(sparse_result.token_count, reference.size());
    EXPECT_EQ(sparse_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(sparse_statistics.query_count, sparse_result.token_count);
    EXPECT_FALSE(sparse_statistics.overflowed);

    for (std::size_t index = 0; index < reference.size(); ++index) {
        EXPECT_TRUE(equal_token(hash_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(binary_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(wavl_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(red_black_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(scapegoat_tokens[index], reference[index]));
        EXPECT_TRUE(equal_token(sparse_tokens[index], reference[index]));
    }
    for (std::size_t index = reference.size(); index < binary_tokens.size();
         ++index) {
        EXPECT_TRUE(equal_token(binary_tokens[index], sentinel));
        EXPECT_TRUE(equal_token(wavl_tokens[index], sentinel));
        EXPECT_TRUE(equal_token(red_black_tokens[index], sentinel));
        EXPECT_TRUE(equal_token(scapegoat_tokens[index], sentinel));
        EXPECT_TRUE(equal_token(sparse_tokens[index], sentinel));
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
    EXPECT_EQ(serialize_typed_tokens(
                  std::span{wavl_tokens}.first(wavl_result.token_count)),
              canonical);
    EXPECT_EQ(serialize_typed_tokens(
                  std::span{red_black_tokens}.first(
                      red_black_result.token_count)),
              canonical);
    EXPECT_EQ(serialize_typed_tokens(
                  std::span{scapegoat_tokens}.first(
                      scapegoat_result.token_count)),
              canonical);
    EXPECT_EQ(serialize_typed_tokens(
                  std::span{sparse_tokens}.first(sparse_result.token_count)),
              canonical);
}

void expect_mnemonic_mixer_typed_equal_exact(
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

    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    AlignedWorkspace legacy_owner(required.workspace_size);
    AlignedWorkspace mnemonic_owner(required.workspace_size);
    std::vector<LzssTypedToken> legacy(input.size());
    std::vector<LzssTypedToken> mnemonic(input.size());
    LzssMatchFinderStatistics legacy_statistics{};
    LzssMatchFinderStatistics mnemonic_statistics{};
    const auto legacy_result =
        encode_lzss_typed_tokens_hash_chain_single_pass(
            input, parameters, {}, legacy,
            legacy_owner.bytes(required.workspace_size),
            &legacy_statistics, variant);
    const auto mnemonic_result =
        encode_lzss_typed_tokens_hash_chain_mnemonic_mixer_v1_single_pass(
            input, parameters, {}, mnemonic,
            mnemonic_owner.bytes(required.workspace_size),
            &mnemonic_statistics, variant);
    ASSERT_EQ(legacy_result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(mnemonic_result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(legacy_result.token_count, reference.size());
    ASSERT_EQ(mnemonic_result.token_count, reference.size());
    EXPECT_EQ(legacy_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(mnemonic_result.token_storage_size,
              reference.size() * sizeof(LzssTypedToken));
    EXPECT_EQ(legacy_statistics.query_count, reference.size());
    EXPECT_EQ(mnemonic_statistics.query_count, reference.size());
    EXPECT_FALSE(legacy_statistics.overflowed);
    EXPECT_FALSE(mnemonic_statistics.overflowed);

    legacy.resize(legacy_result.token_count);
    mnemonic.resize(mnemonic_result.token_count);
    for (std::size_t index = 0; index < reference.size(); ++index) {
        EXPECT_TRUE(equal_token(legacy[index], reference[index])) << index;
        EXPECT_TRUE(equal_token(mnemonic[index], reference[index])) << index;
    }

    const auto byte_plan = plan_lzss_token_stream(input, parameters, {});
    ASSERT_EQ(byte_plan.error, LzssEncodeError::none);
    std::vector<std::byte> canonical(byte_plan.output_size);
    ASSERT_EQ(encode_lzss_token_stream(
                  input, parameters, {}, canonical).error,
              LzssEncodeError::none);
    EXPECT_EQ(serialize_typed_tokens(reference), canonical);
    EXPECT_EQ(serialize_typed_tokens(legacy), canonical);
    EXPECT_EQ(serialize_typed_tokens(mnemonic), canonical);
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

TEST(LzssTypedEncoder, PrivateMatchFinderEntriesMatchExactTokensAndBytes) {
    expect_private_match_finders_typed_equal_exact(bytes(""));
    expect_private_match_finders_typed_equal_exact(bytes("A"));
    expect_private_match_finders_typed_equal_exact(
        bytes("AAAAAAAAAAAAAAAA"));
    expect_private_match_finders_typed_equal_exact(
        bytes("ABCDE1ABCDE2ABCDE3"));

    std::vector<std::byte> all_values{};
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_private_match_finders_typed_equal_exact(all_values);

    std::vector<std::byte> pseudorandom(1024);
    std::uint32_t state = UINT32_C(0x5d2a19c7);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_private_match_finders_typed_equal_exact(pseudorandom);

    std::vector<std::byte> mixed{};
    for (std::size_t index = 0; index < 512; ++index) {
        mixed.push_back(static_cast<std::byte>(
            index % 29 == 0 ? index & 0xffU : index % 7));
    }
    expect_private_match_finders_typed_equal_exact(mixed);

    LzssParameters extended{};
    extended.window_size = 1U << 20;
    expect_private_match_finders_typed_equal_exact(
        all_values, extended, LzssTypedTokenVariant::field_context_1m);
}

TEST(LzssTypedEncoder,
     MnemonicMixerSinglePassMatchesExactTokensAndBytes) {
    expect_mnemonic_mixer_typed_equal_exact(bytes(""));
    expect_mnemonic_mixer_typed_equal_exact(bytes("A"));
    expect_mnemonic_mixer_typed_equal_exact(
        bytes("AAAAAAAAAAAAAAAA"));
    expect_mnemonic_mixer_typed_equal_exact(
        bytes("ABCDE1ABCDE2ABCDE3"));

    std::vector<std::byte> all_values{};
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_mnemonic_mixer_typed_equal_exact(all_values);

    std::vector<std::byte> collision_records{};
    for (std::uint32_t record = 0; record < 256; ++record) {
        const auto first = (record & 1U) == 0U
            ? std::array{std::byte{1}, std::byte{0}, std::byte{0},
                         std::byte{0x58}, std::byte{0x59}}
            : std::array{std::byte{0}, std::byte{0x20}, std::byte{0},
                         std::byte{0x58}, std::byte{0x59}};
        collision_records.insert(
            collision_records.end(), first.begin(), first.end());
        collision_records.push_back(static_cast<std::byte>(record));
    }
    expect_mnemonic_mixer_typed_equal_exact(collision_records);

    LzssParameters extended{};
    extended.window_size = 1U << 20;
    expect_mnemonic_mixer_typed_equal_exact(
        all_values, extended, LzssTypedTokenVariant::field_context_1m);
}

TEST(LzssTypedEncoder,
     MnemonicMixerSinglePassFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};

    std::vector<LzssTypedToken> short_output(input.size() - 1U, sentinel);
    auto result =
        encode_lzss_typed_tokens_hash_chain_mnemonic_mixer_v1_single_pass(
            input, {}, {}, short_output,
            owner.bytes(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_EQ(result.token_count, input.size());
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const LzssTypedToken& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result =
        encode_lzss_typed_tokens_hash_chain_mnemonic_mixer_v1_single_pass(
            input, {}, {}, output,
            owner.bytes(required.workspace_size - 1U));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.match_finder_error,
              LzssHashChainError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const LzssTypedToken& token) {
            return equal_token(token, sentinel);
        }));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = input.size();
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes = input.size()
        + required.workspace_size
        + input.size() * sizeof(LzssTypedToken) - 1U;
    result =
        encode_lzss_typed_tokens_hash_chain_mnemonic_mixer_v1_single_pass(
            input, {}, limits, output,
            owner.bytes(required.workspace_size));
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const LzssTypedToken& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder,
     MnemonicMixerDeterministicBoundedFuzzMatchesExactBytes) {
    std::uint32_t state = UINT32_C(0x9e3779b9);
    constexpr std::array<std::uint32_t, 6> windows{
        1U, 5U, 17U, 64U, 257U, 65'536U};
    constexpr std::array<std::uint32_t, 4> maximum_lengths{
        5U, 17U, 67U, 258U};
    for (std::size_t case_index = 0; case_index < 192; ++case_index) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        const auto size = static_cast<std::size_t>((state >> 16U) % 258U);
        std::vector<std::byte> input(size);
        for (std::size_t index = 0; index < size; ++index) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            const auto mode = case_index % 4U;
            const auto value = mode == 0U ? state >> 24U
                : mode == 1U ? state % 7U
                : mode == 2U ? (index + case_index) % 19U
                             : ((index / 5U) ^ (state >> 29U)) & 0xffU;
            input[index] = static_cast<std::byte>(value);
        }
        LzssParameters parameters{};
        parameters.window_size = windows[case_index % windows.size()];
        parameters.max_match_length =
            maximum_lengths[(case_index / windows.size())
                            % maximum_lengths.size()];
        expect_mnemonic_mixer_typed_equal_exact(input, parameters);
    }
}

TEST(LzssTypedEncoder,
     SparseReuseGateMatchesExactTokensAcrossInputClasses) {
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 15;

    std::vector<std::byte> binary_unit{};
    binary_unit.reserve(128);
    for (std::uint32_t value = 0; value < 128; ++value) {
        binary_unit.push_back(static_cast<std::byte>(value));
    }
    std::vector<std::byte> binary{binary_unit};
    binary.insert(binary.end(), binary_unit.begin(), binary_unit.end());

    std::vector<std::byte> repeated(257, std::byte{0xa5});

    std::vector<std::byte> boundary_unit{};
    boundary_unit.reserve(33);
    for (std::size_t index = 0; index < 33; ++index) {
        boundary_unit.push_back(
            static_cast<std::byte>((index * 37U) & 0xffU));
    }
    std::vector<std::byte> boundary{};
    boundary.reserve(boundary_unit.size() * 4U);
    for (std::size_t repetition = 0; repetition < 4; ++repetition) {
        boundary.insert(
            boundary.end(), boundary_unit.begin(), boundary_unit.end());
    }

    for (const auto& input : {binary, repeated, boundary}) {
        expect_private_match_finders_typed_equal_exact(
            input, parameters, LzssTypedTokenVariant::field_context_64k, 1);
        expect_private_match_finders_typed_equal_exact(
            input, parameters, LzssTypedTokenVariant::field_context_64k, 2);
    }

    expect_private_match_finders_typed_equal_exact(
        boundary, parameters, LzssTypedTokenVariant::field_context_64k,
        std::numeric_limits<std::uint8_t>::max());
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

TEST(LzssTypedEncoder, WavlTreePrivateEntryFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    auto workspace = owner.bytes(required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};

    std::vector<LzssTypedToken> short_output(input.size() - 1U, sentinel);
    auto result = encode_lzss_typed_tokens_wavl_tree_single_pass(
        input, {}, {}, short_output, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result = encode_lzss_typed_tokens_wavl_tree_single_pass(
        input, {}, {}, output, workspace.first(required.workspace_size - 1U));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.wavl_tree_match_finder_error,
              LzssWavlTreeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> input_alias(input.size(), sentinel);
    auto input_alias_bytes = std::as_writable_bytes(std::span{input_alias});
    std::ranges::copy(input, input_alias_bytes.begin());
    const auto input_alias_snapshot = input_alias;
    result = encode_lzss_typed_tokens_wavl_tree_single_pass(
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
    result = encode_lzss_typed_tokens_wavl_tree_single_pass(
        std::span<const std::byte>{input_workspace}.first(input.size()),
        {}, {}, output, input_workspace.first(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.wavl_tree_match_finder_error,
              LzssWavlTreeError::overlapping_buffers);
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
    result = encode_lzss_typed_tokens_wavl_tree_single_pass(
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
    result = encode_lzss_typed_tokens_wavl_tree_single_pass(
        input, {}, limits, output, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder,
     RedBlackTreePrivateEntryFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    auto workspace = owner.bytes(required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};

    std::vector<LzssTypedToken> short_output(input.size() - 1U, sentinel);
    auto result = encode_lzss_typed_tokens_red_black_tree_single_pass(
        input, {}, {}, short_output, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result = encode_lzss_typed_tokens_red_black_tree_single_pass(
        input, {}, {}, output, workspace.first(required.workspace_size - 1U));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.red_black_tree_match_finder_error,
              LzssRedBlackTreeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> input_alias(input.size(), sentinel);
    auto input_alias_bytes = std::as_writable_bytes(std::span{input_alias});
    std::ranges::copy(input, input_alias_bytes.begin());
    const auto input_alias_snapshot = input_alias;
    result = encode_lzss_typed_tokens_red_black_tree_single_pass(
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
    result = encode_lzss_typed_tokens_red_black_tree_single_pass(
        std::span<const std::byte>{input_workspace}.first(input.size()),
        {}, {}, output, input_workspace.first(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.red_black_tree_match_finder_error,
              LzssRedBlackTreeError::overlapping_buffers);
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
    result = encode_lzss_typed_tokens_red_black_tree_single_pass(
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
    result = encode_lzss_typed_tokens_red_black_tree_single_pass(
        input, {}, limits, output, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder,
     ScapegoatTreePrivateEntryFailuresAreAtomicAndBounded) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    auto workspace = owner.bytes(required.workspace_size);
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};

    std::vector<LzssTypedToken> short_output(input.size() - 1U, sentinel);
    auto result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
        input, {}, {}, short_output, workspace);
    EXPECT_EQ(result.error, LzssTypedEncodeError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> output(input.size(), sentinel);
    result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
        input, {}, {}, output, workspace.first(required.workspace_size - 1U));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.scapegoat_tree_match_finder_error,
              LzssScapegoatTreeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    std::vector<LzssTypedToken> input_alias(input.size(), sentinel);
    auto input_alias_bytes = std::as_writable_bytes(std::span{input_alias});
    std::ranges::copy(input, input_alias_bytes.begin());
    const auto input_alias_snapshot = input_alias;
    result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
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
    result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
        std::span<const std::byte>{input_workspace}.first(input.size()),
        {}, {}, output, input_workspace.first(required.workspace_size));
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.scapegoat_tree_match_finder_error,
              LzssScapegoatTreeError::overlapping_buffers);
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
    result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
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
    result = encode_lzss_typed_tokens_scapegoat_tree_single_pass(
        input, {}, limits, output, workspace);
    EXPECT_EQ(result.error,
              LzssTypedEncodeError::token_storage_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder,
     SparseHashTreePrivateEntryRejectsCapacityAndShortWorkspaceAtomically) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const LzssTypedToken sentinel{
        LzssTypedTokenKind::match, 0, 123, 456};
    std::vector<LzssTypedToken> output(input.size(), sentinel);

    LzssSparseHashTreeMatchFinderOptions options{
        input.size() + 1U, 0};
    auto result = encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
        input, {}, {}, output, {}, options);
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.sparse_hash_tree_match_finder_error,
              LzssSparseHashTreeMatchFinderError::invalid_pool_capacity);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));

    options.pool_node_capacity = 4;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    result = encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
        input, {}, {}, output,
        owner.bytes(required.workspace_size - 1U), options);
    EXPECT_EQ(result.error, LzssTypedEncodeError::match_finder_error);
    EXPECT_EQ(result.sparse_hash_tree_match_finder_error,
              LzssSparseHashTreeMatchFinderError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [&sentinel](const auto& token) {
            return equal_token(token, sentinel);
        }));
}

TEST(LzssTypedEncoder,
     SparseHashTreeImmutableSnapshotOptionMatchesReference) {
    const std::vector<std::byte> input(320, std::byte{'A'});
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;
    const auto reference_plan = plan_lzss_typed_tokens(
        input, parameters, {});
    ASSERT_EQ(reference_plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> reference(reference_plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(
                  input, parameters, {}, reference).error,
              LzssTypedEncodeError::none);

    LzssSparseHashTreeMatchFinderOptions options{
        std::min<std::size_t>(input.size(), parameters.window_size), 0};
    options.lifecycle_mode =
        LzssSparseHashTreeLifecycleMode::immutable_snapshot;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    AlignedWorkspace owner(required.workspace_size);
    std::vector<LzssTypedToken> actual(input.size());
    LzssMatchFinderStatistics statistics{};
    const auto result =
        encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
            input, parameters, {}, actual,
            owner.bytes(required.workspace_size), options, &statistics);
    ASSERT_EQ(result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(result.sparse_hash_tree_match_finder_error,
              LzssSparseHashTreeMatchFinderError::none);
    actual.resize(result.token_count);
    ASSERT_EQ(actual.size(), reference.size());
    for (std::size_t index = 0; index < reference.size(); ++index) {
        EXPECT_TRUE(equal_token(actual[index], reference[index])) << index;
    }
    EXPECT_GT(statistics.hash_tree_snapshot_promotion_count, 1U);
    EXPECT_GT(statistics.hash_tree_snapshot_query_count, 0U);
    EXPECT_GT(statistics.hash_tree_snapshot_expiration_count, 0U);
    EXPECT_EQ(statistics.hash_tree_snapshot_expiration_count,
              statistics.hash_tree_snapshot_bulk_release_count);
    EXPECT_EQ(statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(statistics.hash_tree_retirement_count, 0U);
}

TEST(LzssTypedEncoder,
     SparseSnapshotDeltaBudgetMatchesExactTokensAndBytes) {
    const auto input = bytes(
        "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh"
        "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh");
    LzssParameters parameters{};
    parameters.window_size = 40;
    parameters.max_match_length = 6;
    const auto plan = plan_lzss_typed_tokens(input, parameters, {});
    ASSERT_EQ(plan.error, LzssTypedEncodeError::none);
    std::vector<LzssTypedToken> reference(plan.token_count);
    ASSERT_EQ(encode_lzss_typed_tokens(
                  input, parameters, {}, reference).error,
              LzssTypedEncodeError::none);

    const auto hash_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(hash_required.error, LzssHashChainError::none);
    AlignedWorkspace hash_owner(hash_required.workspace_size);
    std::vector<LzssTypedToken> hash_tokens(input.size());
    const auto hash_result =
        encode_lzss_typed_tokens_hash_chain_single_pass(
            input, parameters, {}, hash_tokens,
            hash_owner.bytes(hash_required.workspace_size));
    ASSERT_EQ(hash_result.error, LzssTypedEncodeError::none);
    hash_tokens.resize(hash_result.token_count);

    const auto capacity =
        std::min<std::size_t>(input.size(), parameters.window_size);
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);

    auto encode_snapshot = [&](const std::size_t budget,
                               LzssMatchFinderStatistics& statistics) {
        AlignedWorkspace owner(required.workspace_size);
        std::vector<LzssTypedToken> tokens(input.size());
        LzssSparseHashTreeMatchFinderOptions options{capacity, 0};
        options.lifecycle_mode =
            LzssSparseHashTreeLifecycleMode::immutable_snapshot;
        options.snapshot_delta_candidate_budget = budget;
        const auto result =
            encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
                input, parameters, {}, tokens,
                owner.bytes(required.workspace_size), options, &statistics);
        EXPECT_EQ(result.error, LzssTypedEncodeError::none);
        EXPECT_EQ(result.sparse_hash_tree_match_finder_error,
                  LzssSparseHashTreeMatchFinderError::none);
        tokens.resize(result.token_count);
        return tokens;
    };

    LzssMatchFinderStatistics disabled_statistics{};
    const auto disabled = encode_snapshot(0, disabled_statistics);
    LzssMatchFinderStatistics budgeted_statistics{};
    const auto budgeted = encode_snapshot(1, budgeted_statistics);

    ASSERT_EQ(disabled.size(), reference.size());
    ASSERT_EQ(budgeted.size(), reference.size());
    ASSERT_EQ(hash_tokens.size(), reference.size());
    for (std::size_t index = 0; index < reference.size(); ++index) {
        EXPECT_TRUE(equal_token(hash_tokens[index], reference[index]))
            << index;
        EXPECT_TRUE(equal_token(disabled[index], reference[index])) << index;
        EXPECT_TRUE(equal_token(budgeted[index], reference[index])) << index;
    }
    const auto reference_bytes = serialize_typed_tokens(reference);
    EXPECT_EQ(serialize_typed_tokens(hash_tokens), reference_bytes);
    EXPECT_EQ(serialize_typed_tokens(disabled), reference_bytes);
    EXPECT_EQ(serialize_typed_tokens(budgeted), reference_bytes);
    EXPECT_EQ(
        disabled_statistics.hash_tree_snapshot_delta_budget_query_count,
        0U);
    EXPECT_GT(
        budgeted_statistics.hash_tree_snapshot_delta_budget_query_count,
        0U);
    EXPECT_GT(
        budgeted_statistics.hash_tree_snapshot_delta_budget_breach_count,
        0U);
    EXPECT_EQ(
        budgeted_statistics.hash_tree_snapshot_delta_budget_breach_count,
        budgeted_statistics.hash_tree_snapshot_delta_budget_demotion_count);
    EXPECT_FALSE(budgeted_statistics.overflowed);
}

TEST(LzssTypedEncoder,
     SparseHashTreeImmutableSnapshotShortInputUsesCommonQueryPath) {
    const auto input = bytes("A");
    LzssSparseHashTreeMatchFinderOptions options{};
    options.lifecycle_mode =
        LzssSparseHashTreeLifecycleMode::immutable_snapshot;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), {}, {}, options.pool_node_capacity);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    AlignedWorkspace owner(required.workspace_size);
    std::vector<LzssTypedToken> tokens(input.size());
    LzssMatchFinderStatistics statistics{};

    const auto result =
        encode_lzss_typed_tokens_sparse_hash_tree_single_pass(
            input, {}, {}, tokens, owner.bytes(required.workspace_size),
            options, &statistics);

    ASSERT_EQ(result.error, LzssTypedEncodeError::none);
    ASSERT_EQ(result.sparse_hash_tree_match_finder_error,
              LzssSparseHashTreeMatchFinderError::none);
    ASSERT_EQ(result.token_count, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, static_cast<std::uint8_t>('A'));
    EXPECT_EQ(statistics.query_count, 1U);
    EXPECT_EQ(statistics.hash_tree_chain_query_count, 1U);
    EXPECT_EQ(statistics.hash_tree_snapshot_query_count, 0U);
    EXPECT_FALSE(statistics.overflowed);
}
