cmake_minimum_required(VERSION 3.22)

project(Prism VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

#------------------------------------------------------------------------------
# JUCE：优先用本地路径（-DJUCE_DIR=... 或环境变量 JUCE_DIR），
#       否则用 FetchContent 拉 8.0.9（首次配置需要网络）
#------------------------------------------------------------------------------
if(DEFINED ENV{JUCE_DIR})
    set(JUCE_DIR "$ENV{JUCE_DIR}")
endif()

if(JUCE_DIR)
    message(STATUS "Prism: using local JUCE at ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Prism: fetching JUCE 8.0.9 from GitHub")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

#------------------------------------------------------------------------------
# 插件
#------------------------------------------------------------------------------
juce_add_plugin(Prism
    COMPANY_NAME             "Yunshu_TSK"
    PLUGIN_MANUFACTURER_CODE Ynsu
    PLUGIN_CODE              Prsm
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Prism"
    DESCRIPTION              "All-pass phase dispersion"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          "Fx")

target_sources(Prism PRIVATE
    Source/PrismProcessor.cpp
    Source/PrismEditor.cpp
    Source/DispersionEngine.cpp
    Source/DispersionCurve.cpp
    Source/SpectrumDisplay.cpp
    Source/FrequencyView.cpp
    Source/AboutOverlay.cpp)

target_compile_definitions(Prism PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Prism
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_audio_utils
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#------------------------------------------------------------------------------
# DSP 自检（控制台程序，不需要宿主）
#------------------------------------------------------------------------------
add_executable(PrismSelfTest
    Source/PrismSelfTest.cpp
    Source/DispersionEngine.cpp
    Source/DispersionCurve.cpp)

target_compile_options(PrismSelfTest PRIVATE /utf-8)
target_link_libraries(PrismSelfTest PRIVATE juce::juce_audio_basics juce::juce_dsp)

#------------------------------------------------------------------------------
# 界面回归测试（验证「打开界面 / 宿主 program 回调 / 旁通」都不改参数）
#------------------------------------------------------------------------------
add_executable(PrismEditorProbe
    tools/PrismEditorProbe.cpp
    Source/PrismProcessor.cpp
    Source/PrismEditor.cpp
    Source/DispersionEngine.cpp
    Source/DispersionCurve.cpp
    Source/SpectrumDisplay.cpp
    Source/FrequencyView.cpp
    Source/AboutOverlay.cpp)

target_compile_options(PrismEditorProbe PRIVATE /utf-8)

# 需要 runDispatchLoopUntil 来派发异步消息（ComboBox 的 onChange 就是异步的）
target_compile_definitions(PrismEditorProbe PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

target_include_directories(PrismEditorProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(PrismEditorProbe PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra)
