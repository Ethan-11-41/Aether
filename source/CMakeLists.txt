# aether_core: everything that is not user interface. It is a library so the automated tests
# can use it without opening a window.
add_library(aether_core STATIC
    devices/AudioDeviceService.cpp
    devices/AudioDeviceService.h
    devices/LatencyInfo.cpp
    devices/LatencyInfo.h
    engine/AudioEngine.cpp
    engine/AudioEngine.h
    engine/TestToneGenerator.cpp
    engine/TestToneGenerator.h
    engine/rt/RealtimeAnnotations.h
    engine/rt/SpscQueue.h
    io/Autosaver.cpp
    io/Autosaver.h
    io/JsonValueTree.cpp
    io/JsonValueTree.h
    io/ProjectFiles.cpp
    io/ProjectFiles.h
    io/ProjectFormat.cpp
    io/ProjectFormat.h
    io/ProjectSession.cpp
    io/ProjectSession.h
    io/RecentProjects.cpp
    io/RecentProjects.h
    model/EditCommands.cpp
    model/EditCommands.h
    model/Ids.h
    model/Project.cpp
    model/Project.h
    model/Track.cpp
    model/Track.h
    model/TrackColours.h)

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
    ui/LatencyPanel.cpp
    ui/LatencyPanel.h
    ui/LevelMeter.cpp
    ui/LevelMeter.h
    ui/MessageBanner.cpp
    ui/MessageBanner.h
    ui/Theme.h
    ui/TrackListComponent.cpp
    ui/TrackListComponent.h)

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

    # "cmake --install" copies the finished program into a staging folder, which the
    # installer script (installer/aether.iss) then packages into Setup.exe.
    install(TARGETS AetherApp RUNTIME DESTINATION . BUNDLE DESTINATION .)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"    DESTINATION . RENAME LICENSE.txt)
    install(FILES "${PROJECT_SOURCE_DIR}/CREDITS.md" DESTINATION . RENAME CREDITS.txt)
endif()
