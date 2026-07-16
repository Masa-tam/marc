#include <marc/marc.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t test_frame_size = 64;
constexpr std::size_t test_block_size = 32;
constexpr std::size_t maximum_blocks = test_frame_size / test_block_size;
constexpr std::size_t descriptor_size = 528;
constexpr std::size_t state_size = 2;
constexpr std::size_t maximum_payload_size =
    test_frame_size * 12 / 8 + maximum_blocks * state_size;
constexpr std::size_t maximum_internal_size =
    maximum_payload_size + maximum_blocks * descriptor_size;

struct TransformDeleter {
    void operator()(marc_transform* value) const noexcept {
        marc_transform_destroy(value);
    }
};
using Transform = std::unique_ptr<marc_transform, TransformDeleter>;

struct Workspace {
    marc_workspace_requirements requirements{};
    std::vector<std::uint8_t> primary;
    std::vector<std::uint8_t> secondary;
    std::vector<std::uint8_t> views_storage;
    std::uint8_t* views{};
};

marc_tans_config config(
    const marc_direction direction, const std::size_t original_size) {
    marc_tans_config result{};
    EXPECT_EQ(marc_tans_config_init(direction, &result), MARC_STATUS_OK);
    result.original_size = original_size;
    result.frame_size = test_frame_size;
    result.block_size = test_block_size;
    result.max_total_output_size = 4096;
    result.max_frame_size = test_frame_size;
    result.max_block_size = test_block_size;
    result.max_compressed_payload_size = maximum_payload_size;
    result.max_internal_buffered_bytes = maximum_internal_size;
    result.max_blocks_per_frame = maximum_blocks;
    return result;
}

Workspace workspace_for(const marc_tans_config& settings) {
    Workspace result{};
    EXPECT_EQ(marc_tans_workspace_requirements(
                  &settings, &result.requirements),
              MARC_STATUS_OK);
    result.primary.resize(result.requirements.primary_bytes);
    result.secondary.resize(result.requirements.secondary_bytes);
    if (result.requirements.views_bytes != 0) {
        const auto alignment = result.requirements.views_alignment;
        result.views_storage.resize(
            result.requirements.views_bytes + alignment - 1);
        const auto address = reinterpret_cast<std::uintptr_t>(
            result.views_storage.data());
        const auto remainder = address % alignment;
        result.views = result.views_storage.data()
            + (remainder == 0 ? 0 : alignment - remainder);
    }
    return result;
}

Transform create(const marc_tans_config& settings, Workspace& workspace) {
    marc_transform* raw{};
    EXPECT_EQ(marc_tans_create(
                  &settings,
                  {workspace.primary.data(), workspace.primary.size()},
                  {workspace.secondary.data(), workspace.secondary.size()},
                  {workspace.views, workspace.requirements.views_bytes}, &raw),
              MARC_STATUS_OK);
    return Transform{raw};
}

std::vector<std::uint8_t> process_all(
    const marc_direction direction, const std::span<const std::uint8_t> input,
    const std::size_t original_size, const std::size_t input_chunk,
    const std::size_t output_chunk, const std::size_t output_capacity) {
    auto settings = config(direction, original_size);
    auto workspace = workspace_for(settings);
    auto transform = create(settings, workspace);
    std::vector<std::uint8_t> output(output_capacity);
    std::size_t consumed{};
    std::size_t produced{};
    bool ended{};
    for (std::size_t call = 0;
         call < input.size() + output_capacity + 256; ++call) {
        const auto supplied = std::min(input_chunk, input.size() - consumed);
        const auto available = std::min(output_chunk, output.size() - produced);
        const auto flags = consumed + supplied == input.size()
            ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE;
        const auto* input_data = supplied == 0
            ? static_cast<const std::uint8_t*>(nullptr)
            : input.data() + consumed;
        auto* output_data = available == 0
            ? static_cast<std::uint8_t*>(nullptr)
            : output.data() + produced;
        const auto result = marc_transform_process(
            transform.get(), {input_data, supplied}, {output_data, available},
            flags);
        if (result.input_consumed > supplied
            || result.output_produced > available
            || result.status >= MARC_STATUS_INVALID_ARGUMENT) {
            ADD_FAILURE() << "invalid process result " << result.status;
            break;
        }
        consumed += result.input_consumed;
        produced += result.output_produced;
        if (result.status == MARC_STATUS_END_OF_STREAM) {
            ended = true;
            const auto repeated = marc_transform_process(
                transform.get(), {nullptr, 0}, {nullptr, 0},
                MARC_PROCESS_END_INPUT);
            EXPECT_EQ(repeated.status, MARC_STATUS_END_OF_STREAM);
            EXPECT_EQ(repeated.input_consumed, 0U);
            EXPECT_EQ(repeated.output_produced, 0U);
            break;
        }
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != MARC_STATUS_NEED_INPUT
            && result.status != MARC_STATUS_NEED_OUTPUT) {
            ADD_FAILURE() << "zero-progress process result";
            break;
        }
    }
    EXPECT_TRUE(ended);
    EXPECT_EQ(consumed, input.size());
    output.resize(produced);
    return output;
}

