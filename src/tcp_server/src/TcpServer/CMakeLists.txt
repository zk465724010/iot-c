
project(TcpServer)

#set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
get_filename_component(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PATH)

include_directories(${BASE_DIR}/TcpClient ${BASE_DIR}/thread ${BASE_DIR}/common ${BASE_DIR}/log ${BASE_DIR}/tinyxml)

set(SOURCE_FILE main.cpp main.h tcp_server.cpp tcp_server.h 
    ${BASE_DIR}/thread/thread11.cpp ${BASE_DIR}/thread/thread11.h
    ${BASE_DIR}/TcpClient/tcp_client.h ${BASE_DIR}/TcpClient/tcp_client.cpp
    ${BASE_DIR}/common/common_func.cpp ${BASE_DIR}/common/common_func.h
    ${BASE_DIR}/log/log.h
    ${BASE_DIR}/tinyxml/tinyxml.h ${BASE_DIR}/tinyxml/tinyxml.cpp 
    ${BASE_DIR}/tinyxml/tinystr.h ${BASE_DIR}/tinyxml/tinystr.cpp 
    ${BASE_DIR}/tinyxml/tinyxmlerror.cpp ${BASE_DIR}/tinyxml/tinyxmlparser.cpp
    )

add_executable(${PROJECT_NAME} ${SOURCE_FILE})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

if(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()
