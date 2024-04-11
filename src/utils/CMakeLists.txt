add_library(
        B-TCP_utils
        OBJECT
        eventloop.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_datagram>
        PARENT_SCOPE)