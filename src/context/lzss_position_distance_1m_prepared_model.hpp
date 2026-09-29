#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_1M_PREPARED_MODEL_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_1M_PREPARED_MODEL_HPP
#include "context/lzss_position_distance_1m_tokens.hpp"
namespace marc::context::internal {
// Diagnostic-only, one-use plan. Borrowed tokens and configuration must stay
// alive and unchanged through write. Output is unchanged on preflight failure.
// The prepared object is charged in addition to the retained planner's state.
class PreparedLzssPositionDistance1mModel final {
public:
    PreparedLzssPositionDistance1mModel() noexcept = default;
    PreparedLzssPositionDistance1mModel(const PreparedLzssPositionDistance1mModel&) = delete;
    PreparedLzssPositionDistance1mModel& operator=(const PreparedLzssPositionDistance1mModel&) = delete;
    [[nodiscard]] LzssFieldContextResult prepare(
        std::span<const dictionary::internal::LzssTypedToken>,
        const dictionary::internal::LzssParameters&,
        const dictionary::internal::LzssTypedFrameValidationContext&,
        const core::DecoderLimits&) noexcept;
    [[nodiscard]] LzssFieldContextResult write(std::span<ModeledOperation>) noexcept;
private:
    std::span<const dictionary::internal::LzssTypedToken> tokens_{};
    const dictionary::internal::LzssParameters* parameters_{};
    const dictionary::internal::LzssTypedFrameValidationContext* context_{};
    const core::DecoderLimits* limits_{};
    LzssFieldContextResult plan_{};
    bool ready_{};
};
}
#endif
