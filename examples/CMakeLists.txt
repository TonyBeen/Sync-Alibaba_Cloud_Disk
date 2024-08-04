project(examples)

# 获取目录下所有源文件
file(GLOB_RECURSE SOURCE_FILE_LIST *.cc *.cpp)

# 遍历源文件列表，为每个文件生成一个可执行程序
foreach(SOURCE_FILE ${SOURCE_FILE_LIST})
    # 获取当前源文件的文件名(不包括路径)
    get_filename_component(FILE_NAME ${SOURCE_FILE} NAME_WE)

    # 生成可执行程序，使用源文件的文件名作为目标名称
    add_executable(${FILE_NAME} ${SOURCE_FILE})

    target_include_directories(${FILE_NAME} PUBLIC 
                               ${ROOT_PATH}
                               ${INC_PATH})
    
    target_compile_definitions(${FILE_NAME} PRIVATE NDEBUG)

    # miniupnpc::miniupnpc
    target_link_libraries(${FILE_NAME} PRIVATE eular::inotify_tool eular::upnpclient utils log hv pthread)
endforeach()
