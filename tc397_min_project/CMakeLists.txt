cmake_minimum_required(VERSION 3.15)
project(tc397_min_project LANGUAGES CXX C ASM)

set(EXCLUDE_DIRS "build" "TriCore Debug (TASKING)" "TriCore Release (TASKING)")

# source files
set(EXCLUDE_FILES "ert_main.c")
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)
foreach(_dir IN ITEMS ${EXCLUDE_DIRS})
    file(GLOB_RECURSE FILES RELATIVE ${PROJECT_SOURCE_DIR} "${_dir}/*")
    list(FILTER SOURCES EXCLUDE REGEX "${_dir}/.*")
endforeach()
foreach(_file IN ITEMS ${EXCLUDE_FILES})
    list(FILTER SOURCES EXCLUDE REGEX "${_file}")
endforeach()

# include directories
set(INCLUDE_DIRS "")
file(GLOB_RECURSE FILES RELATIVE ${PROJECT_SOURCE_DIR} "*.h")
foreach(_dir IN ITEMS ${EXCLUDE_DIRS})
    list(FILTER FILES EXCLUDE REGEX "${_dir}/.*")
endforeach()
foreach(_source IN ITEMS ${FILES})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
    list(APPEND INCLUDE_DIRS "${_source_path}")
    # append upper directories
    while(NOT "${_source_path}" STREQUAL "")
        get_filename_component(_source_path "${_source_path}" DIRECTORY)
        list(APPEND INCLUDE_DIRS "${_source_path}")
    endwhile()
endforeach()
list(APPEND INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
list(REMOVE_DUPLICATES INCLUDE_DIRS)

add_executable(${PROJECT_NAME} 
    ${SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})

# postbuild mv .elf .hex
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
)
