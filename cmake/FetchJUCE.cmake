# Provide the juce:: targets. Prefer a vendored checkout in external/JUCE
# (git submodule or manual clone); otherwise fetch the pinned release via git.
set(HIC_JUCE_DIR "${CMAKE_SOURCE_DIR}/external/JUCE")
if(EXISTS "${HIC_JUCE_DIR}/CMakeLists.txt")
  message(STATUS "Hic: using JUCE from ${HIC_JUCE_DIR}")
  add_subdirectory("${HIC_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
  message(STATUS "Hic: fetching JUCE 8.0.9 (set external/JUCE to skip)")
  FetchContent_MakeAvailable(JUCE)
endif()
