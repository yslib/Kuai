project(krt)
cmake_minimum_required(VERSION 4.0)
add_library(krt STATIC main.cpp)

install(TARGETS krt DESTINATION lib)
