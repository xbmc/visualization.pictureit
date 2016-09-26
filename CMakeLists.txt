project(visualization.pictureit)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

if(NOT USE_OPENGLES)
  find_package(OpenGL)
else()
  find_path(OPENGLES_INCLUDE_DIR GLES/gl.h)
  find_library(OPENGLES_LIBRARIES NAMES GLESv2)
  if(OPENGLES_INCLUDE_DIR AND OPENGLES_LIBRARIES)
    set(OPENGLES_FOUND 1)
  endif()
endif()

if(OPENGL_FOUND)
  set(INCLUDES ${OpenGL_INCLUDE_DIR})
  add_definitions(-DHAS_OPENGL -DHAS_GL)
  set(DEPLIBS ${OPENGL_LIBRARIES})
elseif(OPENGLES_FOUND)
  find_path(OPENGLES_INCLUDE_DIR GLES/gl.h)
  find_library(OPENGLES_LIBRARIES NAMES GLESv2)
  add_definitions(-DHAS_OPENGLES -DHAS_GLES)
  set(INCLUDES ${OPENGLES_INCLUDE_DIR})
  set(DEPLIBS ${OPENGLES_LIBRARIES})
else()
  message(FATAL_ERROR "No rendering API found. Bailing")
endif()

include_directories(${INCLUDES}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/lib/kissfft
                    ${CMAKE_INCLUDE_PATH})

add_subdirectory(lib/kissfft)

set(PICTUREIT_SOURCES src/mrfft.cpp
                      src/pictureit.cpp)

list(APPEND DEPLIBS kissfft)

if(APPLE)
  set(DEPLIBS ${DEPLIBS} "-framework CoreVideo")
endif()

add_options(CXX ALL_BUILDS -std=c++11)

build_addon(visualization.pictureit PICTUREIT DEPLIBS)

include(CPack)
