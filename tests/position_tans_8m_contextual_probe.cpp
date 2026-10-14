#include "marc/marc.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

namespace {

constexpr std::uint32_t eight_mib = UINT32_C(8) << 20;

[[nodiscard]] std::uint16_t load16(const std::vector<std::uint8_t>& bytes,
                                    const std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t load32(const std::vector<std::uint8_t>& bytes,
                                    const std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] std::uint64_t load64(const std::vector<std::uint8_t>& bytes,
                                    const std::size_t offset) {
    std::uint64_t result{};
    for (std::size_t index{}; index < 8; ++index)
        result |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
    return result;
}

[[nodiscard]] bool configure(const marc_direction direction,
                              const std::size_t original_size,
                              marc_lzss_contextual_tans_config& config) {
    if (marc_lzss_contextual_tans_config_init(direction, &config)
            != MARC_STATUS_OK
        || marc_lzss_contextual_tans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_16M)
            != MARC_STATUS_OK)
        return false;
    config.original_size = original_size;
    config.frame_size = eight_mib;
    config.window_size = eight_mib;
    config.max_total_output_size = eight_mib;
    config.max_frame_size = eight_mib;
    config.max_block_size = UINT64_C(9) * eight_mib;
    config.max_compressed_payload_size = UINT64_C(128) << 20;
    config.max_internal_buffered_bytes = UINT64_C(1) << 30;
    config.max_lz_distance = eight_mib;
    config.max_lz_match_length = 258;
    config.max_entropy_table_entries = UINT64_C(1) << 20;
    return true;
}

[[nodiscard]] bool encode_8m_contextual(
    const std::span<const std::uint8_t> input,
    std::vector<std::uint8_t>& wire) {
    marc_lzss_contextual_tans_config config{};
    marc_workspace_requirements requirements{};
    if (!configure(MARC_DIRECTION_ENCODE, input.size(), config)) {
        std::cerr << "contextual encode configure failed\n";
        return false;
    }
    const auto requirement_status = marc_lzss_contextual_tans_workspace_requirements(
        &config, &requirements);
    if (requirement_status != MARC_STATUS_OK) {
        std::cerr << "contextual encode requirements=" << requirement_status << '\n';
        return false;
    }
    std::vector<std::uint8_t> primary(requirements.primary_bytes);
    std::vector<std::uint8_t> secondary(requirements.secondary_bytes);
    std::vector<std::uint8_t> views(requirements.views_bytes);
    marc_transform* transform{};
    const auto create_status = marc_lzss_contextual_tans_create(
            &config, {primary.data(), primary.size()},
            {secondary.data(), secondary.size()}, {views.data(), views.size()},
            &transform);
    if (create_status != MARC_STATUS_OK) {
        std::cerr << "contextual encode create=" << create_status
                  << " primary=" << requirements.primary_bytes
                  << " secondary=" << requirements.secondary_bytes
                  << " views=" << requirements.views_bytes
                  << " alignment=" << requirements.views_alignment << '\n';
        return false;
    }
    std::array<std::uint8_t, 40000> output{};
    const auto result = marc_transform_process(
        transform, {input.data(), input.size()}, {output.data(), output.size()},
        MARC_PROCESS_END_INPUT);
    marc_transform_destroy(transform);
    if (result.status != MARC_STATUS_END_OF_STREAM
        || result.input_consumed != input.size()) {
        std::cerr << "contextual encode process=" << result.status
                  << " consumed=" << result.input_consumed
                  << " produced=" << result.output_produced << '\n';
        return false;
    }
    wire.assign(output.begin(), output.begin() + result.output_produced);
    return true;
}

[[nodiscard]] bool decode_8m_contextual(
    const std::span<const std::uint8_t> wire,
    const std::span<const std::uint8_t> expected) {
    marc_lzss_contextual_tans_config config{};
    marc_workspace_requirements requirements{};
    if (!configure(MARC_DIRECTION_DECODE, expected.size(), config)
        || marc_lzss_contextual_tans_workspace_requirements(
               &config, &requirements) != MARC_STATUS_OK)
        return false;
    std::vector<std::uint8_t> primary(requirements.primary_bytes);
    std::vector<std::uint8_t> secondary(requirements.secondary_bytes);
    std::vector<std::uint8_t> views(requirements.views_bytes);
    marc_transform* transform{};
    if (marc_lzss_contextual_tans_create(
            &config, {primary.data(), primary.size()},
            {secondary.data(), secondary.size()}, {views.data(), views.size()},
            &transform) != MARC_STATUS_OK)
        return false;
    std::array<std::uint8_t, 32> output{};
    const auto result = marc_transform_process(
        transform, {wire.data(), wire.size()}, {output.data(), output.size()},
        MARC_PROCESS_END_INPUT);
    marc_transform_destroy(transform);
    return result.status == MARC_STATUS_END_OF_STREAM
        && result.input_consumed == wire.size()
        && result.output_produced == expected.size()
        && std::memcmp(output.data(), expected.data(), expected.size()) == 0;
}

} // namespace

