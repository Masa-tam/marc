cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(REAL_PATH "${SOURCE_DIR}" source_dir)
set(required_documents
    README.md
    CHANGELOG.md
    CONTRIBUTING.md
    THIRD_PARTY_NOTICES.md
    docs/README.md
    docs/architecture.md
    docs/baseline-readiness.md
    docs/benchmarks.md
    docs/c-api.md
    docs/cli.md
    docs/composition.md
    docs/format.md
    docs/fuzzing.md
    docs/interoperability.md
    docs/releasing.md
    docs/implementation/README.md
    docs/implementation/clean-room-record.md
    docs/implementation/design-decisions.md
    docs/implementation/references.md
    docs/implementation/test-vector-generation.md)

foreach(relative_path IN LISTS required_documents)
    if(NOT EXISTS "${source_dir}/${relative_path}")
        message(FATAL_ERROR "Required document is missing: ${relative_path}")
    endif()
endforeach()

set(third_party_notice "${source_dir}/THIRD_PARTY_NOTICES.md")
set(googletest_license "${source_dir}/third_party/googletest/LICENSE")
if(NOT EXISTS "${googletest_license}")
    message(FATAL_ERROR
        "GoogleTest license is unavailable; initialize the submodule")
endif()
file(READ "${third_party_notice}" notice_content)
file(READ "${googletest_license}" googletest_license_content)
string(REPLACE "\r\n" "\n" notice_content "${notice_content}")
string(REPLACE "\r\n" "\n" googletest_license_content
    "${googletest_license_content}")
string(REGEX REPLACE "\n+$" "" googletest_license_content
    "${googletest_license_content}")
set(expected_license_fence
    "```text\n${googletest_license_content}\n```")
string(FIND "${notice_content}" "${expected_license_fence}" license_offset)
if(license_offset EQUAL -1)
    message(FATAL_ERROR
        "GoogleTest notice must reproduce third_party/googletest/LICENSE")
endif()

set(legacy_record_paths
    docs/clean-room-record.md
    docs/design-decisions.md
    docs/references.md
    docs/test-vector-generation.md)
foreach(relative_path IN LISTS legacy_record_paths)
    if(EXISTS "${source_dir}/${relative_path}")
        message(FATAL_ERROR
            "Implementation record must remain separated: ${relative_path}")
    endif()
endforeach()

set(design_decisions
    "${source_dir}/docs/implementation/design-decisions.md")
file(STRINGS "${design_decisions}" decision_headings
    REGEX "^## DD-[0-9]+:")
if(NOT decision_headings)
    message(FATAL_ERROR "No design-decision headings were found")
