
project(video_quality)

include_directories(${FFMPEG_PATH}/include ${OPENCV_PATH}/include)
link_directories(${FFMPEG_PATH}/lib ${OPENCV_PATH}/lib)

set(ffmpeg_libs swresample avcodec avformat avutil swscale)
set(opencv_libs opencv_highgui opencv_core opencv_imgcodecs opencv_imgproc)

# 预处理
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

if(WIN32)
    add_library(${PROJECT_NAME} SHARED  "video_quality.cpp" )
    set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${PROJECT_NAME}")
    target_link_libraries(${PROJECT_NAME} ${ffmpeg_libs} ${opencv_libs})

    add_executable("test"   "test.cpp")
    set_target_properties("test" PROPERTIES OUTPUT_NAME "test")
    target_link_libraries("test" ${PROJECT_NAME} ${ffmpeg_libs} ${opencv_libs})

elseif(APPLE)
elseif(UNIX)
    
    add_library(${PROJECT_NAME} SHARED  "video_quality.cpp" )
    set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${PROJECT_NAME}")
    target_link_libraries(${PROJECT_NAME} ${ffmpeg_libs} ${opencv_libs})

    add_executable("test"   "test.cpp")
    set_target_properties("test" PROPERTIES OUTPUT_NAME "test")
    target_link_libraries("test" ${PROJECT_NAME} ${ffmpeg_libs} ${opencv_libs})

endif()