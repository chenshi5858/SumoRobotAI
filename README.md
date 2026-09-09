# Embedded Systems Robotics Projects

This repository contains a set of embedded systems labs and a final robotics project built around ESP32-based hardware, real-time firmware, computer vision, audio signal processing, wireless communication, and TinyML inference on microcontrollers.

The main result of the repository is an autonomous sumo-style robot platform. The robot combines an ESP32-S3 "brain" board, ESP32-CAM vision modules, motor control, microphone-based command detection, and embedded decision logic to navigate, avoid ring edges, search for a visual target, and react to override commands.

## Project Overview

The repository documents the progression from low-level ESP-IDF exercises to an integrated multi-device embedded system. The labs cover image processing, performance profiling, memory benchmarking, audio FFT analysis, BLE/Web control, WebSocket telemetry, and TensorFlow Lite Micro. These pieces are then combined in the final project as a distributed robot architecture.

The final project focuses on three core problems:

- Keeping the robot inside the ring using a downward-facing camera.
- Detecting whether a target or beacon is present using a camera and an embedded ML model.
- Coordinating autonomous motion, attack/search behavior, and audio override commands in real time.

## Final Project

The final system is organized as several independent ESP-IDF firmware projects that communicate with each other.

### ESP32-S3 Brain

The `proyecto_final/esp32s3_cerebro` firmware is the central controller. It receives messages from the camera modules, reads microphone command events, manages robot state, and drives the motors.

Responsibilities include:

- Initializing motor PWM and direction pins.
- Receiving floor and beacon camera results over UART.
- Running a coordinator task that merges camera and microphone inputs.
- Executing ring-safety behavior when the floor edge is detected.
- Searching for the beacon when it is not visible.
- Moving forward aggressively when the beacon is detected.
- Supporting microphone override commands for full forward and backward motion.
- Maintaining an open-loop odometry estimate from PWM commands.

### Floor Camera

The `proyecto_final/camara_piso` firmware runs on an ESP32-CAM and is responsible for ring-edge detection. It captures 96x96 RGB565 frames, converts them to luminance, applies a Sobel-style vertical gradient over a region of interest, debounces detections across frames, and sends either `FLOOR_SAFE` or `FLOOR_EDGE` to the brain over UART.

This module acts as the robot's safety system. When an edge is detected, the brain backs up, pauses, rotates, and only resumes normal behavior once the floor is safe again.

### Beacon Camera

The `proyecto_final/camara_beacon` firmware runs a TensorFlow Lite Micro model on 96x96 grayscale images. The model classifies whether the visual identifier or beacon is present. The inference result is sent to the brain over UART as `BEACON_PRESENT` or `BEACON_ABSENT`.

This module enables the robot to switch between search behavior and attack behavior:

- If the beacon is absent, the robot rotates and pauses in a search pattern.
- If the beacon is present, the robot drives forward.
- If the camera link times out, the brain disables beacon mode and falls back to ring-safe behavior.

### Microphone Command Detection

The brain firmware includes an ADC-based microphone handler. It samples audio, applies a Hann window, runs an FFT using ESP-DSP, detects dominant frequencies, and maps specific tones to commands.

Implemented commands include:

- Full forward.
- Backward.

The microphone path includes magnitude thresholds, peak-to-noise validation, confirmation frames, and release frames to reduce false detections.

### Dataset and Training Tools

The `proyecto_final/Tools` folder contains Python scripts and a capture firmware used to build image datasets from the ESP32-CAM. These tools automate grayscale image capture, label generation, positive/negative sample collection, and dataset preparation for training and exporting the embedded identifier model.

## Lab Progression

The repository also includes several lab modules that build up the concepts used in the final robot:

- `Lab_1`: ESP-IDF fundamentals, image processing with Sobel filters, arithmetic profiling, memory benchmarks, ESP32-CAM captures, and audio spectrum analysis.
- `Lab_2`: Motor control over BLE and a React-based web control interface using Web Bluetooth / HTTP-style control patterns.
- `Lab_3`: TensorFlow Lite Micro on ESP, direct model inference comparison, person/identifier detection, ESP32-CAM vision, ESP-NOW communication, ring detection, odometry, and WebSocket pose telemetry.
- `proyecto_final`: Integrated multi-firmware autonomous robot system.

## Architecture

```text
ESP32-CAM floor camera
    -> captures floor image
    -> detects ring edge with image processing
    -> sends FLOOR_SAFE / FLOOR_EDGE over UART

ESP32-CAM beacon camera
    -> captures grayscale image
    -> runs TinyML inference with TensorFlow Lite Micro
    -> sends BEACON_PRESENT / BEACON_ABSENT over UART

ESP32-S3 brain
    -> receives camera messages
    -> processes microphone FFT commands
    -> coordinates robot behavior
    -> controls motors with PWM
    -> estimates open-loop odometry
```

The robot behavior is implemented as a real-time coordination loop that prioritizes safety:

1. Stop or recover if camera links are unavailable.
2. React immediately to ring-edge detections.
3. Allow microphone overrides while preserving autonomous state.
4. Search for the beacon when the ring is safe but the beacon is absent.
5. Move forward when the beacon is detected.

## Tech Stack

| Area | Technologies |
| --- | --- |
| Embedded Firmware | C, C++, ESP-IDF, FreeRTOS |
| Hardware Targets | ESP32, ESP32-S3, ESP32-CAM |
| Vision | RGB565 camera capture, grayscale conversion, Sobel-style edge detection |
| Machine Learning | TensorFlow Lite Micro, quantized image classifier |
| Audio Processing | ADC continuous sampling, FFT, ESP-DSP |
| Communication | UART, ESP-NOW experiments, BLE experiments, WebSocket experiments |
| Motor Control | GPIO direction control, LEDC PWM, differential drive |
| Frontend Experiments | React, Vite, browser-based robot controls |
| Tooling | Python dataset scripts, ESP-IDF build system, CMake |

## Repository Structure

```text
Lab_1/               Introductory ESP-IDF, image, memory, timing, and audio exercises
Lab_2/               BLE motor firmware and web-based robot control experiments
Lab_3/               TinyML, ESP32-CAM vision, ESP-NOW, odometry, and WebSocket labs
proyecto_final/
  esp32s3_cerebro/   Central robot brain firmware
  camara_piso/       Floor/ring-edge detection camera firmware
  camara_beacon/     TinyML beacon detection camera firmware
  shared/            Shared message protocol headers
  test_microfono/    Microphone validation firmware
  Tools/             Dataset capture and labeling tools
```

## Project Highlights

- Multi-board embedded architecture with independent firmware modules.
- Real-time coordination using FreeRTOS tasks and queues.
- On-device computer vision for ring-edge detection.
- On-device TinyML inference using TensorFlow Lite Micro.
- Audio command recognition through ADC sampling and FFT analysis.
- Differential-drive motor control with PWM compensation and open-loop odometry.
- Supporting tooling for dataset collection, labeling, and model deployment.
- Clear progression from lab prototypes to an integrated autonomous robot.

## Current Scope

This repository is an embedded systems coursework and robotics portfolio project. It demonstrates practical experience with low-level firmware, multi-sensor integration, microcontroller communication, signal processing, embedded ML, and real-time robot behavior design.
