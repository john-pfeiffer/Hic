# Hic plugin: a thin JUCE wrapper around hic_dsp.
juce_add_plugin(Hic
  COMPANY_NAME "Hic"
  BUNDLE_ID "com.hic.drums"
  PLUGIN_MANUFACTURER_CODE Hicx
  PLUGIN_CODE Hic1
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "Hic"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Hic PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp)

target_include_directories(Hic PRIVATE Source)

target_compile_definitions(Hic PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Hic
  PRIVATE hic_dsp juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# The core keeps a `default:` in every switch and compares floats on purpose.
if(NOT MSVC)
  target_compile_options(Hic PRIVATE -Wno-switch-enum -Wno-float-equal)
endif()

if(HIC_BUILD_CLAP)
  include(FetchContent)
  FetchContent_Declare(clap-juce-extensions GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git GIT_TAG main GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(clap-juce-extensions)
  clap_juce_extensions_plugin(TARGET Hic CLAP_ID "com.hic.drums" CLAP_FEATURES instrument drum-machine)
endif()
