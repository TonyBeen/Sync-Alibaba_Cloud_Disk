cmake_minimum_required(VERSION 3.0)

project(sync_ali_cloud VERSION 1.0.0)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(INC_PATH /usr/local/include/)
set(SO_PATH /usr/local/lib/)

option(BUILD_EXAMPLE "Build example" ON)

add_subdirectory(inotify_tool)
# add_subdirectory(src)

if (BUILD_EXAMPLE)
    add_subdirectory(examples)
endif()