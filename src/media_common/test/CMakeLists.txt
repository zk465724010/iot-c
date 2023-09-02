
project(test)

set(INCLUDE_DIR ../ ${FFMPEG_PATH}/include ${JDK_HEMO}/include)
if(WIN32)
    set(INCLUDE_DIR ${INCLUDE_DIR} ${JDK_HEMO}/include/win32)
elseif(APPLE)
elseif(UNIX)
    set(INCLUDE_DIR ${INCLUDE_DIR} ${JDK_HEMO}/include/linux)
endif()

include_directories(${INCLUDE_DIR})
link_directories(${FFMPEG_PATH}/lib)

set(SOURCE_FILE test.cpp ../media.cpp ../media_common.h)

# 测试工具
add_executable(${PROJECT_NAME} ${SOURCE_FILE})

target_link_libraries(${PROJECT_NAME} avcodec avformat avutil wsock32)

# 设置输出文件的名称
set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION ${PROJECT_NAME})