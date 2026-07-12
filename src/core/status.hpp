#ifndef MARC_CORE_STATUS_HPP
#define MARC_CORE_STATUS_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

enum class StreamStatus : std::uint32_t {
    progress,
    need_input,
    need_output,
    end_of_stream,
    error,
};

enum class ProcessFlags : std::uint32_t {
    none = 0,
    flush = 1U << 0,
    end_input = 1U << 1,
    reset_block = 1U << 2,
};

[[nodiscard]] constexpr std::uint32_t flag_value(
    const ProcessFlags flag) noexcept {
    return static_cast<std::uint32_t>(flag);
}

enum class ErrorCode : std::uint32_t {
    none,
    invalid_argument,
    unsupported,
    limit_exceeded,
    out_of_memory,
    malformed_stream,
    internal_error,
};

struct StreamError {
    ErrorCode code{ErrorCode::none};
    std::uint64_t byte_position{};
    std::uint8_t bit_position{};
};

struct ProcessResult {
    std::size_t input_consumed{};
    std::size_t output_produced{};
    StreamStatus status{StreamStatus::need_input};
    StreamError error{};
};

class Transform {
public:
    virtual ~Transform() = default;
    [[nodiscard]] virtual ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
        std::uint32_t flags) noexcept = 0;
};

[[nodiscard]] bool is_valid(const ProcessResult& result,
                            std::size_t input_size,
                            std::size_t output_size) noexcept;

} // namespace marc::core

#endif
