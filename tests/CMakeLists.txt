find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)

enable_testing()

if (CHESS2D_ENABLE_CLAY_UI)
    include(FetchContent)
    FetchContent_Declare(clay
        GIT_REPOSITORY https://github.com/nicbarker/clay.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(clay)

    add_executable(clay_test clay_test.cpp)

    set_property(TARGET clay_test PROPERTY CXX_STANDARD 20)

    target_include_directories(clay_test PRIVATE
        "${clay_SOURCE_DIR}"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
    )

    target_link_libraries(clay_test PRIVATE
        SDL3::SDL3
    )
endif()

add_executable(evaluation_test
    evaluation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/board_state.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/piece_list.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/precomp_move_data.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/hash_keys.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/magics.cpp
    ${CMAKE_SOURCE_DIR}/utils/logger.cpp
)

set_property(TARGET evaluation_test PROPERTY CXX_STANDARD 20)

target_include_directories(evaluation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/chess_engine
    ${CMAKE_SOURCE_DIR}/include/search
    ${CMAKE_SOURCE_DIR}/utils/include
)

add_test(NAME evaluation_test COMMAND evaluation_test)

target_link_libraries(evaluation_test PRIVATE
    spdlog::spdlog
)

if (CHESS2D_ENABLE_TRACY)
    target_link_libraries(evaluation_test PRIVATE Tracy::TracyClient)
    target_compile_definitions(evaluation_test PRIVATE TRACY_ENABLE)
endif()

add_executable(versus_test
    versus_test.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/board_state.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/piece_list.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/precomp_move_data.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/hash_keys.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/magics.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/move_generator.cpp
    ${CMAKE_SOURCE_DIR}/src/chess2D/board.cpp
    ${CMAKE_SOURCE_DIR}/src/chess2D/board_renderer.cpp
    ${CMAKE_SOURCE_DIR}/utils/logger.cpp
)

set_property(TARGET versus_test PROPERTY CXX_STANDARD 20)

target_include_directories(versus_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/chess_engine
    ${CMAKE_SOURCE_DIR}/include/chess2D
    ${CMAKE_SOURCE_DIR}/include/search
    ${CMAKE_SOURCE_DIR}/utils/include
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
)

target_link_libraries(versus_test PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
)

target_compile_definitions(versus_test PRIVATE
    CHESS2D_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(versus_test PRIVATE
    spdlog::spdlog
)

if (CHESS2D_ENABLE_TRACY)
    target_link_libraries(versus_test PRIVATE Tracy::TracyClient)
    target_compile_definitions(versus_test PRIVATE TRACY_ENABLE)
endif()

add_custom_command(TARGET versus_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:versus_test>/resources"
)

add_test(NAME versus_test COMMAND versus_test)

add_executable(search_benchmark_test
    search_benchmark_test.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/board_state.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/piece_list.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/precomp_move_data.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/hash_keys.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/magics.cpp
    ${CMAKE_SOURCE_DIR}/src/chess_engine/move_generator.cpp
    ${CMAKE_SOURCE_DIR}/utils/logger.cpp
)

set_property(TARGET search_benchmark_test PROPERTY CXX_STANDARD 20)

target_include_directories(search_benchmark_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/chess_engine
    ${CMAKE_SOURCE_DIR}/include/search
    ${CMAKE_SOURCE_DIR}/utils/include
)

target_compile_definitions(search_benchmark_test PRIVATE
    CHESS2D_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(search_benchmark_test PRIVATE
    spdlog::spdlog
)

if (CHESS2D_ENABLE_TRACY)
    target_link_libraries(search_benchmark_test PRIVATE Tracy::TracyClient)
    target_compile_definitions(search_benchmark_test PRIVATE TRACY_ENABLE)
endif()

add_test(NAME search_benchmark_test COMMAND search_benchmark_test)
