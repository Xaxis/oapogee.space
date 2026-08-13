# oApogee firmware build.
#
# Host mode is the only mode that builds today. It compiles core/ and its tests
# with nothing installed but CMake and a C compiler: no SDK, no toolchain
# download, no board. That is the point of the core and port split, and it is
# why the flight state machine, the fusion filter, the packet builders and the
# log packers can be tested at all right now, when no hardware exists.
#
# Target mode is declared below and deliberately refuses to configure. It needs
# the Pico SDK, which this repository does not vendor and this build does not
# fetch.
#
# NOTHING BUILT HERE HAS RUN ON HARDWARE. No board has been fabricated, no
# firmware image has been flashed, and no packet has been transmitted.
#
# Usage:
#   cmake -S firmware -B firmware/build
#   cmake --build firmware/build
#   ctest --test-dir firmware/build            # once tests exist
#   cmake --build firmware/build --target oa-check
#
# oa-check runs the two enforcement checks described in firmware/SAFETY.md.

cmake_minimum_required(VERSION 3.16)

project(oapogee_firmware
        VERSION 0.1.0
        DESCRIPTION "oApogee model rocket telemetry payload firmware"
        LANGUAGES C)

# C11 exactly, with no compiler extensions. Extensions off because core is meant
# to be portable to any C11 compiler, and a GNU extension that compiles quietly
# on the host is a build failure on a toolchain nobody has picked yet.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Target mode.
#
# Declared so that the shape of the eventual build is visible and so that
# somebody looking for it finds this message rather than concluding it does not
# exist. It is not implemented, and pretending otherwise with a half-working
# path would waste more of a builder's time than saying so.
# ---------------------------------------------------------------------------

option(OA_BUILD_TARGET "Build the RP2350 firmware image. Requires the Pico SDK. Not implemented." OFF)

if(OA_BUILD_TARGET)
    message(FATAL_ERROR
        "OA_BUILD_TARGET is not implemented.\n"
        "\n"
        "Building a flashable image needs the Raspberry Pi Pico SDK and an Arm "
        "cross toolchain, neither of which this repository vendors or downloads. "
        "It also needs port/src, which does not exist: the port layer is "
        "currently declarations only, in port/include/oapogee_port/oa_port.h and "
        "oa_out.h.\n"
        "\n"
        "Host mode builds core and its tests with no SDK. Configure without this "
        "option to get it.")
endif()

# ---------------------------------------------------------------------------
# Warnings.
#
# -Werror because a warning nobody fixes is a warning nobody reads, and this is
# a small codebase with no legacy to grandfather in.
#
# -Wconversion is the one that earns its keep here. This firmware spends its
# time moving fixed-width integers between scales that the specs define (cm,
# cg, dm/s, 0.1 degC, Pa) and packing them into exact widths on the wire. A
# silent narrowing in that path produces a number that is wrong and plausible,
# which is the only kind of wrong this project actually fears.
#
# -Wdouble-promotion catches a float sneaking into the sample path and being
# promoted, on a part whose floating point support nobody has characterised.
# -Wvla because a variable length array is a stack allocation whose size is not
# known when the stack was sized. -Wshadow because a shadowed loop variable in a
# packer is a data corruption bug that reads correctly.
# ---------------------------------------------------------------------------

set(OA_WARNINGS
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wshadow
    -Wvla
    -Wdouble-promotion)

