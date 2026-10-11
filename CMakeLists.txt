cmake_minimum_required(VERSION 3.16)
project(can_bsw_stack C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WARN_FLAGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror)

# ---------------------------------------------------------------- BSW ---
# Target-independent BSW sources, built for both firmware and host tests.
set(BSW_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/bsw/include
    ${CMAKE_SOURCE_DIR}/bsw/mcal/reg
    ${CMAKE_SOURCE_DIR}/bsw/mcal/port
    ${CMAKE_SOURCE_DIR}/bsw/mcal/dio
    ${CMAKE_SOURCE_DIR}/bsw/mcal/can
    ${CMAKE_SOURCE_DIR}/bsw/services/det
)
set(BSW_SOURCES
    ${CMAKE_SOURCE_DIR}/bsw/mcal/port/Port.c
    ${CMAKE_SOURCE_DIR}/bsw/mcal/dio/Dio.c
    ${CMAKE_SOURCE_DIR}/bsw/mcal/can/Can_Hw.c
    ${CMAKE_SOURCE_DIR}/bsw/services/det/Det.c
)

add_library(bsw STATIC ${BSW_SOURCES})
target_include_directories(bsw PUBLIC ${BSW_INCLUDE_DIRS})
target_compile_options(bsw PRIVATE ${WARN_FLAGS})

if(CMAKE_CROSSCOMPILING)
    # ----------------------------------------------------- Firmware ---
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/platform/stm32f103/stm32f103c8.ld)

    add_executable(firmware.elf
        ${CMAKE_SOURCE_DIR}/platform/stm32f103/startup_stm32f103.c
        ${CMAKE_SOURCE_DIR}/platform/stm32f103/system_stm32f103.c
        ${CMAKE_SOURCE_DIR}/platform/stm32f103/SchM_CortexM.c
        ${CMAKE_SOURCE_DIR}/platform/stm32f103/Mcal_RegMap.c
        ${CMAKE_SOURCE_DIR}/app/main.c
    )
    # The vector table stores the initial MSP (a data address) in a function pointer
    # slot; ISO C forbids that conversion, so -Wpedantic is relaxed for this file only.
    set_source_files_properties(${CMAKE_SOURCE_DIR}/platform/stm32f103/startup_stm32f103.c
        PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
    target_include_directories(firmware.elf PRIVATE ${CMAKE_SOURCE_DIR}/platform/stm32f103)
    target_compile_options(firmware.elf PRIVATE ${WARN_FLAGS} -Os -g3)
    target_compile_options(bsw PRIVATE -Os -g3)
    target_link_libraries(firmware.elf PRIVATE bsw)
    target_link_options(firmware.elf PRIVATE
        -T${LINKER_SCRIPT}
        -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
        -Wl,--print-memory-usage
    )
    set_target_properties(firmware.elf PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

    add_custom_command(TARGET firmware.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:firmware.elf> ${CMAKE_BINARY_DIR}/firmware.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware.elf> ${CMAKE_BINARY_DIR}/firmware.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware.elf>
    )
else()
    # --------------------------------------------------- Host tests ---
    enable_testing()
    target_compile_options(bsw PRIVATE -O0 -g --coverage -fsanitize=address,undefined)
    target_link_options(bsw PUBLIC --coverage -fsanitize=address,undefined)

    # Host replacements for the exclusive area and the MCAL registers
    # (RAM-backed fakes instead of the addresses in Mcal_RegMap.c)
    add_library(host_stubs STATIC
        ${CMAKE_SOURCE_DIR}/test/host/stubs/Mcal_FakeRegs.c
        ${CMAKE_SOURCE_DIR}/test/host/stubs/SchM_Stub.c
    )
    target_include_directories(host_stubs PUBLIC ${CMAKE_SOURCE_DIR}/test/host/stubs)
    target_compile_options(host_stubs PRIVATE ${WARN_FLAGS} -O0 -g -fsanitize=address,undefined)
    target_link_libraries(host_stubs PUBLIC bsw)
    target_link_libraries(bsw PUBLIC host_stubs)

    function(add_host_test name)
        add_executable(${name} ${CMAKE_SOURCE_DIR}/test/host/${name}.c)
        target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/test/host)
        target_compile_options(${name} PRIVATE ${WARN_FLAGS} -O0 -g -fsanitize=address,undefined)
        target_link_libraries(${name} PRIVATE bsw)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_host_test(test_det)

    # Exercise tests are OFF until the learner's implementation lands, so CI
    # stays green. Turn the default ON in the PR that adds the implementation.
    # MCAL exercise 1 (docs/exercises/ex01-port-dio.vi.md)
    option(MCAL_PORT_DIO_EXERCISE_TESTS "Build and run test_port_dio (Port/Dio exercise)" OFF)
    if(MCAL_PORT_DIO_EXERCISE_TESTS)
        add_host_test(test_port_dio)
    endif()

    # MCAL exercise 3 (docs/exercises/ex03-can-bxcan.vi.md)
    option(CAN_HW_EXERCISE_TESTS "Build and run test_can_hw (bxCAN helper exercise)" OFF)
    if(CAN_HW_EXERCISE_TESTS)
        add_host_test(test_can_hw)
    endif()
endif()
