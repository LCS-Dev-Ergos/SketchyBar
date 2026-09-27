cmake_minimum_required(VERSION 3.25)

# Builds the same universal binary as the makefile, from the same sources and
# frameworks. The makefile remains the upstream build; this file adds
# out-of-tree builds, sanitizers, tests and a compile_commands.json that
# language servers can use.

# Must precede project() so the compiler checks already use it. The makefile
# targets macOS 10.13 for x86_64 and 11 for arm64; one target serves both here.
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "Minimum macOS version")

project(sketchybar LANGUAGES C OBJC)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# The makefile keeps assertions in its optimised build.
foreach(lang C OBJC)
    string(REPLACE "-DNDEBUG" "" CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE}")
endforeach()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SKETCHYBAR_ARCHITECTURES "x86_64;arm64" CACHE STRING "Architectures of the sketchybar binary")
set(SKETCHYBAR_SANITIZE "" CACHE STRING "Sanitizers for sketchybar and its tests: empty, address or thread")
set(SKETCHYBAR_CODESIGN_IDENTITY "" CACHE STRING "Code signing identity applied to sketchybar after linking")
option(SKETCHYBAR_LINK_COMPILE_COMMANDS "Point compile_commands.json in the source tree at this build directory" ON)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PRIVATE_FRAMEWORKS /System/Library/PrivateFrameworks)

# The objects of the makefile, in the same order.
set(SKETCHYBAR_SOURCES
    alias.c background.c bar_item.c custom_events.c event.c graph.c image.c
    mouse.c shadow.c font.c text.c message.c bar.c color.c window.c
    bar_manager.c display.c display_nsscreen.m group.c mach.c popup.c
    animation.c workspace.m volume.c slider.c power.c wifi.m media.m
    hotload.c app_windows.c layer.m surface.c context.c)
list(TRANSFORM SKETCHYBAR_SOURCES PREPEND ${SRC}/)

set(SKETCHYBAR_FRAMEWORKS
    "-framework Carbon" "-framework AppKit" "-framework QuartzCore"
    "-framework CoreAudio" "-framework CoreWLAN" "-framework CoreVideo"
    "-framework IOKit" "-framework SkyLight" "-framework DisplayServices"
    "-framework MediaRemote")

if(SKETCHYBAR_SANITIZE STREQUAL "address")
    set(SKETCHYBAR_SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
elseif(SKETCHYBAR_SANITIZE STREQUAL "thread")
    set(SKETCHYBAR_SANITIZE_FLAGS -fsanitize=thread,undefined -fno-omit-frame-pointer)
elseif(NOT SKETCHYBAR_SANITIZE STREQUAL "")
    message(FATAL_ERROR "SKETCHYBAR_SANITIZE must be empty, address or thread")
endif()

add_executable(sketchybar ${SRC}/sketchybar.c ${SKETCHYBAR_SOURCES})
set_target_properties(sketchybar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OSX_ARCHITECTURES "${SKETCHYBAR_ARCHITECTURES}"
    C_STANDARD 99 C_EXTENSIONS OFF
    OBJC_STANDARD 99 OBJC_EXTENSIONS OFF)
target_compile_options(sketchybar PRIVATE
    -Wall -fvisibility=hidden -fno-common
    $<$<CONFIG:Release>:-O3 -ffast-math>
    ${SKETCHYBAR_SANITIZE_FLAGS})
target_link_options(sketchybar PRIVATE -F${PRIVATE_FRAMEWORKS} ${SKETCHYBAR_SANITIZE_FLAGS})
target_link_libraries(sketchybar PRIVATE ${SKETCHYBAR_FRAMEWORKS})

if(SKETCHYBAR_CODESIGN_IDENTITY)
    add_custom_command(TARGET sketchybar POST_BUILD
        COMMAND codesign -fs ${SKETCHYBAR_CODESIGN_IDENTITY} $<TARGET_FILE:sketchybar>
        VERBATIM)
endif()

# Unit tests of the fork, see docs/testing.md. They exercise code without
# opening windows or talking to the running bar.
enable_testing()

# The client prints the version before it contacts a daemon or the window server.
add_test(NAME version COMMAND sketchybar --version)
set_tests_properties(version PROPERTIES PASS_REGULAR_EXPRESSION "^sketchybar-v[0-9]+\\.[0-9]+\\.[0-9]+")

# libFuzzer targets for the message protocol, see docs/testing.md. They need a
# clang that ships libFuzzer, which Apple's does not. Each also runs as a short
# CTest test, writing new inputs to the build directory.
option(SKETCHYBAR_FUZZ "Build the libFuzzer targets and run them as tests" OFF)
set(SKETCHYBAR_FUZZ_SECONDS 30 CACHE STRING "Seconds each fuzz target runs as a test")

if(SKETCHYBAR_FUZZ)
    set(SKETCHYBAR_FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined)

    foreach(target tokens)
        add_executable(fuzz_${target} tests/fuzz/${target}.c)
        target_include_directories(fuzz_${target} PRIVATE ${SRC})
        set_target_properties(fuzz_${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
            C_STANDARD 99 C_EXTENSIONS OFF)
        target_compile_options(fuzz_${target} PRIVATE -g -UNDEBUG ${SKETCHYBAR_FUZZ_FLAGS})
        target_link_options(fuzz_${target} PRIVATE ${SKETCHYBAR_FUZZ_FLAGS})

        set(corpus ${CMAKE_BINARY_DIR}/fuzz/corpus/${target})
        file(MAKE_DIRECTORY ${corpus})
        add_test(NAME fuzz_${target}
            COMMAND fuzz_${target} -max_total_time=${SKETCHYBAR_FUZZ_SECONDS} -timeout=10 -print_final_stats=1
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz/
                ${corpus} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus/${target})
    endforeach()
endif()

# Editors that only look for compile_commands.json in the workspace root follow
# the most recently configured build directory.
if(SKETCHYBAR_LINK_COMPILE_COMMANDS AND NOT CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json SYMBOLIC)
endif()
