cmake_minimum_required(VERSION 3.20)
project(vinfony)

set(EXT_PATH "../ext" CACHE PATH "Ext folder")
if (IS_ABSOLUTE ${EXT_PATH})
  file(RELATIVE_PATH EXT_PATH ${PROJECT_SOURCE_DIR} ${EXT_PATH})
endif()
message(STATUS "Relative EXT_PATH = ${EXT_PATH}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(VENDOR_DIR kosongg/cpp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_APPLE_BUNDLE "Build Apple Bundle" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")

if (APPLE)
  ## update the environment with VIRTUAL_ENV variable (mimic the activate script)
  set (ENV{VIRTUAL_ENV} "${CMAKE_SOURCE_DIR}/venv")
  ## change the context of the search
  set (Python3_FIND_VIRTUALENV FIRST)
  ## unset Python3_EXECUTABLE because it is also an input variable (see documentation, Artifacts Specification section)
  unset (Python3_EXECUTABLE)
endif()
find_package(OpenGL REQUIRED)
set(SDL_TEST OFF)
set(SDL2_DISABLE_SDL2MAIN ON)
add_subdirectory("${EXT_PATH}/sdl2" "ext/sdl2")
add_subdirectory("kosongg/cpp/imgui")
add_subdirectory("${EXT_PATH}/glad2/cmake" glad_cmake)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
add_subdirectory("${EXT_PATH}/fmt" "ext/fmt")
add_subdirectory("${EXT_PATH}/jdksmidi" "ext/jdksmidi")
add_subdirectory("${EXT_PATH}/tsf" "ext/tsf")
add_subdirectory("${EXT_PATH}/libassert" "ext/libassert")
set(HSCPP_BUILD_EXAMPLES OFF)
set(HSCPP_BUILD_TESTS OFF)
add_subdirectory("${EXT_PATH}/hscpp" "ext/hscpp")
set(ifd_BUILD_EXAMPLES OFF)
add_subdirectory("${EXT_PATH}/ifd" "ext/ifd")

if (IS_ABSOLUTE ${IMGUI_DIR})
  file(RELATIVE_PATH IMGUI_DIR ${PROJECT_SOURCE_DIR} ${IMGUI_DIR})
endif()
message(STATUS "Relative IMGUI_DIR = ${IMGUI_DIR}")

include_directories(SYSTEM
  ${OPENGL_INCLUDE_DIR}
  ${fmt_SOURCE_DIR}/include
)

set(SOURCE
  ${VENDOR_DIR}/main.cpp
  ${VENDOR_DIR}/Engine.cpp
  ${VENDOR_DIR}/GLUtil.cpp
  ${VENDOR_DIR}/UtfConv.c
  ${VENDOR_DIR}/imkosongg/ImKosongg.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
## -- sources --
  src/MainApp.cpp
  src/UI/DawMainProject.cpp
  src/DawSeq.cpp
  src/DawSeqNames.cpp
  src/TsfDev.cpp
  src/Misc.cpp
  src/BaseMidiOut.cpp
  src/BassMidiDev.cpp
  src/DawTrack.cpp
  src/DawTrackNotes.cpp
  src/DawSysEx.cpp
  src/DawDoc.cpp
  src/UI/DawSoundFont.cpp
  src/UI/PianoButton.cpp
  src/UI/Splitter.cpp
  src/UI/MainWidget.cpp
  src/UI/DawPianoRoll.cpp
## -- endsources --
)

set(HEADERS
  ${VENDOR_DIR}/kosongg/Engine.h
  ${VENDOR_DIR}/kosongg/GLUtil.h
  ${VENDOR_DIR}/kosongg/UtfConv.h
  ${VENDOR_DIR}/imkosongg/ImKosongg.hpp
## -- headers --
  src/MainApp.hpp
  src/Globals.hpp
  src/UI/DawMainProject.hpp
  src/DawSeq.hpp
  src/Misc.hpp
  src/BaseMidiOut.hpp
  src/TsfDev.hpp
  src/BassMidiDev.hpp
  src/DawTrack.hpp
  src/DawTrackNotes.hpp
  src/DawSysEx.hpp
  src/DawDoc.hpp
  src/UI/DawSoundFont.hpp
  src/UI/PianoButton.hpp
  src/UI/UI.hpp
  src/UI/MainWidget.hpp
  src/UI/DawPianoRoll.hpp
## -- endheaders --
)

include_directories(
  ${VENDOR_DIR}
  src
)

