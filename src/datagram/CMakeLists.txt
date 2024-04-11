add_library(
        B-TCP_datagram
        OBJECT
        address.cpp
        file_descriptor.cpp
        tcp_message.cpp
        wrapping_integers.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_datagram>
        PARENT_SCOPE)