cmake_minimum_required(VERSION 3.22)

project(ZarlinoSource VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ZARLINO_SOURCE_BUILD_TESTS "Build ZarlinoSource unit tests" ON)
option(ZARLINO_SOURCE_BUILD_BENCHMARKS "Build ZarlinoSource benchmark tools" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE (8.x). When built from the enclosing VST workspace the framework is
# reused from ../JUCE; a standalone checkout can be pointed at with
# -DZARLINO_SOURCE_JUCE_DIR=<path>.
# ---------------------------------------------------------------------------
if(NOT TARGET juce::juce_core)
    set(ZARLINO_SOURCE_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
        CACHE PATH "Path to a JUCE 8.x checkout")

    if(NOT EXISTS "${ZARLINO_SOURCE_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "JUCE not found at '${ZARLINO_SOURCE_JUCE_DIR}'. "
            "Pass -DZARLINO_SOURCE_JUCE_DIR=<path to JUCE>.")
    endif()

    add_subdirectory("${ZARLINO_SOURCE_JUCE_DIR}" JUCE)
endif()

# ---------------------------------------------------------------------------
# Project-wide definitions
# ---------------------------------------------------------------------------
add_compile_definitions(
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    ZARLINO_SOURCE_VERSION="0.1.0"
)

# ---------------------------------------------------------------------------
# Core: dsp / core / inference / ui.
# An INTERFACE library so every consumer target compiles these sources with
# identical JUCE module definitions.
# ---------------------------------------------------------------------------
add_library(ZarlinoSource_Core INTERFACE)

file(GLOB_RECURSE ZARLINO_SOURCE_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/dsp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/inference/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
)

target_sources(ZarlinoSource_Core INTERFACE ${ZARLINO_SOURCE_CORE_SOURCES})

target_include_directories(ZarlinoSource_Core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/include"
)

target_link_libraries(ZarlinoSource_Core INTERFACE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_cryptography
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Product: VST3 plugin + standalone application from one codebase.
# ---------------------------------------------------------------------------
juce_add_plugin(ZarlinoSource
    COMPANY_NAME "Zarlino Audio"
    COMPANY_WEBSITE "https://zarlinoaudio.local"
    BUNDLE_ID "com.zarlinoaudio.zarlinosource"
    PLUGIN_MANUFACTURER_CODE Khri
    PLUGIN_CODE ZSrc
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Zarlino Source"
)

target_sources(ZarlinoSource PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/ExtractionController.cpp
)

target_link_libraries(ZarlinoSource PRIVATE ZarlinoSource_Core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ZARLINO_SOURCE_BUILD_TESTS)
    juce_add_console_app(ZarlinoSource_Tests PRODUCT_NAME "ZarlinoSource Tests")

    file(GLOB_RECURSE ZARLINO_SOURCE_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    target_sources(ZarlinoSource_Tests PRIVATE ${ZARLINO_SOURCE_TEST_SOURCES})
    target_include_directories(ZarlinoSource_Tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(ZarlinoSource_Tests PRIVATE ZarlinoSource_Core)
endif()
