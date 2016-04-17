cmake_minimum_required(VERSION 3.0)
project(visualization.pictureit CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

# Find and include kodi
find_package(kodi REQUIRED)

# Find and include PictureIt
find_package(PictureIt REQUIRED)
find_package(asplib REQUIRED SpectrumVisProcessor FFT)

include_directories(${KODI_INCLUDE_DIR})

set(ADDON_SOURCES src/pictureit_addon.cpp)
set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(DEPLIBS PI::PictureIt ${asplib_LIBRARIES})
build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
