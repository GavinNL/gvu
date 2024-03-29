cmake_minimum_required(VERSION 3.10)


################################################################################
# Project name
#
#  many targets will be created:
#   myProject::warnings
#   myProject:
################################################################################
project( gvu
        VERSION 1.0.0.0
        LANGUAGES CXX)

################################################################################


set(CMAKE_CXX_STANDARD_REQUIRED ON)




################################################################################

# Extra targets.  adds coverage::coverage, warning::all, warning:error
SET(PROJECT_TARGETS_PREFIX ${PROJECT_NAME})

get_directory_property( gvu_is_sub_project PARENT_DIRECTORY)
if(gvu_is_sub_project)

    option( ${PROJECT_NAME}_ENABLE_COVERAGE "Enable Coverage."        FALSE)
    option( ${PROJECT_NAME}_ENABLE_WARNINGS "Enable Strict Warnings" FALSE)
    option( ${PROJECT_NAME}_ENABLE_TESTING  "Enable Unit Tests" FALSE)

else()
    option( ${PROJECT_NAME}_ENABLE_COVERAGE "Enable Coverage. "        TRUE)
    option( ${PROJECT_NAME}_ENABLE_WARNINGS "Enable Strict Warnings" TRUE)
    option( ${PROJECT_NAME}_ENABLE_TESTING  "Enable Unit Tests" TRUE)

endif()

    #######################################################################################
    # Custom targets so that these files show up in QtCreator for easy editing
    #######################################################################################
    add_custom_target(gvu_otherfiles SOURCES conanfile.txt .gitignore README.md)
    #######################################################################################

################################################################################



################################################################################
add_library(gvu INTERFACE)
add_library(gvu::gvu ALIAS gvu)
target_include_directories(gvu INTERFACE "include")
target_compile_features(   gvu INTERFACE cxx_std_17)
################################################################################



if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test")

    if( ${PROJECT_NAME}_ENABLE_TESTING )

        ################################################################################
        # If the conan file exists, include it
        ################################################################################
        if( EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake )
            include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
            conan_basic_setup(NO_OUTPUT_DIRS TARGETS)
        endif()
        ################################################################################


        ################################################################################
        # Build the SDLVulkanWindow as a library so that we can use it for the
        # examples/unit tests
        ################################################################################
        find_package(Vulkan)

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party_sym")
            add_subdirectory(third_party_sym/vkw)
            add_subdirectory(third_party_sym/gul)
        else()
            add_subdirectory(third_party/vkw)
            add_subdirectory(third_party/gul)
        endif()

        ################################################################################


        enable_testing()
        add_subdirectory(test)
    endif()

endif()



return()
