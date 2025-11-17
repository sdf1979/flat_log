cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME flat_log)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    main.cpp
    src/flat_log.h
    src/falt_log.cpp
    src/mapped_file.h
    src/mapped_file.cpp
    src/simd_support.h
    src/simd_support.cpp
    src/argument_parser.h
    src/argument_parser.cpp
)
	
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CMAKE_EXE_LINKER_FLAGS "-static"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{INSTALL_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> $ENV{INSTALL_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> $ENV{INSTALL_DIR}/
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -g)
    target_link_options(${PROJECT_NAME} PRIVATE -g)

    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $ENV{INSTALL_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> $ENV{INSTALL_DIR}/
    )
endif()