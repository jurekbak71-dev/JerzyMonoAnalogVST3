cmake_minimum_required(VERSION 3.24)
project(JerzyBassAmbient VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
option(BASS_AMBIENT_MOBILE_APP "Build standalone touch app" OFF)
option(BASS_AMBIENT_SMOKE_TEST "Enable simulator smoke-test hooks" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(BASS_AMBIENT_MOBILE_APP ON)
endif()
if(NOT BASS_AMBIENT_MOBILE_APP)
  add_executable(BassAmbientPerformance tests/PerformanceTests.cpp)
  target_include_directories(BassAmbientPerformance PRIVATE Source)
  add_test(NAME BassAmbientPerformance COMMAND BassAmbientPerformance)
  add_executable(BassAmbientTests tests/CoreTests.cpp)
  target_include_directories(BassAmbientTests PRIVATE Source)
  add_test(NAME BassAmbientCore COMMAND BassAmbientTests)
endif()
option(BASS_AMBIENT_CORE_ONLY "Build dependency-free DSP tests only" OFF)
if(BASS_AMBIENT_CORE_ONLY)
  return()
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.3 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
if(BASS_AMBIENT_MOBILE_APP)
  juce_add_gui_app(BassAmbientIOS
    COMPANY_NAME "Jerzy" BUNDLE_ID "pl.jerzy.bassambient.mobile"
    PRODUCT_NAME "Bass Ambient" VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/iOS/Icon.png"
    TARGETED_DEVICE_FAMILY "1,2" STATUS_BAR_HIDDEN TRUE REQUIRES_FULL_SCREEN TRUE
    FILE_SHARING_ENABLED TRUE BACKGROUND_AUDIO_ENABLED FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    PLIST_TO_MERGE "<plist><dict><key>LSSupportsOpeningDocumentsInPlace</key><true/><key>ITSAppUsesNonExemptEncryption</key><false/></dict></plist>")
  juce_generate_juce_header(BassAmbientIOS)
  target_sources(BassAmbientIOS PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp iOS/MobileApp.cpp)
  target_include_directories(BassAmbientIOS PRIVATE Source)
  target_compile_definitions(BassAmbientIOS PRIVATE
    BASS_AMBIENT_MOBILE=1 BASS_AMBIENT_SMOKE_TEST=$<BOOL:${BASS_AMBIENT_SMOKE_TEST}>
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0
    JucePlugin_Name="Jerzy Bass Ambient" JucePlugin_VersionString="${PROJECT_VERSION}")
  target_link_libraries(BassAmbientIOS PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  return()
endif()
juce_add_plugin(JerzyBassAmbient
  COMPANY_NAME "Jerzy" BUNDLE_ID "pl.jerzy.bassambient"
  PLUGIN_MANUFACTURER_CODE Jrzy PLUGIN_CODE Jbam
  FORMATS VST3 Standalone PRODUCT_NAME "Jerzy Bass Ambient"
  IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(JerzyBassAmbient)
target_sources(JerzyBassAmbient PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(JerzyBassAmbient PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(JerzyBassAmbient PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(BUILD_TESTING)
  juce_add_console_app(BassAmbientIntegration PRODUCT_NAME "Bass Ambient Integration Tests")
  juce_generate_juce_header(BassAmbientIntegration)
  target_sources(BassAmbientIntegration PRIVATE tests/IntegrationTests.cpp tests/reference/PluginProcessor030.cpp)
  target_include_directories(BassAmbientIntegration PRIVATE Source)
  target_link_libraries(BassAmbientIntegration PRIVATE JerzyBassAmbient juce::juce_audio_utils)
  add_test(NAME BassAmbientHostGUI COMMAND BassAmbientIntegration)
endif()
