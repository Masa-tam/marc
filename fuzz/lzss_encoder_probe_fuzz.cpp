#include "dictionary/lzss_typed_encoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {
using namespace marc::dictionary::internal;
constexpr std::size_t maximum_input = 512;

void require(bool condition) noexcept {
    if (!condition) std::abort();
}

std::uint64_t exercise(std::span<const std::byte> input, std::uint8_t selector) {
    constexpr std::array<std::uint32_t, 4> windows{5, 17, 257, 65536};
    constexpr std::array<std::uint32_t, 4> lengths{5, 6, 17, 258};
    LzssParameters parameters{};
    parameters.window_size = windows[selector % windows.size()];
    parameters.max_match_length = lengths[(selector / 4) % lengths.size()];
    marc::core::DecoderLimits limits{};
    alignas(std::max_align_t) std::array<std::byte, 16384> workspace{};
    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    require(required.error == LzssHashChainError::none
            && required.workspace_size <= workspace.size());
    const auto scratch = std::span{workspace}.first(required.workspace_size);
    std::array<LzssTypedToken, maximum_input> production{}, control{}, reference{};
    LzssMatchFinderStatistics statistics{};
    const auto p = encode_lzss_typed_tokens_hash_chain_single_pass(
        input, parameters, limits, production, scratch, &statistics);
    const auto c = encode_lzss_typed_tokens_hash_chain_no_probe_single_pass(
        input, parameters, limits, control, scratch);
    const auto r = encode_lzss_typed_tokens(
        input, parameters, limits, reference);
    require(p.error == LzssTypedEncodeError::none
            && c.error == LzssTypedEncodeError::none
            && r.error == LzssTypedEncodeError::none);
    require(p.token_count == c.token_count && p.token_count == r.token_count
            && p.token_count <= production.size());
    std::array<std::byte, maximum_input> restored{};
    std::size_t position = 0;
    for (std::size_t i = 0; i < p.token_count; ++i) {
        const auto& token = production[i];
        for (const auto* other : {&control[i], &reference[i]}) {
            require(token.kind == other->kind && token.literal == other->literal
                    && token.distance == other->distance
                    && token.length == other->length);
        }
        std::uint64_t next = 0;
        require(validate_lzss_typed_token(token, parameters,
                    {position, input.size()}, limits, next)
                == LzssTypedTokenError::none);
        require(next <= input.size());
        if (token.kind == LzssTypedTokenKind::literal) {
            restored[position++] = static_cast<std::byte>(token.literal);
        } else {
            require(token.distance != 0 && token.distance <= position);
            for (std::uint32_t j = 0; j < token.length; ++j) {
                restored[position] = restored[position - token.distance];
                ++position;
            }
        }
        require(position == next);
    }
    require(position == input.size()
            && std::equal(input.begin(), input.end(), restored.begin()));
    require(!statistics.overflowed
            && statistics.candidate_count
                == statistics.hash_chain_prefix_match_count
                    + statistics.hash_chain_prefix_mismatch_count
                    + statistics.hash_chain_best_length_probe_pruned_candidate_count);
    return statistics.hash_chain_best_length_probe_pruned_candidate_count;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
    if (size > maximum_input) return 0;
    static const bool checked_probe = [] {
        constexpr char seed[] = "ABCDEaaaaQ|ABCDEbbbbR|ABCDEbbbbSZZ";
        const auto bytes = std::as_bytes(std::span{seed}).first(sizeof(seed) - 1);
        exercise(bytes, 15);
        alignas(std::max_align_t) std::array<std::byte, 16384> storage{};
        LzssHashChainMatchFinder finder{};
        LzssMatchFinderStatistics statistics{};
        require(initialize_lzss_hash_chain_match_finder(
                    bytes, {}, {}, storage, finder, &statistics)
                == LzssHashChainError::none);
        LzssExhaustiveMatchFinder reference{bytes, {}};
        for (std::size_t position = 0; position < bytes.size(); ++position) {
            require(finder.find_match(position) == reference.find_match(position));
            finder.advance(position, position + 1);
            reference.advance(position, position + 1);
        }
        require(statistics.hash_chain_best_length_probe_pruned_candidate_count > 0);
        return true;
    }();
    static_cast<void>(checked_probe);
    const std::span<const std::byte> input{
        reinterpret_cast<const std::byte*>(data), size};
    const auto selector = size == 0 ? std::uint8_t{0} : data[0];
    exercise(input, selector);
    // A small alphabet complements arbitrary bytes with frequent overlapping
    // matches. The same bounded input controls both independent comparisons.
    std::array<std::byte, maximum_input> patterned{};
    for (std::size_t i = 0; i < size; ++i)
        patterned[i] = static_cast<std::byte>(data[i] & 3U);
    exercise(std::span{patterned}.first(size), selector);
    return 0;
}
