add_library(
        B-TCP_polling
        OBJECT
        b_epoll.cpp
        eventpolling.cpp
        rule.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:B-TCP_polling>
        PARENT_SCOPE)