# Hand-rolled test harness: no external dependencies. Each test file is its
# own executable so a crash in one cannot hide the others.
add_library(hic_testlib STATIC HarnessMain.cpp)
target_include_directories(hic_testlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hic_testlib PUBLIC hic_dsp hic_warnings)

function(hic_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE hic_testlib)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

hic_add_test(test_primitives)
