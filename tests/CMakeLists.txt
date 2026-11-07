# Automated tests. Run them with:  ctest --preset <preset-name>
include(Catch)

add_executable(aether_tests
    unit/AudioDeviceServiceTests.cpp
    unit/AudioEngineTests.cpp
    unit/AudioFileTests.cpp
    unit/ClipEditTests.cpp
    unit/ClipPlaybackTests.cpp
    unit/DeviceSettingsPanelTests.cpp
    unit/DrumKitTests.cpp
    unit/DrumSoundTests.cpp
    unit/DiagnosticsTests.cpp
    unit/ExportTests.cpp
    unit/GridTests.cpp
    unit/KeyboardPanelTests.cpp
    unit/LatencyInfoTests.cpp
    unit/MidiEngineTests.cpp
    unit/MidiModelTests.cpp
    unit/MixerEngineTests.cpp
    unit/MixerModelTests.cpp
    unit/MixerUiTests.cpp
    unit/NoteEditTests.cpp
    unit/PerformanceTests.cpp
    unit/PianoRollTests.cpp
    unit/ProjectFileTests.cpp
    unit/ProjectModelTests.cpp
    unit/ProjectSessionTests.cpp
    unit/RecordingEngineTests.cpp
    unit/RecordingModelTests.cpp
    unit/ShortcutListTests.cpp
    unit/SpscQueueTests.cpp
    unit/SynthEditorTests.cpp
    unit/SynthTests.cpp
    unit/TempoMapTests.cpp
    unit/TestToneGeneratorTests.cpp
    unit/TimelineClipTests.cpp
    unit/TimelineEditTests.cpp
    unit/TimelineViewTests.cpp
    unit/TrackListComponentTests.cpp
    unit/TransportTests.cpp
    realtime/RealtimeSafetyTests.cpp)

target_link_libraries(aether_tests PRIVATE aether_ui aether_warnings Catch2::Catch2WithMain)
target_compile_definitions(aether_tests PRIVATE AETHER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                                                AETHER_TEST_MP3LAME="$<TARGET_FILE:mp3lame>")
add_dependencies(aether_tests mp3lame)   # the MP3 export tests load the LAME library

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
