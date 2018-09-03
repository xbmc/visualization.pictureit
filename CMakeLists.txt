cmake_minimum_required(VERSION 3.1)

project(visualization.pictureit LANGUAGES CXX VERSION 0.1.6)
set(CMAKE_BUILD_TYPE Debug)

if(UNIX)
    add_definitions(-DTARGET_LINUX)
else()
    message(FATAL_ERROR "This plattform is NOT supported!")
endif()


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

set(CMAKE_POSITION_INDEPENDENT_CODE 1)

# Make sure we use C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Find and include kodi
find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${p8-platform_INCLUDE_DIRS})


# Find and include OpenGL
find_package(OpenGL)
if(OPENGL_FOUND)
  include_directories(${OpenGL_INCLUDE_DIR})
  add_definitions(-DHAS_OPENGL -DHAS_GL)
  list(APPEND DEPLIBS ${OPENGL_LIBRARIES})
else()
  message(FATAL_ERROR "No rendering API found. Bailing")
endif()


# Add kissfft
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/kissfft)
include_directories(${PROJECT_SOURCE_DIR}/lib/kissfft)
list(APPEND DEPLIBS kissfft)

set(ADDON_SOURCES src/pictureit.cpp
                  src/mrfft.cpp)
set(ADDON_HEADERS src/mrfft.h
                  src/stb_image.h)

build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
