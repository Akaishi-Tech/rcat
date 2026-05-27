cmake_minimum_required(VERSION 3.16)

# ----------------------------------------------------------------------------
# Project metadata. Version is overridden at configure time by `git describe`
# when this is a git checkout (see below) — the literal here is the floor.
# ----------------------------------------------------------------------------
project(rcat
    VERSION 1.0.0
    DESCRIPTION "Render Markdown in the terminal"
    HOMEPAGE_URL "https://github.com/Akaishi-Tech/rcat"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Refuse in-source builds — they clobber CMakeLists.txt with generated files.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Run e.g. "
        "`cmake -S . -B build` from the project root.")
endif()

include(GNUInstallDirs)
include(CMakeDependentOption)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------
option(RCAT_BUILD_TESTS       "Build rcat tests"                            ON)
option(RCAT_INSTALL_MAN       "Install the man page"                        ON)
option(RCAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"          OFF)
option(RCAT_ENABLE_ASAN       "Build with AddressSanitizer + UBSan"         OFF)
option(RCAT_ENABLE_TSAN       "Build with ThreadSanitizer"                  OFF)
option(RCAT_ENABLE_COVERAGE   "Build with gcov instrumentation"             OFF)

if(RCAT_ENABLE_ASAN AND RCAT_ENABLE_TSAN)
    message(FATAL_ERROR "RCAT_ENABLE_ASAN and RCAT_ENABLE_TSAN are mutually exclusive.")
endif()

# ----------------------------------------------------------------------------
# Version: prefer `git describe` so dev builds carry a sha.
# ----------------------------------------------------------------------------
set(RCAT_VERSION ${PROJECT_VERSION})
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always --match "v*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE RCAT_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_describe_rc)
    if(_git_describe_rc EQUAL 0 AND RCAT_GIT_DESCRIBE)
        set(RCAT_VERSION "${RCAT_GIT_DESCRIBE}")
    endif()
endif()
message(STATUS "rcat: version ${RCAT_VERSION}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/rcat_version.hpp
    @ONLY)

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(MD4C REQUIRED IMPORTED_TARGET md4c)
find_package(argparse REQUIRED)

# ----------------------------------------------------------------------------
# Compile flags: collected on an INTERFACE target so every binary picks
# them up consistently. Sanitizer/coverage choices propagate to link too.
# ----------------------------------------------------------------------------
add_library(rcat_warnings INTERFACE)
target_compile_options(rcat_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
        -Wdouble-promotion -Wformat=2
        -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->)
if(RCAT_WARNINGS_AS_ERRORS)
    target_compile_options(rcat_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

add_library(rcat_sanitizers INTERFACE)
if(RCAT_ENABLE_ASAN)
    target_compile_options(rcat_sanitizers INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(rcat_sanitizers INTERFACE
        -fsanitize=address,undefined)
endif()
if(RCAT_ENABLE_TSAN)
    target_compile_options(rcat_sanitizers INTERFACE
        -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(rcat_sanitizers INTERFACE -fsanitize=thread)
endif()
if(RCAT_ENABLE_COVERAGE)
    target_compile_options(rcat_sanitizers INTERFACE
        --coverage -O0 -g)
    target_link_options(rcat_sanitizers INTERFACE --coverage)
endif()

# ----------------------------------------------------------------------------
# Core static library
# ----------------------------------------------------------------------------
add_library(rcat_core STATIC
    src/terminal.cpp
    src/wrap.cpp
    src/renderer.cpp
    src/i18n.cpp
    src/highlight.cpp
    src/pager.cpp
    src/lang/lang_common.cpp
    src/lang/lang_c_family.cpp
    src/lang/lang_python.cpp
    src/lang/lang_ruby.cpp
    src/lang/lang_perl.cpp
    src/lang/lang_r_julia.cpp
    src/lang/lang_shell.cpp
    src/lang/lang_powershell_tcl.cpp
    src/lang/lang_json.cpp
    src/lang/lang_yaml.cpp
    src/lang/lang_toml.cpp
    src/lang/lang_ini.cpp
    src/lang/lang_markup.cpp
    src/lang/lang_css.cpp
    src/lang/lang_sql.cpp
    src/lang/lang_lua.cpp
    src/lang/lang_haskell.cpp
    src/lang/lang_lisp.cpp
    src/lang/lang_ocaml.cpp
    src/lang/lang_erlang.cpp
    src/lang/lang_build.cpp
    src/lang/lang_misc.cpp)
add_library(rcat::core ALIAS rcat_core)

target_include_directories(rcat_core
    PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(rcat_core
    PUBLIC PkgConfig::MD4C
    PRIVATE rcat_warnings rcat_sanitizers)

# ----------------------------------------------------------------------------
# Internationalization: glibc has gettext in libc; some platforms (musl,
# macOS) need a separate libintl. find_package(Intl) handles both.
# ----------------------------------------------------------------------------
find_package(Intl REQUIRED)
target_compile_definitions(rcat_core PUBLIC
    RCAT_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
if(Intl_LIBRARIES)
    target_link_libraries(rcat_core PUBLIC ${Intl_LIBRARIES})
endif()
if(Intl_INCLUDE_DIRS)
    target_include_directories(rcat_core PUBLIC ${Intl_INCLUDE_DIRS})
endif()
message(STATUS "rcat: i18n enabled (locale catalogs install to "
               "${CMAKE_INSTALL_FULL_LOCALEDIR})")

# Compile .po -> .mo into build/locale/<lang>/LC_MESSAGES/rcat.mo and install
# them. Also write the same tree under build/locale so the binary can pick
# them up via RCAT_LOCALEDIR env when running uninstalled.
find_package(Gettext REQUIRED)
set(RCAT_LANGUAGES zh_CN es ja de)
set(RCAT_MO_FILES)
foreach(lang IN LISTS RCAT_LANGUAGES)
    set(po_file ${CMAKE_CURRENT_SOURCE_DIR}/po/${lang}.po)
    set(mo_dir  ${CMAKE_CURRENT_BINARY_DIR}/locale/${lang}/LC_MESSAGES)
    set(mo_file ${mo_dir}/rcat.mo)
    add_custom_command(
        OUTPUT ${mo_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check-format
                -o ${mo_file} ${po_file}
        DEPENDS ${po_file}
        COMMENT "msgfmt ${lang}.po -> rcat.mo"
        VERBATIM)
    list(APPEND RCAT_MO_FILES ${mo_file})
    install(FILES ${mo_file}
            DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES)
endforeach()
add_custom_target(rcat_translations ALL DEPENDS ${RCAT_MO_FILES})

# ----------------------------------------------------------------------------
# Inline image rendering (chafa + stb_image) and remote image fetch (libcurl).
# ----------------------------------------------------------------------------
pkg_check_modules(CHAFA REQUIRED IMPORTED_TARGET chafa)
find_path(STB_IMAGE_INCLUDE_DIR
    NAMES stb_image.h
    PATH_SUFFIXES stb
    REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

target_sources(rcat_core PRIVATE src/image.cpp)
# stb_image.h is third-party — silence its warnings by marking the
# include as SYSTEM, so -Werror builds aren't held hostage by it.
target_include_directories(rcat_core SYSTEM PRIVATE ${STB_IMAGE_INCLUDE_DIR})
target_link_libraries(rcat_core PUBLIC PkgConfig::CHAFA PkgConfig::CURL)
message(STATUS "rcat: image rendering enabled "
               "(chafa ${CHAFA_VERSION}, stb at ${STB_IMAGE_INCLUDE_DIR})")
message(STATUS "rcat: remote image fetch enabled "
               "(libcurl ${CURL_VERSION})")

# ----------------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------------
add_executable(rcat src/rcat.cpp)
add_executable(rcat::rcat ALIAS rcat)
target_link_libraries(rcat
    PRIVATE rcat_core argparse::argparse rcat_warnings rcat_sanitizers)

# rless: paging UI built on top of the same rcat_core renderer/highlighter.
add_executable(rless src/rless.cpp)
add_executable(rcat::rless ALIAS rless)
target_link_libraries(rless
    PRIVATE rcat_core argparse::argparse rcat_warnings rcat_sanitizers)

install(TARGETS rcat rless
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime)

install(FILES README.md LICENSE CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT Documentation)

# ----------------------------------------------------------------------------
# Man pages (build-time configured so we can substitute paths)
# ----------------------------------------------------------------------------
if(RCAT_INSTALL_MAN)
    foreach(_bin rcat rless)
        set(_man_in  ${CMAKE_CURRENT_SOURCE_DIR}/docs/${_bin}.1.in)
        set(_man_out ${CMAKE_CURRENT_BINARY_DIR}/${_bin}.1)
        if(EXISTS ${_man_in})
            configure_file(${_man_in} ${_man_out} @ONLY)
            install(FILES ${_man_out}
                DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
                COMPONENT Documentation)
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------
if(RCAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ----------------------------------------------------------------------------
# Packaging via CPack — `cmake --build build --target package` produces
# .tar.gz, .deb (when dpkg-deb is available), and .rpm (when rpmbuild is).
# ----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME                ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION             ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR              "Akaishi Tech")
set(CPACK_PACKAGE_HOMEPAGE_URL        "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CONTACT             "mahoshojoHCG <hcgstd@proton.me>")
set(CPACK_RESOURCE_FILE_LICENSE       "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README        "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "${PROJECT_NAME}")
set(CPACK_GENERATOR "TGZ")
if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB" "RPM")
endif()
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/build.*/" "/\\.git/" "/\\.cache/" "/\\.idea/" "/\\.vscode/"
    "\\.gcda$" "\\.gcno$" "\\.o$" "\\.a$")

set(CPACK_DEBIAN_PACKAGE_SECTION         "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY        "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS       ON)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE
    "${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP   "Applications/Text")
set(CPACK_RPM_PACKAGE_URL     "${PROJECT_HOMEPAGE_URL}")

include(CPack)
