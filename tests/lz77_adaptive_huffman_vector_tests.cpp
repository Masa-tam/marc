#include "dictionary/lz77_encoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace {

constexpr std::array raw_a{std::byte{0x41}};

constexpr std::array<std::byte, 16> literal_a_token{
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

constexpr std::array adaptive_payload{
    std::byte{0x00}, std::byte{0xff}, std::byte{0x17}, std::byte{0x74}};

constexpr std::array<std::byte, 76> complete_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xff}, std::byte{0x17}, std::byte{0x74}};

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = 1;
    return stream;
}

TEST(Lz77AdaptiveHuffmanVector, EmitsIndependentSingleLiteralFrame) {
    std::array<std::byte, literal_a_token.size()> tokens{};
    const auto dictionary =
        marc::dictionary::internal::encode_lz77_token_stream(
            raw_a, {}, {}, tokens);
    ASSERT_EQ(dictionary.error,
              marc::dictionary::internal::Lz77EncodeError::none);
    ASSERT_EQ(tokens, literal_a_token);

    marc::entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto entropy = marc::entropy::internal::plan_adaptive_huffman_frame(
        tokens, {}, descriptor);
    ASSERT_EQ(entropy.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(entropy.payload_size, adaptive_payload.size());
    EXPECT_EQ(entropy.payload_bits, 31U);

    std::array<std::byte, adaptive_payload.size()> payload{};
    ASSERT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  tokens, {}, payload, descriptor).error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(payload, adaptive_payload);
    EXPECT_EQ(descriptor.symbol_count, literal_a_token.size());
    EXPECT_EQ(descriptor.payload_size, adaptive_payload.size());
    EXPECT_EQ(descriptor.final_valid_bits, 7U);

    std::array<std::byte, complete_frame.size()> frame{};
    marc::frame::FrameHeader header{};
    header.uncompressed_size = 1;
    header.dictionary_serialized_size = literal_a_token.size();
    header.compressed_payload_size = adaptive_payload.size();
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::adaptive_huffman_descriptor_size;
    const marc::core::DecoderLimits limits{};
    const auto stream = stream_for_a();
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    ASSERT_EQ(marc::entropy::internal::serialize_adaptive_huffman_descriptor(
                  descriptor, literal_a_token.size(), adaptive_payload.size(),
                  limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    std::ranges::copy(
        payload,
        frame.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::adaptive_huffman_descriptor_size);
    EXPECT_EQ(frame, complete_frame);
}

} // namespace
