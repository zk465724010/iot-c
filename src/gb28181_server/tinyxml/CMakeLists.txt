
# 指定库的输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ./)

# 指定库的工程的名称
set(OUTPUT_FILE libtinyxml.a)

add_library(libtinyxml tinyxml.cpp tinyxmlerror.cpp tinyxmlparser.cpp tinystr.cpp)

# 指定库的名称 ('libtinyxml'为库的工程名称, 'math'为生成的库的名称)
set_target_properties(libtinyxml PROPERTIES OUTPUT_NAME "${OUTPUT_FILE}")