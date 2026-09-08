# Hand-rolled test harness: no external dependencies. Each test file is its
# own executable so a crash in one cannot hide the others.
add_library(hic_testlib STATIC Harness.cpp)
target_include_directories(hic_testlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hic_testlib PUBLIC hic_dsp hic_warnings)

function(hic_add_test name)
  add_executable(${name} ${name}.cpp TestMain.cpp)
  target_link_libraries(${name} PRIVATE hic_testlib)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

hic_add_test(test_primitives)
hic_add_test(test_voices)
hic_add_test(test_engine)
hic_add_test(test_fx)
hic_add_test(test_sequencer)
hic_add_test(bench_cycles)

add_executable(render_kit render_kit.cpp)
target_link_libraries(render_kit PRIVATE hic_testlib)
