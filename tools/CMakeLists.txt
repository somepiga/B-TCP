set(BROWSER_SIMULATOR_SOURCES BrowserSimulator.cpp)
set(BTCP_SOURCES BTCP.cpp)
set(RAW_TCP_SOURCES RawTCP.cpp)
set(SPEED_TEST_SOURCES speed_test.cpp)

add_executable(BrowserSimulator ${BROWSER_SIMULATOR_SOURCES})
add_executable(BTCP ${BTCP_SOURCES})
add_executable(RawTCP ${RAW_TCP_SOURCES})
add_executable(speed_test ${SPEED_TEST_SOURCES})

target_link_libraries(BrowserSimulator B-TCP pthread)
target_link_libraries(BTCP B-TCP pthread)
target_link_libraries(RawTCP B-TCP pthread)
target_link_libraries(speed_test B-TCP pthread)

set_target_properties(BrowserSimulator PROPERTIES OUTPUT_NAME BrowserSimulator)
set_target_properties(BTCP PROPERTIES OUTPUT_NAME BTCP)
set_target_properties(RawTCP PROPERTIES OUTPUT_NAME RawTCP)
set_target_properties(speed_test PROPERTIES OUTPUT_NAME speed_test)