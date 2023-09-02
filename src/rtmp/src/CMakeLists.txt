
#get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
#set(DIR_LIST_NAME ${DIR_LIST_NAME}-${CURRENT_DIR_NAME})

project(${DIR_LIST_NAME})

set(TINYXML ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml)

include_directories(${TINYXML} ${FFMPEG_PATH}/include ${JDK_HOME}/include)
if(WIN32)
    include_directories(${JDK_HOME}/include/win32)
elseif(APPLE)
elseif(UNIX)
    include_directories(${JDK_HOME}/include/linux)
endif()

include_directories(${INCLUDE_DIR})

link_directories(${FFMPEG_PATH}/lib)

set(TINYXML_SRC ${TINYXML}/tinyxml.h ${TINYXML}/tinyxml.cpp ${TINYXML}/tinystr.h 
    ${TINYXML}/tinystr.cpp ${TINYXML}/tinyxmlerror.cpp ${TINYXML}/tinyxmlparser.cpp)

set(JNI_SRC com_hirain_sdk_rtmp_JNIRtmp.h com_hirain_sdk_rtmp_JNIRtmp.cpp common_jni.h common_jni.cpp)

set(COMPILE_EXE ON)
if(COMPILE_EXE)
    set(SOURCE_FILE main.cpp main.h rtmp_media.h rtmp_media.cpp rtmp_stream.h rtmp_stream.cpp 
        cmap.h log.h thread11.cpp thread11.h ${TINYXML_SRC})
    add_definitions(-DCOMPILE_EXE)
    add_executable(${PROJECT_NAME} ${SOURCE_FILE})
else()
    set(SOURCE_FILE main.cpp main.h rtmp_media.h rtmp_media.cpp rtmp_stream.h rtmp_stream.cpp 
    cmap.h log.h thread11.cpp thread11.h ${JNI_SRC})
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

if(WIN32)
    target_link_libraries(${PROJECT_NAME} avformat avutil avcodec)
elseif(APPLE)
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread avformat avutil avcodec)
endif()

