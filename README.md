# Microvisor HTTP Demo

[![.github/workflows/build.yml](https://github.com/korewireless/Microvisor-Demo-Http/actions/workflows/build.yml/badge.svg)](https://github.com/korewireless/Microvisor-Demo-Http/actions/workflows/build.yml)

This repo provides a basic demonstration of a user application capable of working with Microvisor’s HTTP communications system calls. It has no hardware dependencies beyond the Microvisor Nucleo Development Board.

It is based on the [FreeRTOS](https://freertos.org/) real-time operating system and which will run on the “non-secure” side of Microvisor. FreeRTOS is included as a submodule.

The [ARM CMSIS-RTOS API](https://github.com/ARM-software/CMSIS_5) is used an an intermediary between the application and FreeRTOS to make it easier to swap out the RTOS layer for another should you wish to do so.

The application code files can be found in the [app/](app/) directory. The [ST_Code/](ST_Code/) directory contains required components that are not part of the Microvisor STM32U5 HAL, which this sample accesses as a submodule. The `FreeRTOSConfig.h` and `stm32u5xx_hal_conf.h` configuration files are located in the [config/](config/) directory.

## Actions

The code creates and runs two threads.

One thread periodically toggles GPIO A5, which is the user LED on the [Microvisor Nucleo Development Board](https://www.twilio.com/docs/iot/microvisor/microvisor-nucleo-development-board).

The second thread It also emits a “ping” to the Microvisor logger once a second. Every 30 seconds it makes a `GET` request to `https://jsonplaceholder.typicode.com/todos/1`, a free API the delivers an object JSON testing.

## Polite Deployment

This code now supports Microvisor polite deployments. Bundles will need to be built with polite deployment enabled. Once such a bundle has been uploaded and deployed, future updates will be handled politely: Microvisor will notify the application, which can choose to apply the staged update when it is no longer performing any critical tasks.

In the case of the demo, this is simulated by setting a FreeRTOS/CMSIS timer which fires 30 seconds later to trigger the application update.

## Cloning the Repo

This repo makes uses of git submodules, some of which are nested within other submodules. To clone the repo, run:

```bash
git clone https://github.com/korewireless/Microvisor-Demo-Http.git
```

and then:

```bash
cd Microvisor-Demo-Http
git submodule update --init --recursive
```

## Repo Updates

When the repo is updated, and you pull the changes, you should also always update dependency submodules. To do so, run:

```bash
git submodule update --remote --recursive
```

We recommend following this by deleting your `build` directory.

## Requirements

You will need a Twilio account. [Sign up now if you don’t have one](https://www.twilio.com/try-twilio).

You will also need a Microvisor Nucleo Development Board.

## Software Setup

This project is written in C. At this time, we only support Ubuntu 20.0.4. Users of other operating systems should build the code under a virtual machine running Ubuntu, or with Docker.

**Note** Users of unsupported platforms may attempt to install the Microvisor toolchain using [this guidance](https://www.twilio.com/docs/iot/microvisor/install-microvisor-app-development-tools-on-unsupported-platforms).

### Docker

If you are running on an architecture other than x86/amd64 (such as a Mac with Apple silicon), you will need to override the platform when running docker. This is needed for the Twilio CLI `apt`` package which is x86 only at this time:

```shell
export DOCKER_DEFAULT_PLATFORM=linux/amd64
```

Build the image:

```shell
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -t mv-http-demo-image .
```

Run the build:

```
docker run -it --rm -v $(pwd)/:/home/mvisor/project/ \
  --env-file env.list \
  --name mv-http-demo mv-http-demo-image
```

**Note** You will need to have exported certain environment variables, as [detailed below](#environment-variables).

Under Docker, the demo is compiled, uploaded and deployed to your development board. It also initiates logging — hit <b>ctrl</b>-<b>c</b> to break out to the command prompt.

### Libraries and Tools

Under Ubuntu, run the following:

```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi \
  git curl build-essential cmake libsecret-1-dev jq openssl
```

### Twilio CLI

Install the Twilio CLI. This is required to view streamed logs and for remote debugging. You need version 4.0.1 or above.

**Note** If you have already installed the Twilio CLI using *npm*, we recommend removing it and then reinstalling as outlined below. Remove the old version with `npm remove -g twilio-cli`.

```bash
wget -qO- https://twilio-cli-prod.s3.amazonaws.com/twilio_pub.asc | sudo apt-key add -
sudo touch /etc/apt/sources.list.d/twilio.list
echo 'deb https://twilio-cli-prod.s3.amazonaws.com/apt/ /' | sudo tee /etc/apt/sources.list.d/twilio.list
sudo apt update
sudo apt install -y twilio
```

Close your terminal window or tab, and open a new one. Now run:

```bash
twilio plugins:install @twilio/plugin-microvisor
```

#### Environment Variables

Running the Twilio CLI and the Microvisor Plugin to upload the built code to the Microvisor cloud for subsequent deployment to your Microvisor Nucleo Board uses the following Twilio credentials stored as environment variables. They should be added to your shell profile:

```bash
export TWILIO_ACCOUNT_SID=ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export TWILIO_AUTH_TOKEN=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export MV_DEVICE_SID=UVxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

You can get the first two from your Twilio Console [account dashboard](https://console.twilio.com/).

Enter the following command to get your target device’s SID and, if set, its unique name:

```bash
twilio api:microvisor:v1:devices:list
```

It is also accessible via the QR code on the back of your development board.

## Build and Deploy the Application

Run:

```bash
twilio microvisor:deploy . --devicesid ${MV_DEVICE_SID}
```

This will compile, bundle and upload the code, and stage it for deployment to your device. If you encounter errors, please check your stored Twilio credentials.

The `--log` flag initiates log-streaming.

For more information, run

```bash
twilio microvisor:deploy --help
```

## UART Logging

You may log your application over UART on pin PD5 — pin 41 in bank CN11 on the Microvisor Nucleo Development Board. To use this mode, which is intended as an alternative to application logging, typically when a device is disconnected, connect a 3V3 FTDI USB-to-Serial adapter cable’s RX pin to PD5, and a GND pin to any Nucleo GND pin. Whether you do this or not, the application will continue to log via the Internet.

## Remote Debugging

This release supports remote debugging, and builds are enabled for remote debugging automatically. Change the value of the line

```
set(ENABLE_REMOTE_DEBUGGING 1)
```

in the root `CMakeLists.txt` file to `0` to disable this.

Enabling remote debugging in the build does not initiate a GDB session — you will have to do this manually. Follow the instructions in the [Microvisor documentation](https://www.twilio.com/docs/iot/microvisor/microvisor-remote-debugging).

This repo contains a `.gdbinit` file which sets the remote target to localhost on port 8001 to match the Twilio CLI Microvisor plugin remote debugging defaults.

#### Remote Debugging Encryption

Remote debugging sessions are encrypted. To generate keys, add the `--genkeys` switch to the deploy call above, or generate your own keys — see the documentation linked above for details.

Use the `--publickey /path/to/public/key` and `--privatekey /path/to/private/key` options to either specify existing keys, or to specify where you would like plugin-generated keys to be stored. By default, keys will be stored in the `build` directory so they will not be inadvertently push to a public git repo:

```
twilio microvisor:deploy . --devicesid ${MV_DEVICE_SID} \
  --privatekey /path/to/private/key.pem \
  --publickey /path/to/public/key.pem
```

You will need to pass the path to the private key to the Twilio CLI Microvisor plugin to decrypt debugging data. The plugin will output this path for you.

## Copyright and Licensing

The sample code and Microvisor SDK is © 2024, KORE Wireless. It is licensed under the terms of the [MIT](./LICENSE.md).

The SDK makes used of code © 2021, STMicroelectronics and affiliates. This code is licensed under terms described in [this file](https://github.com/korewireless/Microvisor-HAL-STM32U5/blob/main/LICENSE-STM32CubeU5.md).

The SDK makes use [ARM CMSIS](https://github.com/ARM-software/CMSIS_5) © 2004, ARM. It is licensed under the terms of the [Apache 2.0 License](./LICENSE).

[FreeRTOS](https://freertos.org/) is © 2021, Amazon Web Services, Inc. It is licensed under the terms of the [Apache 2.0 License](./LICENSE).

The Twilio CLI is © 2024, Twilio.
