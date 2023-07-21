# TTgo T-Call ESP32 Module Project

![TTgo T-Call](link_to_image)

This project utilizes the TTgo T-Call ESP32 module to perform various tasks such as making calls, sending and receiving SMS messages, checking battery status, and more. The project is implemented using Arduino and the TinyGSM library, allowing communication with the SIM800 modem on the TTgo T-Call module.

## Table of Contents

- [Introduction](#introduction)
- [Hardware Requirements](#hardware-requirements)
- [Library Dependencies](#library-dependencies)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Usage](#usage)
- [AT Commands](#at-commands)

## Introduction

The TTgo T-Call ESP32 module is a powerful IoT development board that integrates the ESP32 microcontroller and the SIM800 modem, allowing cellular communication. This project provides a basic implementation of interacting with the SIM800 modem using AT commands and performing essential functionalities like making calls, sending SMS, checking battery status, and managing received messages.

## Hardware Requirements

To run this project, you will need the following hardware components:

- TTgo T-Call ESP32 module
- SIM card with an active mobile number (ensure it is not PIN-locked)
- USB-C cable for power and debugging
- A computer with the Arduino IDE and necessary drivers installed

## Library Dependencies

The project relies on the following libraries, which need to be installed in your Arduino IDE:

- [TinyGSM](https://github.com/vshymanskyy/TinyGSM): This library provides support for communicating with GSM modules using AT commands.
- [Wire](https://www.arduino.cc/en/reference/wire): The Wire library enables I2C communication with the TTgo T-Call's power management IC (IP5306).

## Getting Started

To get started with the project, follow these steps:

1. Install the Arduino IDE and ensure that you have the ESP32 board package installed.
2. Install the necessary libraries: TinyGSM and Wire (if not already installed).
3. Connect your TTgo T-Call ESP32 module to your computer using the USB-C cable.
4. Open the `TTgo_T-Call_ESP32_Module_Project.ino` file in the Arduino IDE.
5. Modify the `MOBILE_No` constant to your desired mobile number.
6. Upload the sketch to your TTgo T-Call module.
7. Open the Serial Monitor in the Arduino IDE to view the output and interact with the module.

## Configuration

Before uploading the sketch to your TTgo T-Call module, you may need to modify certain configurations:

- **SIM card PIN**: If your SIM card has a PIN, update the `simPIN` constant with your SIM card's PIN code. If there is no PIN, leave it as an empty string.

## Usage

Once the sketch is uploaded to your TTgo T-Call module, you can use the Serial Monitor to interact with the module. The following commands are available:

- `call`: Initiate a call to the mobile number specified in the `MOBILE_No` constant.
- `sms: <message>`: Send an SMS with the specified `<message>` to the mobile number in the `MOBILE_No` constant.
- `module`: Check for unread messages and display the last received message and its index.
- `battery`: Check the battery voltage of the TTgo T-Call module.

## AT Commands

The project uses AT commands to communicate with the SIM800 modem. Here are some useful AT commands that you can use:

```cpp
// List all SMS messages
AT+CMGL="ALL"

// Read SMS message at index 1
AT+CMGR=1

// Delete SMS message at index 1
AT+CMGD=1

// Enable missed call notification
AT+CLIP=1

// Make a call to +923354888420
ATD+923354888420;

// Hang up the current call
AT+CHUP

// Check if registered to the network
AT+CREG?

// Check signal quality
AT+CSQ

// Check battery voltage
AT+CBC

// Enable verbose error messages
AT+CMEE=2

// Disable verbose error messages
AT+CMEE=0

// Check if verbose error messages are enabled
AT+CMEE?
