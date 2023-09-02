
get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(DIR_LIST_NAME ${CURRENT_DIR_NAME})

project(${DIR_LIST_NAME})

#set(CMAKE_Fortran_COMPILE_FLAGS ${CMAKE_Fortran_COMPILE_FLAGS} -fpp)
add_definitions(-DWITH_DOM -DWITH_OPENSSL -D_CRT_SECURE_NO_WARNINGS)

include_directories(${OPENSSL_PATH}/include ${OPENSSL_PATH}/include/openssl ${ROOT}/src/common)
include_directories(${JDK_HOME}/include ${PROJECT_SOURCE_DIR}/onvif_win64)
include_directories(${FFMPEG_PATH}/include)
if(WIN32)
    include_directories(${JDK_HOME}/include/win32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
elseif(APPLE)
elseif(UNIX)
    include_directories(${JDK_HOME}/include/linux)
endif()
link_directories(${OPENSSL_PATH}/lib ${FFMPEG_PATH}/lib)

# ONVIF框架代码
FILE(GLOB_RECURSE ONVIF_SRC ${ROOT}/src/common/log.h ${ROOT}/src/common/cmap.h ${ROOT}/src/common/Thread11.h ${ROOT}/src/common/Thread11.cpp thread_pool.h onvifSDK_com.h onvifSDK_com.cpp onvif_win64/*.h onvif_win64/*.cpp onvif_win64/wsdd.nsmap)

if(UNIX)
    # ----------------- link openssl -----------------
    add_library(ssl SHARED IMPORTED)
    add_library(crypto SHARED IMPORTED)
    set_target_properties(ssl PROPERTIES IMPORTED_LOCATION ${OPENSSL_PATH}/lib/libssl.so.1.1)
    set_target_properties(crypto PROPERTIES IMPORTED_LOCATION ${OPENSSL_PATH}/lib/libcrypto.so.1.1)
    # ----------------- link  openssl -----------------
endif()

if(COMPILE_EXE)
    set(SOURCE_FILE main.cpp main.h soap.h soap.cpp ${ONVIF_SRC})
    add_executable(${PROJECT_NAME} ${SOURCE_FILE})
else()
    set(SOURCE_FILE com_hirain_sdk_rtsp_JNIPTZ.h com_hirain_sdk_rtsp_JNIPTZ.cpp
    com_hirain_sdk_rtsp_JNIDeviceAccess.h com_hirain_sdk_rtsp_JNIDeviceAccess.cpp
    com_hirain_sdk_rtsp_JNIAudioVideoConfiguration.h com_hirain_sdk_rtsp_JNIAudioVideoConfiguration.cpp
    onvifSDK_structDefine.h ${ONVIF_SRC})
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILE})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${PROJECT_NAME}")
#target_link_libraries(${PROJECT_NAME} ssl crypto -lgthread-2.0)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} libssl libcrypto)
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} ssl crypto pthread)
endif()
target_link_libraries(${PROJECT_NAME} avcodec avformat avutil)

    