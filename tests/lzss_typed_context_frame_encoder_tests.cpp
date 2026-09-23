#include "frame/lzss_typed_context_frame_encoder.hpp"

#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
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

[[nodiscard]] TypedContextStreamHeader four_mib_stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    auto stream = stream_config(frame_size, original_size);
    stream.dictionary.window_size = 4194304;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    return stream;
}

[[nodiscard]] TypedContextStreamHeader sixteen_mib_stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    auto stream = stream_config(frame_size, original_size);
    stream.dictionary.window_size = 16777216;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    return stream;
}

[[nodiscard]] TypedContextStreamHeader sixty_four_mib_stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    auto stream = stream_config(frame_size, original_size);
    stream.dictionary.window_size = 67108864;
    stream.dictionary_variant = 6;
    stream.context_variant = 5;
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

TEST(LzssTypedContextFrameEncoder,
     ProbeAndNoProbeTokensReproduceCompleteFramesAcrossProfiles) {
    using namespace marc::dictionary::internal;
    using namespace marc::entropy::internal;
    using namespace marc::context::internal;
    constexpr std::array windows{65'536U, 1U << 20, 4U << 20,
                                  16U << 20, 64U << 20};
    constexpr std::string_view pattern = "ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ";
    for (std::size_t profile = 0; profile < windows.size(); ++profile) {
        for (const unsigned mode : {0U, 1U, 2U}) {
            SCOPED_TRACE(profile);
            SCOPED_TRACE(mode);
            std::vector<std::byte> input(1039);
            for (std::size_t i = 0; i < input.size(); ++i) {
                input[i] = mode == 0 ? std::byte{'A'}
                    : mode == 1 ? static_cast<std::byte>(pattern[i % pattern.size()])
                                : static_cast<std::byte>((i * 73 + i / 13) % 256);
            }
            auto stream = stream_config(513, input.size());
            stream.frame_size = 513;
            stream.dictionary.window_size = windows[profile];
            stream.dictionary_variant = static_cast<std::uint16_t>(profile + 2);
            stream.context_variant = static_cast<std::uint16_t>(profile + 1);
            auto limits = marc::core::DecoderLimits{};
            limits.max_lz_distance = windows[profile];
            const auto selected = select_lzss_field_context_layout(
                stream.dictionary_variant, stream.context_algorithm,
                stream.context_variant);
            ASSERT_EQ(selected.error, LzssFieldContextLayoutError::none);
            std::uint64_t sequence{};
            for (std::size_t offset = 0; offset < input.size(); offset += stream.frame_size, ++sequence) {
                const auto raw = std::span<const std::byte>{input}.subspan(
                    offset, std::min<std::size_t>(stream.frame_size, input.size() - offset));
                const auto required = calculate_lzss_hash_chain_workspace(
                    raw.size(), stream.dictionary, limits);
                ASSERT_EQ(required.error, LzssHashChainError::none);
                AlignedWorkspace owner(required.workspace_size);
                auto workspace = owner.bytes(required.workspace_size);
                std::vector<LzssTypedToken> tokens(raw.size());
                std::vector<ModeledOperation> operations(raw.size() * 5);
                const auto plan = plan_lzss_typed_context_frame_hash_chain(
                    stream, limits, sequence, offset, raw, tokens, operations, workspace);
                ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
                std::vector<std::byte> baseline(plan.serialized_size);
                ASSERT_EQ(encode_lzss_typed_context_frame_hash_chain(
                              stream, limits, sequence, offset, raw, tokens,
                              operations, workspace, baseline).error,
                          LzssTypedContextFrameEncodeError::none);
                for (const bool probe : {false, true}) {
                    SCOPED_TRACE(probe);
                    LzssMatchFinderStatistics statistics{};
                    const auto token_result = probe
                        ? encode_lzss_typed_tokens_hash_chain_best_length_probe_single_pass(
                              raw, stream.dictionary, limits, tokens, workspace,
                              &statistics, selected.layout.dictionary_variant)
                        : encode_lzss_typed_tokens_hash_chain_no_probe_single_pass(
                              raw, stream.dictionary, limits, tokens, workspace,
                              &statistics, selected.layout.dictionary_variant);
                    ASSERT_EQ(token_result.error, LzssTypedEncodeError::none);
                    ASSERT_EQ(token_result.token_count, plan.token_count);
                    const LzssTypedFrameValidationContext context{
                        static_cast<std::uint32_t>(token_result.token_count),
                        static_cast<std::uint32_t>(raw.size()), offset};
                    const auto modeled = model_lzss_field_context_tokens(
                        std::span<const LzssTypedToken>{tokens}.first(token_result.token_count),
                        stream.dictionary, context, limits, operations,
                        selected.layout.context_variant);
                    ASSERT_EQ(modeled.error, LzssFieldContextError::none);
                    EXPECT_EQ(modeled.operation_count, plan.operation_count);
                    EXPECT_EQ(modeled.decision_count, plan.decision_count);
                    ContextualDynamicRangeDescriptor descriptor{};
                    constexpr auto payload_offset = typed_context_frame_header_size
                        + typed_context_range_descriptor_size;
                    std::vector<std::byte> encoded(plan.serialized_size + 1, std::byte{0xcc});
                    const auto entropy = encode_contextual_dynamic_range_operations(
                        std::span<const ModeledOperation>{operations}.first(modeled.operation_count),
                        limits, std::span{encoded}.subspan(payload_offset, plan.payload_size),
                        descriptor, selected.layout.context_variant);
                    ASSERT_EQ(entropy.error, ContextualDynamicRangeEncodeError::none);
                    ASSERT_EQ(entropy.payload_size, plan.payload_size);
                    EXPECT_EQ(entropy.decision_count, plan.decision_count);
                    TypedContextFrameHeader header{};
                    header.sequence = sequence;
                    header.uncompressed_size = static_cast<std::uint32_t>(raw.size());
                    header.token_count = static_cast<std::uint32_t>(token_result.token_count);
                    header.event_count = static_cast<std::uint32_t>(modeled.operation_count);
                    header.decision_count = entropy.decision_count;
                    header.payload_size = static_cast<std::uint32_t>(entropy.payload_size);
                    header.descriptor_size = typed_context_range_descriptor_size;
                    ASSERT_EQ(serialize_typed_context_frame_header(
                                  header, {stream, limits, sequence, offset},
                                  std::span<std::byte, typed_context_frame_header_size>{
                                      encoded.data(), typed_context_frame_header_size}),
                              TypedContextFrameHeaderError::none);
                    ASSERT_EQ(serialize_typed_context_range_descriptor(
                                  descriptor, header, limits,
                                  std::span<std::byte, typed_context_range_descriptor_size>{
                                      encoded.data() + typed_context_frame_header_size,
                                      typed_context_range_descriptor_size}),
                              TypedContextRangeDescriptorError::none);
                    EXPECT_EQ(encoded.back(), std::byte{0xcc});
                    encoded.pop_back();
                    EXPECT_EQ(encoded, baseline);
                    std::vector<LzssTypedToken> restored_tokens(plan.token_count);
                    std::vector<std::byte> restored(raw.size());
                    const auto decoded = decode_lzss_typed_context_frame(
                        encoded, {stream, limits, sequence, offset}, restored_tokens, restored);
                    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
                    EXPECT_EQ(decoded.serialized_consumed, encoded.size());
                    EXPECT_TRUE(std::ranges::equal(restored, raw));
                    EXPECT_FALSE(statistics.overflowed);
                    if (!probe) {
                        EXPECT_EQ(statistics.hash_chain_best_length_probe_comparison_count, 0U);
                        EXPECT_EQ(statistics.hash_chain_best_length_probe_pruned_candidate_count, 0U);
                    }
                }
            }
        }
    }
}

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

