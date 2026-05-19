cmake_minimum_required(VERSION 3.22)
project(bmp280_tests C)

set(CMAKE_C_STANDARD 11)

# Unity
add_library(unity STATIC
    unity/src/unity.c
)
target_include_directories(unity PUBLIC unity/src)

# HAL stub (for mock tests)
add_library(hal_stub STATIC
    stubs/hal_i2c_stub.c
)
target_include_directories(hal_stub PUBLIC
    stubs/
    ../Core/Inc
)

# Pure logic tests (no mock)
add_executable(test_bmp280_pure
    test_bmp280_pure.c
    ../Core/Src/bmp280.c
)
target_include_directories(test_bmp280_pure PRIVATE
    ../Core/Inc
    stubs/
)
target_link_libraries(test_bmp280_pure unity hal_stub)

# HAL mock tests
add_executable(test_bmp280_i2c
    test_bmp280_i2c.c
    ../Core/Src/bmp280.c
)
target_include_directories(test_bmp280_i2c PRIVATE
    ../Core/Inc
    stubs/
)
target_link_libraries(test_bmp280_i2c unity hal_stub)

enable_testing()
add_test(NAME test_bmp280_pure COMMAND test_bmp280_pure)
add_test(NAME test_bmp280_i2c  COMMAND test_bmp280_i2c)