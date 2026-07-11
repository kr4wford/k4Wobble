cmake_minimum_required(VERSION 3.22)

project(k4Wobble VERSION 1.1.0 LANGUAGES C CXX)

# Use a JUCE checkout sitting next to this project if present (fast local
# builds); otherwise fetch the latest JUCE 8 automatically (CI, fresh clones).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    message(STATUS "k4Wobble: using local JUCE at ../JUCE")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE
                     ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "k4Wobble: fetching JUCE via FetchContent")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        master          # latest JUCE 8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(k4Wobble
    COMPANY_NAME            "k4 Audio"
    BUNDLE_ID               "com.k4audio.k4wobble"
    PLUGIN_MANUFACTURER_CODE K4au
    PLUGIN_CODE             Wob1
    FORMATS                 VST3
    PRODUCT_NAME            "k4 Wobble"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    VST3_CATEGORIES         "Fx" "Filter" "Modulation")

target_sources(k4Wobble PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WobbleEngine.cpp
    Source/WobbleScope.cpp
    Source/PatternStrip.cpp)

target_compile_definitions(k4Wobble PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(k4Wobble PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Console smoke test for the DSP engine (build with: cmake --build build --target k4WobbleTests)
juce_add_console_app(k4WobbleTests PRODUCT_NAME "k4WobbleTests")
target_sources(k4WobbleTests PRIVATE
    Tests/EngineTest.cpp
    Source/WobbleEngine.cpp)
target_compile_definitions(k4WobbleTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(k4WobbleTests PRIVATE
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
