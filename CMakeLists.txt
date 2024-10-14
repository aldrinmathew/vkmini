#// clang-format off

cmake_minimum_required(VERSION 3.16.3)

project("vkmini"
	VERSION "1.0.0"
	HOMEPAGE_URL "https://github.com/aldrinmathew/vkmini.git")

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-std=c++20")
 
add_library(${PROJECT_NAME} src/vkmini.cc)
target_include_directories(${PROJECT_NAME} PUBLIC "${FREETYPE_DIR}/include" "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PUBLIC vulkan freetype)
