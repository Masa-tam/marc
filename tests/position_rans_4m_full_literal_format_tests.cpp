#include "entropy/position_rans_4m_full_literal_format.hpp"
#include "entropy/position_rans_4m_full_literal_decoder.hpp"
#include "entropy/position_rans_4m_full_literal_encoder.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using Error = PositionRans4mFullLiteralFormatError;
int failures{};
void check(bool value, const char* name) {
    if (!value) { ++failures; std::cerr << name << '\n'; }
}
std::vector<std::byte> serialize(const PositionRans4mFullLiteralDescriptor& d) {
    std::vector<std::byte> bytes(position_rans_4m_full_literal_descriptor_capacity);
    std::size_t count = 17;
    check(serialize_position_rans_4m_full_literal_descriptor(d, {}, bytes, count) == Error::none, "serialize");
    bytes.resize(count);
    return bytes;
}
bool rejects(const std::span<const std::byte> bytes, const std::uint32_t dc,
             const std::uint32_t ps, const marc::core::DecoderLimits& limits = {}) {
    PositionRans4mFullLiteralDescriptor sentinel{};
    sentinel.decision_count = 777;
    sentinel.frequencies.fill(77);
    auto destination = sentinel;
    const auto error = parse_position_rans_4m_full_literal_descriptor(bytes, dc, ps, limits, destination);
    check(destination == sentinel, "failed parse mutated destination");
    return error != Error::none;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--decisions") {
        std::ifstream file(argv[2], std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(file)), {});
        if (!file || raw.size() < 12 || raw.size() > (UINT64_C(128)<<20)) return 2;
        std::vector<std::byte> storage(raw.size());
        std::transform(raw.begin(), raw.end(), storage.begin(), [](char v) { return static_cast<std::byte>(v); });
        const auto bytes = std::span<const std::byte>(storage);
        std::uint32_t ds{}, ps{}, dc{};
        (void)marc::core::load_le(bytes, 0, ds);
        (void)marc::core::load_le(bytes, 4, ps);
        (void)marc::core::load_le(bytes, 8, dc);
        if (12+static_cast<std::uint64_t>(ds)+ps+UINT64_C(4)*dc != bytes.size()) return 3;
        marc::core::DecoderLimits limits{};
        limits.max_block_size = UINT64_C(40)<<20;
        PositionRans4mFullLiteralModelBuilder builder;
        for (std::size_t i = 0; i < dc; ++i) {
            std::uint16_t context{}, expected{};
            (void)marc::core::load_le(bytes, 12+ds+ps+4*i, context);
            (void)marc::core::load_le(bytes, 14+ds+ps+4*i, expected);
            if (builder.add(context == 65535 ? -1 : context,expected)
                != PositionRans4mFullLiteralEncodeError::none) return 7;
        }
        PositionRans4mFullLiteralDescriptor model{}, reference{};
        if (builder.finish(model) != PositionRans4mFullLiteralEncodeError::none) return 8;
        if (parse_position_rans_4m_full_literal_descriptor(bytes.subspan(12,ds),dc,ps,limits,reference)
            != PositionRans4mFullLiteralFormatError::none) return 9;
        model.payload_size = ps;
        if (model != reference) return 10;
        std::vector<std::byte> encoded(ps);
        for (unsigned pass = 0; pass < 2; ++pass) {
            PositionRans4mFullLiteralReverseWriter writer(model,limits,
                pass == 0 ? std::span<std::byte>{} : std::span<std::byte>(encoded));
            for (std::size_t i = dc; i != 0; --i) {
                std::uint16_t context{}, expected{};
                (void)marc::core::load_le(bytes,12+ds+ps+4*(i-1),context);
                (void)marc::core::load_le(bytes,14+ds+ps+4*(i-1),expected);
                if (writer.write(context == 65535 ? -1 : context,expected)
                    != PositionRans4mFullLiteralEncodeError::none) return 11;
            }
            std::size_t count{};
            if (writer.finish(count) != PositionRans4mFullLiteralEncodeError::none || count != ps) return 12;
        }
        if (!std::equal(encoded.begin(),encoded.end(),bytes.begin()+12+ds)) return 13;
        PositionRans4mFullLiteralDecoder decoder;
        if (decoder.begin(bytes.subspan(12,ds), dc, bytes.subspan(12+ds,ps), limits)
            != PositionRans4mFullLiteralDecodeError::none) return 4;
        for (std::size_t i = 0; i < dc; ++i) {
            std::uint16_t context{}, expected{};
            (void)marc::core::load_le(bytes, 12+ds+ps+4*i, context);
            (void)marc::core::load_le(bytes, 14+ds+ps+4*i, expected);
            std::uint32_t symbol = 777;
            if (decoder.read(context == 65535 ? -1 : context, symbol)
                != PositionRans4mFullLiteralDecodeError::none || symbol != expected) return 5;
        }
        return decoder.finish() == PositionRans4mFullLiteralDecodeError::none ? 0 : 6;
    }
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(file)), {});
        if (!file || raw.size() < 23) return 2;
        std::vector<std::byte> bytes(raw.size());
        std::transform(raw.begin(), raw.end(), bytes.begin(), [](char v) { return static_cast<std::byte>(v); });
        std::uint32_t dc{}, ps{};
        for (std::size_t i = 0; i < 4; ++i) {
            dc |= std::to_integer<std::uint32_t>(bytes[i]) << (8*i);
            ps |= std::to_integer<std::uint32_t>(bytes[4+i]) << (8*i);
        }
        marc::core::DecoderLimits limits{};
        limits.max_block_size = UINT64_C(40)<<20;
        PositionRans4mFullLiteralDescriptor d{};
        const auto e = parse_position_rans_4m_full_literal_descriptor(bytes, dc, ps, limits, d);
        if (e != Error::none) return 3;
        std::vector<std::byte> copy(9241);
        std::size_t count{};
        if (serialize_position_rans_4m_full_literal_descriptor(d, limits, copy, count) != Error::none
            || count != bytes.size() || !std::equal(bytes.begin(), bytes.end(), copy.begin())) return 4;
        return 0;
    }
    PositionRans4mFullLiteralDescriptor empty{};
    auto bytes = serialize(empty);
    check(bytes.size() == 23, "empty extent");
    PositionRans4mFullLiteralDescriptor decoded{};
    check(parse_position_rans_4m_full_literal_descriptor(bytes, 0, 8, {}, decoded) == Error::none && decoded == empty, "empty roundtrip");
    for (unsigned bit=6;bit<8;++bit) {
        auto bad=bytes;bad[22]=static_cast<std::byte>(1U<<bit);
        check(rejects(bad,0,8), "reserved sixth mask byte");
    }
    auto old_identity=bytes;
    (void)marc::core::store_le<std::uint16_t>(old_identity,10,44);
    (void)marc::core::store_le<std::uint32_t>(old_identity,12,2566);
    check(rejects(old_identity,0,8), "one-MiB descriptor not accepted");
    PositionRans4mFullLiteralDescriptor d{};
    d.decision_count = 4636;
    const auto& offsets = marc::context::internal::lzss_position_distance_4m_full_literal_offsets;
    for (std::size_t c = 0; c < 54; ++c) {
        const auto n = offsets[c+1]-offsets[c];
        for (std::size_t s = 0; s < n; ++s)
            d.frequencies[offsets[c]+s] = static_cast<std::uint16_t>(4096/n + (s < 4096%n));
    }
    bytes = serialize(d);
    check(bytes.size() == 9241, "maximum extent");
    check(parse_position_rans_4m_full_literal_descriptor(bytes, d.decision_count, 8, {}, decoded) == Error::none && decoded == d, "maximum roundtrip");
    for (std::size_t n = 0; n < bytes.size(); ++n)
        check(rejects(std::span(bytes).first(n), d.decision_count, 8), "truncation accepted");
    auto invalid = bytes;
    invalid[22] |= std::byte{0x80};
    check(rejects(invalid, d.decision_count, 8), "mask accepted");
    invalid = bytes; invalid[23] = std::byte{9};
    check(rejects(invalid, d.decision_count, 8), "unknown mode accepted");
    invalid = bytes; invalid.push_back(std::byte{0});
    check(rejects(invalid, d.decision_count, 8), "trailing accepted");
    check(rejects(bytes, 42, 8), "contradictory decision count");
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = d.decision_count-1;
    check(rejects(bytes, d.decision_count, 8, limits), "decision limit accepted");
    limits = {}; limits.max_internal_buffered_bytes = bytes.size()-1;
    check(rejects(bytes, d.decision_count, 8, limits), "descriptor limit accepted");
    std::vector<std::byte> output(9241, std::byte{0xa5});
    const auto saved = output;
    std::size_t count = 123;
    check(serialize_position_rans_4m_full_literal_descriptor(d, {}, std::span(output).first(position_rans_4m_full_literal_descriptor_capacity-1), count) == Error::output_too_small, "short output accepted");
    check(output == saved && count == 123, "short output mutation");
    d.frequencies[0] = 4097;
    check(serialize_position_rans_4m_full_literal_descriptor(d, {}, output, count) == Error::invalid_frequencies, "bad total accepted");
    check(output == saved && count == 123, "invalid model mutation");
    // A dense binary table containing only one symbol is valid frequencies but
    // an invalid representation: canonical mode is single-symbol.
    auto single = serialize(PositionRans4mFullLiteralDescriptor{2,8,{4096}});
    single[23] = std::byte{1}; single[24] = std::byte{0}; single.push_back(std::byte{0x10});
    check(rejects(single, 2, 8), "noncanonical dense accepted");
    // Hand-checkable uniform bit payload [1,0]: state = 4*2^31+2048.
    auto uniform = PositionRans4mFullLiteralDescriptor{4,8,{4096}};
    auto descriptor = serialize(uniform);
    std::array<std::byte,8> payload{};
    (void)marc::core::store_le<std::uint64_t>(payload, 0, (UINT64_C(4)<<31)+2048);
    PositionRans4mFullLiteralDecoder decoder;
    check(decoder.begin(descriptor, 4, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "decoder begin");
    std::uint32_t symbol = 777;
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none && symbol == 0, "single read");
    check(decoder.read(-1,symbol) == PositionRans4mFullLiteralDecodeError::none && symbol == 1, "uniform one");
    // Failed begin must preserve the preceding valid decoder's progress.
    check(decoder.begin(descriptor, 3, payload, {}) == PositionRans4mFullLiteralDecodeError::invalid_descriptor, "bad begin");
    check(decoder.read(-1,symbol) == PositionRans4mFullLiteralDecodeError::none && symbol == 0, "uniform zero / begin invariant");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none && symbol == 0, "last read");
    check(decoder.finish() == PositionRans4mFullLiteralDecodeError::none, "terminal state");
    check(decoder.finish() == PositionRans4mFullLiteralDecodeError::already_finished, "repeat finish");
    check(decoder.begin(descriptor, 4, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "restart begin");
    symbol = 777;
    check(decoder.read(54,symbol) == PositionRans4mFullLiteralDecodeError::invalid_context && symbol == 777, "failed symbol invariant");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::invalid_context && symbol == 777, "sticky failure");
    PositionRans4mFullLiteralDescriptor two{2,8,{4096}};
    auto two_descriptor = serialize(two);
    (void)marc::core::store_le<std::uint64_t>(payload, 0, UINT64_C(1)<<31);
    check(decoder.begin(two_descriptor, 2, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "lower-state begin");
    check(decoder.finish() == PositionRans4mFullLiteralDecodeError::count_mismatch, "short count finish");
    check(decoder.begin(two_descriptor, 2, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "count restart");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "first counted decision");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "second counted decision");
    symbol = 777;
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::decision_count_exceeded && symbol == 777, "count excess");
    two.frequencies[2] = 4096;
    two_descriptor = serialize(two);
    check(decoder.begin(two_descriptor, 2, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "unused-context begin");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "used context first");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "used context second");
    check(decoder.finish() == PositionRans4mFullLiteralDecodeError::unused_context, "unused model refused");
    two.frequencies[2] = 0;
    two.payload_size = 9;
    two_descriptor = serialize(two);
    std::array<std::byte,9> trailing{};
    (void)marc::core::store_le<std::uint64_t>(trailing, 0, UINT64_C(1)<<31);
    check(decoder.begin(two_descriptor, 2, trailing, {}) == PositionRans4mFullLiteralDecodeError::none, "trailing begin");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "trailing first");
    check(decoder.read(0,symbol) == PositionRans4mFullLiteralDecodeError::none, "trailing second");
    check(decoder.finish() == PositionRans4mFullLiteralDecodeError::trailing_payload, "trailing finish");
    PositionRans4mFullLiteralDescriptor rare{1,8,{}};
    rare.frequencies[6] = 1;
    rare.frequencies[7] = 4095;
    const auto rare_descriptor = serialize(rare);
    check(decoder.begin(rare_descriptor, 1, payload, {}) == PositionRans4mFullLiteralDecodeError::none, "rare begin");
    symbol = 777;
    check(decoder.read(3,symbol) == PositionRans4mFullLiteralDecodeError::truncated_payload && symbol == 777, "renormalization truncation");
    (void)marc::core::store_le<std::uint64_t>(payload, 0, UINT64_C(1)<<39);
    check(decoder.begin(rare_descriptor, 1, payload, {}) == PositionRans4mFullLiteralDecodeError::invalid_state, "upper bound state refused");
    (void)marc::core::store_le<std::uint64_t>(payload, 0, (UINT64_C(1)<<31)-1);
    check(decoder.begin(rare_descriptor, 1, payload, {}) == PositionRans4mFullLiteralDecodeError::invalid_state, "lower bound state refused");
    std::cout << "failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
