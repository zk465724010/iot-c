
get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(DIR_LIST_NAME ${DIR_LIST_NAME}-${CURRENT_DIR_NAME})

if(NOT EXISTS ${EXOSIP_PATH})
    message(FATAL_ERROR "Directory does not exist '${EXOSIP_PATH}'")
endif()
if(NOT EXISTS ${FFMPEG_PATH})
    message(FATAL_ERROR "Directory does not exist '${FFMPEG_PATH}'")
endif()
if(NOT EXISTS ${JDK_HOME})
    message(FATAL_ERROR "Directory does not exist '${JDK_HOME}'")
endif()

set(TINYXML_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml)
set(PARENT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(test)

