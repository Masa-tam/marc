#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::max_align_t>& storage,
    const std::size_t bytes) {
    storage.resize(
        (bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    return std::as_writable_bytes(std::span{storage}).first(bytes);
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(marc::core::ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualRansProfile, BuildsCanonicalWorkspaceBounds) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 795'529U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_offset,
              65'536U
                  * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.match_finder_alignment,
              finder.workspace_alignment);
    EXPECT_EQ(workspace.match_finder_strategy,
              marc::dictionary::internal::
                  LzssMatchFinderStrategy::hash_chain_exact);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + finder.workspace_size);
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::dictionary::internal::LzssTypedToken),
                  finder.workspace_alignment));

    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {17}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9301U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace.frame_input_bytes = 1;
    workspace.frame_encoded_bytes = 1;
    workspace.token_count = 1;
    workspace.match_finder_offset = 1;
    workspace.match_finder_bytes = 1;
    workspace.views_bytes = 1;
    workspace.views_alignment = 8;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.token_count, 0U);
    EXPECT_EQ(workspace.match_finder_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);

    LzssContextualRansProfileConfig binary_tree{};
    binary_tree.original_size = 65'536;
    binary_tree.match_finder_strategy = marc::dictionary::internal::
        LzssMatchFinderStrategy::binary_tree_exact;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  binary_tree, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    const auto tree = marc::dictionary::internal::
        calculate_lzss_match_finder_workspace(
            binary_tree.match_finder_strategy, 65'536,
            binary_tree.dictionary, {});
    ASSERT_EQ(tree.error, marc::dictionary::internal::
                              LzssMatchFinderWorkspaceError::none);
    EXPECT_EQ(workspace.match_finder_bytes, tree.workspace_size);
    EXPECT_EQ(workspace.match_finder_alignment,
              tree.workspace_alignment);
    EXPECT_EQ(workspace.match_finder_strategy,
              binary_tree.match_finder_strategy);

    LzssContextualRansProfileConfig extended{};
    extended.original_size = 17;
    extended.frame_size = 17;
    extended.dictionary.window_size = UINT32_C(1) << 20;
    extended.variant = LzssContextualRansProfileVariant::field_context_1m;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  extended, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 3U);
    EXPECT_EQ(stream.context_variant, 2U);
    EXPECT_EQ(stream.frequency_entry_count, 4550U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9365U);
}

