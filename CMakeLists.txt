cmake_minimum_required(VERSION 3.16.3)

project("vkmini"
	VERSION "1.0.0"
	HOMEPAGE_URL "https://github.com/aldrinmathew/vkmini")

add_library(${PROJECT_NAME} vkmini.cc)
target_link_libraries(${PROJECT_NAME} PUBLIC vulkan)