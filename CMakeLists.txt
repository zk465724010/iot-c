cmake_minimum_required (VERSION 3.3.2)
include(root.cmake)
get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(APPEND DIR_LIST_NAME "${CURRENT_DIR_NAME}")
project(${DIR_LIST_NAME})

set(COMPILE_TEST_TOOL OFF)
set(COMPILE_EXE OFF)

if(NOT CMAKE_BUILD_TYPE) 
      set(CMAKE_BUILD_TYPE Debug CACHE STRING "build type: Debug | Release" FORCE)  
endif()
if(MSVC)
    # [C][Debug&Release]
    set(CMAKE_C_FLAGS "$ENV{CMAKE_C_FLAGS}")
     # [C++][Debug] 
    set(CMAKE_C_FLAGS_DEBUG "$ENV{CMAKE_C_FLAGS} -g")
     # [C++][Release] 
    set(CMAKE_C_FLAGS_RELEASE "$ENV{CMAKE_C_FLAGS} -O3 -DNDEBUG")
     # [C++][MinSizeRel] 
    set(CMAKE_C_FLAGS_MINSIZEREL "$ENV{CMAKE_C_FLAGS} -Os -DNDEBUG")
     # [C++][RelWithDebInfo] 
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "$ENV{CMAKE_C_FLAGS} -O2 -g -DNDEBUG")
    ########################################################################
    # [C++][Debug&Release]
    set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS}")

    # [C++][Debug]['属性'->'C/C++']['/Wall'为启用所有警告]
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} /EHa /MDd /Od /ZI /D_DEBUG")
    # [C++][Debug]['属性'->'连接器'] /SUBSYSTEM:WINDOWS /SUBSYSTEM:CONSOLE
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "$ENV{CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    # [C++][Release]['属性'->'连接器']
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "$ENV{CMAKE_EXE_LINKER_FLAGS_RELEASE} /DNDEBUG")
    #add_definitions(/Zi)    # '属性'->'C/C++'->'常规'->'调试信息格式'->'程序数据库(/Zi)'
    #add_definitions(/Od)    # '属性'->'C/C++'->'优化'->'优化'->'禁用(/Od)'
    #set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:CONSOLE")

    # [C++][Release]
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} /EHsc /MT /O2 /Zi /DNDEBUG")
    # [C++][MinSizeRel]
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /MD /O1 /Zi /DNDEBUG /DMIN_SIZE_REL")
    # [C++][RelWithDebInfo]
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /EHsc /Od /ZI /DNDEBUG") 
else()
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -Wall -D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-O1  -DNDEBUG -DMIN_SIZE_REL")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 -ggdb -DNDEBUG")
endif()
message(STATUS "CXX_FLAGS: '${CMAKE_CXX_FLAGS}'")
message(STATUS "CXX_FLAGS_DEBUG: '${CMAKE_CXX_FLAGS_DEBUG}'")
message(STATUS "CXX_FLAGS_RELEASE: '${CMAKE_CXX_FLAGS_RELEASE}'")
message(STATUS "EXE_LINKER_FLAGS_DEBUG: '${CMAKE_EXE_LINKER_FLAGS_DEBUG}'")
message(STATUS "EXE_LINKER_FLAGS_RELEASE: '${CMAKE_EXE_LINKER_FLAGS_RELEASE}'")
if(CMAKE_CL_64)
    set(platform x64)
    ## 可执行程序的输出目录(在add_executable函数之前设置)
    #set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/x64/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x64/bin/Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x64/bin/Release)
    ## 静态库文件的输出目录
    #set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/x64/static)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x64/static/Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x64/static/Release)
    ## 动态库库文件的输出目录
    #set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/x64/lib)
    #set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/x64/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x64/lib/Debug)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x64/lib/Release)
else(CMAKE_CL_64)
    set(platform x86)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x86/bin/Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x86/bin/Release)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x86/static/Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x86/static/Release)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/x86/lib/Debug)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/x86/lib/Release)
endif(CMAKE_CL_64)

add_subdirectory(src)

#[[
    cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=C:/Users/zhangkun/Desktop
    cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/home/work
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=C:/Users/zhangkun/Desktop
    cmake .. -G "MinGW Makefiles"
    cmake .. -DCMAKE_BUILD_TYPE:STRING=Debug
]]
