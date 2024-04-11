add_library(
        B-TCP_buffer
        OBJECT
        stream_buffer.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_buffer>
        PARENT_SCOPE)