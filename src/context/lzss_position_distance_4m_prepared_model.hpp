#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_4M_PREPARED_MODEL_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_4M_PREPARED_MODEL_HPP
#include "context/lzss_position_distance_4m_tokens.hpp"
namespace marc::context::internal {
// Internal one-use plan. Borrowed tokens and configuration must stay
// alive and unchanged through write. Output is unchanged on preflight failure.
// The prepared object is charged in addition to the retained planner's state.
class PreparedLzssPositionDistance4mModel final {
public:
    PreparedLzssPositionDistance4mModel() noexcept = default;
    PreparedLzssPositionDistance4mModel(const PreparedLzssPositionDistance4mModel&) = delete;
    PreparedLzssPositionDistance4mModel& operator=(const PreparedLzssPositionDistance4mModel&) = delete;
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
