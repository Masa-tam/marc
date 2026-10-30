# Retired stage-one reproduction commands

Audited against `v0.8.0` and source snapshot `58b12eda` on 2026-10-31. These are
historical measurement/tool targets and runners/manifests, not current commands.
Use `git show v0.8.0:CMakeLists.txt` or `git show v0.8.0:<path>` to inspect the
original recipes and provenance. Do not run the examples from an old document
against the current tree expecting the retired driver/interface to exist.

The current replacement for public timing/resource comparisons is `marc_benchmark`
and `tools/run_public_benchmark.py`; see the [measurement contract](../benchmarks.md).
Independent dictionary/entropy oracles and the optional rANS diagnostic exporter
remain maintained as described in the [source-role audit](v1-refactoring-stage-1-audit.md).
Experiment-only test targets/private adapter fuzz harnesses are covered by their
retirement records and are not measurement commands in this index.

## Retired executable targets (87)

Each name was declared with a benchmark/tool cpp source in the baseline CMake
file, is absent from both current ordinary and optional target inventories,
and has no remaining source file. The names below are CMake targets; historical
recipes may additionally show an `.exe` suffix or build-directory prefix.

| Historical target | Original source path |
| --- | --- |
| `marc_contextual_huffman_estimator` | `tools/contextual_huffman_estimator.cpp` |
| `marc_lzss_contextual_rans_phase_benchmark` | `benchmarks/lzss_contextual_rans_phase_benchmark.cpp` |
| `marc_lzss_match_finder_benchmark` | `benchmarks/lzss_match_finder_benchmark.cpp` |
| `marc_lzss_position_distance_1m_bounded18_benchmark` | `benchmarks/lzss_position_distance_1m_bounded18_benchmark.cpp` |
| `marc_lzss_position_distance_1m_bounded18_stream_benchmark` | `benchmarks/lzss_position_distance_1m_bounded18_stream_benchmark.cpp` |
| `marc_lzss_position_distance_1m_finder_benchmark` | `benchmarks/lzss_position_distance_1m_finder_benchmark.cpp` |
| `marc_lzss_position_distance_1m_finder_scratch_diagnostic` | `benchmarks/lzss_position_distance_1m_finder_scratch_diagnostic.cpp` |
| `marc_lzss_position_distance_1m_finder_scratch_stream_benchmark` | `benchmarks/lzss_position_distance_1m_finder_scratch_stream_benchmark.cpp` |
| `marc_lzss_position_distance_1m_five_finder_benchmark` | `benchmarks/lzss_position_distance_1m_five_finder_benchmark.cpp` |
| `marc_lzss_position_distance_1m_five_prefix_benchmark` | `benchmarks/lzss_position_distance_1m_five_prefix_benchmark.cpp` |
| `marc_lzss_position_distance_1m_lazy_six_benchmark` | `benchmarks/lzss_position_distance_1m_lazy_six_benchmark.cpp` |
| `marc_lzss_position_distance_1m_observation_benchmark` | `benchmarks/lzss_position_distance_1m_observation_benchmark.cpp` |
| `marc_lzss_position_distance_1m_online_six_benchmark` | `benchmarks/lzss_position_distance_1m_online_six_benchmark.cpp` |
| `marc_lzss_position_distance_1m_phase_benchmark` | `benchmarks/lzss_position_distance_1m_phase_benchmark.cpp` |
| `marc_lzss_position_distance_1m_prepared_model_benchmark` | `benchmarks/lzss_position_distance_1m_prepared_model_benchmark.cpp` |
| `marc_lzss_position_distance_1m_prepared_phase_benchmark` | `benchmarks/lzss_position_distance_1m_prepared_phase_benchmark.cpp` |
| `marc_lzss_position_distance_1m_prepared_stream_benchmark` | `benchmarks/lzss_position_distance_1m_prepared_stream_benchmark.cpp` |
| `marc_lzss_position_distance_1m_runtime_width_benchmark` | `benchmarks/lzss_position_distance_1m_runtime_width_benchmark.cpp` |
| `marc_lzss_position_distance_1m_same_code_benchmark` | `benchmarks/lzss_position_distance_1m_same_code_benchmark.cpp` |
| `marc_lzss_position_distance_1m_search_counts_benchmark` | `benchmarks/lzss_position_distance_1m_search_counts_benchmark.cpp` |
| `marc_lzss_position_distance_1m_shared_advance_benchmark` | `benchmarks/lzss_position_distance_1m_shared_advance_benchmark.cpp` |
| `marc_lzss_position_distance_1m_six_compact_benchmark` | `benchmarks/lzss_position_distance_1m_six_compact_benchmark.cpp` |
| `marc_lzss_position_distance_1m_six_fifth_first_benchmark` | `benchmarks/lzss_position_distance_1m_six_fifth_first_benchmark.cpp` |
| `marc_lzss_position_distance_1m_six_phase_benchmark` | `benchmarks/lzss_position_distance_1m_six_phase_benchmark.cpp` |
| `marc_lzss_position_distance_1m_six_prefix_benchmark` | `benchmarks/lzss_position_distance_1m_six_prefix_benchmark.cpp` |
| `marc_lzss_position_distance_1m_six_rolling_benchmark` | `benchmarks/lzss_position_distance_1m_six_rolling_benchmark.cpp` |
| `marc_lzss_position_distance_1m_stream_benchmark` | `benchmarks/lzss_position_distance_1m_stream_benchmark.cpp` |
| `marc_lzss_position_distance_1m_wide_five_benchmark` | `benchmarks/lzss_position_distance_1m_wide_five_benchmark.cpp` |
| `marc_lzss_position_distance_1m_width_control_benchmark` | `benchmarks/lzss_position_distance_1m_width_control_benchmark.cpp` |
| `marc_lzss_position_distance_4m_bucket18_owner_benchmark` | `benchmarks/lzss_position_distance_4m_bucket18_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_end_probe_diagnostic` | `benchmarks/lzss_position_distance_4m_end_probe_diagnostic.cpp`, `benchmarks/lzss_position_distance_4m_end_probe_finder.cpp` |
| `marc_lzss_position_distance_4m_end_probe_owner_benchmark` | `benchmarks/lzss_position_distance_4m_end_probe_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_finder_diagnostic` | `benchmarks/lzss_position_distance_4m_finder_diagnostic.cpp`, `benchmarks/lzss_position_distance_4m_diagnostic_finder.cpp` |
| `marc_lzss_position_distance_4m_finder_scratch_owner_benchmark` | `benchmarks/lzss_position_distance_4m_finder_scratch_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_five_prefix_phase_benchmark` | `benchmarks/lzss_position_distance_4m_five_prefix_phase_benchmark.cpp` |
| `marc_lzss_position_distance_4m_owner_benchmark` | `benchmarks/lzss_position_distance_4m_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_owner_phase_benchmark` | `benchmarks/lzss_position_distance_4m_owner_phase_benchmark.cpp` |
| `marc_lzss_position_distance_4m_phase_benchmark` | `benchmarks/lzss_position_distance_4m_phase_benchmark.cpp` |
| `marc_lzss_position_distance_4m_phase_split_owner_benchmark` | `benchmarks/lzss_position_distance_4m_phase_split_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_prepared_owner_benchmark` | `benchmarks/lzss_position_distance_4m_prepared_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_probe17_owner_benchmark` | `benchmarks/lzss_position_distance_4m_probe17_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_probe_length_diagnostic` | `benchmarks/lzss_position_distance_4m_probe_length_diagnostic.cpp`, `benchmarks/lzss_position_distance_4m_probe_length_finder.cpp` |
| `marc_lzss_position_distance_4m_profile_benchmark` | `benchmarks/lzss_position_distance_4m_profile_benchmark.cpp` |
| `marc_lzss_position_distance_4m_range_prepared_owner_benchmark` | `benchmarks/lzss_position_distance_4m_range_prepared_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_tokenizer_scope` | `benchmarks/lzss_position_distance_4m_tokenizer_scope_diagnostic.cpp` |
| `marc_lzss_position_distance_4m_word4_owner_benchmark` | `benchmarks/lzss_position_distance_4m_word4_owner_benchmark.cpp` |
| `marc_lzss_position_distance_4m_word8_owner_benchmark` | `benchmarks/lzss_position_distance_4m_word8_owner_benchmark.cpp` |
| `marc_lzss_position_distance_public_benchmark` | `benchmarks/lzss_position_distance_public_benchmark.cpp` |
| `marc_lzss_position_distance_stream_benchmark` | `benchmarks/lzss_position_distance_stream_benchmark.cpp` |
| `marc_lzss_short_match_candidate_benchmark` | `benchmarks/lzss_short_match_candidate_benchmark.cpp` |
| `marc_lzss_short_match_diagnostic` | `benchmarks/lzss_short_match_diagnostic.cpp`, `benchmarks/lzss_short_match_probe.cpp` |
| `marc_position_adaptive_huffman_64k_precursor` | `benchmarks/position_adaptive_huffman_64k_precursor.cpp` |
| `marc_position_adaptive_huffman_measure` | `benchmarks/position_adaptive_huffman_measure.cpp` |
| `marc_position_blocked_huffman_1m_resource_precursor` | `benchmarks/position_blocked_huffman_1m_resource_precursor.cpp` |
| `marc_position_blocked_huffman_4m_resource_precursor` | `benchmarks/position_blocked_huffman_4m_resource_precursor.cpp` |
| `marc_position_blocked_huffman_64k_diagnose` | `benchmarks/position_blocked_huffman_64k_diagnose.cpp` |
| `marc_position_blocked_huffman_8m_resource_precursor` | `benchmarks/position_blocked_huffman_8m_resource_precursor.cpp` |
| `marc_position_blocked_huffman_large_measure` | `benchmarks/position_blocked_huffman_large_measure.cpp` |
| `marc_position_distance_4m_control_repeatability` | `benchmarks/lzss_position_distance_4m_control_repeatability.cpp` |
| `marc_position_distance_4m_end_probe_tests` | `benchmarks/lzss_position_distance_4m_end_probe_finder.cpp` |
| `marc_position_distance_4m_finder_diagnostic_tests` | `benchmarks/lzss_position_distance_4m_diagnostic_finder.cpp` |
| `marc_position_distance_4m_probe_length_tests` | `benchmarks/lzss_position_distance_4m_probe_length_finder.cpp` |
| `marc_position_rans_16m_benchmark` | `benchmarks/position_rans_16m_benchmark.cpp` |
| `marc_position_rans_16m_resources` | `benchmarks/position_rans_16m_resources.cpp` |
| `marc_position_rans_1m_benchmark` | `benchmarks/position_rans_1m_benchmark.cpp` |
| `marc_position_rans_1m_cli_resources` | `benchmarks/position_rans_1m_cli_resources.cpp` |
| `marc_position_rans_32m_benchmark` | `benchmarks/position_rans_32m_benchmark.cpp` |
| `marc_position_rans_32m_resources` | `benchmarks/position_rans_32m_resources.cpp` |
| `marc_position_rans_4m_benchmark` | `benchmarks/position_rans_4m_benchmark.cpp` |
| `marc_position_rans_4m_cli_resources` | `benchmarks/position_rans_1m_cli_resources.cpp` |
| `marc_position_rans_64k_benchmark` | `benchmarks/position_rans_64k_benchmark.cpp` |
| `marc_position_rans_64k_cli_resources` | `benchmarks/position_rans_1m_cli_resources.cpp` |
| `marc_position_rans_64m_benchmark` | `benchmarks/position_rans_64m_benchmark.cpp` |
| `marc_position_rans_64m_resources` | `benchmarks/position_rans_64m_resources.cpp` |
| `marc_position_rans_8m_benchmark` | `benchmarks/position_rans_8m_benchmark.cpp` |
| `marc_position_rans_8m_resources` | `benchmarks/position_rans_8m_resources.cpp` |
| `marc_position_tans_16m_benchmark` | `benchmarks/position_tans_16m_benchmark.cpp` |
| `marc_position_tans_1m_benchmark` | `benchmarks/position_tans_1m_benchmark.cpp` |
| `marc_position_tans_1m_public_resources` | `benchmarks/position_tans_1m_public_resources.cpp` |
| `marc_position_tans_1m_resources` | `benchmarks/position_tans_1m_resources.cpp` |
| `marc_position_tans_32m_benchmark` | `benchmarks/position_tans_32m_benchmark.cpp` |
| `marc_position_tans_4m_benchmark` | `benchmarks/position_tans_4m_benchmark.cpp` |
| `marc_position_tans_64k_benchmark` | `benchmarks/position_tans_64k_benchmark.cpp` |
| `marc_position_tans_64k_public_resources` | `benchmarks/position_tans_64k_public_resources.cpp` |
| `marc_position_tans_64k_resources` | `benchmarks/position_tans_64k_resources.cpp` |
| `marc_position_tans_64m_benchmark` | `benchmarks/position_tans_64m_benchmark.cpp` |
| `marc_position_tans_8m_benchmark` | `benchmarks/position_tans_8m_benchmark.cpp` |

