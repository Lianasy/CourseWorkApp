cmake_minimum_required(VERSION 3.11)
project(CourseWorkApp)

set(CMAKE_CXX_STANDARD 20)

add_executable(Server Server.cpp)
target_link_libraries(Server PRIVATE ws2_32)

add_executable(Client Client.cpp)
target_link_libraries(Client PRIVATE ws2_32)
