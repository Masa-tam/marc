# Explicit inventory, based on the 2026-10-20 hosted MSVC/GCC logs: a test
# exceeded 60 seconds on at least one platform. Do not infer cost from a
# codec's window size or silently classify newly added tests by name.
# Labels affect only CI selection; an unfiltered CTest still runs everything.
set(_marc_ci_deep_tests
    marc_cli_position_adaptive_huffman_32m
    marc_cli_position_adaptive_huffman_32m_static
    marc_cli_position_adaptive_huffman_64m
    marc_cli_position_adaptive_huffman_64m_static
    marc_cli_position_blocked_huffman_32m
    marc_cli_position_blocked_huffman_32m_static
    marc_cli_position_blocked_huffman_64m
    marc_cli_position_blocked_huffman_64m_static
    marc_cli_position_distance_8m_boundaries
    marc_cli_position_distance_16m_boundaries
    marc_cli_position_distance_32m_boundaries
    marc_cli_position_distance_64m_boundaries
    marc_cli_position_distance_64m_stress
    marc_cli_position_rans_32m
    marc_cli_position_rans_64m
    marc_cli_position_tans_64m
    marc_interoperability_schema_compatibility
    marc_position_adaptive_huffman_64m_api
    marc_position_blocked_huffman_8m_public_raw_differential
    marc_position_blocked_huffman_8m_raw_differential
    marc_position_blocked_huffman_16m_public_raw_differential
    marc_position_blocked_huffman_16m_raw_differential
    marc_position_blocked_huffman_32m_frame_differential
    marc_position_blocked_huffman_32m_public_raw_differential
    marc_position_blocked_huffman_32m_raw_differential
    marc_position_blocked_huffman_32m_token_differential
    marc_position_blocked_huffman_64m_frame_differential
    marc_position_blocked_huffman_64m_public_raw_differential
    marc_position_blocked_huffman_64m_raw_differential
    marc_position_blocked_huffman_64m_token_differential
    marc_position_distance_64m_reference
    marc_position_distance_64m_stream_conformance
    marc_position_rans_16m_full_literal_stream_decoder
    marc_position_rans_32m_full_literal_stream_decoder
    marc_position_rans_64m_full_literal_frame_differential
    marc_position_rans_64m_full_literal_stream_decoder
    marc_position_rans_64m_full_literal_token_differential
)
# CLI fuzz campaigns and libFuzzer smoke tests deliberately stay in regular CI,
# even when an individual fuzz campaign exceeded the duration threshold.
foreach(_marc_ci_test IN LISTS _marc_ci_deep_tests)
    if(TEST "${_marc_ci_test}")
        set_property(TEST "${_marc_ci_test}" APPEND PROPERTY LABELS ci-deep)
    endif()
endforeach()
unset(_marc_ci_test)
unset(_marc_ci_deep_tests)
