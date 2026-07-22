cmake_minimum_required(VERSION 3.25)

project(iac LANGUAGES CXX)

if (APPLE)
    enable_language(OBJCXX)
endif ()

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME emberstore
        GITHUB_REPOSITORY tamber-inc/emberstore
        GIT_TAG main)

if (APPLE)
    set(platformSources Source/Platform-macOS.mm)
elseif (WIN32)
    set(platformSources Source/Platform-Windows.cpp)
endif ()

add_executable(iac Source/Main.cpp ${platformSources})
target_compile_features(iac PRIVATE cxx_std_20)
target_link_libraries(iac PRIVATE emberstore)

install(TARGETS iac RUNTIME DESTINATION bin)
