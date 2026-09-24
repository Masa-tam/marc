#include "frame/lzss_short_length_escape_frame_encoder.hpp"

#include "context/lzss_short_length_escape.hpp"
#include "context/lzss_short_length_escape_operation_decoder.hpp"
#include "context/lzss_short_length_escape_operations.hpp"
#include "frame/lzss_short_length_escape_frame_decoder.hpp"
#include "frame/lzss_short_match_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] TypedContextStreamHeader stream_for(const std::uint32_t raw) {
    return {raw, raw, {65536, 3, 258, 0}, typed_context_model_total,
            32, 8, 1, 7};
}

[[nodiscard]] std::array<LzssTypedToken, 2> tokens_for(
    const std::uint32_t length) {
    return {LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
            LzssTypedToken{LzssTypedTokenKind::match, 0, 1, length}};
}

} // namespace

TEST(LzssShortLengthEscapeFrameEncoder, ModelsEveryLengthAndInvertsOperations) {
    const auto limits = marc::core::DecoderLimits{};
    for (std::uint32_t length = 3; length <= 258; ++length) {
        SCOPED_TRACE(length);
        const auto tokens = tokens_for(length);
        const marc::dictionary::internal::LzssTypedFrameValidationContext
            token_context{2, length + 1, 0};
        std::array<ModeledOperation, 6> operations{};
        const auto modeled = marc::context::internal::
            model_lzss_short_length_escape_tokens(
                tokens, {65536, 3, 258, 0}, token_context, limits, operations);
        ASSERT_EQ(modeled.error,
                  marc::context::internal::LzssFieldContextError::none);
        const auto field = marc::context::internal::
            encode_lzss_short_length_escape(length);
        ASSERT_EQ(operations[3].kind, ModeledOperationKind::symbol);
        EXPECT_EQ(operations[3].context_id, 21U);
        EXPECT_EQ(operations[3].value, field.length_class);
        const auto distance_index = field.bit_count == 0 ? 4U : 5U;
        if (field.bit_count != 0) {
            EXPECT_EQ(operations[4].kind, ModeledOperationKind::bypass_bits);
            EXPECT_EQ(operations[4].bit_count, field.bit_count);
            EXPECT_EQ(operations[4].value, field.extra);
        }
        EXPECT_EQ(operations[distance_index].context_id,
                  23U + field.length_class);
        const marc::context::internal::LzssFieldContextValidationContext
            inverse_context{2, static_cast<std::uint32_t>(modeled.operation_count),
                            modeled.decision_count, length + 1, 0};
        std::array<LzssTypedToken, 2> recovered{};
        const auto inverted = marc::context::internal::
            invert_lzss_short_length_escape_operations(
                std::span{operations}.first(modeled.operation_count),
                {65536, 3, 258, 0}, inverse_context, limits, recovered);
        ASSERT_EQ(inverted.error,
                  marc::context::internal::LzssFieldContextError::none);
        EXPECT_EQ(recovered[1].length, length);
        EXPECT_EQ(recovered[1].distance, 1U);
    }
}

TEST(LzssShortLengthEscapeFrameEncoder, EncodesAndDecodesBoundaryFrames) {
    const auto limits = marc::core::DecoderLimits{};
    for (const std::uint32_t length : {3U, 4U, 5U, 258U}) {
        SCOPED_TRACE(length);
        const auto stream = stream_for(length + 1);
        const auto tokens = tokens_for(length);
        std::array<ModeledOperation, 6> operations{};
        const auto planned = plan_lzss_short_length_escape_frame(
            stream, limits, 0, 0, tokens, operations);
        ASSERT_EQ(planned.error, LzssShortMatchFrameEncodeError::none);
        std::vector<std::byte> frame(planned.serialized_size);
        const auto encoded = encode_lzss_short_length_escape_frame(
            stream, limits, 0, 0, tokens, operations, frame);
        ASSERT_EQ(encoded.error, LzssShortMatchFrameEncodeError::none);
        EXPECT_EQ(encoded.serialized_size, frame.size());
        EXPECT_EQ(encoded.decision_count, planned.decision_count);
        const TypedContextFrameValidationContext context{stream, limits, 0, 0};
        std::array<LzssTypedToken, 2> decoded_tokens{};
        std::vector<std::byte> raw(length + 1);
        const auto decoded = decode_lzss_short_length_escape_frame(
            frame, context, decoded_tokens, raw);
        ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
        EXPECT_EQ(decoded.serialized_consumed, frame.size());
        EXPECT_EQ(decoded_tokens[1].length, length);
        for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
        const auto wrong_identity = decode_lzss_short_match_frame(
            frame, context, decoded_tokens, raw);
        EXPECT_NE(wrong_identity.error, LzssShortMatchFrameDecodeError::none);
    }
}

TEST(LzssShortLengthEscapeFrameEncoder, RejectsErrorsBeforeOutputWrite) {
    auto tokens = tokens_for(3);
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 6> operations{};
    std::array<std::byte, 128> output{};
    output.fill(std::byte{0xcc});
    tokens[1].distance = 2;
    auto result = encode_lzss_short_length_escape_frame(
        stream, limits, 0, 0, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchFrameEncodeError::context_error);
    tokens[1].distance = 1;
    result = encode_lzss_short_length_escape_frame(
        stream, limits, 0, 0, tokens,
        std::span{operations}.first(5), output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::operation_workspace_too_small);
    result = encode_lzss_short_length_escape_frame(
        stream, limits, 0, 0, tokens, operations,
        std::span{output}.first(1));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::serialized_output_too_small);
    result = encode_lzss_short_length_escape_frame(
        stream, limits, 1, 0, tokens, operations, output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::invalid_frame_position);
    auto wrong_stream = stream;
    wrong_stream.dictionary_variant = 7;
    result = encode_lzss_short_length_escape_frame(
        wrong_stream, limits, 0, 0, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchFrameEncodeError::invalid_stream);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeFrameEncoder, RejectsOverlappingStorage) {
    auto tokens = tokens_for(3);
    const auto stream = stream_for(4);
    std::array<ModeledOperation, 6> operations{};
    const auto alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(tokens.data()), sizeof(tokens)};
    const auto result = encode_lzss_short_length_escape_frame(
        stream, marc::core::DecoderLimits{}, 0, 0, tokens, operations, alias);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(tokens[1].length, 3U);
}
