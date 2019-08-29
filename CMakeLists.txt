cmake_minimum_required(VERSION 3.5)
project(visualization.pictureit)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(glm REQUIRED)

if(NOT WIN32 AND (APP_RENDER_SYSTEM STREQUAL "gl" OR NOT APP_RENDER_SYSTEM))
  find_package(OpenGl REQUIRED)
  set(DEPLIBS ${OPENGL_LIBRARIES})
  set(includes ${OPENGL_INCLUDE_DIR})
  add_definitions(${OPENGL_DEFINITIONS})
else()
  find_package(OpenGLES REQUIRED)
  set(DEPLIBS ${OPENGLES_LIBRARIES})
  set(includes ${OPENGLES_INCLUDE_DIR})
  add_definitions(${OPENGLES_DEFINITIONS})
endif()

# Add kissfft
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/kissfft)

include_directories(${GLM_INCLUDE_DIR}
                    ${KODI_INCLUDE_DIR}/..
                    ${PROJECT_SOURCE_DIR}/lib/kissfft) # Hack way with "/..", need bigger Kodi cmake rework to match right include ways

list(APPEND DEPLIBS kissfft)

set(ADDON_SOURCES src/pictureit.cpp
                  src/mrfft.cpp)
set(ADDON_HEADERS src/mrfft.h
                  src/stb_image.h)

build_addon(visualization.pictureit ADDON DEPLIBS)

include(CPack)
