cmake_minimum_required (VERSION 3.11)

add_flags("-Wno-unused-parameter")

add_library(${PROJECT_NAME}_core OBJECT)
target_sources(${PROJECT_NAME}_core PRIVATE
    homeblks_impl.cpp
    volume_mgr.cpp
    listener.cpp
    index.cpp
    common.cpp
)
target_link_libraries(${PROJECT_NAME}_core
    ${COMMON_DEPS}
)

#add_subdirectory(homestore_backend)
#add_subdirectory(memory_backend)
add_subdirectory(volume)
add_subdirectory(tests)
