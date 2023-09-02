
project(log)

if(WIN32)
    include_directories()
elseif(APPLE)
elseif(UNIX)
    include_directories()
endif()

add_definitions(-DUSE_DEBUG_OUTPUT)
if(WIN32)
    ADD_DEFINITIONS( /Zi )
    ADD_DEFINITIONS( /Od )
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

set(SOURCE_FILE main.cpp main.h log.h)

add_executable(${PROJECT_NAME} ${SOURCE_FILE})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")