if(NOT (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang"))
    message(WARNING
        "Unrecognised C compiler '${CMAKE_C_COMPILER_ID}'. The warning set this "
        "project builds with is not being applied, so this build is checking "
        "less than CI does.")
    set(OA_WARNINGS "")
endif()

# ---------------------------------------------------------------------------
# core
#
# The whole of the flight logic, and the only part that can be tested today.
# It compiles against stdint.h, stdbool.h, stddef.h and string.h and nothing
# else: no SDK, no allocator, no global mutable state.
#
# Sources are listed rather than globbed. A glob silently picks up a file that
# was never reviewed and silently misses one that was added while the build was
# already configured.
#
# core does NOT get port/include on its include path. That is deliberate and it
# is load bearing: core cannot include oa_out.h, so core cannot name the
# functions that drive the buzzer and the LED. See firmware/SAFETY.md.
# ---------------------------------------------------------------------------

set(OA_CORE_SOURCES
    core/src/oa_baro.c
    core/src/oa_battery.c
    core/src/oa_config.c
    core/src/oa_config_parse.c
    core/src/oa_crc.c
    core/src/oa_flags.c
    core/src/oa_fusion.c
    core/src/oa_health.c
    core/src/oa_log_manifest.c
    core/src/oa_log_record.c
    core/src/oa_packet.c
    core/src/oa_ringbuf.c
    core/src/oa_sched.c
    core/src/oa_sink.c
    core/src/oa_state.c
    core/src/oa_states.c)

add_library(oa_core STATIC ${OA_CORE_SOURCES})

# gen/ is on the include path so the generated flight state header is included
# as "oa_states.h". It is generated from data/flight-phases.yaml and must not be
# hand-edited; see the comment at the top of gen/oa_states.h, which is honest
# about the fact that the generator does not exist yet.
target_include_directories(oa_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/gen)

target_compile_options(oa_core PRIVATE ${OA_WARNINGS})

# core is compiled as freestanding, with the host's hardening opt-ins turned off.
#
# -ffreestanding is a statement of fact rather than a workaround. core includes
# stdint.h, stdbool.h, stddef.h and string.h and nothing else, holds no global
# mutable state, allocates nothing, and has no hosted runtime under it. Declaring
# that is also what stops the compiler rewriting core's calls into calls core did
# not make: on Darwin, Clang lowers memset(p, 0, n) into bzero, which is the same
# operation under a name that is not on core/allowed-undefined.txt.
#
# The other two switch off host hardening that inserts its own symbols:
# _FORTIFY_SOURCE redirects memcpy and memset to __memcpy_chk and __memset_chk
# through Apple's headers, and the default stack protector emits __stack_chk_fail
# and __stack_chk_guard.
#
# The reason to care about six symbols is that core/allowed-undefined.txt is one
# of the two artifacts enforcing the passive payload boundary. Every name on it
# weakens what it proves, and a list long enough to stop being read proves
# nothing. All six are also host-only: none is on by default on the bare metal
# toolchain this eventually cross-compiles with, so leaving them in would mean the
# check that runs today and the check that runs on the target were inspecting
# different code.
#
# The cost is real and worth stating: core's own objects lose the fortified bounds
# check on memcpy and memset here, and lose the stack canary. Neither protects the
# target build, where neither is enabled. The tests are compiled without these
# flags and are where that checking belongs.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(oa_core PRIVATE
        -ffreestanding
        -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=0
        -fno-stack-protector)
endif()

# ---------------------------------------------------------------------------
# port
#
# Declarations only. There is no port/src and no port library, because no board
# exists to implement against. This interface target exists so that a consumer
# can pick up the headers and so that the include path is stated in one place
# when an implementation does land.
# ---------------------------------------------------------------------------

add_library(oa_port INTERFACE)

target_include_directories(oa_port INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include)

target_link_libraries(oa_port INTERFACE oa_core)

# ---------------------------------------------------------------------------
# Tests.
#
# The directory is test/, singular. An earlier revision of this file looked for
# tests/ and every test written since went into test/, so nothing was wired into
# ctest at all while twelve test files sat there passing individually. The
# directory that exists wins.
#
# Tests are not compiled with the fortify and stack-protector opt-outs that core
# carries. They are host-only code, they may use libc freely, and the checks are
# worth having in the code that drives the checking.
# ---------------------------------------------------------------------------

enable_testing()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt)
    add_subdirectory(test)
else()
    message(STATUS "firmware: no test/ yet, configuring core only")
endif()

# ---------------------------------------------------------------------------
# Enforcement.
#
# Two checks, both mechanical, both described in firmware/SAFETY.md. They are
# separate targets rather than part of ALL so that a builder can see them run
# and read what they printed, which for a safety boundary is worth more than a
# check that passes invisibly.
#
# The scripts are written into the build directory at configure time rather than
# checked in, so that the only files under firmware/ describing the boundary are
# the two artifacts a reviewer is meant to read.
# ---------------------------------------------------------------------------

find_program(OA_NM NAMES nm llvm-nm)

