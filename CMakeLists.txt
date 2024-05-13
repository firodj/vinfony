cmake_minimum_required(VERSION 3.20)
project(vinfony)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(VENDOR_DIR kosongg/cpp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
  ## update the environment with VIRTUAL_ENV variable (mimic the activate script)
  set (ENV{VIRTUAL_ENV} "${CMAKE_SOURCE_DIR}/venv")
  ## change the context of the search
  set (Python3_FIND_VIRTUALENV FIRST)
  ## unset Python3_EXECUTABLE because it is also an input variable (see documentation, Artifacts Specification section)
  unset (Python3_EXECUTABLE)
endif()
find_package(OpenGL REQUIRED)
set("SDL_TEST" OFF)
set("SDL2_DISABLE_SDL2MAIN" ON)
add_subdirectory(ext/sdl2)
set("IMGUI_DIR" "ext/imgui-docking")
set("GLAD_SOURCES_DIR" "${PROJECT_SOURCE_DIR}/ext/glad2")
add_subdirectory("${GLAD_SOURCES_DIR}/cmake" glad_cmake)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
add_subdirectory(ext/jdksmidi)

include_directories(SYSTEM
  ${OPENGL_INCLUDE_DIR}
  ${SDL2_SOURCE_DIR}/include
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

set(SOURCE
  ${VENDOR_DIR}/main.cpp
  ${VENDOR_DIR}/Engine.cpp
  ${VENDOR_DIR}/GLUtil.cpp
  ${VENDOR_DIR}/Component.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
## -- sources --
  src/MainApp.cpp
  src/DawMain.cpp
## -- endsources --
)

set(HEADERS
  ${VENDOR_DIR}/kosongg/Component.h
  ${VENDOR_DIR}/kosongg/Engine.h
  ${VENDOR_DIR}/kosongg/GLUtil.h
## -- headers --
  src/MainApp.hpp
  src/DawMain.hpp
## -- endheaders --
)

include_directories(
  ${VENDOR_DIR}
  src
)

add_executable(vinfony
  ${SOURCE} ${HEADERS}
)

source_group("Source Files" FILES ${SOURCE})
source_group("Header Files" FILES ${HEADERS})

if (MSVC)

  # Silence MSVC CRT security warnings
  target_compile_definitions(vinfony PUBLIC
    -D_CRT_SECURE_NO_WARNINGS

  )

  if (USE_ASAN)
    target_compile_options(vinfony PRIVATE /fsanitize=address)
  endif()
else()
  target_compile_options(vinfony PRIVATE -Wno-deprecated)
  if (USE_ASAN)
    target_compile_options(vinfony PRIVATE -fsanitize=address)
    target_link_options(vinfony PRIVATE -fsanitize=address)
  endif()
  if (APPLE)
    if (CMAKE_GENERATOR STREQUAL Xcode)
      set_target_properties(vinfony PROPERTIES XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    else()
      add_custom_command(
        TARGET vinfony
        POST_BUILD
        COMMAND dsymutil $<TARGET_FILE:vinfony>
      )
    endif()
  endif()
endif()

target_compile_definitions(vinfony PUBLIC -D_USE_
)

target_link_libraries(vinfony
  ${OPENGL_LIBRARIES}
  SDL2-static
  glad_gl_core_33
  jdksmidi
)

set_target_properties(vinfony PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DEBUG_POSTFIX "d"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vinfony)

## -- others --
add_executable(test_sequencer examples/test_sequencer.cpp)
target_link_libraries(test_sequencer jdksmidi)
## -- endothers --