TEST(LzssTypedContextFrameEncoder,
     FourMiBVariantEmitsAndDecodesOneLiteralFrame) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = four_mib_stream_config(4194304, 1);
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    std::array<std::byte, 86> output{};
    const auto plan = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(plan.error, LzssTypedContextFrameEncodeError::none);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, {}, 0, 0, input, tokens, operations, output).error,
              LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(output, one_literal_frame());

    std::array<marc::dictionary::internal::LzssTypedToken, 1>
        decoded_tokens{};
    std::array<std::byte, 1> reconstructed{};
    const auto decoded = decode_lzss_typed_context_frame(
        output, {stream, {}, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedContextFrameEncoder,
     SixteenMiBVariantEmitsAndDecodesOneLiteralFrame) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = sixteen_mib_stream_config(16777216, 1);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16777216;
    limits.max_lz_distance = 16777216;
    limits.max_entropy_table_entries = 4582;
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    std::array<std::byte, 86> output{};
    ASSERT_EQ(plan_lzss_typed_context_frame(
                  stream, limits, 0, 0, input, tokens, operations).error,
              LzssTypedContextFrameEncodeError::none);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, limits, 0, 0, input, tokens, operations, output)
                  .error,
              LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(output, one_literal_frame());

    std::array<marc::dictionary::internal::LzssTypedToken, 1>
        decoded_tokens{};
    std::array<std::byte, 1> reconstructed{};
    const auto decoded = decode_lzss_typed_context_frame(
        output, {stream, limits, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);
}