if (BUILD_APPLE_BUNDLE)
set(RESOURCE_FILES "")
set(ICON_NAME "vinfony.icns")
set(ICON_PATH ${PROJECT_SOURCE_DIR}/${ICON_NAME})
set_source_files_properties(${PROJECT_SOURCE_DIR}/${ICON_NAME} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties(${PROJECT_SOURCE_DIR}/kosongg/fonts/selawkl.ttf PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
set_source_files_properties(${PROJECT_SOURCE_DIR}/kosongg/fonts/fa-solid-900.ttf PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")

list(APPEND SOURCE
  ${ICON_PATH}
  ${PROJECT_SOURCE_DIR}/kosongg/fonts/selawkl.ttf
  ${PROJECT_SOURCE_DIR}/kosongg/fonts/fa-solid-900.ttf
  ${VENDOR_DIR}/Utils.mm
)
add_executable(vinfony MACOSX_BUNDLE ${SOURCE} ${HEADERS})
set_target_properties(vinfony PROPERTIES
  MACOSX_BUNDLE_NAME                             "vinfony"
  MACOSX_BUNDLE_GUI_IDENTIFIER                   "org.firodj.vinfony"
  MACOSX_BUNDLE_ICON_FILE                        ${ICON_NAME}
  MACOSX_BUNDLE_INFO_PLIST                       "${CMAKE_SOURCE_DIR}/Info.plist.in"
  MACOSX_BUNDLE                                  "TRUE"
)
list(APPEND _COMPILE_DEFINITIONS
  BUILD_APPLE_BUNDLE
)
else()
add_executable(vinfony
  ${SOURCE} ${HEADERS}
)
endif()

foreach(_source IN ITEMS ${SOURCE})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
    source_group("${_source_path_msvc}" FILES "${_source}")
endforeach()

foreach(_source IN ITEMS ${HEADERS})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
    source_group("${_source_path_msvc}" FILES "${_source}")
endforeach()

if (MSVC)

  # Silence MSVC CRT security warnings
  list(APPEND _COMPILE_DEFINITIONS
    _CRT_SECURE_NO_WARNINGS

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

list(APPEND _COMPILE_DEFINITIONS
  _PROJECT_SRC_PATH_="${CMAKE_SOURCE_DIR}"
  SDL_MAIN_HANDLED
  _USE_HSCPP_
  _USE_IFD_
)

add_compile_definitions(${_COMPILE_DEFINITIONS})

target_link_libraries(vinfony
  SDL2
  ${OPENGL_LIBRARIES}
  imgui
  glad_gl_core_33
  fmt
  jdksmidi
  libassert::assert
  hscpp
  hscpp-mem
  ifd
)

if (APPLE)
  # Cocoa
  find_library(COCOA_LIBRARY Cocoa)
  find_library(APPKIT_LIB AppKit)
  target_link_libraries(vinfony ${COCOA_LIBRARY} ${APPKIT_LIB})
endif()

if (BUILD_APPLE_BUNDLE)

else()
set_target_properties(vinfony PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
  DEBUG_POSTFIX "d"
)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vinfony)

## -- others --

# -- bass --
message(STATUS "CMAKE_SIZEOF_VOID_P = ${CMAKE_SIZEOF_VOID_P}")
set(BASSMIDI_MIRROR "${PROJECT_SOURCE_DIR}/bassmidi-mirror")

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/win32/x64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/win32/x32")
  endif()
  
elseif(APPLE)
  set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/osx")
else()
  set(BASS_LIBRARY_DIR "${BASSMIDI_MIRROR}/bass24/linux/${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "BASS_LIBRARY_DIR = ${BASS_LIBRARY_DIR}" )

add_executable(test_model
  examples/test_model.cpp
  src/TsfDev.cpp
  src/BaseMidiOut.cpp
  src/DawTrackNotes.cpp
  src/DawSeq.cpp
  src/DawSysEx.cpp
  src/DawDoc.cpp
  src/DawTrack.cpp
  src/Misc.cpp
  )
target_link_libraries(test_model jdksmidi tsf fmt libassert::assert SDL2)

add_executable(test_sequencer examples/test_sequencer.cpp)
target_include_directories(test_sequencer PRIVATE "${BASSMIDI_MIRROR}/bass24/include")
target_link_directories(test_sequencer PRIVATE "${BASS_LIBRARY_DIR}")
target_link_libraries(test_sequencer jdksmidi rtmidi bass bassmidi SDL2 tsf)
target_compile_definitions(test_sequencer PUBLIC
  -DUSE_BASS -DUSE_TSF
)

add_executable(test_tsf1 "${PROJECT_SOURCE_DIR}/examples/tsf_example1.cpp")
target_compile_definitions(test_tsf1 PRIVATE ${_COMPILE_DEFINITIONS})
target_link_libraries(test_tsf1 SDL2 tsf)

add_executable(test_tsf2 "${PROJECT_SOURCE_DIR}/examples/tsf_example2.cpp")
target_compile_definitions(test_tsf2 PRIVATE ${_COMPILE_DEFINITIONS})
target_link_libraries(test_tsf2 SDL2 tsf)

add_executable(test_tsf3 "${PROJECT_SOURCE_DIR}/examples/tsf_example3.cpp")
target_link_libraries(test_tsf3 SDL2 tsf)

# -- vinvonfy : bass --
target_include_directories(vinfony PRIVATE "${BASSMIDI_MIRROR}/bass24/include")
target_link_directories(vinfony PRIVATE "${BASS_LIBRARY_DIR}")
target_link_libraries(vinfony tsf bass bassmidi)
if(WIN32)
  add_custom_command(TARGET vinfony POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${BASS_LIBRARY_DIR}/bass.dll"
      $<TARGET_FILE_DIR:vinfony>)
  add_custom_command(TARGET vinfony POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${BASS_LIBRARY_DIR}/bassmidi.dll"
      $<TARGET_FILE_DIR:vinfony>)
endif()

## -- endothers --