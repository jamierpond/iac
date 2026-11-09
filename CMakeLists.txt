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

add_executable(iac Source/Main.cpp ${platformSources})
target_compile_features(iac PRIVATE cxx_std_20)
target_link_libraries(iac PRIVATE emberstore)

if (WIN32)
    # CommandLineToArgvW
    target_link_libraries(iac PRIVATE Shell32)
endif ()

install(TARGETS iac RUNTIME DESTINATION bin)
