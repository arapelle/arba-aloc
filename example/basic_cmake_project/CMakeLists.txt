cmake_minimum_required(VERSION 3.26)

project(basic_cmake_project)

add_executable(${PROJECT_NAME} main.cpp)
# Find package arba-aloc:
find_package(arba-aloc 0.1.0 CONFIG REQUIRED)
# Use arba-aloc release shared target:
target_link_libraries(${PROJECT_NAME} PRIVATE arba-aloc)
