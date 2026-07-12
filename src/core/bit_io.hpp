#ifndef MARC_CORE_BIT_IO_HPP
#define MARC_CORE_BIT_IO_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

enum class BitIoStatus : std::uint8_t {
    complete,
    need_input,
    need_output,
    finished,
    invalid_argument,
    invalid_padding,
};

struct BitWriteResult {
    std::uint8_t bits_consumed{};
    std::size_t bytes_produced{};
    BitIoStatus status{BitIoStatus::complete};
};

struct BitReadResult {
    std::uint64_t value{};
    std::uint8_t bits_produced{};
    std::size_t bytes_consumed{};
    BitIoStatus status{BitIoStatus::complete};
};

class BitWriter final {
public:
    [[nodiscard]] BitWriteResult write_bits(
        std::uint64_t value,
        std::uint8_t bit_count,
        std::span<std::byte> output) noexcept;

    [[nodiscard]] BitWriteResult finish(
        std::span<std::byte> output) noexcept;

    [[nodiscard]] bool is_finished() const noexcept { return finished_; }
    [[nodiscard]] std::uint8_t pending_bit_count() const noexcept {
        return pending_bit_count_;
    }

private:
    std::uint8_t pending_byte_{};
    std::uint8_t pending_bit_count_{};
    bool finished_{};
};

class BitReader final {
public:
    [[nodiscard]] BitReadResult read_bits(
        std::span<const std::byte> input,
        std::uint8_t bit_count) noexcept;

    [[nodiscard]] BitIoStatus align_to_byte(bool require_zero_padding) noexcept;

    [[nodiscard]] std::uint8_t buffered_bit_count() const noexcept {
        return buffered_bit_count_;
    }

private:
    std::uint8_t buffered_byte_{};
    std::uint8_t buffered_bit_count_{};
};

} // namespace marc::core

#endif
