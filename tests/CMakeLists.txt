if(NOT HCRE_TEST_DB)
    message(WARNING "HCRE_TEST_DB is empty, which may lead to problem")
    message(WARNING
            "Example -DHCRE_TEST_DB='host=localhost dbname=test user=postgres'")
endif()


add_executable(server-test)

target_sources(server-test
    PRIVATE
        server-test.cpp
)

target_compile_definitions(server-test
    PRIVATE HCRE_TEST_DB=\"${HCRE_TEST_DB}\"
)

if(NOT HCRE_TEST_DB_PASSWORD)
    message(WARNING "HCRE_TEST_DB_PASSWORD is empty; if you are on Windows it may lead to problem")
    message(WARNING "Example -DHCRE_TEST_DB_PASSWORD='your_password'")
else()
    target_compile_definitions(server-test
        PRIVATE HCRE_TEST_DB_PASSWORD=\"${HCRE_TEST_DB_PASSWORD}\"
    )
endif()

target_link_libraries(server-test
    PRIVATE
        hc::hc
        gtest::gtest
        spdlog::spdlog
        libpqxx::pqxx # For mocking PG database
        nlohmann_json::nlohmann_json
)

add_executable(playground pg.cpp)

target_link_libraries(playground
    PRIVATE
        hc::hc
)

add_executable(optional-test)

target_sources(optional-test
    PRIVATE
        optional-test.cpp
)

target_link_libraries(optional-test
    PRIVATE
        hc::hc
        gtest::gtest
        nlohmann_json::nlohmann_json
)

add_executable(json-test)

target_sources(json-test
    PRIVATE
        json-test.cpp
)

target_link_libraries(json-test
    PRIVATE
        hc::hc
        gtest::gtest
        nlohmann_json::nlohmann_json
)

add_executable(archive-test)

target_sources(archive-test
    PRIVATE
        archive-test.cpp
)

target_link_libraries(archive-test
    PRIVATE
        hc::hc
        gtest::gtest
)


enable_testing()

include(GoogleTest)
gtest_discover_tests(server-test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
gtest_discover_tests(optional-test)
gtest_discover_tests(json-test)
gtest_discover_tests(archive-test)
