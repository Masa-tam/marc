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
        "## Composed profile boundaries")
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
if(NOT format_top_section_count EQUAL 3)
    message(FATAL_ERROR
        "Stream format must have exactly three top-level sections; found "
        "${format_top_section_count}")
endif()
set(previous_format_section_offset -1)
foreach(required_format_section IN ITEMS
        "## Stream framing and shared records"
        "## Dictionary representations and common vectors"
        "## Entropy representations and composed profiles")
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
    "${composition_history_count} ordered composition records")
