cmake_minimum_required(VERSION 3.30)

project(hc-re VERSION 0.1.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# target_compile_options(hc
#     PRIVATE
#         -fsanitize=thread
# )
#
# target_link_options(hc
#     PRIVATE
#         -fsanitize=thread
# )



# Resolves dependencies
## Header-only
find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)

include(FetchContent)
FetchContent_Declare(sqlpp23
    GIT_REPOSITORY  https://github.com/rbock/sqlpp23
    GIT_TAG         origin/main
)
set(BUILD_POSTGRESQL_CONNECTOR ON)
FetchContent_MakeAvailable(sqlpp23)

find_package(Boost REQUIRED)
find_package(httplib REQUIRED)
find_package(cppcodec REQUIRED)

## Compiled lib
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(libpqxx REQUIRED)
find_package(LibArchive REQUIRED)
# End

add_subdirectory(src)
add_subdirectory(apps/cli)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/hc/version.h.in
    ${CMAKE_BINARY_DIR}/include/hc/version.h
    @ONLY
)


## TEST
option(HCRE_BUILD_TESTS "Whether to build tests" OFF)
set(HCRE_TEST_DB "" CACHE STRING
    "Postgresql test databse connection string, used for tests")
set(HCRE_TEST_DB_PASSWORD "" CACHE STRING
    "Postgresql test databse password, used for tests")

if(HCRE_BUILD_TESTS)
    add_subdirectory(tests)
endif()


## Benchmarking
option(HCRE_BUILD_BENCHMARK "Whether to build benchmarks" OFF)

if(HCRE_BUILD_BENCHMARK)
    add_subdirectory(benchmark)
endif()



install(TARGETS hc
    EXPORT hc-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Set the package name and version
set(CPACK_PACKAGE_NAME "hc-re")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "ShelpAm <shelpam@outlook.com>")

# CPACK_GENERATOR set by root CMakeLists.txt
include(CPack)
