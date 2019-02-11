aux_source_directory(. SRC_LIST)

file(GLOB HEADERS "*.h")

SET(CMAKE_CXX_FLAGS "-std=c++11 -O3")

add_library(sha3 ${SRC_LIST} ${HEADERS})

target_include_directories(sha3 PRIVATE ..)
target_link_libraries(sha3 Boost::Filesystem)
