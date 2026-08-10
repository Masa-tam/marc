#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "entropy/contextual_huffman_estimator.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace {

[[nodiscard]] bool read_file(const std::filesystem::path& path,
                             std::vector<std::byte>& bytes) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > std::numeric_limits<std::size_t>::max()
        || size > static_cast<std::uintmax_t>(
                      std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "input size is unavailable or unsupported\n";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "input open failed\n";
        return false;
    }
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            std::cerr << "input read failed\n";
            return false;
        }
    }
    return true;
}

void print_estimate(
    const char* name,
    const marc::entropy::internal::ContextualHuffmanEstimate& estimate) {
    std::cout << name << "_active_tables=" << estimate.active_tables
              << '\n'
              << name << "_stored_models=" << estimate.stored_models << '\n'
              << name << "_selected_contexts="
              << estimate.selected_contexts << '\n'
              << name << "_descriptor_bytes=" << estimate.descriptor_bytes
              << '\n'
              << name << "_symbol_bits=" << estimate.symbol_bits << '\n'
              << name << "_bypass_bits=" << estimate.bypass_bits << '\n'
              << name << "_payload_bytes=" << estimate.payload_bytes << '\n'
              << name << "_total_bytes=" << estimate.total_bytes << '\n';
}

[[nodiscard]] int run(const std::filesystem::path& path) {
    std::vector<std::byte> input;
    if (!read_file(path, input)) return 1;

    marc::core::DecoderLimits limits{};
    const marc::dictionary::internal::LzssParameters parameters{};
    const auto typed_plan =
        marc::dictionary::internal::plan_lzss_typed_tokens(
            input, parameters, limits);
    if (typed_plan.error
        != marc::dictionary::internal::LzssTypedEncodeError::none) {
        std::cerr << "typed LZSS planning failed\n";
        return 1;
    }
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(
        typed_plan.token_count);
    const auto typed = marc::dictionary::internal::encode_lzss_typed_tokens(
        input, parameters, limits, tokens);
    if (typed.error
        != marc::dictionary::internal::LzssTypedEncodeError::none) {
        std::cerr << "typed LZSS encoding failed\n";
        return 1;
    }

    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(input.size()), 0};
    const auto operation_plan =
        marc::context::internal::plan_lzss_field_context_operations(
            tokens, parameters, context, limits);
    if (operation_plan.error
        != marc::context::internal::LzssFieldContextError::none) {
        std::cerr << "field-context planning failed\n";
        return 1;
    }
    std::vector<marc::context::internal::ModeledOperation> operations(
        operation_plan.operation_count);
    const auto modeled =
        marc::context::internal::model_lzss_field_context_tokens(
            tokens, parameters, context, limits, operations);
    if (modeled.error
        != marc::context::internal::LzssFieldContextError::none) {
        std::cerr << "field-context modeling failed\n";
        return 1;
    }
    const auto estimate =
        marc::entropy::internal::estimate_contextual_huffman_cost(operations);
    if (estimate.error
        != marc::entropy::internal::ContextualHuffmanEstimateError::none) {
        std::cerr << "contextual Huffman estimation failed at operation "
                  << estimate.operation_index << '\n';
        return 1;
    }
    const auto serialized =
        marc::dictionary::internal::plan_lzss_token_stream(
            input, parameters, limits);
    if (serialized.error
        != marc::dictionary::internal::LzssEncodeError::none) {
        std::cerr << "serialized LZSS planning failed\n";
        return 1;
    }

    std::cout << "input_bytes=" << input.size() << '\n'
              << "token_count=" << tokens.size() << '\n'
              << "operation_count=" << operations.size() << '\n'
              << "serialized_lzss_bytes=" << serialized.output_size << '\n';
    print_estimate("field_tables", estimate.estimates.field_tables);
    print_estimate("selective_context_tables",
                   estimate.estimates.selective_context_tables);
    print_estimate("contextual_tables",
                   estimate.estimates.contextual_tables);
    print_estimate("shared_contextual_tables",
                   estimate.estimates.shared_contextual_tables);
    return 0;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        std::cerr << "usage: marc_contextual_huffman_estimator <input>\n";
        return 2;
    }
    return run(argv[1]);
}
