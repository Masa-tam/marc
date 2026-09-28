#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "context/lzss_position_distance_1m_tokens.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string_view>
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"

// Diagnostic only: production functions are called without instrumentation.
// Replay timings below overlap the complete frame timing and are not additive.
int main(int argc, char** argv) {
    if (argc != 2 && (argc != 3 || std::string_view(argv[2]) != "--five-prefix")) return 2;
    const bool five_prefix = argc == 3;
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) return 2;
    const auto size = file.tellg();
    if (size <= 0 || size > 64 * 1024 * 1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(input.data()), size)) return 2;
    using namespace marc::frame::internal;
    using namespace marc::dictionary::internal;
    using namespace marc::context::internal;
    using namespace marc::entropy::internal;
    using Clock = std::chrono::steady_clock;
    constexpr std::size_t frame_size = 1048576;
    const TypedContextStreamHeader stream{frame_size, input.size(),
        {frame_size, 3, 258, 0}, 32768, 44, 9, 1, 10};
    const marc::core::DecoderLimits limits{};
    const auto search = five_prefix ? LzssPositionDistance1mSearch::indexed_five_prefix
        : LzssPositionDistance1mSearch::indexed;
    const auto required = five_prefix
        ? calculate_lzss_position_distance_1m_five_prefix_workspace(frame_size, stream.dictionary, limits)
        : calculate_lzss_position_distance_1m_match_workspace(frame_size, stream.dictionary, limits);
    if (required.error != LzssShortPrefixError::none) return 1;
    std::vector<std::max_align_t> storage((required.workspace_size + sizeof(std::max_align_t)-1) / sizeof(std::max_align_t));
    const auto finder = std::as_writable_bytes(std::span{storage}).first(required.workspace_size);
    std::vector<LzssTypedToken> tokens(frame_size), decoded(frame_size);
    std::vector<ModeledOperation> operations(2 * frame_size);
    std::vector<std::byte> expected(18 * frame_size + 85), encoded(expected.size()), payload(expected.size()), restored(frame_size);
    std::array<std::array<double, 5>, 3> totals{};
    std::uint64_t token_count{}, operation_count{}, serialized_bytes{};
    for (std::size_t offset = 0; offset < input.size(); offset += frame_size) {
        const auto raw = std::span{input}.subspan(offset, std::min(frame_size, input.size()-offset));
        const auto sequence = offset / frame_size;
        const auto oracle = encode_lzss_position_distance_1m_raw_frame(stream, limits, sequence, offset,
            raw, 3, LzssPositionDistance1mSearch::indexed, tokens, operations, finder, expected);
        if (oracle.error != LzssPositionDistanceRawFrameError::none) return 1;
        const auto bytes = std::span{expected}.first(oracle.frame.serialized_size);
        const auto check = decode_lzss_position_distance_1m_frame(bytes, {stream, limits, sequence, offset}, decoded, restored);
        if (check.error != LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(), raw.end(), restored.begin())) return 1;
        token_count += oracle.candidate.token_count;
        operation_count += oracle.frame.operation_count;
        serialized_bytes += bytes.size();
        // One split-path warmup, then three measured repetitions per frame.
        for (int iteration = -1; iteration < 3; ++iteration) {
            auto begin = Clock::now();
            const auto candidate = tokenize_lzss_position_distance_1m_candidate(raw, stream.dictionary, limits,
                3, search, tokens, finder);
            const auto after_tokens = Clock::now();
            if (candidate.error != LzssShortMatchCandidateError::none) return 1;
            const auto selected = std::span{tokens}.first(candidate.token_count);
            const auto frame = encode_lzss_position_distance_1m_frame(stream, limits, sequence, offset, selected, operations, encoded);
            const auto after_frame = Clock::now();
            if (frame.error != LzssShortMatchFrameEncodeError::none || frame.serialized_size != bytes.size()
                || !std::equal(bytes.begin(), bytes.end(), encoded.begin())) return 1;
            const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(selected.size()), static_cast<std::uint32_t>(raw.size()), offset};
            const auto replay_begin = Clock::now();
            const auto plan = plan_lzss_position_distance_1m_operations(selected, stream.dictionary, context, limits);
            const auto modeled = model_lzss_position_distance_1m_tokens(selected, stream.dictionary, context, limits, operations);
            const auto after_model = Clock::now();
            if (plan.error != LzssFieldContextError::none || modeled.error != LzssFieldContextError::none
                || modeled.operation_count != frame.operation_count) return 1;
            PreparedLzssPositionDistance1mEncode prepared;
            ContextualDynamicRangeDescriptor descriptor{};
            const auto entropy = prepared.prepare(std::span{operations}.first(modeled.operation_count), limits, descriptor);
            const auto after_prepare = Clock::now();
            if (entropy.error != ContextualDynamicRangeEncodeError::none) return 1;
            const auto written = prepared.write(payload, descriptor);
            const auto after_write = Clock::now();
            if (written.error != ContextualDynamicRangeEncodeError::none || written.payload_size != frame.payload_size
                || !std::equal(bytes.begin()+80, bytes.end(), payload.begin())) return 1;
            if (iteration >= 0) {
                auto& t = totals[iteration];
                t[0] += std::chrono::duration<double>(after_tokens-begin).count();
                t[1] += std::chrono::duration<double>(after_frame-after_tokens).count();
                t[2] += std::chrono::duration<double>(after_model-replay_begin).count();
                t[3] += std::chrono::duration<double>(after_prepare-after_model).count();
                t[4] += std::chrono::duration<double>(after_write-after_prepare).count();
            }
        }
    }
    std::cout << std::setprecision(12) << "input_bytes=" << input.size() << "\ntokens=" << token_count
        << "\nsearch_mode=" << (five_prefix ? "five-prefix" : "indexed")
        << "\noperations=" << operation_count << "\nframe_bytes=" << serialized_bytes << "\nverified_iterations=3\n";
    const std::array names{"tokenize", "frame", "replay_model", "replay_prepare", "replay_write"};
    for (std::size_t i=0; i<totals.size(); ++i)
        for (std::size_t j=0; j<names.size(); ++j)
            std::cout << "iteration_" << i << '_' << names[j] << "_seconds=" << totals[i][j] << '\n';
}
