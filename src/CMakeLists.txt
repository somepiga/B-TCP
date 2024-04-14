add_subdirectory(buffer)
add_subdirectory(connect)
add_subdirectory(datagram)
add_subdirectory(polling)
add_subdirectory(tun)
add_subdirectory(utils)

add_library(B-TCP STATIC ${ALL_OBJECT_FILES})

set(BUSTUB_LIBS
        B-TCP_buffer
        B-TCP_connect
        B-TCP_datagram
        B-TCP_polling
        B-TCP_tun
        B-TCP_utils)

target_link_libraries(
        B-TCP
        ${BUSTUB_LIBS})

target_include_directories(
        B-TCP PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