## Retired runners and manifests (68)

These baseline paths have retired with their experimental interfaces. JSON
manifests were inert configuration data, not shell commands. Keep historical
results separate from the currently maintained benchmark/corpus verifier.

- `benchmarks/experiments/lzss-hash-chain-bucket-scaling-synthetic-v1.json`
- `benchmarks/experiments/lzss-hash-chain-prefix-mixer-synthetic-v1.json`
- `benchmarks/experiments/lzss-sparse-snapshot-delta-budget-synthetic-v1.json`
- `benchmarks/experiments/silesia-contextual-rans-best-length-probe-full-v1.json`
- `benchmarks/experiments/silesia-contextual-rans-best-length-probe-v1.json`
- `benchmarks/experiments/silesia-contextual-rans-phase-full-v1.json`
- `benchmarks/experiments/silesia-contextual-rans-tokenize-full-v1.json`
- `benchmarks/experiments/silesia-hash-chain-best-length-probe-full-v1.json`
- `benchmarks/experiments/silesia-hash-chain-best-length-probe-v1.json`
- `benchmarks/experiments/silesia-hash-chain-bucket-scaling-v1.json`
- `benchmarks/experiments/silesia-hash-chain-end-to-end-ab-v1.json`
- `benchmarks/experiments/silesia-position-distance-streams-v1.json`
- `benchmarks/experiments/silesia-sparse-hash-tree-immutable-snapshot-v1.json`
- `benchmarks/experiments/silesia-sparse-hash-tree-reuse-gate-v1.json`
- `benchmarks/position_distance_1m_activation_model.py`
- `benchmarks/run_position_distance_1m_activation_model.py`
- `benchmarks/verify_position_distance_1m_online_six_report.py`
- `tools/analyze_position_distance_4m_phases.py`
- `tools/audit_position_adaptive_huffman.py`
- `tools/audit_position_adaptive_huffman_64k_precursor.py`
- `tools/audit_position_blocked_huffman_64k_refit_comparison.py`
- `tools/audit_position_blocked_huffman_large_measurements.py`
- `tools/audit_position_blocked_huffman_pool_refit_comparison.py`
- `tools/diagnose_position_blocked_huffman_64k.py`
- `tools/generate_lzss_snapshot_delta_synthetic.py`
- `tools/measure_position_adaptive_huffman.py`
- `tools/measure_position_adaptive_huffman_64k_precursor.py`
- `tools/run_lzss_hash_chain_bucket_scaling_experiment.py`
- `tools/run_lzss_hash_chain_prefix_mixer_experiment.py`
- `tools/run_lzss_hash_tree_threshold_matrix.py`
- `tools/run_lzss_match_finder_synthetic_matrix.py`
- `tools/run_lzss_snapshot_delta_budget_synthetic_experiment.py`
- `tools/run_lzss_wavl_tree_synthetic_experiment.py`
- `tools/run_position_blocked_huffman_1m_comparison.py`
- `tools/run_position_blocked_huffman_1m_resource_precursor.py`
- `tools/run_position_blocked_huffman_4m_comparison.py`
- `tools/run_position_blocked_huffman_4m_resource_precursor.py`
- `tools/run_position_blocked_huffman_64k_comparison.py`
- `tools/run_position_blocked_huffman_64k_refit_comparison.py`
- `tools/run_position_blocked_huffman_8m_comparison.py`
- `tools/run_position_blocked_huffman_8m_resource_precursor.py`
- `tools/run_position_blocked_huffman_large_measurements.py`
- `tools/run_position_blocked_huffman_pool_refit_comparison.py`
- `tools/run_position_tans_16m_comparison.py`
- `tools/run_position_tans_1m_resources.py`
- `tools/run_position_tans_64k_resources.py`
- `tools/run_position_tans_8m_comparison.py`
- `tools/run_silesia_binary_tree_16m_experiment.py`
- `tools/run_silesia_binary_tree_64m_experiment.py`
- `tools/run_silesia_contextual_rans_best_length_probe.py`
- `tools/run_silesia_contextual_rans_best_length_probe_full.py`
- `tools/run_silesia_contextual_rans_phase_full.py`
- `tools/run_silesia_contextual_rans_phase_pilot.py`
- `tools/run_silesia_contextual_rans_tokenize_full.py`
- `tools/run_silesia_hash_chain_best_length_probe.py`
- `tools/run_silesia_hash_chain_best_length_probe_full.py`
- `tools/run_silesia_hash_chain_bucket_scaling_experiment.py`
- `tools/run_silesia_hash_chain_end_to_end_ab.py`
- `tools/run_silesia_hash_tree_4m_experiment.py`
- `tools/run_silesia_hash_tree_threshold_benchmark.py`
- `tools/run_silesia_match_finder_benchmark.py`
- `tools/run_silesia_position_distance_streams.py`
- `tools/run_silesia_red_black_tree_experiment.py`
- `tools/run_silesia_scapegoat_tree_experiment.py`
- `tools/run_silesia_sparse_hash_tree_large_window_experiment.py`
- `tools/run_silesia_sparse_hash_tree_matrix.py`
- `tools/run_silesia_sparse_hash_tree_reuse_gate_experiment.py`
- `tools/run_silesia_sparse_hash_tree_snapshot_experiment.py`
