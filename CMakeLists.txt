cmake_minimum_required(VERSION 2.6)
project(visualization.pictureit CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

# Find and include kodi
find_package(kodi REQUIRED)
include_directories(${KODI_INCLUDE_DIR})

# Find and include PictureIt
find_package(PictureIt REQUIRED)
include_directories(${PICTUREIT_INCLUDE_DIR} ${PICTUREIT_INCLUDE_DIRS})
list(APPEND DEPLIBS ${PICTUREIT_LIBRARY} ${PICTUREIT_LIBRARIES})

set(ADDON_SOURCES src/pictureit_addon.cpp)

build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
