
project(thread)

if(WIN32)
    include_directories(${ROOT}/src ${ROOT}/src/common ${ROOT}/src/log)
elseif(APPLE)
elseif(UNIX)
    include_directories(${ROOT}/src ${ROOT}/src/common ${ROOT}/src/log)
endif()

add_definitions(-DUSE_DEBUG_OUTPUT)
if(WIN32)
    ADD_DEFINITIONS( /Zi )
    ADD_DEFINITIONS( /Od )
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
endif()

set(SOURCE_FILE main.cpp main.h thread11.cpp thread11.h 
    ${ROOT}/src/common/common_func.cpp ${ROOT}/src/common/common_func.h 
    ${ROOT}/src/log/log.h)

add_executable(${PROJECT_NAME} ${SOURCE_FILE})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
if(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

