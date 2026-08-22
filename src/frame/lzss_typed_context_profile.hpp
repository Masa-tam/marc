#ifndef MARC_FRAME_LZSS_TYPED_CONTEXT_PROFILE_HPP
#define MARC_FRAME_LZSS_TYPED_CONTEXT_PROFILE_HPP

#include "context/lzss_field_context.hpp"
#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "frame/typed_context_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssTypedContextProfileVariant : std::uint8_t {
    field_context_64k,
    field_context_1m,
    field_context_4m,
};

struct LzssTypedContextProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters dictionary{};
    LzssTypedContextProfileVariant variant{
        LzssTypedContextProfileVariant::field_context_64k};
};

struct LzssTypedContextEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t token_count{};
    std::size_t operation_count{};
    std::size_t operation_offset{};
    std::size_t match_finder_offset{};
    std::size_t match_finder_bytes{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzssTypedContextDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t token_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzssTypedContextProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzssTypedContextWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzssTypedContextEncoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<context::internal::ModeledOperation> operations{};
    std::span<std::byte> match_finder{};
};

struct LzssTypedContextDecoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
};

[[nodiscard]] LzssTypedContextProfileError make_lzss_typed_context_profile(
    const LzssTypedContextProfileConfig& config,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream,
    LzssTypedContextEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssTypedContextProfileError
calculate_lzss_typed_context_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssTypedContextDecoderWorkspaceRequirements& workspace,
    LzssTypedContextProfileVariant variant =
        LzssTypedContextProfileVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedContextWorkspaceError
partition_lzss_typed_context_encoder_views(
    const LzssTypedContextEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssTypedContextEncoderViews& views) noexcept;

[[nodiscard]] LzssTypedContextWorkspaceError
partition_lzss_typed_context_decoder_views(
    const LzssTypedContextDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssTypedContextDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzss_typed_context_profile_error_code(
    LzssTypedContextProfileError error) noexcept;

} // namespace marc::frame::internal

#endif
