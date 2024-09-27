project(httpd)

# 获取目录下所有源文件
file(GLOB_RECURSE SOURCE_FILES *.c *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h *.hpp)

add_executable(httpd ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(httpd PUBLIC 
                           ${ROOT_PATH}
                           ${INC_PATH})

target_compile_definitions(httpd PRIVATE NDEBUG)

target_link_libraries(httpd PRIVATE eular::upnpclient hv utils log config)