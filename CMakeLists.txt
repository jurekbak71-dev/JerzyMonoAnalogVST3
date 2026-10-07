cmake_minimum_required(VERSION 3.24)
project(JerzyMonoAnalog VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JerzyMonoAnalog
    COMPANY_NAME "Jerzy"
    BUNDLE_ID "pl.jerzy.monoanaloggrid"
    PLUGIN_MANUFACTURER_CODE Jrzy
    PLUGIN_CODE Jmag
    FORMATS VST3 Standalone
    PRODUCT_NAME "Jerzy Mono Analog Grid"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(JerzyMonoAnalog)

target_sources(JerzyMonoAnalog PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/JerzyVSTGuiKit.h
    Source/AnalogDSP.h
    Source/AnalogEnvelope.h
    Source/SequencerClock.h
    Source/MonoFxChain.h
)

target_compile_definitions(JerzyMonoAnalog
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(JerzyMonoAnalog
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)


include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(MonoDSPTests PRODUCT_NAME "Mono DSP Tests")
    juce_generate_juce_header(MonoDSPTests)
    target_sources(MonoDSPTests PRIVATE tests/DSPTests.cpp Source/SequencerClock.h)
    target_include_directories(MonoDSPTests PRIVATE Source)
    target_compile_definitions(MonoDSPTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MonoDSPTests PRIVATE juce::juce_core)
    add_test(NAME MonoDSP COMMAND MonoDSPTests)

    juce_add_console_app(MonoProcessorTests PRODUCT_NAME "Mono Processor Tests")
    juce_generate_juce_header(MonoProcessorTests)
    target_sources(MonoProcessorTests PRIVATE tests/ProcessorTests.cpp)
    target_include_directories(MonoProcessorTests PRIVATE Source)
    target_link_libraries(MonoProcessorTests PRIVATE JerzyMonoAnalog juce::juce_audio_utils)
    add_test(NAME MonoProcessor COMMAND MonoProcessorTests)
endif()
