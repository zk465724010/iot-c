set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBS ${ROOT}/libs)
if(WIN32)
    set(OPENSSL_PATH ${LIBS}/openssl/win64)
    set(OPENCV_PATH ${LIBS}/opencv/win)
    set(EXOSIP_PATH ${LIBS}/eXosip2-4.0.0/win)
    set(FFMPEG_PATH ${LIBS}/ffmpeg/win64)
    set(JDK_HOME ${LIBS}/jdk-16.0.1/win)
    set(MINIO_PATH ${LIBS}/aws-sdk-cpp/win)
elseif(APPLE)
elseif(UNIX)
    set(OPENSSL_PATH ${LIBS}/openssl/linux)
    set(OPENCV_PATH ${LIBS}/opencv/linux)
    set(EXOSIP_PATH ${LIBS}/eXosip2-4.0.0/linux)
    set(FFMPEG_PATH ${LIBS}/ffmpeg/linux)
    #set(FFMPEG_PATH /home/zk/work/extend/ffmpeg-all)
    set(JDK_HOME ${LIBS}/jdk-16.0.1/linux)
    set(MINIO_PATH ${LIBS}/aws-sdk-cpp/linux)
endif()
if(NOT EXISTS ${JDK_HOME})
    message(FATAL_ERROR "Directory does not exist '${JDK_HOME}'")
endif()