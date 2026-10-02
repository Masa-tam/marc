#ifndef MARC_BENCHMARKS_POSITION_DISTANCE_4M_CONTROL_ENGINE_HPP
#define MARC_BENCHMARKS_POSITION_DISTANCE_4M_CONTROL_ENGINE_HPP
#include "position_distance_4m_control_repeatability.hpp"
#include "frame/lzss_position_distance_4m_five_prefix_owned_encoder.hpp"
#include "frame/lzss_position_distance_4m_owned_decoder.hpp"

namespace marc::benchmarks::control_repeatability {
inline core::DecoderLimits limits() {
    core::DecoderLimits result{};
    result.max_block_size = frame;
    result.max_compressed_payload_size = 75497477;
    result.max_internal_buffered_bytes = 512U * 1024U * 1024U;
    return result;
}
inline frame::internal::TypedContextStreamHeader configuration(std::size_t size) {
    return {frame, size, {frame, 3, 258, 0}, 32768, 46, 10, 1, 11};
}
class ScalarEngine {
public:
    explicit ScalarEngine(std::size_t size) : original_(size) {}
    bool create(bool encode) {
        if (codec_) return false;
        core::ErrorCode error{};
        if (encode) codec_ = frame::internal::LzssPositionDistance4mFivePrefixOwnedEncoder::create(
            configuration(original_), limits(), error);
        else codec_ = frame::internal::LzssPositionDistance4mOwnedDecoder::create(frame, limits(), error);
        return error == core::ErrorCode::none && codec_ != nullptr;
    }
    core::ProcessResult process(std::span<const std::byte> in, std::span<std::byte> out, bool end) {
        return codec_->process(in, out, end ? core::flag_value(core::ProcessFlags::end_input) : 0);
    }
    void clear() noexcept { codec_.reset(); }
private:
    std::size_t original_;
    std::unique_ptr<core::Transform> codec_;
};
}
#endif
