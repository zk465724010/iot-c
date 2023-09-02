
#[[
if(NOT EXISTS ${FFMPEG_PATH})
    message(FATAL_ERROR "Directory does not exist '${FFMPEG_PATH}'")
endif()
if(NOT EXISTS ${OPENCV_PATH})
    message(FATAL_ERROR "Directory does not exist '${OPENCV_PATH}'")
endif()
]]
add_subdirectory(src)