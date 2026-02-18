cmake_minimum_required(VERSION 3.10)
project(fatx_tests)

# Activer C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trouver GTest
find_package(GTest REQUIRED)
include(GoogleTest)

# Activer la couverture de code en mode Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_compile_options(--coverage)
    add_link_options(--coverage)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)
        
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '*/tests/*' '/usr/*' --output-file coverage.info
            COMMAND ${GENHTML_PATH} coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
endif()

# Ajouter les fichiers de test
set(TEST_SOURCES
    test_basic.cpp
    test_device.cpp
    test_diskmap.cpp
    test_entry.cpp
    test_partition.cpp
    test_utils.cpp
)

# Créer l'exécutable de test
add_executable(fatx_tests ${TEST_SOURCES})

# Inclure les répertoires nécessaires
target_include_directories(fatx_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)

# Lier les bibliothèques
target_link_libraries(fatx_tests PRIVATE
    fatx
    GTest::GTest 
    GTest::Main
    pthread
)

# Activer les avertissements
if(MSVC)
    target_compile_options(fatx_tests PRIVATE /W4 /WX)
else()
    target_compile_options(fatx_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Désactiver les avertissements spécifiques si nécessaire
if(MSVC)
    target_compile_options(fatx_tests PRIVATE /wd4996)  # Désactive les avertissements de dépréciation
else()
    target_compile_options(fatx_tests PRIVATE -Wno-deprecated-declarations)
endif()

# Ajouter les tests à CTest
gtest_discover_tests(fatx_tests
    EXTRA_ARGS --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results.xml
)
