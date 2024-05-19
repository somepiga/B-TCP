add_library(minnow_testing_sanitized EXCLUDE_FROM_ALL STATIC common.cpp)  
target_compile_options(minnow_testing_sanitized PUBLIC ${SANITIZING_FLAGS}) 

add_custom_target(functionality_testing)

macro(add_test_exec exec_name)
  add_executable("${exec_name}_sanitized" EXCLUDE_FROM_ALL "${exec_name}.cpp")  
  target_compile_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})  
  target_link_libraries("${exec_name}_sanitized" minnow_testing_sanitized B-TCP)
  add_dependencies(functionality_testing "${exec_name}_sanitized") 
endmacro(add_test_exec)

add_test_exec(reassembler_dup)

