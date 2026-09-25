#include "context/lzss_position_distance_context_layout.hpp"
#include "entropy/lzss_position_distance_binary_models.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {
using namespace marc::context::internal;
using namespace marc::entropy::internal;

TEST(LzssPositionDistanceLayout, DenseExtentsAndFrozenPrefix) {
    EXPECT_EQ(lzss_position_distance_context_count, 40);
    EXPECT_EQ(lzss_position_distance_frequency_entries, 2522);
    std::size_t offset{};
    for (std::size_t id = 0; id < 40; ++id) {
        const unsigned alphabet = id < 3 ? 2 : id < 12 ? 256 : id < 15 ? 9 : id < 24 ? 17 : 2;
        EXPECT_EQ(lzss_position_distance_alphabets[id], alphabet);
        EXPECT_EQ(lzss_position_distance_offsets[id], offset);
        offset += alphabet;
        if (id < 24) EXPECT_EQ(lzss_position_distance_alphabets[id], lzss_reduced_literal_alphabets[id]);
    }
    EXPECT_EQ(lzss_position_distance_offsets.back(), offset);
    EXPECT_EQ(lzss_reduced_literal_context_count, 24);
    EXPECT_EQ(lzss_reduced_literal_frequency_entries, 2490);
}

TEST(LzssPositionDistanceLayout, ExactIdentityAndCountOnly) {
    const std::array<std::uint16_t, 7> original{2,8,1,9,3,2,40};
    const auto accepted = [](const auto& f) {
        return is_lzss_position_distance_identity(f[0],f[1],f[2],f[3],f[4],f[5],f[6]);
    };
    ASSERT_TRUE(accepted(original));
    for (std::size_t i = 0; i < original.size(); ++i) {
        for (const auto value : {0U,1U,2U,3U,7U,8U,9U,24U,32U,40U,65535U}) {
            if (value == original[i]) continue;
            auto fields = original;
            fields[i] = static_cast<std::uint16_t>(value);
            EXPECT_FALSE(accepted(fields));
        }
    }
    EXPECT_FALSE(is_lzss_reduced_literal_identity(2,8,1,9,3,2,40));
}

TEST(LzssPositionDistanceLayout, BackendContextsCannotBeInjectedAsTokenSymbols) {
    for (std::uint16_t id = 0; id < 40; ++id) {
        const auto alphabet = lzss_position_distance_alphabets[id];
        ModeledOperation op{ModeledOperationKind::symbol,id,alphabet,0,0};
        EXPECT_EQ(validate_lzss_position_distance_symbol(op), id < 24
            ? LzssFieldContextError::none : LzssFieldContextError::unexpected_context);
    }
    EXPECT_EQ(validate_lzss_position_distance_symbol({ModeledOperationKind::symbol,65535,2,0,0}), LzssFieldContextError::unexpected_context);
    EXPECT_EQ(validate_lzss_position_distance_symbol({ModeledOperationKind::symbol,0,2,2,0}), LzssFieldContextError::invalid_symbol);
    EXPECT_EQ(validate_lzss_position_distance_symbol({ModeledOperationKind::symbol,0,3,0,0}), LzssFieldContextError::unexpected_alphabet);
    EXPECT_EQ(validate_lzss_position_distance_symbol({ModeledOperationKind::symbol,0,2,0,1}), LzssFieldContextError::nonzero_unused_field);
    EXPECT_EQ(validate_lzss_position_distance_symbol({ModeledOperationKind::bypass_bits,0,0,1,1}), LzssFieldContextError::unexpected_operation_kind);
}

void expect_interval(const LzssPositionDistanceBinaryModels& models, std::size_t p,
                     std::uint32_t bit, std::uint32_t cumulative,
                     std::uint16_t frequency, std::uint32_t total) {
    DistanceBinaryInterval result{};
    ASSERT_TRUE(models.interval(p, bit, result));
    EXPECT_EQ(result.cumulative, cumulative);
    EXPECT_EQ(result.frequency, frequency);
    EXPECT_EQ(result.total, total);
}

TEST(LzssPositionDistanceBinary, HandIntervalsLsbOrderAndReset) {
    LzssPositionDistanceBinaryModels models;
    expect_interval(models,0,0,0,1,2);
    ASSERT_TRUE(models.update(0,0));
    expect_interval(models,0,0,0,2,3);
    ASSERT_TRUE(models.update(0,0));
    expect_interval(models,0,1,3,1,4);
    models.reset();
    for (std::size_t p = 0; p < 3; ++p) {
        const auto bit = static_cast<std::uint32_t>((5U >> p) & 1U);
        expect_interval(models,p,bit,bit,1,2);
        ASSERT_TRUE(models.update(p,bit));
    }
    expect_interval(models,0,1,1,2,3);
    expect_interval(models,1,0,0,2,3);
    expect_interval(models,2,1,1,2,3);
    for (std::size_t p = 3; p < 16; ++p) expect_interval(models,p,0,0,1,2);
    models.reset();
    for (std::size_t p = 0; p < 16; ++p) expect_interval(models,p,1,1,1,2);
}

TEST(LzssPositionDistanceBinary, CeilingHalfRescalingForBothSymbols) {
    for (std::uint32_t bit = 0; bit < 2; ++bit) {
        LzssPositionDistanceBinaryModels models;
        for (unsigned i = 0; i < 32765; ++i) ASSERT_TRUE(models.update(15,bit));
        expect_interval(models,15,bit,bit,32766,32767);
        ASSERT_TRUE(models.update(15,bit));
        expect_interval(models,15,bit,bit,16384,16385);
        expect_interval(models,0,0,0,1,2);
    }
}

TEST(LzssPositionDistanceBinary, InvalidAccessPreservesStateAndOutput) {
    LzssPositionDistanceBinaryModels models;
    for (const auto p : {std::size_t{16},std::numeric_limits<std::size_t>::max()}) {
        DistanceBinaryInterval result{7,8,9};
        EXPECT_FALSE(models.interval(p,0,result));
        EXPECT_FALSE(models.update(p,0));
        EXPECT_EQ(result.cumulative,7); EXPECT_EQ(result.frequency,8); EXPECT_EQ(result.total,9);
    }
    DistanceBinaryInterval result{7,8,9};
    EXPECT_FALSE(models.interval(0,UINT32_MAX,result));
    EXPECT_FALSE(models.update(0,2));
    EXPECT_EQ(result.cumulative,7); EXPECT_EQ(result.frequency,8); EXPECT_EQ(result.total,9);
    for (std::size_t p = 0; p < 16; ++p) expect_interval(models,p,0,0,1,2);
}
} // namespace
