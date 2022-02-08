cmake_minimum_required(VERSION 3.14)

set(BUILD_NUMBER "1")
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})
add_compile_definitions(APP_NAME="Microvisor HTTP Demo")

add_executable(microvisor-http-demo
    main.c
    logging.c
    stm32u5xx_hal_timebase_tim_template.c
)

target_link_libraries(microvisor-http-demo LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Optional informational and additional format generation
# including Bundle output
add_custom_command(OUTPUT EXTRAS
    DEPENDS microvisor-http-demo
    COMMAND cp "microvisor-http-demo" "microvisor-http-demo.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "microvisor-http-demo"
    COMMAND ${CMAKE_OBJDUMP} -h -S "microvisor-http-demo.elf" > "microvisor-http-demo.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "microvisor-http-demo.elf" "microvisor-http-demo.bin"
    COMMAND python3 ../../twilio-microvisor-tools/bundler-py/bundler.py microvisor-http-demo.elf microvisor-http-demo.zip
    COMMAND rm -f ../../null.d
    COMMAND echo "LATEST BUILD: ${BUILD_NUMBER}"
)

add_custom_target(extras ALL
    ${CMAKE_COMMAND} -E echo "Outputting additional artifacts"
    DEPENDS EXTRAS
)
