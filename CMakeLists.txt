cmake_minimum_required(VERSION 3.22)
# Must come BEFORE project(), or CMake ignores them: universal (Apple Silicon + Intel), macOS 11+
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
project(AFTERLIFE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same JUCE version as Flatliner, Cutthroat, Torment, Possessed and Dead Air.
# (Pass -DJUCE_LOCAL_DIR=/path/to/JUCE to build against a local checkout.)
if(JUCE_LOCAL_DIR)
    add_subdirectory(${JUCE_LOCAL_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ARA (Afterlife applied to the clip: the whole waveform is drawn at once, no playing it through first):
# pass -DAFTERLIFE_ARA_SDK=/path/to/ARA_SDK (releases/2.3.0). Without it Afterlife builds as a plain plugin
# that draws the waveform as the song plays.
set(AFTERLIFE_ARA_SDK "" CACHE PATH "ARA SDK location (empty = no ARA)")
set(AFTERLIFE_ARA_ARGS "")
if(NOT AFTERLIFE_TESTS_ONLY AND AFTERLIFE_ARA_SDK)
    juce_set_ara_sdk_path("${AFTERLIFE_ARA_SDK}")
    set(AFTERLIFE_ARA_ARGS IS_ARA_EFFECT TRUE)
    message(STATUS "Afterlife: ARA on (${AFTERLIFE_ARA_SDK})")
endif()

set(AFTERLIFE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT AFTERLIFE_TESTS_ONLY)
juce_add_plugin(Afterlife
    ${AFTERLIFE_ARA_ARGS}
    COMPANY_NAME "Castle Danger"
    PRODUCT_NAME "Afterlife"
    BUNDLE_ID com.castledanger.afterlife
    PLUGIN_MANUFACTURER_CODE Cdgr
    PLUGIN_CODE Aftl
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Afterlife PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Afterlife PUBLIC ${AFTERLIFE_DEFS})

target_link_libraries(Afterlife
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

# Headless checks: DSP + sections + state tests, and renders the editor to PNGs
juce_add_console_app(AfterlifeSnapshot PRODUCT_NAME "AfterlifeSnapshot")
target_sources(AfterlifeSnapshot PRIVATE test/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(AfterlifeSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    "JucePlugin_VersionString=\"${PROJECT_VERSION}\"" "JucePlugin_Name=\"Afterlife\"")
target_link_libraries(AfterlifeSnapshot PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags)

# A tiny ARA host that applies the built Afterlife to an in-memory clip and checks it (ARA builds only)
if(NOT AFTERLIFE_TESTS_ONLY AND AFTERLIFE_ARA_SDK)
    juce_add_console_app(AfterlifeARAHost PRODUCT_NAME "AfterlifeARAHost")
    target_sources(AfterlifeARAHost PRIVATE test/ara_host.cpp)
    target_compile_definitions(AfterlifeARAHost PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_ARA=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(AfterlifeARAHost PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce_ara_sdk
        PUBLIC juce::juce_recommended_config_flags)
endif()
