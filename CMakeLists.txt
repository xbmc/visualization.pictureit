cmake_minimum_required(VERSION 2.6)
project(visualization.pictureit)

enable_language(CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
list(APPEND CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR}/depends/common)

# Find and include KODI
find_package(kodi REQUIRED)
include_directories(${KODI_INCLUDE_DIR})


# Find and include PictureIt
find_package(PictureIt REQUIRED)
include_directories(${PICTUREIT_INCLUDE_DIRS})
list(APPEND DEPLIBS ${PICTUREIT_LIB_DIR})


set(ADDON_SOURCES ${PROJECT_SOURCE_DIR}/src/pictureit_addon.cpp)


if(APPLE)
    list(APPEND DEPLIBS "-framework CoreVideo")
endif(APPLE)

build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
