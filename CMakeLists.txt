cmake_minimum_required(VERSION 3.22)
project(AuraSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(AURA_BUILD_TESTS "Build the offline render / QA test tool" ON)
option(AURA_COPY_AFTER_BUILD "Copy the VST3 to the system VST3 folder after build (needs admin rights on Windows)" OFF)

# JUCE is expected in third_party/JUCE (git clone --branch 8.0.8). Override with -DAURA_JUCE_DIR=...
set(AURA_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory(${AURA_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)

set(AURA_SOURCES
    Source/Params.cpp
    Source/Tables.cpp
    Source/SynthEngine.cpp
    Source/Effects.cpp
    Source/Presets.cpp
    Source/PresetManager.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(AURA_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

juce_add_plugin(AuraSynth
    COMPANY_NAME "AuraSynth Audio"
    PLUGIN_MANUFACTURER_CODE Aura
    PLUGIN_CODE Asy1
    FORMATS VST3 Standalone
    PRODUCT_NAME "AuraSynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${AURA_COPY_AFTER_BUILD})

target_sources(AuraSynth PRIVATE ${AURA_SOURCES})
target_include_directories(AuraSynth PRIVATE Source)
target_compile_definitions(AuraSynth PUBLIC ${AURA_DEFS})
target_link_libraries(AuraSynth
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------------------ AuraDrums (second plug-in)
set(DRUMS_LIB_SOURCES
    Drums/DrumSounds.cpp
    Drums/DrumEngine.cpp)
set(DRUMS_SOURCES
    ${DRUMS_LIB_SOURCES}
    Drums/DrumsProcessor.cpp
    Drums/DrumsEditor.cpp)

juce_add_plugin(AuraDrums
    COMPANY_NAME "AuraSynth Audio"
    PLUGIN_MANUFACTURER_CODE Aura
    PLUGIN_CODE Adr1
    FORMATS VST3 Standalone
    PRODUCT_NAME "AuraDrums"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD ${AURA_COPY_AFTER_BUILD})

target_sources(AuraDrums PRIVATE ${DRUMS_SOURCES})
target_include_directories(AuraDrums PRIVATE Drums)
target_compile_definitions(AuraDrums PUBLIC ${AURA_DEFS})
target_link_libraries(AuraDrums
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(AURA_BUILD_TESTS)
    # sound-library QA (no JUCE): numbers, balance, pitch, engine behaviour
    add_executable(AuraDrumsLibTest ${DRUMS_LIB_SOURCES} Tests/DrumsTest.cpp)
    target_include_directories(AuraDrumsLibTest PRIVATE Drums)
    if(MSVC)
        target_compile_options(AuraDrumsLibTest PRIVATE /W4 /wd4244 /wd4267 /wd4100)
    else()
        target_compile_options(AuraDrumsLibTest PRIVATE -Wall -Wextra)
    endif()

    # real-plug-in test: state, MIDI, screenshots, demo render
    juce_add_console_app(AuraDrumsPluginTest PRODUCT_NAME "AuraDrumsPluginTest")
    target_sources(AuraDrumsPluginTest PRIVATE ${DRUMS_SOURCES} Tests/DrumsPluginTest.cpp)
    target_include_directories(AuraDrumsPluginTest PRIVATE Drums)
    target_compile_definitions(AuraDrumsPluginTest PUBLIC ${AURA_DEFS} AURA_TEST_BUILD=1)
    target_link_libraries(AuraDrumsPluginTest
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    juce_add_console_app(AuraRenderTest PRODUCT_NAME "AuraRenderTest")
    target_sources(AuraRenderTest PRIVATE ${AURA_SOURCES} Tests/RenderTest.cpp)
    target_include_directories(AuraRenderTest PRIVATE Source)
    target_compile_definitions(AuraRenderTest PUBLIC ${AURA_DEFS} AURA_TEST_BUILD=1
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(AuraRenderTest
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
