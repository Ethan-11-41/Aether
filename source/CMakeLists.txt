# aether_core: everything that is not user interface. It is a library so the automated tests
# can use it without opening a window.
add_library(aether_core STATIC
    devices/AudioDeviceService.cpp
    devices/AudioDeviceService.h
    devices/DriverErrors.cpp
    devices/DriverErrors.h
    devices/LatencyInfo.cpp
    devices/LatencyInfo.h
    dsp/Effect.h
    dsp/Effects.cpp
    dsp/FastMath.h
    dsp/Instrument.h
    dsp/StarterSynth.cpp
    dsp/EffectTypes.cpp
    dsp/EffectTypes.h
    engine/AudioEngine.cpp
    engine/AudioFileCache.cpp
    engine/AudioFileCache.h
    engine/AudioEngine.h
    engine/Exporter.cpp
    engine/Exporter.h
    engine/LatencyProbe.cpp
    engine/LatencyProbe.h
    engine/Metronome.cpp
    engine/Metronome.h
    engine/MixerResources.cpp
    engine/MixerResources.h
    engine/MixerRouting.cpp
    engine/MixerRouting.h
    engine/Recorder.cpp
    engine/Recorder.h
    engine/RecordingRing.cpp
    engine/RecordingRing.h
    engine/Resampler.cpp
    engine/Resampler.h
    engine/TestToneGenerator.cpp
    engine/TestToneGenerator.h
    engine/TimelineSnapshot.h
    engine/TimelineSync.cpp
    engine/TimelineSync.h
    engine/rt/RealtimeAnnotations.h
    engine/rt/SnapshotExchange.h
    engine/rt/SpscQueue.h
    io/AppFolders.h
    io/AppLog.cpp
    io/AppLog.h
    io/AudioImport.cpp
    io/AudioImport.h
    io/Autosaver.cpp
    io/Autosaver.h
    io/JsonValueTree.cpp
    io/FileTypes.cpp
    io/FileTypes.h
    io/MidiImport.cpp
    io/MidiImport.h
    io/Mp3Encoder.cpp
    io/Mp3Encoder.h
    io/JsonValueTree.h
    io/ProjectFiles.cpp
    io/ProjectFiles.h
    io/ProjectFormat.cpp
    io/ProjectFormat.h
    io/ProjectSession.cpp
    io/ProjectSession.h
    io/RecentProjects.cpp
    io/RecentProjects.h
    model/Clip.cpp
    model/Clip.h
    model/ClipEdits.cpp
    model/ClipEdits.h
    model/EditCommands.cpp
    model/EditCommands.h
    model/Ids.h
    model/MidiClips.cpp
    model/MidiClips.h
    model/Mixer.cpp
    model/Mixer.h
    model/Project.cpp
    model/Project.h
    model/Recording.cpp
    model/Recording.h
    model/TimelineEdits.cpp
    model/TimelineEdits.h
    model/Track.cpp
    model/Track.h
    model/TrackColours.h
    time/Grid.cpp
    time/Grid.h
    time/TempoMap.cpp
    time/TempoMap.h)

target_include_directories(aether_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aether_core
    PUBLIC  aether_juce
    PRIVATE aether_warnings)
target_compile_definitions(aether_core PUBLIC AETHER_VERSION_STRING="${PROJECT_VERSION}")

# aether_ui: reusable interface pieces (track list, meters, look and feel). Also a library, so
# the tests can drive them without opening a window.
add_library(aether_ui STATIC
    ui/AetherLookAndFeel.cpp
    ui/AetherLookAndFeel.h
    ui/DelayedTooltipWindow.h
    ui/DeviceSettingsPanel.cpp
    ui/DeviceSettingsPanel.h
    ui/EffectEditor.cpp
    ui/EffectEditor.h
    ui/IconButton.cpp
    ui/KeyboardPanel.cpp
    ui/KeyboardPanel.h
    ui/IconButton.h
    ui/LatencyPanel.cpp
    ui/LatencyPanel.h
    ui/LevelMeter.cpp
    ui/LevelMeter.h
    ui/MixerComponent.cpp
    ui/MixerComponent.h
    ui/MixerControls.cpp
    ui/MixerControls.h
    ui/MessageBanner.cpp
    ui/MessageBanner.h
    ui/NumberDragField.cpp
    ui/NumberDragField.h
    ui/ShortcutList.cpp
    ui/ShortcutList.h
    ui/Theme.h
    ui/TimelineComponent.cpp
    ui/TimelineComponent.h
    ui/TimelineView.h
    ui/TrackListComponent.cpp
    ui/TrackListComponent.h
    ui/TransportBar.cpp
    ui/TransportBar.h)

target_link_libraries(aether_ui
    PUBLIC  aether_core
    PRIVATE aether_warnings)

# The Aether application itself.
if(AETHER_BUILD_APP)
    juce_add_gui_app(AetherApp
        PRODUCT_NAME                  "Aether"
        VERSION                       "${PROJECT_VERSION}"
        COMPANY_NAME                  "Aether Project"
        COMPANY_WEBSITE               "https://github.com/Ethan-11-41/aether"
        BUNDLE_ID                     "io.github.ethan-11-41.aether"
        ICON_BIG                      "${PROJECT_SOURCE_DIR}/resources/icon/aether-icon-1024.png"
        ICON_SMALL                    "${PROJECT_SOURCE_DIR}/resources/icon/aether-icon-256.png"
        MICROPHONE_PERMISSION_ENABLED TRUE
        NEEDS_WEB_BROWSER             FALSE
        NEEDS_CURL                    FALSE)

    target_sources(AetherApp PRIVATE
        app/AppCommands.h
        app/AppContext.h
        app/AppSettings.h
        app/AudioSettingsComponent.cpp
        app/AudioSettingsComponent.h
        app/CrashReporter.cpp
        app/ExportDialog.cpp
        app/ExportDialog.h
        app/CrashReporter.h
        app/Main.cpp
        app/MainWindow.cpp
        app/MainWindow.h
        app/ProjectView.cpp
        app/ProjectView.h
        app/StartScreen.cpp
        app/StartScreen.h
        ui/Theme.h)

    target_link_libraries(AetherApp PRIVATE aether_ui aether_warnings
                                            $<$<PLATFORM_ID:Windows>:Dbghelp>)   # crash reports

    # "cmake --install <build> --component Aether" copies the finished program into a staging
    # folder, which the installer script (installer/aether.iss) packages into Setup.exe.
    # The "Aether" component keeps out the developer files JUCE installs (headers, build tools).
    # The MP3 encoder goes next to the program, so running it straight from the build works too.
    add_dependencies(AetherApp mp3lame)
    add_custom_command(TARGET AetherApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:mp3lame>" "$<TARGET_FILE_DIR:AetherApp>")

    install(TARGETS AetherApp RUNTIME DESTINATION . COMPONENT Aether
                              BUNDLE  DESTINATION . COMPONENT Aether)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"    DESTINATION . RENAME LICENSE.txt COMPONENT Aether)
    install(FILES "${PROJECT_SOURCE_DIR}/CREDITS.md" DESTINATION . RENAME CREDITS.txt COMPONENT Aether)
endif()
