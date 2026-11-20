add_executable(iacTests
        CommandLineTests.cpp
        HubConfigurationTests.cpp
        MessageAddressingTests.cpp
        MessageTests.cpp
        PublishTests.cpp
        RemoteRoomTests.cpp)
target_link_libraries(iacTests PRIVATE NanoTest NanoTestMain iacCore)
# The command line tests run the real iac binary, the way agents do.
add_dependencies(iacTests iac)
target_compile_definitions(iacTests PRIVATE IAC_BINARY_PATH="$<TARGET_FILE:iac>")
pond_add_version_test(iac iac)
nano_discover_tests(iacTests)
