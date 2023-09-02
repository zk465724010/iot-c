
project(media)

set(SOURCE_DIR ..)

include_directories(${SOURCE_DIR} ${TINYXML_PATH} ${FFMPEG_PATH}/include)
link_directories(${FFMPEG_PATH}/lib)

if(WIN32)
    add_definitions(/Zi)
    add_definitions(/Od)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")
endif()
if(COMPILE_EXE)
    add_definitions(-DUSE_COMPILE_EXE)
endif()

set(SOURCE_FILE main.cpp main.h ${SOURCE_DIR}/media.cpp ${SOURCE_DIR}/media.h ${SOURCE_DIR}/av_stream.cpp ${SOURCE_DIR}/av_stream.h 
    ${SOURCE_DIR}/h264.cpp ${SOURCE_DIR}/h264.h
    ${SOURCE_DIR}/udp_client.cpp ${SOURCE_DIR}/udp_client.h ${SOURCE_DIR}/Thread11.cpp ${SOURCE_DIR}/Thread11.h 
    ${SOURCE_DIR}/debug.h ${SOURCE_DIR}/common.h
    ${TINYXML_PATH}/tinyxml.cpp 
    ${TINYXML_PATH}/tinystr.cpp 
    ${TINYXML_PATH}/tinyxmlerror.cpp 
    ${TINYXML_PATH}/tinyxmlparser.cpp
    ${TINYXML_PATH}/tinystr.h
    ${TINYXML_PATH}/tinyxml.h
)
add_executable(${PROJECT_NAME} ${SOURCE_FILE})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

if(WIN32)
    target_link_libraries(${PROJECT_NAME} avformat avutil avcodec)
elseif(APPLE)
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} avformat avutil avcodec pthread)
endif()

