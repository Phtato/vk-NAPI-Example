# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(vkNDKExample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

add_definitions(-DVK_USE_PLATFORM_OHOS)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(entry SHARED napi_init.cpp
        include/nativeStreamBuf.h)
target_link_libraries(entry
    PUBLIC libace_napi.z.so
    PUBLIC libace_ndk.z.so
    PUBLIC libnative_window.so  # 窗口系统相关
    PUBLIC rawfile.z            # 资源文件库
    PUBLIC libhilog_ndk.z.so
)