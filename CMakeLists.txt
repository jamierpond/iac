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

# text-buffer turns its own warnings into errors unless the includer defines
# this; iac keeps -Werror to its own targets (below), so a new compiler warning
# inside a dependency can't stop iac building.
function(set_strict_warnings_level target)
endfunction()

# eacp has no terminal UI. FTXUI draws iac chat; its own loop is not used:
# an eacp timer pumps it, as in DiffView.
lockstep_add_package(
        NAME ftxui
        GITHUB_REPOSITORY ArthurSonzogni/FTXUI
        GIT_TAG v7.0.3
        SYSTEM YES
        OPTIONS
            "FTXUI_ENABLE_INSTALL OFF"
            "FTXUI_QUIET ON")

# VimEditor, the editor behind chat's prompt, the same one dv's comment box uses.
#   cmake -B build -DLOCKSTEP_text-buffer_SOURCE=$HOME/projects/text-buffer
lockstep_add_package(
        NAME text-buffer
        GITHUB_REPOSITORY jamierpond/text-buffer
        GIT_TAG main
        SYSTEM YES)

# Only Rudder's SessionLog (the per-launch usage log) and PondApp (the pond
# data folder) are used; RUDDER_CLIENT_ONLY leaves the Rudder app unbuilt.
#   cmake -B build -DLOCKSTEP_Rudder_SOURCE=$HOME/projects/Rudder
lockstep_add_package(
        NAME Rudder
        GITHUB_REPOSITORY jamierpond/Rudder
        GIT_TAG main
        OPTIONS "RUDDER_CLIENT_ONLY ON"
        SYSTEM YES)

if (NOT TARGET NanoTest)
    lockstep_add_package(
            NAME NanoTest
            GITHUB_REPOSITORY eyalamirmusic/NanoTest
            GIT_TAG main
            SYSTEM YES)
endif ()

# Everything but main(), so the tests link the same code the binary runs.
add_library(iacCore STATIC
        Source/ChatCommand.cpp
        Source/ChatInput.cpp
        Source/ChatKeymap.cpp
        Source/ChatMessagePane.cpp
        Source/ChatRooms.cpp
        Source/ChatScreen.cpp
        Source/ChatSession.cpp
        Source/ChatView.cpp
        Source/ComposeLine.cpp
        Source/EditorKeys.cpp
        Source/HelpCommand.cpp
        Source/HubConfiguration.cpp
        Source/LineEdit.cpp
        Source/Message.cpp
        Source/MessageAddressing.cpp
        Source/MonitorCommand.cpp
        Source/Publish.cpp
        Source/PublishCommand.cpp
        Source/ReadCommand.cpp
        Source/RemoteRoom.cpp
        Source/Room.cpp
        Source/RoomsCommand.cpp
        Source/StoreRooms.cpp
        Source/UsageLog.cpp
        ${platformSources})
target_include_directories(iacCore PUBLIC Source)
target_link_libraries(iacCore PUBLIC
        emberstore
        VimEditor
        SessionLog
        PondApp
        ftxui::screen
        ftxui::dom
        ftxui::component)

if (WIN32)
    # CommandLineToArgvW
    target_link_libraries(iacCore PUBLIC Shell32)
endif ()

add_executable(iac Source/Main.cpp)
pond_add_version_library(iacVersion)
target_link_libraries(iac PRIVATE iacCore iacVersion)

# Keeps the directory libcurl was linked from in the installed binary, so it still loads when libcurl lives outside the system directories (Nix).
set_target_properties(iac PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)

install(TARGETS iac RUNTIME DESTINATION bin)

if (PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(Tests)
endif ()
