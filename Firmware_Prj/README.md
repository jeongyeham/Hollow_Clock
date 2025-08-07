# Hollow Clock Firmware 空心时钟固件

基于 ESP32-S3 的空心时钟固件项目。

Firmware project for a hollow clock based on ESP32-S3.

## 项目简介 Project Introduction

Hollow Clock 是一个使用 ESP32-S3 微控制器实现的嵌入式固件项目，用于驱动和控制空心时钟中的步进电机，以精确显示时间。该项目利用 FreeRTOS 实现多任务管理，通过 PID 控制算法优化电机控制精度。

Hollow Clock is an embedded firmware project using the ESP32-S3 microcontroller to drive and control the stepper motor in a hollow clock for accurate time display. This project uses FreeRTOS for multitasking and PID control algorithm to optimize motor control precision.

## 功能特性 Features

- **FreeRTOS 多任务管理 FreeRTOS Multitasking**: 实现并发处理，包括电机控制、WiFi 初始化和时钟控制等任务
  Concurrent processing implementation, including motor control, WiFi initialization, and clock control tasks

- **步进电机控制 Stepper Motor Control**: 精确控制 28BYJ-48 步进电机的位置和速度
  Precise control of 28BYJ-48 stepper motor position and speed

- **PID 控制算法 PID Control Algorithm**: 使用增量式 PID 控制提高电机控制精度和稳定性
  Using incremental PID control to improve motor control accuracy and stability

- **网络时间同步 Network Time Synchronization**: 通过 SNTP 协议与网络时间服务器同步，确保时间准确性
  Synchronize with network time servers via SNTP protocol to ensure time accuracy

- **智能配置 Smart Configuration**: 支持通过 ESP-Touch 进行 WiFi 智能配置
  Support WiFi smart configuration via ESP-Touch

## 硬件要求 Hardware Requirements

- ESP32-S3 开发板 ESP32-S3 Development Board
- 28BYJ-48 步进电机 28BYJ-48 Stepper Motor
- ULN2003 驱动板 ULN2003 Driver Board
- 电源供应 Power Supply

## 软件架构 Software Architecture

项目采用模块化设计，主要包括以下组件：

The project uses modular design and mainly includes the following components:

- **主应用模块 Main Application Module** (`main`): 负责系统初始化和任务创建
  Responsible for system initialization and task creation

- **步进电机控制模块 Stepper Motor Control Module** (`components/step_motor`): 实现步进电机的具体控制逻辑
  Implements specific stepper motor control logic

- **PID 控制模块 PID Control Module** (`components/pid_ctrl`): 提供 PID 控制算法实现
  Provides PID control algorithm implementation

- **FreeRTOS 任务模块 FreeRTOS Task Module** (`main/FreeRTOS_task`): 管理各种 FreeRTOS 任务
  Manages various FreeRTOS tasks

任务间通过事件组进行通信，确保系统的实时性和可靠性。

Tasks communicate through event groups to ensure system real-time performance and reliability.

## 开发环境 Development Environment

- ESP-IDF v5.x
- CMake 3.28+
- ESP32-S3 开发环境 ESP32-S3 Development Environment

## 构建和烧录 Build and Flash

1. 设置 ESP-IDF 环境 Set up ESP-IDF environment
2. 进入项目目录 Enter project directory
3. 配置项目（可选）Configure project (optional):
   ```
   idf.py menuconfig
   ```
4. 构建项目 Build project:
   ```
   idf.py build
   ```
5. 烧录固件 Flash firmware:
   ```
   idf.py -p PORT flash
   ```
6. 监视串口输出 Monitor serial output:
   ```
   idf.py -p PORT monitor
   ```

## 许可证 License

本项目采用 Apache-2.0 许可证，详情请参见 [LICENSE](LICENSE) 文件。

This project is licensed under the Apache-2.0 License - see the [LICENSE](LICENSE) file for details.