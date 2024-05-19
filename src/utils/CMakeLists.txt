add_library(
        B-TCP_utils
        OBJECT
        stream_copy.cpp
        random.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_utils>
        PARENT_SCOPE)