std::vector<std::uint8_t> generated_bytes(const std::size_t size,
                                          std::uint32_t state) {
    std::vector<std::uint8_t> result(size);
    for (auto& value : result) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::uint8_t>(state >> 24);
    }
    return result;
}

std::vector<std::uint8_t> encode(
    const std::span<const std::uint8_t> input,
    const std::size_t input_chunk = SIZE_MAX,
    const std::size_t output_chunk = SIZE_MAX) {
    const auto frames = input.empty() ? std::size_t{0}
        : std::size_t{1} + (input.size() - 1) / test_frame_size;
    const auto transition_bytes = input.size() + input.size() / 2
        + input.size() % 2;
    const auto capacity = 64 + transition_bytes
        + frames * (56 + maximum_blocks * (descriptor_size + state_size));
    return process_all(MARC_DIRECTION_ENCODE, input, input.size(), input_chunk,
                       output_chunk, capacity);
}

void expect_round_trip(const std::span<const std::uint8_t> input) {
    const auto first = encode(input);
    EXPECT_EQ(encode(input), first);
    const auto decoded = process_all(
        MARC_DIRECTION_DECODE, first, input.size(), SIZE_MAX, SIZE_MAX,
        input.size());
    EXPECT_TRUE(std::ranges::equal(decoded, input));
}

std::uint32_t load_le32(const std::span<const std::uint8_t> input,
                        const std::size_t offset) {
    return static_cast<std::uint32_t>(input[offset])
        | static_cast<std::uint32_t>(input[offset + 1]) << 8
        | static_cast<std::uint32_t>(input[offset + 2]) << 16
        | static_cast<std::uint32_t>(input[offset + 3]) << 24;
}

std::size_t frame_extent(const std::span<const std::uint8_t> stream,
                         const std::size_t offset) {
    return 56 + load_le32(stream, offset + 32)
        + load_le32(stream, offset + 24);
}

