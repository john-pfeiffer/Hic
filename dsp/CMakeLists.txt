# hic_dsp: the dependency-free synthesis core. No JUCE, no heap use in the
# audio path, no exceptions, no RTTI. Everything the plugin (and a future
# hardware build) needs lives here.
add_library(hic_dsp STATIC
  src/Hic.cpp
  src/Kit.cpp
  src/Engine.cpp
  src/Pattern.cpp
  src/Sequencer.cpp
)
target_include_directories(hic_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(hic_dsp PUBLIC HIC_MAX_SAMPLE_RATE=${HIC_MAX_SAMPLE_RATE})
target_compile_features(hic_dsp PUBLIC cxx_std_17)
target_link_libraries(hic_dsp PRIVATE hic_warnings hic_core_flags)
set_target_properties(hic_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
