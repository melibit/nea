cmake_minimum_required(VERSION 3.16)
project(MapApiGraphicsProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SFML 3.0 COMPONENTS Graphics Window REQUIRED)

set(ASMJIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/asmjit" CACHE PATH "")

set(BLEND2D_STATIC ON CACHE INTERNAL "")

add_subdirectory(external/blend2d)

add_executable(test-api test-api.cpp)
target_compile_options(test-api PRIVATE -Wall)
target_link_libraries(test-api PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

add_executable(test-graphics test-graphics.cpp)
target_compile_options(test-graphics PRIVATE -Wall)

target_link_libraries(test-graphics PRIVATE 
    nlohmann_json::nlohmann_json
    blend2d
    SFML::Graphics
    SFML::Window
    )

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test-map.json
    COMMAND curl --silent --show-error -X POST
            --data-urlencode "data@${CMAKE_CURRENT_SOURCE_DIR}/test-query.overpassql"
            "https://maps.mail.ru/osm/tools/overpass/api/interpreter"
            -o ${CMAKE_CURRENT_BINARY_DIR}/test-map.json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test-query.overpassql
    COMMENT "Fetching map data from Overpass API..."
)

add_custom_target(map DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test-map.json)

set(FONT_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts")

add_custom_target(font
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FONT_OUTPUT_DIR}"    
    COMMENT "Fetching Font data from Google Fonts..."
    COMMAND curl --silent --show-error "https://fonts.gstatic.com/s/hammersmithone/v18/qWcyB624q4L_C4jGQ9IK0O_dFlnbshsks4MRXw.ttf" -o "${FONT_OUTPUT_DIR}/HammersmithOne.ttf"
)

add_custom_target(default_target ALL DEPENDS test-api test-graphics)