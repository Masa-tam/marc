#include "core/bit_io.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "core/status.hpp"
#include "marc/marc.h"

#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

int main() {
    assert(marc_abi_version() == MARC_ABI_VERSION);
    assert(std::string_view{marc_version_string()} == "0.1.0");
    assert(std::string_view{marc_status_name(MARC_STATUS_NEED_INPUT)} == "need_input");

    using marc::core::ErrorCode;
    using marc::core::ProcessResult;
    using marc::core::StreamStatus;

    assert(marc::core::is_valid(
        ProcessResult{1, 0, StreamStatus::progress, {}}, 1, 0));
    assert(!marc::core::is_valid(
        ProcessResult{0, 0, StreamStatus::progress, {}}, 1, 1));
    assert(!marc::core::is_valid(
        ProcessResult{2, 0, StreamStatus::need_input, {}}, 1, 1));
    assert(marc::core::is_valid(
        ProcessResult{0, 0, StreamStatus::error,
                      {ErrorCode::malformed_stream, 4, 3}}, 1, 1));

    std::size_t arithmetic_result{};
    assert(marc::core::checked_add<std::size_t>(4, 5, arithmetic_result));
    assert(arithmetic_result == 9);
    assert(!marc::core::checked_add<std::size_t>(
        std::numeric_limits<std::size_t>::max(), 1, arithmetic_result));
    assert(marc::core::checked_multiply<std::size_t>(7, 6, arithmetic_result));
    assert(arithmetic_result == 42);
    assert(!marc::core::checked_multiply<std::size_t>(
        std::numeric_limits<std::size_t>::max(), 2, arithmetic_result));

    std::array<std::byte, 14> serialized{};
    assert(marc::core::store_le<std::uint16_t>(serialized, 0, 0x1234));
    assert(marc::core::store_le<std::uint32_t>(serialized, 2, 0x12345678));
    assert(marc::core::store_le<std::uint64_t>(
        serialized, 6, UINT64_C(0x0123456789abcdef)));
    assert(serialized[0] == std::byte{0x34});
    assert(serialized[1] == std::byte{0x12});
    assert(serialized[2] == std::byte{0x78});
    assert(serialized[13] == std::byte{0x01});
    assert(!marc::core::store_le<std::uint32_t>(serialized, 12, 0));
    std::uint64_t loaded64{};
    assert(marc::core::load_le<std::uint64_t>(serialized, 6, loaded64));
    assert(loaded64 == UINT64_C(0x0123456789abcdef));
    assert(!marc::core::load_le<std::uint64_t>(serialized, 7, loaded64));

    using marc::core::BitIoStatus;
    marc::core::BitWriter writer;
    std::array<std::byte, 1> encoded{};
    auto written = writer.write_bits(0x4d, 8, {});
    assert(written.bits_consumed == 8);
    assert(written.bytes_produced == 0);
    written = writer.write_bits(0, 1, encoded);
    assert(written.bytes_produced == 1);
    assert(written.bits_consumed == 1);
    assert(encoded[0] == std::byte{0x4d});
    written = writer.finish({});
    assert(written.status == BitIoStatus::need_output);
    written = writer.finish(encoded);
    assert(written.status == BitIoStatus::finished);
    assert(written.bytes_produced == 1);
    assert(encoded[0] == std::byte{0x00});

    marc::core::BitWriter padded_writer;
    written = padded_writer.write_bits(5, 3, {});
    assert(written.bits_consumed == 3);
    written = padded_writer.finish(encoded);
    assert(written.status == BitIoStatus::finished);
    assert(encoded[0] == std::byte{0x05});

    marc::core::BitReader reader;
    const std::array input{std::byte{0x4d}};
    auto read = reader.read_bits({}, 3);
    assert(read.status == BitIoStatus::need_input);
    read = reader.read_bits(input, 3);
    assert(read.status == BitIoStatus::complete);
    assert(read.value == 5);
    assert(read.bytes_consumed == 1);
    read = reader.read_bits({}, 5);
    assert(read.status == BitIoStatus::complete);
    assert(read.value == 9);
    assert(read.bytes_consumed == 0);
    assert(reader.align_to_byte(true) == BitIoStatus::complete);

    marc::core::BitReader strict_reader;
    const std::array bad_padding{std::byte{0xfd}};
    read = strict_reader.read_bits(bad_padding, 3);
    assert(read.value == 5);
    assert(strict_reader.align_to_byte(true) == BitIoStatus::invalid_padding);
}
