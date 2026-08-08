#ifndef MARC_CONTEXT_LZSS_FIELD_CONTEXT_STATE_HPP
#define MARC_CONTEXT_LZSS_FIELD_CONTEXT_STATE_HPP

#include "dictionary/lzss_typed_token.hpp"

#include <cstdint>

namespace marc::context::internal {

class LzssFieldContextState {
public:
    [[nodiscard]] std::uint16_t token_context() const noexcept {
        return static_cast<std::uint16_t>(previous_);
    }

    [[nodiscard]] std::uint16_t literal_context() const noexcept {
        if (!has_literal_) return 3;
        return static_cast<std::uint16_t>(4 + (literal_ >> 4));
    }

    [[nodiscard]] std::uint16_t length_context() const noexcept {
        return static_cast<std::uint16_t>(20 + token_context());
    }

    [[nodiscard]] static std::uint16_t distance_context(
        const std::uint32_t length_class) noexcept {
        return static_cast<std::uint16_t>(23 + length_class);
    }

    void accept(
        const dictionary::internal::LzssTypedToken& token) noexcept {
        if (token.kind
            == dictionary::internal::LzssTypedTokenKind::literal) {
            previous_ = PreviousToken::literal;
            has_literal_ = true;
            literal_ = token.literal;
        } else {
            previous_ = PreviousToken::match;
        }
    }

private:
    enum class PreviousToken : std::uint8_t {
        start = 0,
        literal = 1,
        match = 2,
    };

    PreviousToken previous_{PreviousToken::start};
    bool has_literal_{};
    std::uint8_t literal_{};
};

} // namespace marc::context::internal

#endif
