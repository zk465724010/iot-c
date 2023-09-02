
if(NOT EXISTS ${MINIO_PATH})
    message(FATAL_ERROR "Directory does not exist '${MINIO_PATH}'")
endif()
if(NOT EXISTS ${FFMPEG_PATH})
    message(FATAL_ERROR "Directory does not exist '${FFMPEG_PATH}'")
endif()
message("FFmpeg: '${FFMPEG_PATH}'")
message("MinIO: '${MINIO_PATH}'")

get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(DIR_LIST_NAME ${CURRENT_DIR_NAME})

project(${DIR_LIST_NAME})

include_directories(${FFMPEG_PATH}/include ${JDK_HOME}/include ${ROOT}/src/cmap ${ROOT}/src/gb28181_server/src)
include_directories(${ROOT}/src/log)
include_directories(${MINIO_PATH}/include ${ROOT}/src/common)
message(STATUS "MinIO_include= '${MINIO_PATH}/include'")
if(WIN32)
    include_directories(${JDK_HOME}/include)
    include_directories(${JDK_HOME}/include/win32)
    link_directories(${FFMPEG_PATH}/lib ${MINIO_PATH}/lib/x64/release)
elseif(APPLE)
elseif(UNIX)
    include_directories(${JDK_HOME}/include)
    include_directories(${JDK_HOME}/include/linux)
    link_directories(${FFMPEG_PATH}/lib ${MINIO_PATH}/lib)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()



set(SOURCE_FILE media.cpp media.h hirain_media.cpp hirain_media.h
    ${ROOT}/src/common/cmap.h ${ROOT}/src/common/minio.h ${ROOT}/src/common/minio.cpp
    ${ROOT}/src/common/Thread11.h ${ROOT}/src/common/Thread11.cpp
    ${ROOT}/src/common/log.h ${ROOT}/src/common/buffer.h)

if(COMPILE_EXE)
    set(SOURCE_FILE main.cpp main.h ${SOURCE_FILE})

    add_definitions(-DCOMPILE_EXE)
    add_executable(${PROJECT_NAME} ${SOURCE_FILE})
else()
    set(SOURCE_FILE com_hirain_sdk_rtsp_JNIDeviceStream.cpp com_hirain_sdk_rtsp_JNIDeviceStream.h
     ${SOURCE_FILE})

    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})
endif()

target_link_libraries(${PROJECT_NAME} avcodec avformat avutil)
target_link_libraries(${PROJECT_NAME} aws-cpp-sdk-s3 aws-cpp-sdk-core)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
#set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION ${PROJECT_NAME})

if(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()
    
if(COMPILE_TEST_TOOL)    
    add_subdirectory(test)
    add_subdirectory(265player)
endif()

#install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION rtsp)