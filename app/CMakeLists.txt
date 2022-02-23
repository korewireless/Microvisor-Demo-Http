cmake_minimum_required(VERSION 3.14)

# Application data
set(APP_TITLE "Microvisor HTTP Demo")
set(VERSION_NUMBER "1.0.1")
set(BUILD_NUMBER "1")

message("Building ${APP_TITLE} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# Make app data available to the compiler
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})
add_compile_definitions(APP_NAME="${APP_TITLE}")
add_compile_definitions(APP_VERSION="${VERSION_NUMBER}")

# Compile app source code file(s)
add_executable(mv-http-demo
    main.c
    logging.c
    stm32u5xx_hal_timebase_tim_template.c
)

# Link libraries
target_link_libraries(mv-http-demo LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Optional informational and additional format generation
# including Bundle output
add_custom_command(OUTPUT EXTRAS
    DEPENDS mv-http-demo
    COMMAND cp "mv-http-demo" "mv-http-demo.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "mv-http-demo"
    COMMAND ${CMAKE_OBJDUMP} -h -S "mv-http-demo.elf" > "mv-http-demo.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "mv-http-demo.elf" "mv-http-demo.bin"
    COMMAND python3 ../../twilio-microvisor-tools/bundler-py/bundler.py mv-http-demo.elf mv-http-demo.zip
    COMMAND rm -f ../../null.d
)

add_custom_target(extras ALL DEPENDS EXTRAS)

