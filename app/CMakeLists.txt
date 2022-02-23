cmake_minimum_required(VERSION 3.14)

# Set application data
set(APP "Microvisor HTTP Demo")
set(VERSION_NUMBER "1.0.1")
set(BUILD_NUMBER "1")

message("Building ${APP} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# Make app data available to the compiler
add_compile_definitions(APP_NAME="${APP}")
add_compile_definitions(APP_VERSION="${VERSION_NUMBER}")
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})

# Compile app source code file(s)
add_executable(${PROJECT_NAME}
    main.c
    logging.c
    stm32u5xx_hal_timebase_tim_template.c
)

# Link built libraries
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Optional informational and additional format generation
# including Bundle output
add_custom_command(OUTPUT EXTRAS
    DEPENDS ${PROJECT_NAME}
    COMMAND cp "${PROJECT_NAME}" "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "${PROJECT_NAME}"
    COMMAND ${CMAKE_OBJDUMP} -h -S "${PROJECT_NAME}.elf" > "${PROJECT_NAME}.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "${PROJECT_NAME}.elf" "${PROJECT_NAME}.bin"
    COMMAND python3 ../../twilio-microvisor-tools/bundler-py/bundler.py "${PROJECT_NAME}.elf" "${PROJECT_NAME}.zip"
    COMMAND rm -f ../../null.d
)

add_custom_target(extras ALL DEPENDS EXTRAS)

