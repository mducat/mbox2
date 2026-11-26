include(FetchContent)

# ---------------------------------------------------------------------------
# JUCE (fetched at configure time). To use a local checkout instead, pass
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE
# ---------------------------------------------------------------------------
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MusicBox2
    PRODUCT_NAME                "Music Box 2"
    COMPANY_NAME                "mducat"
    BUNDLE_ID                   "com.mducat.musicbox2"
    PLUGIN_MANUFACTURER_CODE    Mduc
    PLUGIN_CODE                 Mbx2
    VERSION                     ${PROJECT_VERSION}

    FORMATS                     VST3 Standalone

    # Declared as an instrument with MIDI in/out: this is what FL Studio expects
    # for a MIDI-generating plugin (route its MIDI out with the wrapper's MIDI
    # output port). The audio output stays silent.
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    VST3_CATEGORIES             Instrument Tools
    COPY_PLUGIN_AFTER_BUILD     FALSE)

# Every .cpp under Source/ (widgets included) is compiled: adding a widget
# needs no CMake change. Re-run CMake after adding a file if your generator
# does not pick it up (CONFIGURE_DEPENDS handles it for Ninja / VS).
file(GLOB_RECURSE MBOX_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
target_sources(MusicBox2 PRIVATE ${MBOX_PLUGIN_SOURCES})

target_compile_definitions(MusicBox2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MusicBox2
    PRIVATE
        mbox::theory
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Optional CLAP build (-DMBOX_BUILD_CLAP=ON)
# ---------------------------------------------------------------------------
if(MBOX_BUILD_CLAP)
    FetchContent_Declare(clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        main
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(clap-juce-extensions)

    clap_juce_extensions_plugin(TARGET MusicBox2
        CLAP_ID "com.mducat.musicbox2"
        CLAP_FEATURES instrument note-effect utility)
endif()
