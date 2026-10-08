#include "context/lzss_position_rans_16m_full_literal_tokens.hpp"
#include "context/lzss_position_distance_16m_full_literal_field_cursor.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/buffer_overlap.hpp"
#include <bit>

namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using namespace entropy::internal;
using Error = LzssPositionRans16mFullLiteralTokenError;
using Result = LzssPositionRans16mFullLiteralTokenResult;
constexpr auto variant = LzssTypedTokenVariant::field_context_16m_short_length_escape;
struct Region { const void* data; std::size_t size; };
template<std::size_t N>
Error disjoint(const Region output, const std::array<Region,N>& inputs) noexcept {
    for (const auto& input : inputs) {
        const auto error = core::check_buffer_overlap(output.data,output.size,input.data,input.size);
        if (error != core::BufferOverlap::disjoint)
            return error == core::BufferOverlap::arithmetic_overflow
                ? Error::arithmetic_overflow : Error::overlapping_buffers;
    }
    return Error::none;
}
bool budget(const std::size_t tokens, const std::size_t descriptor,
            const std::size_t payload, const core::DecoderLimits& limits, Result& r) noexcept {
    std::size_t bytes{};
    if (!core::checked_multiply(tokens,sizeof(LzssTypedToken),bytes)
        || !core::checked_add(bytes,descriptor,bytes)
        || !core::checked_add(bytes,payload,bytes)
        || !core::checked_add(bytes,lzss_position_rans_16m_full_literal_fixed_working_bytes,bytes)) {
        r.error = Error::arithmetic_overflow; return false;
    }
    if (bytes > limits.max_internal_buffered_bytes) { r.error = Error::limit_exceeded; return false; }
    return true;
}
PositionRans16mFullLiteralEncodeError forward(const std::span<const LzssTypedToken> tokens,
    PositionRans16mFullLiteralModelBuilder& builder, Result& r) noexcept {
    LzssPositionDistance16mFullLiteralFieldCursor cursor;
    const auto emit = [&](std::uint32_t value) {
        const auto request = cursor.next();
        auto op = request.shape; op.value = value;
        if (cursor.accept(op) != LzssFieldContextError::none) return PositionRans16mFullLiteralEncodeError::invalid_model;
        ++r.event_count;
        if (op.kind == ModeledOperationKind::symbol) return builder.add(op.context_id,value);
        for (unsigned bit = 0; bit < op.bit_count; ++bit) {
            const auto error = builder.add(request.field == LzssPositionDistance16mFullLiteralField::adaptive_distance_extra
                ? static_cast<int>(32+bit) : -1, (value >> bit)&1);
            if (error != PositionRans16mFullLiteralEncodeError::none) return error;
        }
        return PositionRans16mFullLiteralEncodeError::none;
    };
    for (const auto& token : tokens) {
        auto error = emit(token.kind == LzssTypedTokenKind::literal ? 0 : 1);
        if (error != PositionRans16mFullLiteralEncodeError::none) return error;
        if (token.kind == LzssTypedTokenKind::literal) error = emit(token.literal);
        else {
            const auto length = encode_lzss_short_length_escape(token.length);
            error = emit(length.length_class);
            if (error == PositionRans16mFullLiteralEncodeError::none && length.bit_count) error = emit(length.extra);
            const auto dc = std::bit_width(token.distance)-1U;
            if (error == PositionRans16mFullLiteralEncodeError::none) error = emit(dc);
            if (error == PositionRans16mFullLiteralEncodeError::none && dc) error = emit(token.distance-(UINT32_C(1)<<dc));
        }
        if (error != PositionRans16mFullLiteralEncodeError::none) return error;
    }
    return cursor.finish() == LzssFieldContextError::none
        ? PositionRans16mFullLiteralEncodeError::none : PositionRans16mFullLiteralEncodeError::invalid_model;
}
std::size_t literal_before(const std::span<const LzssTypedToken> tokens, std::size_t before) noexcept {
    while (before) { --before; if (tokens[before].kind == LzssTypedTokenKind::literal) return before; }
    return tokens.size();
}
PositionRans16mFullLiteralEncodeError reverse(const std::span<const LzssTypedToken> tokens,
    const PositionRans16mFullLiteralDescriptor& descriptor, const core::DecoderLimits& limits,
    std::span<std::byte> scratch, std::size_t& size) noexcept {
    PositionRans16mFullLiteralReverseWriter writer(descriptor,limits,scratch);
    auto preceding = tokens.empty() ? tokens.size() : literal_before(tokens,tokens.size()-1);
    const auto bits = [&](std::uint32_t value, unsigned width, bool positional) {
        while (width) {
            --width;
            const auto error = writer.write(positional ? static_cast<int>(32+width) : -1,(value>>width)&1);
            if (error != PositionRans16mFullLiteralEncodeError::none) return error;
        }
        return PositionRans16mFullLiteralEncodeError::none;
    };
    for (std::size_t n = tokens.size(); n; --n) {
        const auto i = n-1;
        const auto& token = tokens[i];
        const int kind_context = i == 0 ? 0 : tokens[i-1].kind == LzssTypedTokenKind::literal ? 1 : 2;
        auto error = PositionRans16mFullLiteralEncodeError::none;
        if (token.kind == LzssTypedTokenKind::literal) {
            error = writer.write(preceding == tokens.size() ? 3 : 4+(tokens[preceding].literal>>4),token.literal);
        } else {
            const auto length = encode_lzss_short_length_escape(token.length);
            const auto dc = std::bit_width(token.distance)-1U;
            error = bits(token.distance-(UINT32_C(1)<<dc),dc,true);
            if (error == PositionRans16mFullLiteralEncodeError::none) error = writer.write(23+length.length_class,dc);
            if (error == PositionRans16mFullLiteralEncodeError::none) error = bits(length.extra,length.bit_count,false);
            if (error == PositionRans16mFullLiteralEncodeError::none) error = writer.write(20+kind_context,length.length_class);
        }
        if (error == PositionRans16mFullLiteralEncodeError::none)
            error = writer.write(kind_context,token.kind == LzssTypedTokenKind::literal ? 0 : 1);
        if (error != PositionRans16mFullLiteralEncodeError::none) return error;
        if (i && preceding == i-1) preceding = literal_before(tokens,i-1);
    }
    return writer.finish(size);
}
Result plan(const std::span<const LzssTypedToken> tokens, const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context, const core::DecoderLimits& limits,
    PositionRans16mFullLiteralDescriptor& output) noexcept {
    Result r{};
    const auto checked = validate_lzss_typed_frame(tokens,parameters,context,limits,variant);
    r.token_error = checked.token_error; r.raw_size = checked.raw_size;
    if (checked.error != LzssTypedFrameValidationError::none) {
        r.error = checked.error == LzssTypedFrameValidationError::limit_exceeded ? Error::limit_exceeded
            : checked.error == LzssTypedFrameValidationError::arithmetic_overflow ? Error::arithmetic_overflow
            : checked.error == LzssTypedFrameValidationError::invalid_parameters ? Error::invalid_parameters : Error::invalid_token;
        return r;
    }
    r.token_count = static_cast<std::uint32_t>(tokens.size());
    if (!budget(tokens.size(),0,0,limits,r)) return r;
    PositionRans16mFullLiteralModelBuilder builder;
    r.encode_error = forward(tokens,builder,r);
    if (r.encode_error != PositionRans16mFullLiteralEncodeError::none) { r.error = Error::entropy_error; return r; }
    PositionRans16mFullLiteralDescriptor d{};
    r.encode_error = builder.finish(d);
    if (r.encode_error != PositionRans16mFullLiteralEncodeError::none) { r.error = Error::entropy_error; return r; }
    r.decision_count = d.decision_count;
    if (r.decision_count > limits.max_block_size) { r.error = Error::limit_exceeded; return r; }
    r.encode_error = reverse(tokens,d,limits,{},r.payload_size);
    if (r.encode_error != PositionRans16mFullLiteralEncodeError::none) { r.error = Error::entropy_error; return r; }
    if (r.payload_size > UINT32_MAX) { r.error = Error::arithmetic_overflow; return r; }
    d.payload_size = static_cast<std::uint32_t>(r.payload_size);
    if (r.payload_size > limits.max_compressed_payload_size) { r.error = Error::limit_exceeded; return r; }
    std::array<std::byte,position_rans_16m_full_literal_descriptor_capacity> bytes{};
    if (serialize_position_rans_16m_full_literal_descriptor(d,limits,bytes,r.descriptor_size) != PositionRans16mFullLiteralFormatError::none) {
        r.error = Error::entropy_error; return r;
    }
    if (!budget(tokens.size(),r.descriptor_size,r.payload_size,limits,r)) return r;
    output = d;
    return r;
}
Error decode_overlap(std::span<LzssTypedToken> output, std::span<const std::byte> descriptor,
    std::span<const std::byte> payload, const LzssParameters& p,
    const LzssFieldContextValidationContext& c, const core::DecoderLimits& l) noexcept {
    return disjoint(Region{output.data(),output.size_bytes()},std::array{
        Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
        Region{&p,sizeof(p)},Region{&c,sizeof(c)},Region{&l,sizeof(l)}});
}
Result run(std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const LzssParameters& parameters, const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits, std::span<LzssTypedToken> output) noexcept {
    Result r{};
    r.token_error = validate_lzss_typed_parameters(parameters,limits,variant);
    if (r.token_error != LzssTypedTokenError::none) {
        r.error = r.token_error == LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded : Error::invalid_parameters; return r;
    }
    const std::uint64_t t = context.declared_token_count, e = context.declared_event_count,
        d = context.declared_decision_count, f = context.declared_raw_size;
    if (f > 16777216 || t > f || e < 2*t || e > 5*t || e > 2*f
        || d < e || d > 34*t || d > 10*f || payload.size() > 2*d+8 || payload.size() > 20*f+8
        || ((f == 0) != (t == 0))) { r.error = Error::invalid_counts; return r; }
    std::uint64_t total{};
    if (!core::checked_add(context.output_already_committed,f,total)) { r.error = Error::arithmetic_overflow; return r; }
    if (f > limits.max_frame_size || f > limits.max_block_size || d > limits.max_block_size
        || total > limits.max_total_output_size || payload.size() > limits.max_compressed_payload_size) {
        r.error = Error::limit_exceeded; return r;
    }
    if (!budget(static_cast<std::size_t>(t),descriptor.size(),payload.size(),limits,r)) return r;
    PositionRans16mFullLiteralDecoder decoder;
    r.decode_error = decoder.begin(descriptor,context.declared_decision_count,payload,limits);
    if (r.decode_error != PositionRans16mFullLiteralDecodeError::none) { r.error = Error::entropy_error; return r; }
    LzssPositionDistance16mFullLiteralFieldCursor cursor;
    const auto read = [&](std::uint32_t& value) {
        const auto request = cursor.next();
        auto op = request.shape;
        if (op.kind == ModeledOperationKind::symbol) {
            r.decode_error = decoder.read(op.context_id,op.value);
            ++r.decision_count;
        } else {
            for (unsigned bit = 0; bit < op.bit_count; ++bit) {
                std::uint32_t symbol{};
                r.decode_error = decoder.read(request.field == LzssPositionDistance16mFullLiteralField::adaptive_distance_extra
                    ? static_cast<int>(32+bit) : -1,symbol);
                if (r.decode_error != PositionRans16mFullLiteralDecodeError::none) break;
                op.value |= symbol << bit; ++r.decision_count;
            }
        }
        if (r.decode_error != PositionRans16mFullLiteralDecodeError::none) { r.error = Error::entropy_error; return false; }
        ++r.event_count;
        if (cursor.accept(op) != LzssFieldContextError::none) { r.error = Error::invalid_token; return false; }
        value = op.value; return true;
    };
    while (r.token_count < t) {
        std::uint32_t kind{}, value{};
        if (!read(kind)) return r;
        LzssTypedToken token{};
        if (kind == 0) {
            if (!read(value)) return r;
            token.literal = static_cast<std::uint8_t>(value);
        } else {
            std::uint32_t lc{}, extra{}, dc{};
            if (!read(lc)) return r;
            const auto width = static_cast<std::uint8_t>(lc == 8 ? 1 : lc);
            if (width && !read(extra)) return r;
            const auto length = decode_lzss_short_length_escape(lc,width,extra);
            if (length.error != LzssShortLengthEscapeError::none) { r.error = Error::invalid_token; return r; }
            if (!read(dc)) return r;
            extra = 0;
            if (dc && !read(extra)) return r;
            token = {LzssTypedTokenKind::match,0,(UINT32_C(1)<<dc)+extra,length.length};
        }
        std::uint64_t next{};
        r.token_error = validate_lzss_typed_token(token,parameters,{r.raw_size,f},limits,next,variant);
        if (r.token_error != LzssTypedTokenError::none) { r.error = Error::invalid_token; return r; }
        if (!output.empty()) output[r.token_count] = token;
        r.raw_size = next; ++r.token_count;
    }
    if (r.event_count != e || r.decision_count != d || cursor.finish() != LzssFieldContextError::none) {
        r.error = Error::invalid_counts; return r;
    }
    r.decode_error = decoder.finish();
    if (r.decode_error != PositionRans16mFullLiteralDecodeError::none) r.error = Error::entropy_error;
    else if (r.raw_size != f) r.error = Error::raw_size_mismatch;
    r.descriptor_size = descriptor.size(); r.payload_size = payload.size();
    return r;
}
} // namespace

