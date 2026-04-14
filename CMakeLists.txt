cmake_minimum_required(VERSION 3.20)
project(GA_3DBPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG QUIET)

add_executable(GA_3DBPP
    src/main.cpp
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp
    src/SupportChecker.cpp
    src/CenterOfMass.cpp
    src/Hausdorff.cpp
    src/LayerGenerator.cpp
    src/BlockBuilder.cpp
    src/PalletRegistry.cpp
    src/ExtremePointEngine.cpp
    src/Packer.cpp
    src/GeneticOperators.cpp
    src/NSGA2.cpp
)

target_include_directories(GA_3DBPP PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GA_3DBPP PRIVATE
    nlohmann_json::nlohmann_json
)

if(spdlog_FOUND)
    target_link_libraries(GA_3DBPP PRIVATE spdlog::spdlog)
endif()

add_executable(GA_3DBPPBenchmark
    src/BenchmarkMain.cpp
    src/BenchmarkRunner.cpp
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp
    src/SupportChecker.cpp
    src/CenterOfMass.cpp
    src/Hausdorff.cpp
    src/LayerGenerator.cpp
    src/BlockBuilder.cpp
    src/PalletRegistry.cpp
    src/ExtremePointEngine.cpp
    src/Packer.cpp
    src/GeneticOperators.cpp
    src/NSGA2.cpp
)

target_include_directories(GA_3DBPPBenchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GA_3DBPPBenchmark PRIVATE
    nlohmann_json::nlohmann_json
)

if(spdlog_FOUND)
    target_link_libraries(GA_3DBPPBenchmark PRIVATE spdlog::spdlog)
endif()

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(GA_3DBPPTests
    test/test_main.cpp
    test/test_types.cpp
    test/test_io.cpp
    test/test_csv.cpp
    test/test_aabb.cpp
    test/test_support.cpp
    test/test_com.cpp
    test/test_hausdorff.cpp
    test/test_layers.cpp
    test/test_extreme_points.cpp
    test/test_nsga2.cpp
    test/test_integration.cpp
    src/BRReader.cpp
    src/CSVReader.cpp
    src/JSONWriter.cpp
    src/Logger.cpp
    src/SupportChecker.cpp
    src/CenterOfMass.cpp
    src/Hausdorff.cpp
    src/LayerGenerator.cpp
    src/BlockBuilder.cpp
    src/PalletRegistry.cpp
    src/ExtremePointEngine.cpp
    src/Packer.cpp
    src/GeneticOperators.cpp
    src/NSGA2.cpp
    src/BenchmarkRunner.cpp
)

target_include_directories(GA_3DBPPTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GA_3DBPPTests PRIVATE
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

if(spdlog_FOUND)
    target_link_libraries(GA_3DBPPTests PRIVATE spdlog::spdlog)
endif()

include(GoogleTest)
gtest_discover_tests(GA_3DBPPTests)

# Propagate sanitizer flags to linker (needed for ASan/UBSan in Debug preset)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_CXX_FLAGS}")
