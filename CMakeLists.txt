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
    hotload.c app_windows.c layer.m surface.c context.c script.c
    display_layout.c display_reconcile.c)
list(TRANSFORM SKETCHYBAR_SOURCES PREPEND ${SRC}/)

set(SKETCHYBAR_FRAMEWORKS
    "-framework Carbon" "-framework AppKit" "-framework QuartzCore"
    "-framework CoreAudio" "-framework CoreWLAN" "-framework CoreVideo"
    "-framework IOKit" "-framework SkyLight" "-framework DisplayServices"
    "-framework MediaRemote")

if(SKETCHYBAR_SANITIZE STREQUAL "address")
    set(SKETCHYBAR_SANITIZE_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
elseif(SKETCHYBAR_SANITIZE STREQUAL "thread")
    set(SKETCHYBAR_SANITIZE_FLAGS -fsanitize=thread,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
elseif(NOT SKETCHYBAR_SANITIZE STREQUAL "")
    message(FATAL_ERROR "SKETCHYBAR_SANITIZE must be empty, address or thread")
endif()

# Everything but main, which the message tests link against as well.
add_library(sketchybar_objects OBJECT ${SKETCHYBAR_SOURCES})
add_executable(sketchybar ${SRC}/sketchybar.c)

foreach(target sketchybar_objects sketchybar)
    set_target_properties(${target} PROPERTIES
        OSX_ARCHITECTURES "${SKETCHYBAR_ARCHITECTURES}"
        C_STANDARD 99 C_EXTENSIONS OFF
        OBJC_STANDARD 99 OBJC_EXTENSIONS OFF)
    target_compile_options(${target} PRIVATE
        -Wall -fvisibility=hidden -fno-common
        $<$<CONFIG:Release>:-O3 -ffast-math>
        ${SKETCHYBAR_SANITIZE_FLAGS})
endforeach()

set_target_properties(sketchybar PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
target_link_options(sketchybar PRIVATE -F${PRIVATE_FRAMEWORKS} ${SKETCHYBAR_SANITIZE_FLAGS})
target_link_libraries(sketchybar PRIVATE sketchybar_objects ${SKETCHYBAR_FRAMEWORKS})

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

# Adds a test executable built from the given sources, with the sources
# directory on the include path and the sanitizers of the build.
function(sketchybar_test name)
    add_executable(${name} ${ARGN})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
        C_STANDARD 99 C_EXTENSIONS OFF)
    target_include_directories(${name} PRIVATE ${SRC})
    target_compile_options(${name} PRIVATE -UNDEBUG ${SKETCHYBAR_SANITIZE_FLAGS})
    target_link_options(${name} PRIVATE ${SKETCHYBAR_SANITIZE_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sketchybar_test(display_layout_tests tests/display/layout.c)
target_link_libraries(display_layout_tests PRIVATE "-framework CoreGraphics")

# The reconciliation runs against stubs of the bar manager and simulated
# displays, with a 50 ms quiet period and a 2 s settle period.
sketchybar_test(display_reconcile_tests tests/display/reconcile.c ${SRC}/display_reconcile.c)
target_compile_definitions(display_reconcile_tests PRIVATE
    "DISPLAY_RECONCILE_QUIET_NS=(50 * NSEC_PER_MSEC)"
    "DISPLAY_WAKE_SETTLE_NS=(2 * NSEC_PER_SEC)")
target_link_libraries(display_reconcile_tests PRIVATE "-framework CoreFoundation" "-framework CoreGraphics")

sketchybar_test(bar_level_tests tests/bar/level.c)

# The Mach receive callback runs on a private port instead of the bootstrap
# port, with a recording handler.
sketchybar_test(mach_server_tests tests/mach/server.c ${SRC}/mach.c)
target_link_libraries(mach_server_tests PRIVATE "-framework CoreFoundation")

# Event sends to a full queue time out; without the timeout the test hangs
# until CTest stops it.
sketchybar_test(mach_send_tests tests/mach/send.c ${SRC}/mach.c)
target_link_libraries(mach_send_tests PRIVATE "-framework CoreFoundation")
set_tests_properties(mach_send_tests PROPERTIES TIMEOUT 20)

# Scripts run in a temporary directory, with a one-second alarm.
sketchybar_test(script_tests tests/script/script.c ${SRC}/script.c)
target_compile_definitions(script_tests PRIVATE FORK_TIMEOUT=1)

# The message handlers of the daemon, without bars and hence without windows
# (tests/message/harness.h).
function(sketchybar_message_test name source)
    sketchybar_test(${name} ${source} tests/message/harness.c)
    set_target_properties(${name} PROPERTIES OSX_ARCHITECTURES "${SKETCHYBAR_ARCHITECTURES}")
    target_link_options(${name} PRIVATE -F${PRIVATE_FRAMEWORKS})
    target_link_libraries(${name} PRIVATE sketchybar_objects ${SKETCHYBAR_FRAMEWORKS})
endfunction()

sketchybar_message_test(message_animation_tests tests/message/animation.c)
sketchybar_message_test(message_input_tests tests/message/inputs.c)
sketchybar_message_test(message_windowserver_tests tests/message/windowserver.c)
sketchybar_message_test(message_memory_tests tests/message/memory.c)

# libFuzzer targets for the message protocol, see docs/testing.md. They need a
# clang that ships libFuzzer, which Apple's does not. Each also runs as a short
# CTest test, writing new inputs to the build directory.
option(SKETCHYBAR_FUZZ "Build the libFuzzer targets and run them as tests" OFF)
set(SKETCHYBAR_FUZZ_SECONDS 30 CACHE STRING "Seconds each fuzz target runs as a test")

if(SKETCHYBAR_FUZZ)
    set(SKETCHYBAR_FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined)

    foreach(target tokens message)
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

# Tools that inspect the running bar, see docs/testing.md. They are never run
# as tests.
option(SKETCHYBAR_BUILD_TOOLS "Build the tools that inspect the running bar" ON)

if(SKETCHYBAR_BUILD_TOOLS)
    add_executable(window_order tools/window_order.m)
    set_target_properties(window_order PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
    target_compile_options(window_order PRIVATE -Wall -fobjc-arc)
    target_link_libraries(window_order PRIVATE "-framework Foundation" "-framework CoreGraphics")
endif()

# Editors that only look for compile_commands.json in the workspace root follow
# the most recently configured build directory.
if(SKETCHYBAR_LINK_COMPILE_COMMANDS AND NOT CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json SYMBOLIC)
endif()
