find_package(SDL3 REQUIRED CONFIG)

# Download clay header-only library
include(FetchContent)
FetchContent_Declare(clay
    GIT_REPOSITORY https://github.com/nicbarker/clay.git
    GIT_TAG main
)
FetchContent_MakeAvailable(clay)

# Create test executable for clay UI
add_executable(clay_test clay_test.cpp)

set_property(TARGET clay_test PROPERTY CXX_STANDARD 20)

target_include_directories(clay_test PRIVATE
    "${clay_SOURCE_DIR}"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
)

target_link_libraries(clay_test PRIVATE
    SDL3::SDL3
)
