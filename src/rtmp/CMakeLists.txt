
get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(DIR_LIST_NAME ${DIR_LIST_NAME}-${CURRENT_DIR_NAME})

if(NOT EXISTS ${FFMPEG_PATH})
    message(FATAL_ERROR "Directory does not exist '${FFMPEG_PATH}'")
endif()
if(NOT EXISTS ${JDK_HOME})
    message(FATAL_ERROR "Directory does not exist '${JDK_HOME}'")
endif()

add_subdirectory(src)