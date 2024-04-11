add_library(
        B-TCP_connect
        OBJECT
        b_socket.cpp
        base_socket.cpp
        reassembler.cpp
        tcp_endpoint.cpp
        transceiver.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_connect>
        PARENT_SCOPE)