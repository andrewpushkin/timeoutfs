cmake_minimum_required(VERSION 3.10)
project(timeoutfs CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(timeoutfs timeoutfs.cpp)
target_include_directories(timeoutfs PRIVATE ${FUSE3_INCLUDE_DIRS})
target_link_libraries(timeoutfs ${FUSE3_LIBRARIES} pthread)
set_target_properties(timeoutfs PROPERTIES CXX_STANDARD 17)

# Add test executable
add_executable(timeoutfs_test test/timeoutfs_test.cpp)
target_link_libraries(timeoutfs_test GTest::gtest_main ${FUSE3_LIBRARIES} pthread)
target_include_directories(timeoutfs_test PRIVATE ${FUSE3_INCLUDE_DIRS})

include(GoogleTest)
gtest_discover_tests(timeoutfs_test)