cmake_minimum_required(VERSION 3.10)
project(tracking_solution LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CLI11 2.1.2 REQUIRED)

add_executable(tracking-solution
    src/main.cpp
    src/Tracker.cpp
)
target_include_directories(tracking-solution PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(tracking-solution PRIVATE ${OpenCV_LIBS} nlohmann_json::nlohmann_json CLI11::CLI11)
install(TARGETS tracking-solution DESTINATION /usr/local/bin)
