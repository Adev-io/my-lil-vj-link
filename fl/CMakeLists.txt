# My lil VJ Link: VST3 insert effect for FL Studio (and any VST3 host), built on JUCE.
# Author: Ari Klopfer / A_Development · AGPL-3.0-or-later (links JUCE, see LICENSE)
cmake_minimum_required(VERSION 3.22)
project(MyLilVJLink VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MyLilVJLink
    COMPANY_NAME "A_Development"
    PLUGIN_MANUFACTURER_CODE Adev
    PLUGIN_CODE Vjlk
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PRODUCT_NAME "My lil VJ Link"
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MyLilVJLink PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/Editor.cpp Source/Editor.h)
target_include_directories(MyLilVJLink PUBLIC Source ../core)
target_compile_definitions(MyLilVJLink PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(MyLilVJLink PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Headless smoke test: drives the real processor with audio and checks what it would send (no host, no MIDI port).
enable_testing()
juce_add_console_app(MyLilVJLinkTest PRODUCT_NAME "MyLilVJLinkTest")
target_sources(MyLilVJLinkTest PRIVATE Tests/smoke.cpp Source/PluginProcessor.cpp Source/Editor.cpp)
target_include_directories(MyLilVJLinkTest PRIVATE Source ../core)
target_compile_definitions(MyLilVJLinkTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MyLilVJLinkTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
add_test(NAME smoke COMMAND MyLilVJLinkTest)