TEST(LzssContextualRansProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 11'089U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_rans_decode_table_entries);
    EXPECT_EQ(workspace.token_count, 1024U);

    LzssContextualRansDecoderWorkspaceRequirements extended{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, extended,
                  LzssContextualRansProfileVariant::field_context_1m),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(extended.frame_encoded_bytes, 11'153U);
    EXPECT_EQ(extended.table_count, workspace.table_count);
    EXPECT_EQ(extended.token_count, workspace.token_count);

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualRansProfile,
     FourMiBProfileFitsDefaultAggregateWithExactBoundaries) {
    LzssContextualRansProfileConfig config{};
    config.original_size = UINT32_C(1) << 22;
    config.frame_size = UINT32_C(1) << 22;
    config.dictionary.window_size = UINT32_C(1) << 22;
    config.variant = LzssContextualRansProfileVariant::field_context_4m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT32_C(1) << 22;
    limits.max_block_size = UINT32_C(7) << 22;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 4U);
    EXPECT_EQ(stream.context_variant, 3U);
    EXPECT_EQ(stream.frequency_entry_count, 4566U);
    EXPECT_EQ(encoder.frame_input_bytes, 4'194'304U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 58'729'449U);
    EXPECT_EQ(encoder.token_count, 4'194'304U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.match_finder_offset, 50'331'648U);
        EXPECT_EQ(encoder.match_finder_bytes, 17'301'504U);
        EXPECT_EQ(encoder.views_bytes, 67'633'152U);
        EXPECT_EQ(encoder_aggregate, 130'556'905U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);

    limits.max_block_size = (UINT32_C(7) << 22) - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    limits.max_block_size = UINT32_C(7) << 22;

    LzssContextualRansDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_4m),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 58'729'449U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 4'194'304U);
    EXPECT_EQ(decoder.table_count, 126'976U);
    EXPECT_EQ(decoder.token_count, 4'194'304U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (sizeof(marc::entropy::internal::RansDecodeEntry) == 6
                  && sizeof(marc::dictionary::internal::LzssTypedToken)
                         == 12) {
        EXPECT_EQ(decoder.token_offset, 761'856U);
        EXPECT_EQ(decoder.views_bytes, 51'093'504U);
        EXPECT_EQ(decoder_aggregate, 114'017'257U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_4m),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_4m),
              LzssContextualRansProfileError::none);
}

TEST(LzssContextualRansProfile,
     SixteenMiBProfileRequiresExplicitAggregateWithExactBoundaries) {
    LzssContextualRansProfileConfig config{};
    config.original_size = UINT32_C(1) << 24;
    config.frame_size = UINT32_C(1) << 24;
    config.dictionary.window_size = UINT32_C(1) << 24;
    config.variant = LzssContextualRansProfileVariant::field_context_16m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT32_C(1) << 24;
    limits.max_block_size = UINT32_C(7) << 24;
    limits.max_compressed_payload_size = 234'881'032U;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder{};

    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 5U);
    EXPECT_EQ(stream.context_variant, 4U);
    EXPECT_EQ(stream.frequency_entry_count, 4582U);
    EXPECT_EQ(encoder.frame_input_bytes, 16'777'216U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 234'890'249U);
    EXPECT_EQ(encoder.token_count, 16'777'216U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.match_finder_offset, 201'326'592U);
        EXPECT_EQ(encoder.match_finder_bytes, 67'633'152U);
        EXPECT_EQ(encoder.views_bytes, 268'959'744U);
        EXPECT_EQ(encoder_aggregate, 520'627'209U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);

    limits.max_block_size = (UINT32_C(7) << 24) - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    limits.max_block_size = UINT32_C(7) << 24;

    LzssContextualRansDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_16m),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_16m),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 234'890'249U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 16'777'216U);
    EXPECT_EQ(decoder.table_count, 126'976U);
    EXPECT_EQ(decoder.token_count, 16'777'216U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (sizeof(marc::entropy::internal::RansDecodeEntry) == 6
                  && sizeof(marc::dictionary::internal::LzssTypedToken)
                         == 12) {
        EXPECT_EQ(decoder.token_offset, 761'856U);
        EXPECT_EQ(decoder.views_bytes, 202'088'448U);
        EXPECT_EQ(decoder_aggregate, 453'755'913U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_16m),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_16m),
              LzssContextualRansProfileError::none);
}

TEST(LzssContextualRansProfile,
     SixtyFourMiBProfileCalculatesBothExactFindersAndDecoder) {
    LzssContextualRansProfileConfig config{};
    config.original_size = UINT32_C(1) << 26;
    config.frame_size = UINT32_C(1) << 26;
    config.dictionary.window_size = UINT32_C(1) << 26;
    config.variant = LzssContextualRansProfileVariant::field_context_64m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT32_C(1) << 26;
    limits.max_block_size = UINT32_C(8) << 26;
    limits.max_compressed_payload_size = 1'073'741'832U;
    limits.max_internal_buffered_bytes = UINT64_C(4) << 30;
    limits.max_lz_distance = UINT32_C(1) << 26;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder{};

    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 6U);
    EXPECT_EQ(stream.context_variant, 5U);
    EXPECT_EQ(stream.frequency_entry_count, 4598U);
    EXPECT_EQ(encoder.frame_input_bytes, 67'108'864U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 1'073'751'081U);
    EXPECT_EQ(encoder.token_count, 67'108'864U);
    EXPECT_EQ(encoder.match_finder_offset, 805'306'368U);
    EXPECT_EQ(encoder.match_finder_bytes, 268'959'744U);
    EXPECT_EQ(encoder.views_bytes, 1'074'266'112U);
    const auto chain_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    EXPECT_EQ(chain_aggregate, 2'215'126'057U);
    limits.max_internal_buffered_bytes = chain_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = chain_aggregate;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);

    config.match_finder_strategy = marc::dictionary::internal::
        LzssMatchFinderStrategy::binary_tree_exact;
    limits.max_internal_buffered_bytes = UINT64_C(4) << 30;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(encoder.match_finder_offset, 805'306'368U);
    EXPECT_EQ(encoder.match_finder_bytes, 1'946'157'056U);
    EXPECT_EQ(encoder.views_bytes, 2'751'463'424U);
    const auto tree_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    EXPECT_EQ(tree_aggregate, 3'892'323'369U);
    limits.max_internal_buffered_bytes = tree_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = tree_aggregate;
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  config, limits, stream, encoder),
              LzssContextualRansProfileError::none);

    LzssContextualRansDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(4) << 30;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_64m),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 1'073'751'081U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 67'108'864U);
    EXPECT_EQ(decoder.table_count, 126'976U);
    EXPECT_EQ(decoder.token_count, 67'108'864U);
    EXPECT_EQ(decoder.token_offset, 761'856U);
    EXPECT_EQ(decoder.views_bytes, 806'068'224U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    EXPECT_EQ(decoder_aggregate, 1'946'928'169U);
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_64m),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder,
                  LzssContextualRansProfileVariant::field_context_64m),
              LzssContextualRansProfileError::none);
}

TEST(LzssContextualRansProfile,
     RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    LzssContextualRansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);
    unsupported = {};
    unsupported.match_finder_strategy = static_cast<
        marc::dictionary::internal::LzssMatchFinderStrategy>(255);
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);
    unsupported = {};
    unsupported.variant =
        static_cast<LzssContextualRansProfileVariant>(255);
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);
    LzssContextualRansDecoderWorkspaceRequirements decoder_workspace{};
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  {}, decoder_workspace,
                  static_cast<LzssContextualRansProfileVariant>(255)),
              LzssContextualRansProfileError::unsupported);
    EXPECT_EQ(decoder_workspace.views_bytes, 0U);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 211;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 100;
    limits.max_block_size = 100;
    limits.max_internal_buffered_bytes = 10'000;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {100, 100, {}}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
}

