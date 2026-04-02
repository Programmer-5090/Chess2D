find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

# Download clay header-only library
include(FetchContent)
FetchContent_Declare(clay
    GIT_REPOSITORY https://github.com/nicbarker/clay.git
    GIT_TAG main
)
FetchContent_MakeAvailable(clay)

# Collect all source files automatically
file(GLOB_RECURSE CHESS2D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/chess2D/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/chess_engine/*.cpp"
    "${CMAKE_SOURCE_DIR}/utils/*.cpp"
)

add_executable(Chess2D ${CHESS2D_SOURCES})

# Enable link-time optimization (LTO) / interprocedural optimization where supported
set_property(TARGET Chess2D PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

set_property(TARGET Chess2D PROPERTY CXX_STANDARD 20)

target_include_directories(Chess2D PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/chess_engine"
    "${CMAKE_SOURCE_DIR}/include/chess2D"
    "${CMAKE_SOURCE_DIR}/include/search"
    "${CMAKE_SOURCE_DIR}/utils/include"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
    "${clay_SOURCE_DIR}"
)

target_link_libraries(Chess2D PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

# Copy resources folder next to the executable after every build
add_custom_command(TARGET Chess2D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:Chess2D>/resources"
)

# TODO: Add tests and install targets if needed.
