# Automated tests. Run them with:  ctest --preset <preset-name>
include(Catch)

add_executable(aether_tests
    unit/AudioEngineTests.cpp
    unit/LatencyInfoTests.cpp
    unit/SpscQueueTests.cpp
    unit/TestToneGeneratorTests.cpp
    realtime/RealtimeSafetyTests.cpp)

target_link_libraries(aether_tests PRIVATE aether_core aether_warnings Catch2::Catch2WithMain)

# PRE_TEST discovery lists the tests when ctest runs rather than during the build.
catch_discover_tests(aether_tests DISCOVERY_MODE PRE_TEST)

# Proof that RealtimeSanitizer is really switched on: this program deliberately allocates
# memory inside an AETHER_NONBLOCKING function, so it MUST crash. If it ever passes, the
# real-time safety tests above would be checking nothing.
if(AETHER_RTSAN)
    add_executable(aether_rtsan_canary realtime/RtsanCanary.cpp)
    target_link_libraries(aether_rtsan_canary PRIVATE aether_core)
    add_test(NAME rtsan_canary_must_fail COMMAND aether_rtsan_canary)
    set_tests_properties(rtsan_canary_must_fail PROPERTIES WILL_FAIL TRUE)
endif()
