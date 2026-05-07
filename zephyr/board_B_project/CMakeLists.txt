cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(image_project)

# Abilita hard float per compatibilità con libreria ST
zephyr_compile_options(-mfpu=fpv5-d16 -mfloat-abi=hard)
zephyr_link_libraries(-mfpu=fpv5-d16 -mfloat-abi=hard)

target_sources(app PRIVATE
    src/main.c
    cube_ai/network.c
    cube_ai/network_data.c
)

target_include_directories(app PRIVATE
    cube_ai/
)

target_link_libraries(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cube_ai/NetworkRuntime1200_CM7_GCC.a
)

target_link_libraries(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cube_ai/NetworkRuntime1200_CM7_GCC.a
    m   # ← libreria math per expf
)