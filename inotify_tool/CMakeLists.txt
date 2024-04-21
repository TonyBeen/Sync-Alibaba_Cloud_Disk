project(inotify_tool)

# 获取目录下所有源文件
file(GLOB_RECURSE SOURCE_FILES *.c *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h *.hpp)

add_library(inotify_tool SHARED ${SOURCE_FILES} ${HEADER_FILES})
add_library(eular::inotify_tool ALIAS inotify_tool)

target_include_directories(inotify_tool PUBLIC 
                            ${ROOT_PATH}
                            ${INC_PATH})

target_compile_definitions(inotify_tool PRIVATE NDEBUG)

target_link_libraries(inotify_tool PRIVATE ${SO_PATH}/libutils.so ${SO_PATH}/liblog.so)