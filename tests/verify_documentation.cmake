cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(REAL_PATH "${SOURCE_DIR}" source_dir)
set(required_documents
    README.md
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

file(GLOB_RECURSE documentation_files "${source_dir}/docs/*.md")
list(APPEND documentation_files
    "${source_dir}/README.md"
    "${source_dir}/CONTRIBUTING.md"
    "${source_dir}/THIRD_PARTY_NOTICES.md")
list(SORT documentation_files)

set(relative_link_count 0)
foreach(document IN LISTS documentation_files)
    file(READ "${document}" content)
    string(REGEX MATCHALL "!?\\[[^]]*\\]\\([^)]+\\)" links "${content}")
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
    "Verified ${relative_link_count} relative links in ${document_count} documents")
