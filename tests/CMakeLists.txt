add_executable(test_wrap test_wrap.cpp)
target_link_libraries(test_wrap PRIVATE rcat_core rcat_warnings rcat_sanitizers)
add_test(NAME wrap COMMAND test_wrap)

add_executable(test_render test_render.cpp)
target_link_libraries(test_render PRIVATE rcat_core rcat_warnings rcat_sanitizers)
add_test(NAME render COMMAND test_render)

add_executable(test_highlight test_highlight.cpp)
target_link_libraries(test_highlight PRIVATE rcat_core rcat_warnings rcat_sanitizers)
add_test(NAME highlight COMMAND test_highlight)

add_executable(test_pager test_pager.cpp)
target_link_libraries(test_pager PRIVATE rcat_core rcat_warnings rcat_sanitizers)
add_test(NAME pager COMMAND test_pager)

add_test(
    NAME cli_plain
    COMMAND rcat --plain --columns 60 ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.md
)
set_tests_properties(cli_plain PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, rcat"
)

# CLI: render a source file end-to-end with color forced; output must
# carry SGR sequences (we look for "38;5;" — the 256-color foreground
# prefix the highlighter emits for any keyword token).
add_test(
    NAME cli_highlight_python
    COMMAND rcat --force-color --color=256
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.py
)
set_tests_properties(cli_highlight_python PROPERTIES
    PASS_REGULAR_EXPRESSION "38;5;"
)
add_test(
    NAME cli_highlight_json
    COMMAND rcat --force-color --color=256
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.json
)
set_tests_properties(cli_highlight_json PROPERTIES
    PASS_REGULAR_EXPRESSION "38;5;"
)

add_test(
    NAME cli_version
    COMMAND rcat --version
)
set_tests_properties(cli_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^rcat "
)

add_test(
    NAME cli_lang_list
    COMMAND rcat --lang list
)
set_tests_properties(cli_lang_list PROPERTIES
    PASS_REGULAR_EXPRESSION "(^|\n)python(\n|$)"
)

# ----------------------------------------------------------------------------
# rless CLI smoke tests.
# rless detects whether stdout is a TTY; ctest drives commands with a pipe,
# so stdout is non-TTY and rless behaves like rcat (writes rendered output
# and exits). That keeps the pager loop out of the test suite while still
# exercising argument parsing, file reading, and the render path.
# ----------------------------------------------------------------------------
add_test(
    NAME rless_version
    COMMAND rless --version
)
set_tests_properties(rless_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^rless "
)

add_test(
    NAME rless_plain
    COMMAND rless --plain --columns 60
                  ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.md
)
set_tests_properties(rless_plain PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, rcat"
)

add_test(
    NAME rless_no_pager
    COMMAND rless --no-pager --plain --columns 60
                  ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.md
)
set_tests_properties(rless_no_pager PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, rcat"
)

add_test(
    NAME rless_highlight_python
    COMMAND rless --no-pager --force-color --color=256
                  ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.py
)
set_tests_properties(rless_highlight_python PROPERTIES
    PASS_REGULAR_EXPRESSION "38;5;"
)

if(RCAT_HAVE_CHAFA)
    add_executable(test_image test_image.cpp)
    target_link_libraries(test_image
        PRIVATE rcat_core rcat_warnings rcat_sanitizers)
    target_compile_definitions(test_image PRIVATE
        RCAT_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    add_test(NAME image COMMAND test_image)

    # End-to-end: feeds sample.md through the CLI with color forced; the
    # rendered local PNG must produce ANSI background-color sequences.
    add_test(
        NAME cli_image
        COMMAND rcat --force-color --color=truecolor --columns 60
                      --image-height 8
                      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample.md
    )
    set_tests_properties(cli_image PROPERTIES
        PASS_REGULAR_EXPRESSION "48;2;"
    )
endif()

# i18n: only meaningful for languages whose locale is generated on the
# build host. We probe with `locale -a` and skip silently otherwise so
# the suite stays portable.
execute_process(
    COMMAND locale -a
    OUTPUT_VARIABLE RCAT_HOST_LOCALES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(RCAT_HOST_LOCALES MATCHES "zh_CN(\\.utf8|\\.UTF-8)")
    add_test(
        NAME i18n_zh_CN
        COMMAND ${CMAKE_COMMAND} -E env
                LANG=zh_CN.UTF-8
                LC_ALL=zh_CN.UTF-8
                RCAT_LOCALEDIR=${CMAKE_BINARY_DIR}/locale
                $<TARGET_FILE:rcat> --help
    )
    set_tests_properties(i18n_zh_CN PROPERTIES
        PASS_REGULAR_EXPRESSION "在终端中渲染 Markdown"
    )
endif()
