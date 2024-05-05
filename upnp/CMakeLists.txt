project(upnpclient)

# 获取目录下所有源文件
file(GLOB_RECURSE SOURCE_FILES *.c *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h *.hpp)

add_library(upnpclient STATIC ${SOURCE_FILES} ${HEADER_FILES})
add_library(eular::upnpclient ALIAS upnpclient)

target_include_directories(upnpclient PUBLIC
                            ${ROOT_PATH}
                            ${INC_PATH}
                            ${MINIUPNPC_INCLUDE_DIR})

target_compile_definitions(upnpclient PRIVATE NDEBUG)

target_link_libraries(upnpclient PRIVATE miniupnpc::miniupnpc ${SO_PATH}/libutils.so ${SO_PATH}/liblog.so)