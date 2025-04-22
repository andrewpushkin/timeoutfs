cmake_minimum_required(VERSION 3.10)
project(timeoutfs CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(timeoutfs timeoutfs.cpp)
target_include_directories(timeoutfs PRIVATE ${FUSE3_INCLUDE_DIRS})
target_link_libraries(timeoutfs ${FUSE3_LIBRARIES} pthread)
set_target_properties(timeoutfs PROPERTIES CXX_STANDARD 17)