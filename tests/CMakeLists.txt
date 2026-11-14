find_package(doctest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(EngineTests ${TEST_SOURCES})
target_link_libraries(EngineTests PRIVATE engine doctest::doctest)
add_test(NAME EngineTests COMMAND EngineTests)