cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Keep the build focused on main and its explicitly declared component dependencies.
idf_build_set_property(MINIMAL_BUILD ON)
project(summit_sense_watch)
