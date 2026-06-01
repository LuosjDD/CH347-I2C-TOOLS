cmake_minimum_required(VERSION 3.10)
project(I2C_TOOLS C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories("${CMAKE_SOURCE_DIR}/ch341par/")
include_directories("${CMAKE_SOURCE_DIR}/src/")

add_executable(i2cset src/i2c_set.c)
add_executable(i2cdump src/i2c_dump.c)
add_executable(i2cdetect src/i2c_detect.c)
add_executable(i2ctransfer src/i2ctransfer.c)
add_executable(i2cget src/i2cget.c)

target_link_libraries(i2cset "${CMAKE_SOURCE_DIR}/ch341par/amd64/CH347DLLA64.lib")
target_link_libraries(i2cdump "${CMAKE_SOURCE_DIR}/ch341par/amd64/CH347DLLA64.lib")
target_link_libraries(i2cdetect "${CMAKE_SOURCE_DIR}/ch341par/amd64/CH347DLLA64.lib")
target_link_libraries(i2ctransfer "${CMAKE_SOURCE_DIR}/ch341par/amd64/CH347DLLA64.lib")
target_link_libraries(i2cget "${CMAKE_SOURCE_DIR}/ch341par/amd64/CH347DLLA64.lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Copy 64-bit CH347 DLL to output directory for each tool
foreach(tool i2cset i2cdump i2cdetect i2ctransfer i2cget)
    add_custom_command(TARGET ${tool} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ch341par/CH347DLLA64.DLL"
            $<TARGET_FILE_DIR:${tool}>
        COMMENT "Copying CH347DLLA64.DLL to output directory for ${tool}"
    )
endforeach()