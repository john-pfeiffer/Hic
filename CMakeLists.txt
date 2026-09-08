cmake_minimum_required(VERSION 3.22)
project(Hic VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(HIC_BUILD_TESTS  "Build DSP tests and the render tool" ON)
option(HIC_BUILD_PLUGIN "Build the JUCE plugin (needs JUCE and Linux dev libs)" OFF)
option(HIC_BUILD_CLAP   "Also build a CLAP via clap-juce-extensions" OFF)
set(HIC_MAX_SAMPLE_RATE 96000 CACHE STRING "Compile-time sizing of fixed audio buffers")

include(cmake/Warnings.cmake)
add_subdirectory(dsp)

if(HIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(HIC_BUILD_PLUGIN)
  include(cmake/FetchJUCE.cmake)
  add_subdirectory(plugin)
endif()
