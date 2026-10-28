# ESP32 Rover — Custom Controller

## Project Overview

I designed and built an ESP32-based rover controlled through a custom handheld controller.

Initially, I planned to control the rover using my phone. However, I decided to design a physical controller with its own interface, electronics, and 3D-printed enclosure.

This project allowed me to combine programming, electronics, CAD design, and mechanical engineering.

## Hardware

### Rover
- ESP32 microcontroller
- DC motors
- L298N motor driver
- Battery-powered system

### Custom Controller
- ESP32 microcontroller
- Joystick
- LCD display
- Three physical buttons
- Buzzer
- Custom 3D-printed enclosure

## Software Features

I programmed the controller's interface and movement-control logic.

The main features include:

- Joystick-based rover control
- Adjustable joystick sensitivity
- Configurable dead zone
- LCD settings menu
- Real-time motor command display
- Motor command values ranging from 0 to 255
- Wireless communication using ESP-NOW

The displayed motor values are software commands, not measured motor speeds.

## Engineering Challenges

### 3D Printing Failure

During the manufacturing process, one of my prints failed because of a filament jam.

I identified the issue, repaired the printer, and successfully continued manufacturing the controller.

### Motor Power Supply

During testing, I noticed inconsistent motor responses.

After investigating the problem, I improved the power supply, resulting in more reliable motor operation.

### Controller Development

I designed the controller enclosure and internal supports using CAD software.

I also programmed the LCD interface, allowing sensitivity and dead-zone adjustments without modifying the source code.

## My Contributions

I personally worked on:

- Designing the controller and its enclosure
- Assembling the electronics
- Programming the LCD interface
- Implementing sensitivity and dead-zone settings
- Developing and testing movement controls
- Troubleshooting hardware and manufacturing problems



## Current Status

The rover can be controlled using the custom handheld controller.

The LCD displays motor command values in real time and allows adjustments to the joystick response.

## Source Code

The source code for the rover and its controller is included in this repository.

## What I Learned

This project taught me how closely electronics, programming, and mechanical design depend on one another.

I learned that solving engineering problems often requires investigating the interaction between different systems rather than focusing on a single component.



