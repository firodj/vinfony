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
add_subdirectory(ext/fmt)
add_subdirectory(ext/libassert)
add_subdirectory(ext/jdksmidi)
add_subdirectory(ext/tsf)

include_directories(SYSTEM
  ${OPENGL_INCLUDE_DIR}
  ${SDL2_SOURCE_DIR}/include
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${fmt_SOURCE_DIR}/include
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
  src/DawSeq.cpp
  src/BaseMidiOut.cpp
  src/TsfDev.cpp
## -- endsources --
)

set(HEADERS
  ${VENDOR_DIR}/kosongg/Component.h
  ${VENDOR_DIR}/kosongg/Engine.h
  ${VENDOR_DIR}/kosongg/GLUtil.h
## -- headers --
  src/MainApp.hpp
  src/DawMain.hpp
  src/DawSeq.hpp
  src/BaseMidiOut.hpp
  src/TsfDev.hpp
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

# ImFileDialog (ifd)
list(APPEND ifd_SOURCES
  ${VENDOR_DIR}/ifd/ImFileDialog.cpp
  ${VENDOR_DIR}/ifd/ImFileDialog.hpp
)

if (APPLE)
  list(APPEND ifd_SOURCES
    ${VENDOR_DIR}/ifd/ImFileDialog_osx.mm
    ${VENDOR_DIR}/ifd/ImFileDialog_osx.hpp
  )
endif()
if (WIN32)
  list(APPEND ifd_SOURCES
    ${VENDOR_DIR}/ifd/ImFileDialog_win32.cpp
    ${VENDOR_DIR}/ifd/ImFileDialog_win32.hpp
  )
endif()

add_library(ifd STATIC ${ifd_SOURCES})

if (APPLE)
  # Cocoa
  find_library(COCOA_LIBRARY Cocoa)
  find_library(APPKIT_LIB AppKit)
  target_link_libraries(ifd PUBLIC ${COCOA_LIBRARY} ${APPKIT_LIB})
endif()

if (MSVC)

  # Silence MSVC CRT security warnings
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS

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

add_compile_definitions(_USE_
  SDL_MAIN_HANDLED
)

target_link_libraries(vinfony
  ${OPENGL_LIBRARIES}
  SDL2-static
  glad_gl_core_33
  fmt
  libassert::assert
  jdksmidi
  ifd
)

set_target_properties(vinfony PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DEBUG_POSTFIX "d"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vinfony)

## -- others --
message(STATUS "CMAKE_SIZEOF_VOID_P = ${CMAKE_SIZEOF_VOID_P}")
set(BASSMIDI_MIRROR "${PROJECT_SOURCE_DIR}/bassmidi-mirror")

add_executable(test_model examples/test_model.cpp)
target_link_libraries(test_model jdksmidi tsf fmt)

add_executable(test_sequencer examples/test_sequencer.cpp)
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/win32/x64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/win32/x32")
  endif()

  message(STATUS "BASS_LIBRARY_DIR = ${BASS_LIBRARY_DIR}" )

  add_custom_command(TARGET test_sequencer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${BASS_LIBRARY_DIR}/bass.dll"
      $<TARGET_FILE_DIR:test_sequencer>)
  add_custom_command(TARGET test_sequencer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${BASS_LIBRARY_DIR}/bassmidi.dll"
      $<TARGET_FILE_DIR:test_sequencer>)
elseif(APPLE)
  set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/osx")

endif()

target_include_directories(test_sequencer PRIVATE "${BASSMIDI_MIRROR}/bass24/include")
target_link_directories(test_sequencer PRIVATE "${BASS_LIBRARY_DIR}")
target_link_libraries(test_sequencer jdksmidi rtmidi bass bassmidi SDL2-static tsf)

target_compile_definitions(test_sequencer PUBLIC
  -DUSE_BASS -DUSE_TSF
)

add_executable(test_tsf1 "${PROJECT_SOURCE_DIR}/examples/tsf_example1.cpp")
target_link_libraries(test_tsf1 SDL2-static tsf)

add_executable(test_tsf2 "${PROJECT_SOURCE_DIR}/examples/tsf_example2.cpp")
target_link_libraries(test_tsf2 SDL2-static tsf)

add_executable(test_tsf3 "${PROJECT_SOURCE_DIR}/examples/tsf_example3.cpp")
target_link_libraries(test_tsf3 SDL2-static tsf)

target_link_libraries(vinfony tsf)
## -- endothers --