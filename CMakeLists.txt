cmake_minimum_required(VERSION 3.31)

# The oldest macOS supported, as in eacp and the other pond apps: one value lets them share cached dependency libraries.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)

project(iac VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJCXX)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No source this project compiles uses C++20 modules, so scanning each one for them is wasted work.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Declared ahead of emberstore so this pin, not emberstore's, decides which eacp commit iac builds.
lockstep_add_package(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG develop
        OPTIONS "EACP_BUILD_GRAPHICS OFF"
        SYSTEM YES)

lockstep_add_package(
        NAME emberstore
        GITHUB_REPOSITORY tamber-inc/emberstore
        GIT_TAG main)

# Set after the packages, so only this project's own targets build with -Werror.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

if (WIN32)
    set(platformSources Source/Platform-Windows.cpp)
else ()
    set(platformSources Source/Platform-Posix.cpp)
endif ()

add_executable(iac
        Source/Main.cpp
        Source/HelpCommand.cpp
        Source/Message.cpp
        Source/MonitorCommand.cpp
        Source/PublishCommand.cpp
        Source/ReadCommand.cpp
        Source/RemoteRoom.cpp
        Source/Room.cpp
        Source/RoomsCommand.cpp
        ${platformSources})
target_link_libraries(iac PRIVATE emberstore)

if (WIN32)
    # CommandLineToArgvW
    target_link_libraries(iac PRIVATE Shell32)
endif ()

# Keeps the directory libcurl was linked from in the installed binary, so it still loads when libcurl lives outside the system directories (Nix).
set_target_properties(iac PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)

install(TARGETS iac RUNTIME DESTINATION bin)
