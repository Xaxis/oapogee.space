# oApogee core tests.
#
# One executable per test file, each with its own main() and plain assert. That
# convention was settled by the people who wrote the tests, not chosen here, and
# there is no framework to install: `cmake --build` and `ctest` is the whole
# toolchain. ctest is the runner, and what it reports is the pass and fail count.
#
# The reason there is no single main.c aggregating these: a test that ends in
# abort() takes its process with it, so twelve processes means a failure in one
# module cannot stop the other eleven from reporting. That is worth more than a
# single summary line, and ctest prints the summary line anyway.
#
# NOTHING HERE HAS RUN ON HARDWARE. These tests run on a laptop against tables of
# samples and buffers standing in for a flash part. No board has been fabricated,
# no sensor has been read, and no packet has been transmitted.

# test_oa_conformance is deliberately first. It is the one that checks the
# firmware against the two specifications rather than against its own headers,
# and when several tests fail at once it is the one to read first.
set(OA_TESTS
    test_oa_conformance
    test_oa_baro
    test_oa_battery
    test_oa_config_parse
    test_oa_crc
    test_oa_fusion
    test_oa_health
    test_oa_log_manifest
    test_oa_log_record
    test_oa_packet
    test_oa_ringbuf
    test_oa_sched
    test_oa_state)

find_library(M_LIBRARY m)

foreach(_test IN LISTS OA_TESTS)
    add_executable(${_test} ${_test}.c)
    target_link_libraries(${_test} PRIVATE oa_core)

    # The tests compute reference values with pow() and friends to check the
    # firmware's fixed-point arithmetic against floating point. glibc keeps the
    # maths functions in a separate library; Apple's libc does not, so this is
    # linked only where it exists. The core itself uses no floating point and
    # links nothing.
    if(M_LIBRARY)
        target_link_libraries(${_test} PRIVATE ${M_LIBRARY})
    endif()
    target_compile_options(${_test} PRIVATE ${OA_WARNINGS})

    # assert() is a no-op under NDEBUG, and every one of these tests is written as
    # a sequence of assert() calls. A release-configured build defines NDEBUG, so
    # without this the suite would run to completion having checked nothing and
    # report success, which is the worst outcome available to a test suite. This
    # lands after CMake's own -DNDEBUG on the command line, which is what makes it
    # win.
    target_compile_options(${_test} PRIVATE -UNDEBUG)

    add_test(NAME ${_test} COMMAND ${_test})
endforeach()

# The cross-implementation vector dumper.
#
# Not a test: it prints canonical packets as hex so tools/check-crossimpl.mjs can
# compare them against the TypeScript encoder in apps/web/lib/packet.ts. The two
# were written from the same specification by different hands, and neither from
# the other, so agreement is evidence the document is unambiguous and a
# difference is a defect in the document even when both sides look right.
add_executable(oa_vectors oa_vectors.c)
target_link_libraries(oa_vectors PRIVATE oa_core)
target_compile_options(oa_vectors PRIVATE ${OA_WARNINGS})
