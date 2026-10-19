#include "entropy/position_adaptive_huffman_format.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::entropy::internal;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
using Error=PositionAdaptiveHuffmanError;
void check(bool condition) { if (!condition) throw std::runtime_error("native Adaptive Huffman format check failed"); }
std::vector<std::byte> unhex(const std::string& text) {
    check(text.size()%2==0);
    std::vector<std::byte> bytes;
    for (std::size_t i=0;i<text.size();i+=2)
        bytes.push_back(static_cast<std::byte>(std::stoul(text.substr(i,2),nullptr,16)));
    return bytes;
}
void oracle(const std::vector<std::byte>& bytes,std::uint32_t decisions,std::uint32_t payload) {
    PositionAdaptiveHuffmanDescriptor parsed{};
    check(parse_position_adaptive_huffman_descriptor(profile,bytes,decisions,payload,{},parsed)==Error::none);
    std::array<std::byte,20> output; output.fill(std::byte{0x7b});
    check(serialize_position_adaptive_huffman_descriptor(profile,parsed,{},output)==Error::none);
    check(std::equal(bytes.begin(),bytes.end(),output.begin()));
    check(std::all_of(output.begin()+16,output.end(),[](auto b){return b==std::byte{0x7b};}));
}
}
int main(int argc,char** argv) {
    try {
        if (argc==4) {
            oracle(unhex(argv[1]),static_cast<std::uint32_t>(std::stoul(argv[2])),static_cast<std::uint32_t>(std::stoul(argv[3])));
            return 0;
        }
        check(argc==1);
        const auto hand=unhex("02000000020000003000010000000000");
        oracle(hand,2,2);
        oracle(unhex("00000000000000003000000000000000"),0,0);
        PositionAdaptiveHuffmanDescriptor sentinel{17,19,23,7,9};
        const auto rejects=[&](std::vector<std::byte> bytes) {
            auto output=sentinel;
            check(parse_position_adaptive_huffman_descriptor(profile,bytes,2,2,{},output)!=Error::none);
            check(output==sentinel);
        };
        for (std::size_t n=0;n<16;++n) rejects({hand.begin(),hand.begin()+n});
        for (const auto offset:std::array<std::size_t,9>{0,4,8,10,11,12,13,14,15}) {
            auto bytes=hand; bytes[offset]^=std::byte{0x80}; rejects(bytes);
        }
        auto extra=hand;extra.push_back(std::byte{});rejects(extra);
        PositionAdaptiveHuffmanDescriptor d{2,2,48,1,0};
        std::array<std::byte,20> target;target.fill(std::byte{0x52});const auto before=target;
        check(serialize_position_adaptive_huffman_descriptor(profile,d,{},std::span(target).first(15))==Error::output_too_small);
        check(target==before);
        auto limits=marc::core::DecoderLimits{};
        limits.max_block_size=2;
        limits.max_internal_buffered_bytes=17;
        check(marc::core::validate_limits(limits)==marc::core::LimitError::none);
        check(serialize_position_adaptive_huffman_descriptor(profile,d,limits,target)==Error::limit_exceeded);
        check(target==before);
        check(validate_position_adaptive_huffman_descriptor(static_cast<PositionAdaptiveHuffmanProfile>(255),d,2,2,{})==Error::invalid_profile);
        const auto* spec=position_adaptive_huffman_spec(profile);check(spec!=nullptr);
        std::size_t entries{};
        for (std::size_t c=0;c<spec->contexts;++c) entries+=position_adaptive_huffman_alphabet(*spec,c);
        check(entries==4570 && spec->node_entries()==9188);
        check(position_adaptive_huffman_alphabet(*spec,48)==0);
        std::cout<<"native Adaptive Huffman64KiB format checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