TEST(LzssContextualRansProfile,
     RequirementsConstructCanonicalStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {raw.size(), 2, {}}, limits, stream,
                  encoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_requirements.views_bytes);
    LzssContextualRansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::none);
    EXPECT_EQ(encode_views.tokens.size(), encoder_requirements.token_count);
    EXPECT_EQ(encode_views.match_finder.size(),
              encoder_requirements.match_finder_bytes);
    EXPECT_EQ(encode_views.match_finder.data(),
              encode_storage.data()
                  + encoder_requirements.match_finder_offset);
    auto forged = encoder_requirements;
    ++forged.match_finder_offset;
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  forged, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(encode_views.tokens.empty());
    EXPECT_TRUE(encode_views.match_finder.empty());
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements,
                  encode_storage.first(encode_storage.size() - 1),
                  encode_views),
              LzssContextualRansWorkspaceError::too_small);
    std::vector<std::max_align_t> misaligned_backing;
    auto misaligned = aligned_storage(
        misaligned_backing, encoder_requirements.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements,
                  misaligned.subspan(1, encoder_requirements.views_bytes),
                  encode_views),
              LzssContextualRansWorkspaceError::misaligned);
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.match_finder, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);
    ASSERT_GE(encoded.size(), lzss_contextual_rans_stream_header_size);
    EXPECT_EQ(encoded[18], std::byte{0x03});

    LzssContextualRansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_requirements.views_bytes);
    LzssContextualRansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements, decode_storage, decode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}
