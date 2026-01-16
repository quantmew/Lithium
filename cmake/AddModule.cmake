# AddModule.cmake
# Helper function to add a Lithium module

function(lithium_add_module MODULE_NAME)
    set(options HEADER_ONLY)
    set(oneValueArgs "")
    set(multiValueArgs SOURCES HEADERS DEPENDENCIES PUBLIC_DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TARGET_NAME "lithium_${MODULE_NAME}")

    if(ARG_HEADER_ONLY)
        # Header-only library
        add_library(${TARGET_NAME} INTERFACE)
        target_include_directories(${TARGET_NAME} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
    else()
        # Regular library
        add_library(${TARGET_NAME} STATIC ${ARG_SOURCES} ${ARG_HEADERS})

        target_include_directories(${TARGET_NAME}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

        # Link compiler options
        target_link_libraries(${TARGET_NAME} PRIVATE lithium_compiler_options)
    endif()

    # Add dependencies
    if(ARG_DEPENDENCIES)
        foreach(DEP ${ARG_DEPENDENCIES})
            target_link_libraries(${TARGET_NAME} PRIVATE ${DEP})
        endforeach()
    endif()

    if(ARG_PUBLIC_DEPENDENCIES)
        foreach(DEP ${ARG_PUBLIC_DEPENDENCIES})
            if(ARG_HEADER_ONLY)
                target_link_libraries(${TARGET_NAME} INTERFACE ${DEP})
            else()
                target_link_libraries(${TARGET_NAME} PUBLIC ${DEP})
            endif()
        endforeach()
    endif()

    # Add alias for cleaner CMake usage
    add_library(Lithium::${MODULE_NAME} ALIAS ${TARGET_NAME})

    # Set library properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )

endfunction()

# Helper function to add tests for a module
function(lithium_add_module_tests MODULE_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TEST_TARGET "test_${MODULE_NAME}")
    set(MODULE_TARGET "lithium_${MODULE_NAME}")

    add_executable(${TEST_TARGET} ${ARG_SOURCES})

    target_link_libraries(${TEST_TARGET} PRIVATE
        ${MODULE_TARGET}
        GTest::gtest
        GTest::gtest_main
        lithium_compiler_options
    )

    if(ARG_DEPENDENCIES)
        target_link_libraries(${TEST_TARGET} PRIVATE ${ARG_DEPENDENCIES})
    endif()

    include(GoogleTest)
    gtest_discover_tests(${TEST_TARGET})

endfunction()