endif()
set(expected_decision 1)
foreach(heading IN LISTS decision_headings)
    if(NOT heading MATCHES "^## DD-([0-9]+):")
        message(FATAL_ERROR "Invalid design-decision heading: ${heading}")
    endif()
    set(decision_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" decision_number "${decision_number}")
    if(decision_number STREQUAL "")
        set(decision_number 0)
    endif()
    if(NOT decision_number EQUAL expected_decision)
        message(FATAL_ERROR
            "Design decisions must be contiguous and ordered: expected "
            "DD-${expected_decision}, found ${heading}")
    endif()
    math(EXPR expected_decision "${expected_decision} + 1")
endforeach()
list(LENGTH decision_headings decision_count)

set(clean_room_record
    "${source_dir}/docs/implementation/clean-room-record.md")
file(STRINGS "${clean_room_record}" clean_room_headings
    REGEX "^## CR-[0-9]+: [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]")
if(NOT clean_room_headings)
    message(FATAL_ERROR "No dated clean-room headings were found")
endif()
set(previous_record_date "")
set(expected_clean_room_record 1)
foreach(heading IN LISTS clean_room_headings)
    if(NOT heading MATCHES
       "^## CR-([0-9]+): ([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9])")
        message(FATAL_ERROR "Invalid clean-room heading: ${heading}")
    endif()
    set(record_number "${CMAKE_MATCH_1}")
    set(record_date "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "^0+" "" record_number "${record_number}")
    if(record_number STREQUAL "")
        set(record_number 0)
    endif()
    if(NOT record_number EQUAL expected_clean_room_record)
        message(FATAL_ERROR
            "Clean-room records must be contiguous and ordered: expected "
            "CR-${expected_clean_room_record}, found ${heading}")
    endif()
    if(NOT previous_record_date STREQUAL ""
       AND record_date STRLESS previous_record_date)
        message(FATAL_ERROR
            "Clean-room records must be chronological: ${record_date} "
            "follows ${previous_record_date} at ${heading}")
    endif()
    set(previous_record_date "${record_date}")
    math(EXPR expected_clean_room_record
        "${expected_clean_room_record} + 1")
endforeach()
list(LENGTH clean_room_headings clean_room_record_count)

set(reference_record
    "${source_dir}/docs/implementation/references.md")
file(READ "${reference_record}" reference_content)
foreach(required_reference_section IN ITEMS
        "## Foundational and project references"
        "## Implementation reference ledger")
    string(FIND "${reference_content}" "${required_reference_section}"
        reference_section_offset)
    if(reference_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing reference section: ${required_reference_section}")
    endif()
endforeach()
file(STRINGS "${reference_record}" implementation_reference_headings
    REGEX "^### IR-[0-9]+$")
if(NOT implementation_reference_headings)
    message(FATAL_ERROR "No implementation-reference headings were found")
endif()
set(expected_implementation_reference 1)
foreach(heading IN LISTS implementation_reference_headings)
    if(NOT heading MATCHES "^### IR-([0-9]+)$")
        message(FATAL_ERROR "Invalid implementation-reference heading: ${heading}")
    endif()
    set(reference_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" reference_number "${reference_number}")
    if(reference_number STREQUAL "")
        set(reference_number 0)
    endif()
    if(NOT reference_number EQUAL expected_implementation_reference)
        message(FATAL_ERROR
            "Implementation references must be contiguous and ordered: expected "
            "IR-${expected_implementation_reference}, found ${heading}")
    endif()
    math(EXPR expected_implementation_reference
        "${expected_implementation_reference} + 1")
endforeach()
list(LENGTH implementation_reference_headings
    implementation_reference_count)

set(test_vector_record
    "${source_dir}/docs/implementation/test-vector-generation.md")
file(READ "${test_vector_record}" test_vector_content)
foreach(required_test_vector_section IN ITEMS
        "## Generation policy"
        "## Vector development ledger")
    string(FIND "${test_vector_content}" "${required_test_vector_section}"
        test_vector_section_offset)
    if(test_vector_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing test-vector section: ${required_test_vector_section}")
    endif()
endforeach()
file(STRINGS "${test_vector_record}" test_vector_headings
    REGEX "^### TVG-[0-9]+$")
if(NOT test_vector_headings)
    message(FATAL_ERROR "No test-vector record headings were found")
endif()
set(expected_test_vector_record 1)
foreach(heading IN LISTS test_vector_headings)
    if(NOT heading MATCHES "^### TVG-([0-9]+)$")
        message(FATAL_ERROR "Invalid test-vector record heading: ${heading}")
    endif()
    set(test_vector_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" test_vector_number
        "${test_vector_number}")
    if(test_vector_number STREQUAL "")
        set(test_vector_number 0)
    endif()
    if(NOT test_vector_number EQUAL expected_test_vector_record)
        message(FATAL_ERROR
            "Test-vector records must be contiguous and ordered: expected "
            "TVG-${expected_test_vector_record}, found ${heading}")
    endif()
    math(EXPR expected_test_vector_record
        "${expected_test_vector_record} + 1")
endforeach()
list(LENGTH test_vector_headings test_vector_record_count)

set(architecture_document "${source_dir}/docs/architecture.md")
file(READ "${architecture_document}" architecture_content)
set(previous_architecture_section_offset -1)
foreach(required_architecture_section IN ITEMS
        "## Buffered incremental reference encoder"
        "## Entropy codec foundations"
        "## C transform ABI"
        "## Composed profile boundaries"
        "## Experimental typed-token context pipeline")
    string(FIND "${architecture_content}" "${required_architecture_section}"
        architecture_section_offset)
    if(architecture_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing architecture section: ${required_architecture_section}")
    endif()
    if(architecture_section_offset LESS_EQUAL previous_architecture_section_offset)
        message(FATAL_ERROR
            "Architecture sections are out of order at: "
            "${required_architecture_section}")
    endif()
    set(previous_architecture_section_offset
        "${architecture_section_offset}")
endforeach()

set(expected_architecture_profiles
    "LZ77 plus Blocked Huffman validation boundary"
    "LZ77 plus Blocked Huffman publication evidence"
    "LZ77 plus Adaptive Huffman validation boundary"
    "LZ77 plus Dynamic Range staged boundary"
    "Specified LZ77 plus rANS boundary"
    "Specified LZ77 plus tANS boundary"
    "LZSS plus Blocked Huffman validation boundary"
    "LZSS plus Adaptive Huffman specified boundary"
    "LZSS plus Dynamic Range specified boundary"
    "Specified LZSS plus rANS boundary"
    "Specified LZSS plus tANS boundary"
    "Published LZ78 plus Blocked Huffman frame boundary"
    "Specified LZ78 plus Adaptive Huffman boundary"
    "LZ78 plus Dynamic Range specified boundary"
    "Validated LZ78 plus rANS boundary"
    "Specified LZ78 plus tANS boundary"
    "Published LZW plus Blocked Huffman boundary"
    "Published LZW plus Adaptive Huffman boundary"
    "Specified LZW plus Dynamic Range boundary"
    "Specified LZW plus rANS boundary"
    "Specified LZW plus tANS boundary"
    "Published LZD plus Blocked Huffman boundary"
    "Published LZD plus Blocked Huffman implementation evidence"
    "Published LZD plus Adaptive Huffman boundary"
    "Specified LZD plus Dynamic Range boundary"
    "LZD plus rANS boundary"
    "LZD plus tANS boundary"
    "Published LZMW plus Blocked Huffman boundary"
    "Published LZMW plus Adaptive Huffman boundary"
    "Published LZMW plus Dynamic Range boundary"
    "LZMW plus rANS boundary"
    "LZMW plus tANS boundary"
    "LZMW plus tANS public profile")
set(previous_architecture_profile_offset -1)
foreach(architecture_profile IN LISTS expected_architecture_profiles)
    set(architecture_profile_heading "### ${architecture_profile}")
    string(FIND "${architecture_content}" "${architecture_profile_heading}"
        architecture_profile_offset)
    if(architecture_profile_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing architecture profile: ${architecture_profile_heading}")
    endif()
    if(architecture_profile_offset LESS_EQUAL previous_architecture_profile_offset)
        message(FATAL_ERROR
            "Architecture profiles are out of matrix order at: "
            "${architecture_profile_heading}")
    endif()
    set(previous_architecture_profile_offset
        "${architecture_profile_offset}")
endforeach()

set(format_document "${source_dir}/docs/format.md")
file(READ "${format_document}" format_content)
file(STRINGS "${format_document}" format_top_sections REGEX "^## ")
list(LENGTH format_top_sections format_top_section_count)
if(NOT format_top_section_count EQUAL 4)
    message(FATAL_ERROR
        "Stream format must have exactly four top-level sections; found "
        "${format_top_section_count}")
endif()
set(previous_format_section_offset -1)
foreach(required_format_section IN ITEMS
        "## Stream framing and shared records"
        "## Dictionary representations and common vectors"
        "## Entropy representations and composed profiles"
        "## Experimental typed-token format 2.0")
    string(FIND "${format_content}" "${required_format_section}"
        format_section_offset)
    if(format_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing stream-format section: ${required_format_section}")
    endif()
    if(format_section_offset LESS_EQUAL previous_format_section_offset)
        message(FATAL_ERROR
            "Stream-format sections are out of order at: "
            "${required_format_section}")
    endif()
    set(previous_format_section_offset "${format_section_offset}")
endforeach()

foreach(experimental_design IN ITEMS
        "lzss-typed-token-protocol.md"
        "context-model-contract.md"
        "entropy-backend-contract.md")
    set(experimental_design_path
        "${source_dir}/docs/design/${experimental_design}")
    if(NOT EXISTS "${experimental_design_path}")
        message(FATAL_ERROR
            "Missing experimental design document: ${experimental_design}")
    endif()
endforeach()
foreach(required_typed_format_term IN ITEMS
        "dictionary algorithm ID 2, dictionary variant 2"
        "context-model algorithm ID 1, context variant 1"
        "entropy algorithm ID 3, entropy variant 2"
        "4D 52 46 32 40 00 00 00"
        "00 20 7F FF BF 00")
    string(FIND "${format_content}" "${required_typed_format_term}"
        typed_format_term_offset)
    if(typed_format_term_offset EQUAL -1)
        message(FATAL_ERROR
            "Incomplete typed format reservation: ${required_typed_format_term}")
    endif()
endforeach()

set(format_entropy_names
    "Blocked Huffman variant 1"
    "Adaptive Huffman FGK variant 1"
    "Dynamic Range Coder variant 1"
    "rANS variant 1"
    "tANS variant 1")
set(format_dictionary_names LZ77 LZSS LZ78 LZW LZD LZMW)
set(expected_format_profiles ${format_entropy_names})
foreach(dictionary_name IN LISTS format_dictionary_names)
    foreach(entropy_name IN LISTS format_entropy_names)
        list(APPEND expected_format_profiles
            "${dictionary_name} variant 1 plus ${entropy_name}")
    endforeach()
endforeach()
list(LENGTH expected_format_profiles expected_format_profile_count)
if(NOT expected_format_profile_count EQUAL 35)
    message(FATAL_ERROR "Internal format-profile expectation is incomplete")
endif()
set(previous_format_profile_offset -1)
foreach(format_profile IN LISTS expected_format_profiles)
    set(format_profile_heading "### ${format_profile}")
    string(FIND "${format_content}" "${format_profile_heading}"
        format_profile_offset)
    if(format_profile_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing stream-format profile: ${format_profile_heading}")
    endif()
    if(format_profile_offset LESS_EQUAL previous_format_profile_offset)
        message(FATAL_ERROR
            "Stream-format profiles are out of matrix order at: "
            "${format_profile_heading}")
    endif()
    set(previous_format_profile_offset "${format_profile_offset}")
endforeach()

set(readme_document "${source_dir}/README.md")
file(READ "${readme_document}" readme_content)
foreach(required_readme_status IN ITEMS
        "All forty-two profiles are exposed"
        "all forty-two benchmark-admitted")
    string(FIND "${readme_content}" "${required_readme_status}"
        readme_status_offset)
    if(readme_status_offset EQUAL -1)
        message(FATAL_ERROR
            "README current-profile status is missing: ${required_readme_status}")
    endif()
endforeach()
foreach(obsolete_readme_status IN ITEMS
        "All thirty-nine profiles"
        "all thirty-nine benchmark-admitted"
        "LZ77, LZSS, and LZ78 are composed with tANS")
    string(FIND "${readme_content}" "${obsolete_readme_status}"
        obsolete_readme_status_offset)
    if(NOT obsolete_readme_status_offset EQUAL -1)
        message(FATAL_ERROR
            "README retains obsolete profile status: ${obsolete_readme_status}")
    endif()
endforeach()

set(composition_document "${source_dir}/docs/composition.md")
file(READ "${composition_document}" composition_content)
set(previous_composition_section_offset -1)
foreach(required_composition_section IN ITEMS
        "## Current matrix"
        "## Why publication is not automatic"
        "## Deferred code-generation path"
        "## Profile admission history")
    string(FIND "${composition_content}" "${required_composition_section}"
        composition_section_offset)
    if(composition_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing composition section: ${required_composition_section}")
    endif()
    if(composition_section_offset LESS_EQUAL previous_composition_section_offset)
        message(FATAL_ERROR
            "Composition sections are out of order at: "
            "${required_composition_section}")
    endif()
    set(previous_composition_section_offset
        "${composition_section_offset}")
endforeach()
file(STRINGS "${composition_document}" composition_history_headings
    REGEX "^### CP-[0-9]+$")
if(NOT composition_history_headings)
    message(FATAL_ERROR "No composition admission records were found")
endif()
set(expected_composition_record 1)
foreach(heading IN LISTS composition_history_headings)
    if(NOT heading MATCHES "^### CP-([0-9]+)$")
        message(FATAL_ERROR "Invalid composition record heading: ${heading}")
    endif()
    set(composition_record_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" composition_record_number
        "${composition_record_number}")
    if(composition_record_number STREQUAL "")
        set(composition_record_number 0)
    endif()
    if(NOT composition_record_number EQUAL expected_composition_record)
        message(FATAL_ERROR
            "Composition records must be contiguous and ordered: expected "
            "CP-${expected_composition_record}, found ${heading}")
    endif()
    math(EXPR expected_composition_record
        "${expected_composition_record} + 1")
endforeach()
list(LENGTH composition_history_headings composition_history_count)

set(documentation_index "${source_dir}/docs/README.md")
file(READ "${documentation_index}" documentation_index_content)
foreach(required_index_description IN ITEMS
        "Composition matrix and admission history"
        "profile matrix is authoritative"
        "historical admission and CI evidence"
        "Numbered design decisions")
    string(FIND "${documentation_index_content}" "${required_index_description}"
        index_description_offset)
    if(index_description_offset EQUAL -1)
        message(FATAL_ERROR
            "Documentation index is stale: ${required_index_description}")
    endif()
endforeach()
set(implementation_index
    "${source_dir}/docs/implementation/README.md")
file(READ "${implementation_index}" implementation_index_content)
foreach(required_record_description IN ITEMS
        "contiguous decision-number order"
        "numbered Git action order"
        "numbered implementation-reference ledger"
        "numbered development ledger")
    string(FIND "${implementation_index_content}" "${required_record_description}"
        record_description_offset)
    if(record_description_offset EQUAL -1)
        message(FATAL_ERROR
            "Implementation-record index is stale: "
            "${required_record_description}")
    endif()
endforeach()

set(readiness_document "${source_dir}/docs/baseline-readiness.md")
file(READ "${readiness_document}" readiness_content)
set(previous_readiness_section_offset -1)
foreach(required_readiness_section IN ITEMS
        "## Local implementation matrix"
        "## Additional public profiles"
        "## Public-profile evidence matrix"
        "## Current validation baseline"
        "## Remaining release evidence"
        "## Readiness evidence history")
    string(FIND "${readiness_content}" "${required_readiness_section}"
        readiness_section_offset)
    if(readiness_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing readiness section: ${required_readiness_section}")
    endif()
    if(readiness_section_offset LESS_EQUAL previous_readiness_section_offset)
        message(FATAL_ERROR
            "Readiness sections are out of order at: "
            "${required_readiness_section}")
    endif()
    set(previous_readiness_section_offset "${readiness_section_offset}")
endforeach()
foreach(required_current_baseline IN ITEMS
        "All forty-two baseline profiles"
        "each enumerate 2,835 tests under"
        "four-direction schema-37 exchange")
    string(FIND "${readiness_content}" "${required_current_baseline}"
        current_baseline_offset)
    if(current_baseline_offset EQUAL -1)
        message(FATAL_ERROR
            "Readiness baseline is stale: ${required_current_baseline}")
    endif()
endforeach()
file(STRINGS "${readiness_document}" readiness_history_headings
    REGEX "^### BR-[0-9]+$")
if(NOT readiness_history_headings)
    message(FATAL_ERROR "No readiness evidence records were found")
endif()
set(expected_readiness_record 1)
foreach(heading IN LISTS readiness_history_headings)
    if(NOT heading MATCHES "^### BR-([0-9]+)$")
        message(FATAL_ERROR "Invalid readiness record heading: ${heading}")
    endif()
    set(readiness_record_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" readiness_record_number
        "${readiness_record_number}")
    if(readiness_record_number STREQUAL "")
        set(readiness_record_number 0)
    endif()
    if(NOT readiness_record_number EQUAL expected_readiness_record)
        message(FATAL_ERROR
            "Readiness records must be contiguous and ordered: expected "
            "BR-${expected_readiness_record}, found ${heading}")
    endif()
    math(EXPR expected_readiness_record "${expected_readiness_record} + 1")
endforeach()
list(LENGTH readiness_history_headings readiness_history_count)

set(interoperability_document "${source_dir}/docs/interoperability.md")
file(READ "${interoperability_document}" interoperability_content)
set(previous_interoperability_section_offset -1)
foreach(required_interoperability_section IN ITEMS
        "## Current bundle and verification"
        "## Schema compatibility"
        "## Integrity and current evidence"
        "## Work-product policy"
        "## Recorded external cross-checks")
    string(FIND "${interoperability_content}"
        "${required_interoperability_section}" interoperability_section_offset)
    if(interoperability_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing interoperability section: "
            "${required_interoperability_section}")
    endif()
    if(interoperability_section_offset LESS_EQUAL
       previous_interoperability_section_offset)
        message(FATAL_ERROR
            "Interoperability sections are out of order at: "
            "${required_interoperability_section}")
    endif()
    set(previous_interoperability_section_offset
        "${interoperability_section_offset}")
endforeach()
file(STRINGS "${interoperability_document}" interoperability_headings
    REGEX "^### IX-[0-9]+: Schema [0-9]+$")
if(NOT interoperability_headings)
    message(FATAL_ERROR "No interoperability evidence records were found")
endif()
set(expected_interoperability_record 1)
set(expected_interoperability_schema 7)
foreach(heading IN LISTS interoperability_headings)
    if(NOT heading MATCHES "^### IX-([0-9]+): Schema ([0-9]+)$")
        message(FATAL_ERROR "Invalid interoperability heading: ${heading}")
    endif()
    set(interoperability_record_number "${CMAKE_MATCH_1}")
    set(interoperability_schema_number "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "^0+" "" interoperability_record_number
        "${interoperability_record_number}")
    if(interoperability_record_number STREQUAL "")
        set(interoperability_record_number 0)
    endif()
    if(NOT interoperability_record_number EQUAL
       expected_interoperability_record OR
       NOT interoperability_schema_number EQUAL
       expected_interoperability_schema)
        message(FATAL_ERROR
            "Interoperability records must be contiguous from schema 7: "
            "${heading}")
    endif()
    math(EXPR expected_interoperability_record
        "${expected_interoperability_record} + 1")
    math(EXPR expected_interoperability_schema
        "${expected_interoperability_schema} + 1")
endforeach()
list(LENGTH interoperability_headings interoperability_record_count)

set(cli_document "${source_dir}/docs/cli.md")
file(READ "${cli_document}" cli_content)
set(previous_cli_section_offset -1)
foreach(required_cli_section IN ITEMS
        "## Usage"
        "## Profiles"
        "### Profile inventory"
        "### Common stream rules"
        "### LZ77 profile parameters"
        "### LZSS profile parameters"
        "### LZ78 profile parameters"
        "### LZW profile parameters"
        "### LZD profile parameters"
        "### LZMW profile parameters"
        "## File and error behavior")
    string(FIND "${cli_content}" "${required_cli_section}"
        cli_section_offset)
    if(cli_section_offset EQUAL -1)
        message(FATAL_ERROR "Missing CLI section: ${required_cli_section}")
    endif()
    if(cli_section_offset LESS_EQUAL previous_cli_section_offset)
        message(FATAL_ERROR
            "CLI sections are out of order at: ${required_cli_section}")
    endif()
    set(previous_cli_section_offset "${cli_section_offset}")
endforeach()
file(STRINGS "${cli_document}" cli_profile_rows
    REGEX "^\\| `[a-z0-9-]+` \\|")
list(LENGTH cli_profile_rows cli_profile_count)
if(NOT cli_profile_count EQUAL 42)
    message(FATAL_ERROR
        "CLI profile inventory must contain 42 profiles, found "
        "${cli_profile_count}")
endif()
foreach(required_experimental_cli IN ITEMS
        "`lzss-contextual-dynamic-range`"
        "`lzss-contextual-rans`"
        "`lzss-contextual-tans`"
        "`lzss-contextual-blocked-huffman`"
        "`lzss-contextual-adaptive-huffman`")
    string(FIND "${cli_content}" "${required_experimental_cli}"
        experimental_cli_offset)
    if(experimental_cli_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing experimental CLI selector: ${required_experimental_cli}")
    endif()
endforeach()

set(c_api_document "${source_dir}/docs/c-api.md")
file(READ "${c_api_document}" c_api_content)
set(previous_c_api_section_offset -1)
foreach(required_c_api_section IN ITEMS
        "## Profiles and composition"
        "## Lifecycle"
        "### Common lifecycle"
        "### LZ77 profiles"
        "### LZSS profiles"
        "### LZ78 profiles"
        "### LZW profiles"
        "### LZD profiles"
        "### LZMW profiles"
        "## Processing contract"
        "## Configuration rules")
    string(FIND "${c_api_content}" "${required_c_api_section}"
        c_api_section_offset)
    if(c_api_section_offset EQUAL -1)
        message(FATAL_ERROR "Missing C API section: ${required_c_api_section}")
    endif()
    if(c_api_section_offset LESS_EQUAL previous_c_api_section_offset)
        message(FATAL_ERROR
            "C API sections are out of order at: ${required_c_api_section}")
    endif()
    set(previous_c_api_section_offset "${c_api_section_offset}")
endforeach()
string(FIND "${c_api_content}" "same forty-two"
    c_api_profile_count_offset)
if(c_api_profile_count_offset EQUAL -1)
    message(FATAL_ERROR "C API profile count is stale")
endif()
string(FIND "${c_api_content}"
    "five experimental Format 2 LZSS contextual profiles"
    c_api_experimental_profile_offset)
if(c_api_experimental_profile_offset EQUAL -1)
    message(FATAL_ERROR "C API experimental profile inventory is stale")
endif()
string(FIND "${c_api_content}"
    "canonical lifecycle emits only variable-length entropy variant 3"
    c_api_contextual_rans_profile_offset)
if(c_api_contextual_rans_profile_offset EQUAL -1)
    message(FATAL_ERROR "C API contextual rANS inventory is stale")
endif()
string(FIND "${c_api_content}"
    "`marc_lzss_contextual_tans_workspace_requirements()`"
    c_api_contextual_tans_profile_offset)
if(c_api_contextual_tans_profile_offset EQUAL -1)
    message(FATAL_ERROR "C API contextual tANS inventory is stale")
endif()
string(FIND "${c_api_content}"
    "`marc_lzss_contextual_blocked_huffman_workspace_requirements()`"
    c_api_contextual_blocked_huffman_profile_offset)
if(c_api_contextual_blocked_huffman_profile_offset EQUAL -1)
    message(FATAL_ERROR
        "C API Contextual Blocked Huffman inventory is stale")
endif()
string(FIND "${c_api_content}"
    "`marc_lzss_contextual_adaptive_huffman_workspace_requirements()`"
    c_api_contextual_adaptive_huffman_profile_offset)
if(c_api_contextual_adaptive_huffman_profile_offset EQUAL -1)
    message(FATAL_ERROR
        "C API Contextual Adaptive Huffman inventory is stale")
endif()
foreach(prohibited_c_api_history IN ITEMS
        "completion matrix"
        "Interoperability schema"
        "CLI selector"
        "fuzz harness")
    string(FIND "${c_api_content}" "${prohibited_c_api_history}"
        prohibited_c_api_history_offset)
    if(NOT prohibited_c_api_history_offset EQUAL -1)
        message(FATAL_ERROR
            "Historical evidence is mixed into the C API reference: "
            "${prohibited_c_api_history}")
    endif()
endforeach()
file(STRINGS "${source_dir}/include/marc/marc.h" c_api_config_initializers
    REGEX "^MARC_API marc_status marc_.*_config_init\\(")
list(LENGTH c_api_config_initializers c_api_profile_count)
math(EXPR expected_c_api_profile_count "${cli_profile_count} + 5")
if(NOT c_api_profile_count EQUAL expected_c_api_profile_count)
    message(FATAL_ERROR
        "C API initializer count ${c_api_profile_count} must contain the "
        "${cli_profile_count} CLI profiles plus five experimental profiles")
endif()
list(FILTER c_api_config_initializers INCLUDE REGEX
    "marc_lzss_contextual_(dynamic_range|rans|tans|adaptive_huffman|blocked_huffman)_config_init")
list(LENGTH c_api_config_initializers c_api_experimental_profile_count)
if(NOT c_api_experimental_profile_count EQUAL 5)
    message(FATAL_ERROR
        "C API must contain exactly five contextual LZSS experimental "
        "initializers")
endif()

set(releasing_document "${source_dir}/docs/releasing.md")
file(READ "${releasing_document}" releasing_content)
set(previous_releasing_section_offset -1)
foreach(required_releasing_section IN ITEMS
        "## Version namespaces"
        "## Release scope"
        "## Pre-tag checklist"
        "## Tag and publication"
        "## Post-release")
    string(FIND "${releasing_content}" "${required_releasing_section}"
        releasing_section_offset)
    if(releasing_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing release-process section: ${required_releasing_section}")
    endif()
    if(releasing_section_offset LESS_EQUAL previous_releasing_section_offset)
        message(FATAL_ERROR
            "Release-process sections are out of order at: "
            "${required_releasing_section}")
    endif()
    set(previous_releasing_section_offset "${releasing_section_offset}")
endforeach()
foreach(required_release_instruction IN ITEMS
        "## X.Y.Z - YYYY-MM-DD"
        "git tag -a vX.Y.Z -m \"marc X.Y.Z\""
        "git rev-list -n 1 vX.Y.Z"
        "new top-level `## Unreleased` section")
    string(FIND "${releasing_content}" "${required_release_instruction}"
        release_instruction_offset)
    if(release_instruction_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing generic release instruction: "
            "${required_release_instruction}")
    endif()
endforeach()

file(READ "${source_dir}/CMakeLists.txt" root_cmake_content)
if(NOT root_cmake_content MATCHES
   "project\\(marc VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)")
    message(FATAL_ERROR "Could not read the marc project version")
endif()
set(project_release_version "${CMAKE_MATCH_1}")
file(READ "${source_dir}/CHANGELOG.md" changelog_content)
if(NOT changelog_content MATCHES
   "## ([0-9]+\\.[0-9]+\\.[0-9]+) - [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]")
    message(FATAL_ERROR "Could not read the latest changelog release")
endif()
set(changelog_release_version "${CMAKE_MATCH_1}")
if(NOT project_release_version VERSION_EQUAL changelog_release_version)
    message(FATAL_ERROR
        "CMake project version ${project_release_version} does not match "
        "latest changelog release ${changelog_release_version}")
endif()

set(benchmark_document "${source_dir}/docs/benchmarks.md")
file(READ "${benchmark_document}" benchmark_content)
set(previous_benchmark_section_offset -1)
foreach(required_benchmark_section IN ITEMS
        "## Running the benchmark"
        "## Measurement contract"
        "## Profile configurations"
        "### Framing baseline"
        "### Standalone entropy profiles"
        "### LZ77 profiles"
        "### LZSS profiles"
        "### LZ78 profiles"
        "### LZW profiles"
        "### LZD profiles"
        "### LZMW profiles"
        "## Recorded smoke measurements"
        "## Reporting results")
    string(FIND "${benchmark_content}" "${required_benchmark_section}"
        benchmark_section_offset)
    if(benchmark_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing benchmark section: ${required_benchmark_section}")
    endif()
    if(benchmark_section_offset LESS_EQUAL previous_benchmark_section_offset)
        message(FATAL_ERROR
            "Benchmark sections are out of order at: "
            "${required_benchmark_section}")
    endif()
    set(previous_benchmark_section_offset "${benchmark_section_offset}")
endforeach()
foreach(required_experimental_benchmark IN ITEMS
        "`lzss-contextual-dynamic-range`"
        "`lzss-contextual-rans`"
        "`lzss-contextual-tans`"
        "`lzss-contextual-blocked-huffman`"
        "`lzss-contextual-adaptive-huffman`")
    string(FIND "${benchmark_content}" "${required_experimental_benchmark}"
        experimental_benchmark_offset)
    if(experimental_benchmark_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing experimental benchmark profile: "
            "${required_experimental_benchmark}")
    endif()
endforeach()
file(STRINGS "${benchmark_document}" benchmark_commands
    REGEX "^marc_benchmark ")
list(LENGTH benchmark_commands benchmark_command_count)
if(NOT benchmark_command_count EQUAL 42)
    message(FATAL_ERROR
        "Benchmark command matrix must contain 42 profiles, found "
        "${benchmark_command_count}")
endif()
file(STRINGS "${benchmark_document}" benchmark_record_headings
    REGEX "^### BM-[0-9]+: .+$")
if(NOT benchmark_record_headings)
    message(FATAL_ERROR "No benchmark smoke records were found")
endif()
set(expected_benchmark_record 1)
foreach(heading IN LISTS benchmark_record_headings)
    if(NOT heading MATCHES "^### BM-([0-9]+): .+$")
        message(FATAL_ERROR "Invalid benchmark record heading: ${heading}")
    endif()
    set(benchmark_record_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" benchmark_record_number
        "${benchmark_record_number}")
    if(benchmark_record_number STREQUAL "")
        set(benchmark_record_number 0)
    endif()
    if(NOT benchmark_record_number EQUAL expected_benchmark_record)
        message(FATAL_ERROR
            "Benchmark records must be contiguous and ordered: expected "
            "BM-${expected_benchmark_record}, found ${heading}")
    endif()
    math(EXPR expected_benchmark_record "${expected_benchmark_record} + 1")
endforeach()
list(LENGTH benchmark_record_headings benchmark_record_count)

set(fuzzing_document "${source_dir}/docs/fuzzing.md")
file(READ "${fuzzing_document}" fuzzing_content)
set(previous_fuzzing_section_offset -1)
foreach(required_fuzzing_section IN ITEMS
        "## Target coverage and fixed bounds"
        "## Build and execution workflow"
        "## Recorded bounded campaigns"
        "## Finding retention policy")
    string(FIND "${fuzzing_content}" "${required_fuzzing_section}"
        fuzzing_section_offset)
    if(fuzzing_section_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing fuzzing section: ${required_fuzzing_section}")
    endif()
    if(fuzzing_section_offset LESS_EQUAL previous_fuzzing_section_offset)
        message(FATAL_ERROR
            "Fuzzing sections are out of order at: "
            "${required_fuzzing_section}")
    endif()
    set(previous_fuzzing_section_offset "${fuzzing_section_offset}")
endforeach()
string(FIND "${fuzzing_content}" "The forty-two bounded targets"
    fuzzing_target_count_offset)
if(fuzzing_target_count_offset EQUAL -1)
    message(FATAL_ERROR "Fuzzing target count is stale")
endif()
file(STRINGS "${fuzzing_document}" fuzzing_campaign_headings
    REGEX "^### FZ-[0-9]+: .+$")
if(NOT fuzzing_campaign_headings)
    message(FATAL_ERROR "No fuzzing campaign records were found")
endif()
set(expected_fuzzing_campaign 1)
foreach(heading IN LISTS fuzzing_campaign_headings)
    if(NOT heading MATCHES "^### FZ-([0-9]+): .+$")
        message(FATAL_ERROR "Invalid fuzzing campaign heading: ${heading}")
    endif()
    set(fuzzing_campaign_number "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "^0+" "" fuzzing_campaign_number
        "${fuzzing_campaign_number}")
    if(fuzzing_campaign_number STREQUAL "")
        set(fuzzing_campaign_number 0)
    endif()
    if(NOT fuzzing_campaign_number EQUAL expected_fuzzing_campaign)
        message(FATAL_ERROR
            "Fuzzing campaign records must be contiguous and ordered: "
            "expected FZ-${expected_fuzzing_campaign}, found ${heading}")
    endif()
    math(EXPR expected_fuzzing_campaign
        "${expected_fuzzing_campaign} + 1")
endforeach()
list(LENGTH fuzzing_campaign_headings fuzzing_campaign_count)

file(GLOB_RECURSE documentation_files "${source_dir}/docs/*.md")
list(APPEND documentation_files
    "${source_dir}/README.md"
    "${source_dir}/CHANGELOG.md"
    "${source_dir}/CONTRIBUTING.md"
    "${source_dir}/THIRD_PARTY_NOTICES.md")
list(SORT documentation_files)

set(relative_link_count 0)
foreach(document IN LISTS documentation_files)
    file(READ "${document}" content)
    # Rewrite linked images into two ordinary links before scanning.  A direct
    # MATCHALL over Markdown links otherwise starts at the outer '[' and joins
    # the badge image target to later links because the image contributes a
    # nested ']'.
    string(REGEX REPLACE
        "\\[!\\[([^]]*)\\]\\(([^)]+)\\)\\]\\(([^)]+)\\)"
        "[\\1 image](\\2)[\\1 target](\\3)"
        link_scan_content "${content}")
    string(REGEX MATCHALL "!?\\[[^]]*\\]\\([^)]+\\)" links
        "${link_scan_content}")
    get_filename_component(document_directory "${document}" DIRECTORY)

    foreach(link IN LISTS links)
        string(REGEX REPLACE
            "^!?\\[[^]]*\\]\\(([^)]+)\\)$" "\\1" target "${link}")
        string(STRIP "${target}" target)
        if(target MATCHES "^(https?:|mailto:|#)")
            continue()
        endif()
        if(target MATCHES "^<(.*)>$")
            set(target "${CMAKE_MATCH_1}")
        endif()
        string(REGEX REPLACE "#.*$" "" target_path "${target}")
        if(target_path STREQUAL "")
            continue()
        endif()

        get_filename_component(
            resolved_target "${document_directory}/${target_path}" ABSOLUTE)
        if(NOT EXISTS "${resolved_target}")
            file(RELATIVE_PATH document_name "${source_dir}" "${document}")
            message(FATAL_ERROR
                "Broken relative link in ${document_name}: ${target}")
        endif()
        math(EXPR relative_link_count "${relative_link_count} + 1")
    endforeach()
endforeach()

list(LENGTH documentation_files document_count)
message(STATUS
    "Verified ${relative_link_count} relative links in ${document_count} "
    "documents, ${decision_count} ordered design decisions, and "
    "${clean_room_record_count} chronological clean-room records, and "
    "${implementation_reference_count} ordered implementation references, and "
    "${test_vector_record_count} ordered test-vector records, and "
    "${composition_history_count} ordered composition records, and "
    "${readiness_history_count} ordered readiness records, and "
    "${interoperability_record_count} interoperability records, and "
    "${benchmark_record_count} benchmark records, and "
    "${fuzzing_campaign_count} fuzzing campaign records")
