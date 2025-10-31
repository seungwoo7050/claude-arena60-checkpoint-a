find_package(GTest CONFIG REQUIRED)

# Unit tests
file(GLOB_RECURSE UNIT_TEST_SOURCES "unit/*.cpp")
if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        Boost::system
        libpq::pq
        arena60_lib
    )
    add_test(NAME UnitTests COMMAND unit_tests)
    set_tests_properties(UnitTests PROPERTIES LABELS "unit")
endif()

# Integration tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "integration/*.cpp")
if(INTEGRATION_TEST_SOURCES)
    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(integration_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        Boost::system
        libpq::pq
        arena60_lib
    )
    add_test(NAME IntegrationTests COMMAND integration_tests)
    set_tests_properties(IntegrationTests PROPERTIES LABELS "integration")
endif()

# Performance tests
file(GLOB_RECURSE PERFORMANCE_TEST_SOURCES "performance/*.cpp")
if(PERFORMANCE_TEST_SOURCES)
    add_executable(performance_tests ${PERFORMANCE_TEST_SOURCES})
    target_link_libraries(performance_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        Boost::system
        arena60_lib
    )
    add_test(NAME PerformanceTests COMMAND performance_tests)
    set_tests_properties(PerformanceTests PROPERTIES LABELS "performance")
endif()
