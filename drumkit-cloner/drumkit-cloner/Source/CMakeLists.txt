cmake_minimum_required(VERSION 3.22)

project(DrumkitCloner VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

# JUCE se descarga solo al configurar (hace falta internet la primera vez)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DKC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DKC_FORMATS AU)
endif()

option(DKC_COPY_AFTER_BUILD "Copiar el plugin a la carpeta del sistema tras compilar" OFF)

juce_add_plugin(DrumkitCloner
    COMPANY_NAME "MiEstudio"                 # <- cambia esto
    PLUGIN_MANUFACTURER_CODE Mest            # 4 letras, al menos 1 mayuscula
    PLUGIN_CODE Dkcl                         # 4 letras, exactamente 1 mayuscula
    PRODUCT_NAME "Drumkit Cloner"            # <- cambia esto
    FORMATS ${DKC_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${DKC_COPY_AFTER_BUILD}
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Drum Sampler)

target_sources(DrumkitCloner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DrumkitCloner PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DrumkitCloner
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
