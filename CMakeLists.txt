cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)
set(MAIN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(reterminal_e1002_epaper)