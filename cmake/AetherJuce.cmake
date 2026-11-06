# Builds the JUCE modules that Aether uses into one static library, "aether_juce".
#
# JUCE modules are normally compiled into every target that links them. Aether has several
# targets (the app, the tests, and later helper tools), so we compile JUCE once here and share it.
# This mirrors what JUCE itself does for plug-in "shared code" targets.

add_library(aether_juce STATIC)

target_link_libraries(aether_juce
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags)

# JUCE configuration switches. These are PUBLIC so that every target sees the same settings
# as the compiled library (mismatches would cause hard-to-find bugs).
target_compile_definitions(aether_juce
    PUBLIC
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0                 # no embedded web views
        JUCE_USE_CURL=0                    # no network code
        JUCE_DIRECTSOUND=0                 # legacy, high-latency Windows API: hidden to avoid confusion
        JUCE_ASIO=$<IF:$<PLATFORM_ID:Windows>,1,0>   # ASIO headers are bundled with JUCE (GPLv3)
        JUCE_JACK=0
        JUCE_PLUGINHOST_VST3=0             # switched on in milestone M15 (plug-in hosting)
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_WASAPI_LOGGING=1              # Windows audio errors go to Aether's log file
        $<$<CONFIG:Debug>:DEBUG=1>
        $<$<CONFIG:Debug>:_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:_NDEBUG=1>)

# Re-export the module include paths and module definitions to everything that links
# aether_juce, without re-adding the module sources (which would compile JUCE twice).
target_include_directories(aether_juce SYSTEM INTERFACE
    $<TARGET_PROPERTY:aether_juce,INCLUDE_DIRECTORIES>)
target_compile_definitions(aether_juce INTERFACE
    $<TARGET_GENEX_EVAL:aether_juce,$<TARGET_PROPERTY:aether_juce,COMPILE_DEFINITIONS>>)