file(WRITE ${CMAKE_BINARY_DIR}/oa_check_outputs.cmake [==[
cmake_minimum_required(VERSION 3.16)

# Compare the oa_out_t enumerators against port/outputs.allowlist.
#
# Catches: an output added to the C without a line in the plain text list a
# reviewer reads, an output listed but not implemented, and any change to the
# count away from exactly two.

if(NOT EXISTS "${OA_OUT_HEADER}")
    message(FATAL_ERROR "check-outputs: cannot read ${OA_OUT_HEADER}")
endif()
if(NOT EXISTS "${OA_ALLOWLIST}")
    message(FATAL_ERROR "check-outputs: cannot read ${OA_ALLOWLIST}")
endif()

file(READ "${OA_OUT_HEADER}" _header)

# Only the enum body, so that a mention of an output name in a comment or a
# static assertion elsewhere in the file cannot be mistaken for a declaration.
if(NOT _header MATCHES "typedef enum \\{(.*)\\} oa_out_t;")
    message(FATAL_ERROR "check-outputs: no 'typedef enum { ... } oa_out_t;' found in ${OA_OUT_HEADER}")
endif()
set(_body "${CMAKE_MATCH_1}")

set(_from_c "")
string(REGEX MATCHALL "OA_OUT_[A-Z0-9_]+[ \t]*=" _decls "${_body}")
foreach(_d IN LISTS _decls)
    string(REGEX REPLACE "[ \t]*=$" "" _name "${_d}")
    list(APPEND _from_c "${_name}")
endforeach()

file(STRINGS "${OA_ALLOWLIST}" _lines)
set(_from_txt "")
foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    string(REGEX MATCH "^[^ \t]+" _name "${_line}")
    list(APPEND _from_txt "${_name}")
endforeach()

list(SORT _from_c)
list(SORT _from_txt)
list(LENGTH _from_c _n_c)
list(LENGTH _from_txt _n_txt)

message(STATUS "check-outputs: oa_out.h declares    ${_from_c}")
message(STATUS "check-outputs: allowlist declares   ${_from_txt}")

if(NOT _from_c STREQUAL _from_txt)
    message(FATAL_ERROR
        "check-outputs: oa_out.h and outputs.allowlist disagree.\n"
        "  oa_out.h:  ${_from_c}\n"
        "  allowlist: ${_from_txt}\n"
        "This is the passive payload boundary. Reconcile them deliberately, in a "
        "commit that says why, or revert whichever one moved.")
endif()

if(NOT _n_c EQUAL 2)
    message(FATAL_ERROR
        "check-outputs: expected exactly 2 outputs, the buzzer and the status LED, "
        "and found ${_n_c}. oApogee is a passive instrumentation payload. Changing "
        "the number of physical outputs is a project decision, not a firmware "
        "change.")
endif()

message(STATUS "check-outputs: OK, 2 outputs, both accounted for")
]==])

