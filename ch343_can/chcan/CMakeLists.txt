cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(chcan LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp)

# https://github.com/cpm-cmake/CPM.cmake
include(CPM.cmake)

# https://github.com/chriskohlhoff/asio
CPMAddPackage("gh:chriskohlhoff/asio#asio-1-28-2@1.28.2")
find_package(Threads REQUIRED)
if(asio_ADDED)
  add_library(asio INTERFACE)
  target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
  target_link_libraries(asio INTERFACE Threads::Threads)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE 
  asio
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

