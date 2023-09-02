
project(gb28181client)
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

include_directories(${PARENT} ${COMMON_PATH} ${TINYXML_PATH})
include_directories(${JDK_HOME}/include ${FFMPEG_PATH}/include)
if(WIN32)
    include_directories(${JDK_HOME}/include/win32)
elseif(APPLE)
elseif(UNIX)
    include_directories(${JDK_HOME}/include/linux)
endif()
link_directories(${FFMPEG_PATH}/lib)
    
set(SOURCE_FILE gb28181client_api.cpp gb28181client_api.h
    ${COMMON_PATH}/udp_client.cpp ${COMMON_PATH}/udp_client.h ${COMMON_PATH}/log.h
    ${COMMON_PATH}/Thread11.cpp ${COMMON_PATH}/Thread11.h
    ${PARENT}/media.cpp ${PARENT}/media.h
    ${PARENT}/av_stream.h ${PARENT}/av_stream.cpp
    ${PARENT}/h264.cpp ${PARENT}/h264.h
    ${PARENT}/common_jni.h ${PARENT}/common.h
    )

if(COMPILE_EXE)
    set(SOURCE_FILE
    main.cpp main.h 
    ${TINYXML_PATH}/tinyxml.cpp 
    ${TINYXML_PATH}/tinystr.cpp
    ${TINYXML_PATH}/tinyxmlerror.cpp
    ${TINYXML_PATH}/tinyxmlparser.cpp
    ${SOURCE_FILE}
    )
    add_definitions(-DUSE_COMPILE_EXE)
    add_executable(${PROJECT_NAME} ${SOURCE_FILE})
else()
    set(SOURCE_FILE com_hirain_sdk_gb28181_JNIGB28181Client.cpp com_hirain_sdk_gb28181_JNIGB28181Client.h ${SOURCE_FILE})
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
target_link_libraries(${PROJECT_NAME} avformat avutil avcodec swscale)

if(UNIX)
elseif(APPLE)
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

#install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION gb28181)