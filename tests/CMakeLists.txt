add_executable(ptxscope_unit_tests unit_tests.cpp)
target_link_libraries(ptxscope_unit_tests PRIVATE ptxscope_core)
target_compile_features(ptxscope_unit_tests PRIVATE cxx_std_20)
add_test(NAME unit COMMAND ptxscope_unit_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME cli
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cli_tests.py
                $<TARGET_FILE:ptxscope> ${CMAKE_CURRENT_SOURCE_DIR}/../examples/vector_add.ptx)
endif()
