#ifndef MARC_CORE_HEADER_ACCUMULATOR_HPP
#define MARC_CORE_HEADER_ACCUMULATOR_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace marc::core {

enum class HeaderCollectionStatus : unsigned char {
    progress,
    need_input,
    complete,
};

struct HeaderCollectionResult {
    std::size_t input_consumed{};
    HeaderCollectionStatus status{HeaderCollectionStatus::need_input};
};

template<std::size_t Size>
class HeaderAccumulator final {
public:
    [[nodiscard]] HeaderCollectionResult append(
        const std::span<const std::byte> input) noexcept {
        if (complete()) {
            return {0, HeaderCollectionStatus::complete};
        }
        if (input.empty()) {
            return {0, HeaderCollectionStatus::need_input};
        }

        const auto consumed = std::min(input.size(), remaining());
        std::copy_n(input.begin(), consumed, storage_.begin() + collected_);
        collected_ += consumed;
        return {consumed,
                complete() ? HeaderCollectionStatus::complete
                           : HeaderCollectionStatus::progress};
    }

    [[nodiscard]] constexpr bool complete() const noexcept {
        return collected_ == Size;
    }

    [[nodiscard]] constexpr std::size_t collected() const noexcept {
        return collected_;
    }

    [[nodiscard]] constexpr std::size_t remaining() const noexcept {
        return Size - collected_;
    }

    [[nodiscard]] std::optional<std::span<const std::byte, Size>> bytes()
        const noexcept {
        if (!complete()) {
            return std::nullopt;
        }
        return std::span<const std::byte, Size>{storage_};
    }

    void reset() noexcept {
        storage_.fill(std::byte{0});
        collected_ = 0;
    }

private:
    std::array<std::byte, Size> storage_{};
    std::size_t collected_{};
};

} // namespace marc::core

#endif
