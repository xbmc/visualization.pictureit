cmake_minimum_required(VERSION 2.6)
project(visualization.pictureit)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(kodi REQUIRED)

# Include PictureIt
list(APPEND CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR}/depends/common)
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/common/PictureIt)
find_package(PictureIt REQUIRED)
if(PictureIt_FOUND)
    list(APPEND INCLUDES ${PICTUREIT_INCLUDE_DIRS})
    list(APPEND DEPLIBS PictureIt)
else(PictureIt_FOUND)
    message(FATAL_ERROR "PictureIt not found!")
endif(PictureIt_FOUND)

include_directories(${INCLUDES})
include_directories(${KODI_INCLUDE_DIR})

set(ADDON_SOURCES ${PROJECT_SOURCE_DIR}/src/pictureit_addon.cpp)

if(APPLE)
    list(APPEND DEPLIBS "-framework CoreVideo")
endif(APPLE)

build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
