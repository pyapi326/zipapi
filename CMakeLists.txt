cmake_minimum_required(VERSION 3.16)
project(zip LANGUAGES C CXX)

# 添加构建选项：ZLIB_STATIC
option(ZLIB_STATIC "Link zlib statically" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置zlib安装路径
set(ZLIB_ROOT "F:/zlib-install" CACHE PATH "Path to zlib installation")
set(CMAKE_PREFIX_PATH "${ZLIB_ROOT}")

# 允许用户显式指定库文件
set(ZLIB_STATIC_LIB "${ZLIB_ROOT}/lib/libzlib.a" CACHE FILEPATH "Path to zlib static library")
set(ZLIB_IMPORT_LIB "${ZLIB_ROOT}/lib/libzlib.dll.a" CACHE FILEPATH "Path to zlib import library")
set(ZLIB_DLL_PATH "${ZLIB_ROOT}/bin/libzlib.dll" CACHE FILEPATH "Path to zlib DLL")

# 查找zlib
if(ZLIB_STATIC)
    # 静态链接：使用指定的静态库
    if(NOT EXISTS "${ZLIB_STATIC_LIB}")
        message(FATAL_ERROR "Static zlib library not found at: ${ZLIB_STATIC_LIB}")
    endif()
    
    # 创建静态库目标
    add_library(zlib_static STATIC IMPORTED)
    set_target_properties(zlib_static PROPERTIES
        IMPORTED_LOCATION "${ZLIB_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_ROOT}/include"
    )
    set(ZLIB_TARGET zlib_static)
else()
    # 动态链接：使用指定的导入库
    if(NOT EXISTS "${ZLIB_IMPORT_LIB}")
        message(FATAL_ERROR "zlib import library not found at: ${ZLIB_IMPORT_LIB}")
    endif()
    
    # 创建动态库目标
    add_library(zlib_shared SHARED IMPORTED)
    set_target_properties(zlib_shared PROPERTIES
        IMPORTED_IMPLIB "${ZLIB_IMPORT_LIB}"
        IMPORTED_LOCATION "${ZLIB_DLL_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_ROOT}/include"
    )
    set(ZLIB_TARGET zlib_shared)
endif()

# 添加共享库
add_library(zip SHARED
  zip.cpp
  zip.h
)

# 链接zlib
target_link_libraries(zip PRIVATE ${ZLIB_TARGET})

# 设置包含目录
target_include_directories(zip PRIVATE
    "${ZLIB_ROOT}/include"
)

# 设置安装规则
install(TARGETS zip
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES zip.h DESTINATION include)

# Windows平台动态链接时的DLL复制
if(WIN32 AND NOT ZLIB_STATIC)
    if(EXISTS "${ZLIB_DLL_PATH}")
        # 创建自定义目标用于复制DLL
        add_custom_command(TARGET zip POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZLIB_DLL_PATH}"
                "$<TARGET_FILE_DIR:zip>"
            COMMENT "Copying libzlib.dll to output directory"
        )
        
        # 安装时也复制DLL
        install(FILES ${ZLIB_DLL_PATH} DESTINATION bin)
    else()
        message(WARNING "Could not find libzlib.dll at ${ZLIB_DLL_PATH}")
    endif()
endif()