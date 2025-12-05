cmake_minimum_required(VERSION 3.20)
project(StartClean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
endif()

# 使用 Unicode 版本 API
add_definitions(-DUNICODE -D_UNICODE)

# Win32 GUI 子系统
add_executable(StartClean WIN32
    src/main.cpp
    res/resources.rc
)

# 静态运行时，生成单文件 EXE
if (MSVC)
    foreach(flag_var
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE)
        if (${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# ListView 等公共控件需要链接 comctl32
target_link_libraries(StartClean PRIVATE comctl32 version)
