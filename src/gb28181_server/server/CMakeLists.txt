
project(gb28181server)
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

include_directories(${PARENT} ${COMMON_PATH} ${TINYXML_PATH})
include_directories(${EXOSIP_PATH}/include ${JDK_HOME}/include ${FFMPEG_PATH}/include)
if(WIN32)
    include_directories(${JDK_HOME}/include/win32)
    link_directories(${EXOSIP_PATH}/lib/x64 ${FFMPEG_PATH}/lib)
elseif(APPLE)
elseif(UNIX)
    include_directories(${JDK_HOME}/include/linux)
    link_directories(${EXOSIP_PATH}/lib ${FFMPEG_PATH}/lib)
endif()

set(SOURCE_FILE gb28181server_api.cpp gb28181server_api.h md5.cpp md5.h 
    sip.cpp sip.h ${PARENT}/common_jni.h ${PARENT}/common.h parse_xml.cpp parse_xml.h
    ${COMMON_PATH}/udp_client.cpp ${COMMON_PATH}/udp_client.h 
    ${COMMON_PATH}/Thread11.cpp ${COMMON_PATH}/Thread11.h
    ${COMMON_PATH}/log.h
    ${PARENT}/common_func.cpp ${PARENT}/common_func.h
    ${COMMON_PATH}/cmap.h
    ${TINYXML_PATH}/tinyxml.cpp 
    ${TINYXML_PATH}/tinystr.cpp
    ${TINYXML_PATH}/tinyxmlerror.cpp
    ${TINYXML_PATH}/tinyxmlparser.cpp
    ${PARENT}/media.cpp ${PARENT}/media.h
    ${PARENT}/av_stream.h ${PARENT}/av_stream.cpp
    ${PARENT}/h264.cpp ${PARENT}/h264.h
    )

if(COMPILE_EXE)
    set(SOURCE_FILE test.h test.cpp ${SOURCE_FILE})
    add_definitions(-DUSE_COMPILE_EXE)
    add_executable(${PROJECT_NAME} ${SOURCE_FILE})
else()
    set(SOURCE_FILE com_hirain_sdk_gb28181_JNIGB28181.cpp com_hirain_sdk_gb28181_JNIGB28181.h ${SOURCE_FILE})
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} eXosip2 osip2 osipparser2 avformat avutil avcodec swscale)
if(UNIX)  
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

#install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION gb28181)