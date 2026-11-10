cmake_minimum_required(VERSION 3.25)

set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)

project(iac LANGUAGES CXX)

if (APPLE)
    enable_language(OBJCXX)
endif ()

# Declared ahead of emberstore so this pin wins dedup: carries the
# Windows captureOutput=false fix until it lands upstream.
lockstep_add_package(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG main
        OPTIONS "EACP_BUILD_GRAPHICS OFF"
)

lockstep_add_package(
        NAME emberstore
        GITHUB_REPOSITORY tamber-inc/emberstore
        GIT_TAG main)

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
target_compile_features(iac PRIVATE cxx_std_20)
target_link_libraries(iac PRIVATE emberstore)

if (WIN32)
    # CommandLineToArgvW
    target_link_libraries(iac PRIVATE Shell32)
endif ()

# Keeps the directory libcurl was linked from in the installed binary, so it still loads when libcurl lives outside the system directories (Nix).
set_target_properties(iac PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)

install(TARGETS iac RUNTIME DESTINATION bin)