TEST(LzssTypedContextFrameEncoder,
     SixtyFourMiBVariantEmitsAndDecodesOneLiteralFrame) {
    constexpr std::array input{std::byte{'A'}};
    const auto stream = sixty_four_mib_stream_config(67108864, 1);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 67108864;
    limits.max_block_size = 67108864;
    limits.max_lz_distance = 67108864;
    limits.max_entropy_table_entries = 4598;
    limits.max_internal_buffered_bytes = UINT64_C(8) * 1024 * 1024 * 1024;
    std::array<marc::dictionary::internal::LzssTypedToken, 1> tokens{};
    std::array<marc::context::internal::ModeledOperation, 2> operations{};
    std::array<std::byte, 86> output{};

    ASSERT_EQ(plan_lzss_typed_context_frame(
                  stream, limits, 0, 0, input, tokens, operations).error,
              LzssTypedContextFrameEncodeError::none);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, limits, 0, 0, input, tokens, operations, output)
                  .error,
              LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(output, one_literal_frame());

    std::array<marc::dictionary::internal::LzssTypedToken, 1>
        decoded_tokens{};
    std::array<std::byte, 1> reconstructed{};
    const auto decoded = decode_lzss_typed_context_frame(
        output, {stream, limits, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);
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
     BinaryTreeMatchesHashChainTokensBytesAndFailuresAreAtomic) {
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
    const auto hash_required = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            marc::dictionary::internal::LzssMatchFinderStrategy::
                hash_chain_exact,
            input.size(), stream.dictionary, {});
    const auto tree_required = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            marc::dictionary::internal::LzssMatchFinderStrategy::
                binary_tree_exact,
            input.size(), stream.dictionary, {});
    ASSERT_EQ(hash_required.error,
              marc::dictionary::internal::
                  LzssMatchFinderWorkspaceError::none);
    ASSERT_EQ(tree_required.error,
              marc::dictionary::internal::
                  LzssMatchFinderWorkspaceError::none);
    AlignedWorkspace hash_owner(hash_required.workspace_size);
    AlignedWorkspace tree_owner(tree_required.workspace_size);
    auto hash_workspace = hash_owner.bytes(hash_required.workspace_size);
    auto tree_workspace = tree_owner.bytes(tree_required.workspace_size);

    marc::dictionary::internal::LzssMatchFinderStatistics hash_statistics{};
    const auto hash_plan = plan_lzss_typed_context_frame_with_match_finder(
        stream, {}, 0, 0, input, tokens, operations,
        marc::dictionary::internal::LzssMatchFinderStrategy::
            hash_chain_exact,
        hash_workspace, &hash_statistics);
    ASSERT_EQ(hash_plan.error, LzssTypedContextFrameEncodeError::none);
    const std::vector hash_tokens(
        tokens.begin(), tokens.begin() + hash_plan.token_count);
    std::vector<std::byte> hash_encoded(hash_plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame_with_match_finder(
                  stream, {}, 0, 0, input, tokens, operations,
                  marc::dictionary::internal::LzssMatchFinderStrategy::
                      hash_chain_exact,
                  hash_workspace, hash_encoded).error,
              LzssTypedContextFrameEncodeError::none);

    marc::dictionary::internal::LzssMatchFinderStatistics tree_statistics{};
    const auto tree_plan = plan_lzss_typed_context_frame_with_match_finder(
        stream, {}, 0, 0, input, tokens, operations,
        marc::dictionary::internal::LzssMatchFinderStrategy::
            binary_tree_exact,
        tree_workspace, &tree_statistics);
    ASSERT_EQ(tree_plan.error, LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(tree_plan.token_count, hash_plan.token_count);
    EXPECT_EQ(tree_plan.operation_count, hash_plan.operation_count);
    for (std::size_t index = 0; index < tree_plan.token_count; ++index) {
        EXPECT_EQ(tokens[index].kind, hash_tokens[index].kind);
        EXPECT_EQ(tokens[index].literal, hash_tokens[index].literal);
        EXPECT_EQ(tokens[index].distance, hash_tokens[index].distance);
        EXPECT_EQ(tokens[index].length, hash_tokens[index].length);
    }
    EXPECT_EQ(tree_statistics.query_count, tree_plan.token_count);
    EXPECT_EQ(hash_statistics.query_count, hash_plan.token_count);
    std::vector<std::byte> tree_encoded(tree_plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame_with_match_finder(
                  stream, {}, 0, 0, input, tokens, operations,
                  marc::dictionary::internal::LzssMatchFinderStrategy::
                      binary_tree_exact,
                  tree_workspace, tree_encoded).error,
              LzssTypedContextFrameEncodeError::none);
    EXPECT_EQ(tree_encoded, hash_encoded);
    std::vector<marc::dictionary::internal::LzssTypedToken> decoded_tokens(
        tree_plan.token_count);
    std::vector<std::byte> reconstructed(input.size());
    const auto decoded = decode_lzss_typed_context_frame(
        tree_encoded, {stream, {}, 0, 0}, decoded_tokens, reconstructed);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(reconstructed, input);

    std::array<std::byte, 1'024> untouched{};
    untouched.fill(std::byte{0xCC});
    auto failed = encode_lzss_typed_context_frame_with_match_finder(
        stream, {}, 0, 0, input, tokens, operations,
        marc::dictionary::internal::LzssMatchFinderStrategy::
            binary_tree_exact,
        tree_workspace.first(tree_workspace.size() - 1), untouched);
    EXPECT_EQ(failed.error,
              LzssTypedContextFrameEncodeError::token_encode_error);
    EXPECT_EQ(failed.token_encode.binary_tree_match_finder_error,
              marc::dictionary::internal::LzssBinaryTreeError::
                  workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(untouched, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));

    failed = encode_lzss_typed_context_frame_with_match_finder(
        stream, {}, 0, 0, input, tokens, operations,
        static_cast<marc::dictionary::internal::LzssMatchFinderStrategy>(255),
        tree_workspace, untouched);
    EXPECT_EQ(failed.error,
              LzssTypedContextFrameEncodeError::
                  unsupported_match_finder_strategy);
    EXPECT_TRUE(std::ranges::all_of(untouched, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));
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
