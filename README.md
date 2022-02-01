# Microvisor Nucleo

## Directories

* `Config/`
    * Contains the `FreeRTOSConfig.h` and `stm32u5xx_hal_conf.h` configuration files.
* `Code/`
    * Contains `.h` and `.c` files.
* `ST_Code/`
    * STM32U5 code incorporated into the project.

## Build Process

The sample code has the following dependencies:

- `cmake`
- `gcc-arm-none-eabi` (Linux)
- `arm-none-eabi-gcc` (macOS)

Clone with:

```shell
git clone --recursive https://github.com/smittytone/mv-nucleo.git
```

Generate the Makefiles and project:

```shell
cmake -S . -B build/
```

Build the executable:

```shell
cmake --build build --clean-first
```

The deliverable you can provision onto the Microvisor Nucleo Development Board will be built to `build/Code/mv-nucleo.zip`.

## Deployment

Upload with:

```shell
curl -X POST https://microvisor-upload.twilio.com/v1/Apps -H "Content-Type: multipart/form-data" -u $ACCOUNT_SID:$AUTH_TOKEN -s -F File=@./build/Code/mv-nucleo.zip | jq
```

Deploy with:

```shell
curl -X POST https://microvisor.twilio.com/v1/Devices/{SID} -u $ACCOUNT_SID:$AUTH_TOKEN -s -d AppSid={APP_SID_FROM_UPLOAD} | jq
```