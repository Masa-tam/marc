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

marc_lzd_config config(const marc_direction direction,
                       const std::size_t original_size) {
    marc_lzd_config result{};
    EXPECT_EQ(marc_lzd_config_init(direction, &result), MARC_STATUS_OK);
    result.original_size = original_size;
    result.frame_size = test_frame_size;
    result.maximum_entries = 32;
    result.max_total_output_size = 4096;
    result.max_frame_size = test_frame_size;
    result.max_compressed_payload_size = 256;
    result.max_dictionary_serialized_size = 256;
    result.max_internal_buffered_bytes = 4096;
    result.max_dictionary_entries = 32;
    return result;
}

Workspace workspace_for(const marc_lzd_config& config) {
    Workspace result{};
    EXPECT_EQ(marc_lzd_workspace_requirements(&config, &result.requirements),
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

Transform create(const marc_lzd_config& config, Workspace& workspace) {
    marc_transform* raw{};
    EXPECT_EQ(marc_lzd_create(
                  &config,
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
            || result.status == MARC_STATUS_INTERNAL_ERROR
            || result.status == MARC_STATUS_INVALID_ARGUMENT
            || result.status == MARC_STATUS_MALFORMED_STREAM) {
            ADD_FAILURE() << "invalid process result " << result.status;
            break;
        }
        consumed += result.input_consumed;
        produced += result.output_produced;
        if (result.status == MARC_STATUS_END_OF_STREAM) {
            ended = true;
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
    const auto capacity = 80 + input.size() * 4 + frames * 60;
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

TEST(LzdCompletion, RequiredDataClassesRoundTripDeterministically) {
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
    for (const auto size : {63U, 64U, 65U})
        expect_round_trip(generated_bytes(size, 1));
}

TEST(LzdCompletion, MultiFrameStreamIsIndependentOfChunking) {
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

} // namespace
