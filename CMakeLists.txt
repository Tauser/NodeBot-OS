cmake_minimum_required(VERSION 3.22)

# Extra component search paths (one level deep each)
set(EXTRA_COMPONENT_DIRS src/drivers src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(NodeBot)
