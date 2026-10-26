# Small registration helpers. Profile configuration and differing flags stay
# explicit at each call site; these helpers add only identical common setup.
function(marc_add_cpp_executable target)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
endfunction()

function(marc_add_fuzzer target)
    marc_add_cpp_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(${target} PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(${target} PRIVATE marc::static)
endfunction()
