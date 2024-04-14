add_library(
        B-TCP_tun
        OBJECT
        tun_adapter.cpp
        tun.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_tun>
        PARENT_SCOPE)