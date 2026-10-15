# UltimateCraft - CMake build (CMake 3.10 .. 4.x).
#
# Windows (MSYS2 UCRT64, recommended):
#   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
#   ultimatecraft.exe            (the executable is placed in the project folder next to res/)
#
# Linux:  sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
#         cmake -S . -B build && cmake --build build -j && ./ultimatecraft
#
# GLFW is compiled directly from lib/glfw/src, so its own (old) CMake scripts are never used -
# this avoids the "Compatibility with CMake < 3.5 has been removed" error that broke Craft.
cmake_minimum_required(VERSION 3.10...3.31)
project(UltimateCraft C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)          # gnu11
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------- GLFW (static, from source)
set(GLFW_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/src)
set(GLFW_SOURCES context.c init.c input.c monitor.c vulkan.c window.c egl_context.c osmesa_context.c)
if(WIN32)
  list(APPEND GLFW_SOURCES win32_init.c win32_joystick.c win32_monitor.c win32_time.c win32_thread.c win32_window.c wgl_context.c)
  set(GLFW_PLATFORM_DEF _GLFW_WIN32)
elseif(APPLE)
  enable_language(OBJC)
  list(APPEND GLFW_SOURCES cocoa_init.m cocoa_joystick.m cocoa_monitor.m cocoa_window.m cocoa_time.c posix_thread.c nsgl_context.m)
  set(GLFW_PLATFORM_DEF _GLFW_COCOA)
else()
  list(APPEND GLFW_SOURCES x11_init.c x11_monitor.c x11_window.c xkb_unicode.c posix_time.c posix_thread.c glx_context.c linux_joystick.c)
  set(GLFW_PLATFORM_DEF _GLFW_X11)
endif()
list(TRANSFORM GLFW_SOURCES PREPEND ${GLFW_SRC_DIR}/)
add_library(uc_glfw STATIC ${GLFW_SOURCES})
target_include_directories(uc_glfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/include)
target_compile_definitions(uc_glfw PRIVATE ${GLFW_PLATFORM_DEF} $<$<NOT:$<BOOL:${WIN32}>>:_DEFAULT_SOURCE>)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(uc_glfw PRIVATE -w)
endif()
if(WIN32)
  target_link_libraries(uc_glfw PUBLIC gdi32)
elseif(APPLE)
  target_link_libraries(uc_glfw PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation" "-framework CoreVideo")
else()
  find_package(X11 REQUIRED)
  target_include_directories(uc_glfw PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(uc_glfw PUBLIC ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------- small vendored libraries
add_library(uc_glad STATIC lib/glad/src/glad.c)
target_include_directories(uc_glad PUBLIC lib/glad/include)
add_library(uc_noise STATIC lib/noise/noise1234.c)
add_library(uc_vorbis STATIC lib/stb/stb_vorbis.c)   # Ogg Vorbis decoder (public domain)
target_include_directories(uc_noise PUBLIC lib/noise)

# ---------------------------------------------------------------- the game
file(GLOB UC_SOURCES CONFIGURE_DEPENDS src/*.c src/*/*.c)
add_executable(ultimatecraft ${UC_SOURCES})
target_include_directories(ultimatecraft PRIVATE lib/stb)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(ultimatecraft PRIVATE uc_glfw uc_glad uc_noise uc_vorbis Threads::Threads)
if(WIN32)
  target_link_libraries(ultimatecraft PRIVATE winmm)
  if(MINGW)
    # self-contained .exe: no libgcc / libwinpthread DLLs needed next to it
    target_link_options(ultimatecraft PRIVATE -static -static-libgcc)
  endif()
else()
  target_link_libraries(ultimatecraft PRIVATE m ${CMAKE_DL_LIBS})
endif()
if(APPLE)
  target_link_libraries(ultimatecraft PRIVATE "-framework OpenGL")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultimatecraft PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-truncation)
endif()
# put the executable next to res/ so it finds its assets when double-clicked
set_target_properties(ultimatecraft PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR})
