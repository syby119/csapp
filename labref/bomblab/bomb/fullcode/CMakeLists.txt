cmake_minimum_required(VERSION 3.10)

project(bomb VERSION 1.0)

aux_source_directory(. PROJECT_SRC)

add_executable(bomb ${PROJECT_SRC})