LzssPositionRans16mFullLiteralTokenResult plan_lzss_position_rans_16m_full_literal_tokens(
    std::span<const LzssTypedToken> tokens, const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context, const core::DecoderLimits& limits,
    PositionRans16mFullLiteralDescriptor& descriptor) noexcept {
    PositionRans16mFullLiteralDescriptor d{};
    auto r = plan(tokens,parameters,context,limits,d);
    if (r.error != Error::none) return r;
    r.error = disjoint(Region{&descriptor,sizeof(descriptor)},std::array{
        Region{tokens.data(),tokens.size_bytes()},Region{&parameters,sizeof(parameters)},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}});
    if (r.error == Error::none) descriptor = d;
    return r;
}
LzssPositionRans16mFullLiteralTokenResult encode_lzss_position_rans_16m_full_literal_tokens(
    std::span<const LzssTypedToken> tokens, const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context, const core::DecoderLimits& limits,
    std::span<std::byte> descriptor_output, std::span<std::byte> payload_output) noexcept {
    PositionRans16mFullLiteralDescriptor d{};
    auto r = plan(tokens,parameters,context,limits,d);
    if (r.error != Error::none) return r;
    if (descriptor_output.size() < r.descriptor_size || payload_output.size() < r.payload_size) {
        r.error = Error::output_too_small; return r;
    }
    const auto desc = descriptor_output.first(r.descriptor_size);
    const auto payload = payload_output.first(r.payload_size);
    const auto inputs = std::array{Region{tokens.data(),tokens.size_bytes()},Region{&parameters,sizeof(parameters)},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    r.error = disjoint(Region{desc.data(),desc.size()},inputs);
    if (r.error != Error::none) return r;
    r.error = disjoint(Region{payload.data(),payload.size()},inputs);
    if (r.error != Error::none) return r;
    r.error = disjoint(Region{desc.data(),desc.size()},std::array{Region{payload.data(),payload.size()}});
    if (r.error != Error::none) return r;
    r.encode_error = reverse(tokens,d,limits,payload,r.payload_size);
    if (r.encode_error != PositionRans16mFullLiteralEncodeError::none) { r.error = Error::entropy_error; return r; }
    if (serialize_position_rans_16m_full_literal_descriptor(d,limits,desc,r.descriptor_size) != PositionRans16mFullLiteralFormatError::none)
        r.error = Error::entropy_error;
    return r;
}
LzssPositionRans16mFullLiteralTokenResult validate_lzss_position_rans_16m_full_literal_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const LzssParameters& parameters, const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept { return run(descriptor,payload,parameters,context,limits,{}); }
LzssPositionRans16mFullLiteralTokenResult decode_lzss_position_rans_16m_full_literal_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const LzssParameters& parameters, const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits, std::span<LzssTypedToken> output) noexcept {
    auto r = run(descriptor,payload,parameters,context,limits,{});
    if (r.error != Error::none) return r;
    if (output.size() < context.declared_token_count) { r.error = Error::output_too_small; return r; }
    const auto target = output.first(context.declared_token_count);
    r.error = decode_overlap(target,descriptor,payload,parameters,context,limits);
    if (r.error != Error::none) return r;
    return run(descriptor,payload,parameters,context,limits,target);
}
LzssPositionRans16mFullLiteralTokenResult decode_lzss_position_rans_16m_full_literal_token_scratch(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const LzssParameters& parameters, const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits, std::span<LzssTypedToken> scratch) noexcept {
    if (scratch.size() < context.declared_token_count
        || decode_overlap(scratch.first(context.declared_token_count),descriptor,payload,parameters,context,limits) != Error::none)
        return decode_lzss_position_rans_16m_full_literal_tokens(descriptor,payload,parameters,context,limits,scratch);
    return run(descriptor,payload,parameters,context,limits,scratch.first(context.declared_token_count));
}
} // namespace marc::context::internal