file(WRITE ${CMAKE_BINARY_DIR}/oa_check_undefined.cmake [==[
# Script mode does not inherit the project's policy settings, and this script
# uses IN_LIST. Without this, the check fails to run at exactly the moment it
# first has a symbol to reject, which is the worst time for a check to break.
cmake_minimum_required(VERSION 3.16)

# Check every symbol core needs from OUTSIDE core against
# core/allowed-undefined.txt.
#
# Catches: core reaching for the platform. malloc, printf, an SDK call, a
# compiler runtime helper pulled in by floating point or a 64-bit divide, and,
# most importantly, any call from core into the two functions in port/oa_out.h
# that drive the buzzer and the status LED.
#
# "Outside core" is the load-bearing phrase, and it is why this script makes two
# passes. nm -u on a single object reports every symbol that object does not
# itself define, which includes oa_crc16 called from oa_packet.c and
# oa_config_is_flightworthy called from oa_state.c. Those are calls from core into
# core: the state machine is required by its own header to ask the configuration
# whether it may arm. Reporting them as violations would say nothing about the
# platform boundary, and the repair everyone would reach for is to add oa_* names
# to the allowlist, which is exactly the repair that would gut it: oa_out_buzzer_set
# matches that shape too.
#
# So pass one collects every symbol the core objects define between them, and pass
# two subtracts that set before comparing. What is left is what core genuinely
# needs from outside itself, which is the property SAFETY.md actually claims.

if(NOT OA_NM)
    message(FATAL_ERROR
        "check-undefined: no nm found. This check cannot be skipped quietly: "
        "install binutils or llvm and configure again.")
endif()

file(STRINGS "${OA_ALLOWED}" _lines)
set(_allowed "")
foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    list(APPEND _allowed "${_line}")
endforeach()

file(GLOB_RECURSE _objs "${OA_OBJDIR}/*.o" "${OA_OBJDIR}/*.obj")
list(LENGTH _objs _n_objs)
if(_n_objs EQUAL 0)
    message(FATAL_ERROR
        "check-undefined: found no object files under ${OA_OBJDIR}. A check that "
        "inspects nothing must not report success. Build oa_core first.")
endif()

# GNU nm prints "         U symbol" and "0000 T symbol"; Mach-O nm prints the name
# with a leading underscore. Take the last field either way, then strip the Mach-O
# underscore prefix, so one allowlist covers both platforms.
function(oa_symbols _obj _outvar)
    execute_process(
        COMMAND "${OA_NM}" ${ARGN} "${_obj}"
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        RESULT_VARIABLE _rc
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "check-undefined: nm failed on ${_obj}: ${_err}")
    endif()
    string(REPLACE "\n" ";" _lines "${_out}")
    set(_syms "")
    foreach(_l IN LISTS _lines)
        string(STRIP "${_l}" _l)
        if(_l STREQUAL "" OR _l MATCHES ":$")
            continue()
        endif()
        string(REGEX REPLACE "^.*[ \t]" "" _sym "${_l}")
        string(REGEX REPLACE "^_" "" _sym "${_sym}")
        if(NOT _sym STREQUAL "")
            list(APPEND _syms "${_sym}")
        endif()
    endforeach()
    set(${_outvar} "${_syms}" PARENT_SCOPE)
endfunction()

# Pass one: everything core defines. A call from one core translation unit into
# another is not core reaching outside core.
set(_defined "")
foreach(_obj IN LISTS _objs)
    oa_symbols("${_obj}" _d -g --defined-only)
    list(APPEND _defined ${_d})
endforeach()
if(_defined)
    list(REMOVE_DUPLICATES _defined)
endif()

# Pass two: what is left after subtracting both core's own symbols and the list.
set(_violations "")
foreach(_obj IN LISTS _objs)
    oa_symbols("${_obj}" _u -u)
    foreach(_sym IN LISTS _u)
        if(_sym IN_LIST _defined)
            continue()
        endif()
        if(NOT _sym IN_LIST _allowed)
            get_filename_component(_base "${_obj}" NAME)
            list(APPEND _violations "${_base}: ${_sym}")
        endif()
    endforeach()
endforeach()

if(_violations)
    list(REMOVE_DUPLICATES _violations)
    string(REPLACE ";" "\n  " _pretty "${_violations}")
    message(FATAL_ERROR
        "check-undefined: core references symbols that are not on "
        "core/allowed-undefined.txt:\n  ${_pretty}\n"
        "The first question is whether core should be doing that, not whether the "
        "list should be longer. If one of these is a function that drives a "
        "physical output, core has crossed the passive payload boundary.")
endif()

list(LENGTH _defined _n_defined)
message(STATUS
    "check-undefined: OK, ${_n_objs} core objects defining ${_n_defined} symbols "
    "between them, and nothing needed from outside core that is not on the list")
]==])

add_custom_target(check-outputs
    COMMAND ${CMAKE_COMMAND}
            -DOA_OUT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/port/include/oapogee_port/oa_out.h
            -DOA_ALLOWLIST=${CMAKE_CURRENT_SOURCE_DIR}/port/outputs.allowlist
            -P ${CMAKE_BINARY_DIR}/oa_check_outputs.cmake
    COMMENT "Checking the output allowlist against oa_out.h"
    VERBATIM)

add_custom_target(check-undefined
    COMMAND ${CMAKE_COMMAND}
            -DOA_NM=${OA_NM}
            -DOA_ALLOWED=${CMAKE_CURRENT_SOURCE_DIR}/core/allowed-undefined.txt
            -DOA_OBJDIR=${CMAKE_BINARY_DIR}/CMakeFiles/oa_core.dir
            -P ${CMAKE_BINARY_DIR}/oa_check_undefined.cmake
    DEPENDS oa_core
    COMMENT "Checking core's external symbols against allowed-undefined.txt"
    VERBATIM)

add_custom_target(oa-check)
add_dependencies(oa-check check-outputs check-undefined)