int main() {
    marc_lzss_contextual_tans_config config{};
    if (marc_lzss_contextual_tans_config_init(MARC_DIRECTION_ENCODE, &config)
        != MARC_STATUS_OK) return 2;
    const auto retained = config;
    // The public enum has no 8 MiB member. Value five is outside its complete
    // documented set and must reject without partially applying a profile.
    const auto status = marc_lzss_contextual_tans_config_apply_profile(
        &config, static_cast<marc_lzss_contextual_profile>(UINT32_C(5)));
    if (status != MARC_STATUS_INVALID_ARGUMENT) return 3;
    if (config.profile != retained.profile || config.frame_size != retained.frame_size
        || config.window_size != retained.window_size
        || config.max_internal_buffered_bytes != retained.max_internal_buffered_bytes)
        return 4;
    std::cout << "eight-MiB contextual tANS profile rejected without config mutation\n";
    for (auto direction : {MARC_DIRECTION_ENCODE}) {
        for (auto profile : {MARC_LZSS_CONTEXTUAL_PROFILE_64K,
                             MARC_LZSS_CONTEXTUAL_PROFILE_1M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_4M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_16M,
                             MARC_LZSS_CONTEXTUAL_PROFILE_64M}) {
            if (marc_lzss_contextual_tans_config_init(direction,&config)!=MARC_STATUS_OK
                || marc_lzss_contextual_tans_config_apply_profile(&config,profile)!=MARC_STATUS_OK)
                return 5;
            // Probe admission only; no workspace is allocated. Raise resource
            // limits so a resource cap cannot masquerade as an identity error.
            config.frame_size=config.window_size=8388608;
            config.original_size=1;
            config.max_frame_size=config.max_lz_distance=8388608;
            config.max_block_size=UINT64_C(9)*8388608;
            config.max_compressed_payload_size=UINT64_C(128)<<20;
            config.max_internal_buffered_bytes=UINT64_C(4)<<30;
            marc_workspace_requirements requirements{};
            const auto queried=marc_lzss_contextual_tans_workspace_requirements(&config,&requirements);
            std::cout<<"direction="<<direction<<" profile="<<profile
                     <<" window=8388608 frame=8388608 status="<<queried<<'\n';
            const auto expected=profile>=MARC_LZSS_CONTEXTUAL_PROFILE_16M
                ? MARC_STATUS_OK : MARC_STATUS_UNSUPPORTED;
            if (queried!=expected) return 6;
        }
    }
    const std::array<std::uint8_t, 5> input{{0x41, 0x42, 0x41, 0x42, 0x58}};
    std::vector<std::uint8_t> wire;
    if (!encode_8m_contextual(input, wire) || wire.size() < 112) return 7;
    if (wire[0] != 0x4d || wire[1] != 0x41 || wire[2] != 0x52 || wire[3] != 0x43
        || load16(wire, 12) != 2 || load16(wire, 14) != 5
        || load16(wire, 16) != 5 || load16(wire, 18) != 2
        || load32(wire, 20) != eight_mib || load64(wire, 40) != input.size()
        || load32(wire, 64) != eight_mib || load16(wire, 98) != 4)
        return 8;
    if (!decode_8m_contextual(wire, input)) return 9;
    std::cout << "eight-MiB contextual tANS control encodes and restores profile-16 identity\n";
    return 0;
}