marc_process_result decode_once(const std::span<const std::uint8_t> input,
                                const std::size_t original_size,
                                const std::span<std::uint8_t> output,
                                marc_process_result* repeated) {
    auto settings = config(MARC_DIRECTION_DECODE, original_size);
    auto workspace = workspace_for(settings);
    auto transform = create(settings, workspace);
    const auto result = marc_transform_process(
        transform.get(), {input.data(), input.size()},
        {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
    *repeated = marc_transform_process(
        transform.get(), {nullptr, 0}, {nullptr, 0}, MARC_PROCESS_END_INPUT);
    return result;
}

void expect_sticky_error(const marc_process_result& first,
                         const marc_process_result& repeated) {
    EXPECT_EQ(repeated.status, first.status);
    EXPECT_EQ(repeated.error_byte_position, first.error_byte_position);
    EXPECT_EQ(repeated.error_bit_position, first.error_bit_position);
    EXPECT_EQ(repeated.input_consumed, 0U);
    EXPECT_EQ(repeated.output_produced, 0U);
}

TEST(TansCompletion, RequiredDataClassesRoundTripDeterministically) {
    expect_round_trip({});
    for (std::uint32_t value = 0; value < 256; ++value) {
        const std::array one{static_cast<std::uint8_t>(value)};
        expect_round_trip(one);
    }
    std::vector<std::uint8_t> all_values(256);
    for (std::size_t value = 0; value < all_values.size(); ++value)
        all_values[value] = static_cast<std::uint8_t>(value);
    expect_round_trip(all_values);
    expect_round_trip(std::vector<std::uint8_t>(257, 0));
    std::vector<std::uint8_t> pattern(259);
    constexpr std::array bytes{UINT8_C(0), UINT8_C(255), UINT8_C(0x55),
                               UINT8_C(0xaa)};
    for (std::size_t index = 0; index < pattern.size(); ++index)
        pattern[index] = bytes[index % bytes.size()];
    expect_round_trip(pattern);
    expect_round_trip(generated_bytes(513, UINT32_C(0xc001d00d)));
    for (const auto size : {31U, 32U, 33U, 63U, 64U, 65U})
        expect_round_trip(generated_bytes(size, 1));
}

TEST(TansCompletion, MultiFrameStreamIsIndependentOfChunking) {
    const auto input = generated_bytes(193, UINT32_C(0x6d617263));
    const auto expected = encode(input);
    for (const auto chunks : {std::pair{1U, 1U}, std::pair{7U, 5U},
                              std::pair{13U, 17U}}) {
        const auto encoded = encode(input, chunks.first, chunks.second);
        EXPECT_EQ(encoded, expected);
        const auto decoded = process_all(
            MARC_DIRECTION_DECODE, encoded, input.size(), chunks.first,
            chunks.second, input.size());
        EXPECT_EQ(decoded, input);
    }
}

TEST(TansCompletion, MalformedFinalFrameIsNeverCommitted) {
    const auto input = generated_bytes(193, UINT32_C(0x13579bdf));
    const auto encoded = encode(input);
    auto final_frame = std::size_t{64};
    for (std::size_t frame = 0; frame < 3; ++frame)
        final_frame += frame_extent(encoded, final_frame);
    ASSERT_LT(final_frame + 8, encoded.size());

    std::vector<std::uint8_t> output(input.size(), UINT8_C(0xa5));
    auto corrupted = encoded;
    corrupted[final_frame + 8] ^= 1;
    marc_process_result repeated{};
    const auto corrupt_result = decode_once(
        corrupted, input.size(), output, &repeated);
    EXPECT_EQ(corrupt_result.status, MARC_STATUS_MALFORMED_STREAM);
    EXPECT_EQ(corrupt_result.output_produced, 192U);
    EXPECT_TRUE(std::ranges::equal(
        std::span{output}.first(192), std::span{input}.first(192)));
    EXPECT_EQ(output.back(), UINT8_C(0xa5));
    expect_sticky_error(corrupt_result, repeated);

    output.assign(input.size(), UINT8_C(0xa5));
    const auto truncated = std::span{encoded}.first(encoded.size() - 1);
    const auto truncate_result = decode_once(
        truncated, input.size(), output, &repeated);
    EXPECT_EQ(truncate_result.status, MARC_STATUS_MALFORMED_STREAM);
    EXPECT_EQ(truncate_result.output_produced, 192U);
    EXPECT_EQ(output.back(), UINT8_C(0xa5));
    expect_sticky_error(truncate_result, repeated);

    auto trailing = encoded;
    trailing.push_back(0);
    output.assign(input.size(), UINT8_C(0xa5));
    const auto trailing_result = decode_once(
        trailing, input.size(), output, &repeated);
    EXPECT_EQ(trailing_result.status, MARC_STATUS_MALFORMED_STREAM);
    EXPECT_EQ(trailing_result.output_produced, 192U);
    EXPECT_TRUE(std::ranges::equal(
        std::span{output}.first(192), std::span{input}.first(192)));
    EXPECT_EQ(output.back(), UINT8_C(0xa5));
    expect_sticky_error(trailing_result, repeated);
}

} // namespace
