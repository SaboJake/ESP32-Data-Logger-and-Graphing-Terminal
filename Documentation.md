====== Data Logger and Graphing Terminal ======
  * **Author: Lupoi Ștefan-Alexandru**
  * **Group: 335CA**
===== Introduction =====
The project consists of a standalone data logging and visualization system based on the ESP32 microcontroller. The device reads data from analog and digital sensors (temperature/pressure, light). The data is also rendered in real time on a color TFT display. The system can be operated independently from a PC using a custom hardware interface and a standard PS/2 keyboard.

**The Scope of the Project:** The main scope of the project is creating a set of portable tools capable of logging and storing physical data whilst also offering the user a local control panel. The user can type commands to modify the sampling rate, graph scale, or to save/load data from memory.

**Starting Idea:** I wanted to build an embedded system that can integrate many communication protocols and peripherals that I studied (SPI, I2C, UART, ADC, PWM, timers, system interrupts) without using a computer. I was inspired by budget oscilloscopes.

**Why It Is Useful:** For regular users, the project serves as a practical and visual instrument for monitoring the environment or testing analog signals. For me as a developer, I wanted to write from scratch a driver for the PS/2 protocol (managing external interrupt timings) and coordinating multiple data buses (high-speed SPI for graphics and I2C for sensors and storing data) without blocking operations.

===== General Description =====
**Block Diagram**
{{ :pm:prj2026:bianca.popa1106:block_diagram_lupoi_stefan-alexandru.png?direct&&800 |}}

**Project Modules and User-System Interaction**

The system is split into hardware and software modules. The system is partially event-driven (using interrupts).

  - **User Input (PS/2 Keyboard and Custom Driver)**
    * //Hardware:// Signals that are coming from the keyboard (5V) go through resistors to protect the ESP32 (3.3V).
    * //Software:// The ISR listens to the CLK pin of the keyboard and receives the data bits at every negedge cycle. The scan codes are decoded and inserted into a buffer.
  - **Data Reading (Sensors and Timers)**
    * //Hardware/Software:// A hardware timer dictates the reading of data from the sensors in a strict interval (ex. default = 100ms, may be changed by the user). The module reads the data from the I2C bus (BMP180) or from the ADC pins (photoresistor) and stores the values in a circular array.
  - **Non-volatile Storage (SD Card)**
    * //Hardware/Software:// The system uses a microSD card reader integrated into the TFT display, communicating over the shared SPI bus (with chip select GPIO 16). Recorded data is written directly to binary files (e.g. ''.bin'') on the card. Dynamic paging allows viewing datasets much larger than the ESP32's RAM capacity by streaming blocks on-demand during zooming and panning.
  - **Graphics (SPI TFT Display)**
    * Using the high-speed TFT_eSPI library, the screen can render the graph smoothly. The refresh rate is optimized by drawing the "delete rectangles" ahead of the new data, thus bypassing the flickering effect. The brightness of the screen is adjustable through software using a PWM signal on the backlight (LED) pin.

===== Hardware Design =====
**Bill of Materials:**
  * **Microcontroller:** ESP32-WROOM-32D (DevKitC V4)
  * **Display & Storage:** TFT SPI 2.4 inch display module (240x320 resolution) with integrated SD card reader slot
  * **Non-volatile Storage:** MicroSD Card (formatted in FAT32) utilizing display's integrated reader
  * **Input:** Standard PS/2 keyboard and female PS/2 connector (modified extension cable)
  * **Non-volatile Memory:** EEPROM I2C AT24C256 (32K) (reserved / legacy storage)
  * **Digital Sensors:** BMP180 (I2C) pressure and temperature sensor
  * **Analog Sensors:** Photoresistor (type 5528) + 10kΩ resistor (voltage divider), 10kΩ potentiometer
  * **Resistors**: 1kΩ and 10kΩ resistors
  * **Transistor**: BC547B NPN transistor 
  * **Infrastructure:** Breadboard and wires (female-male, male-male)
  * **Power:** TBD

**Electrical diagram**

{{ :pm:prj2026:bianca.popa1106:electrical_diagram_lupoi_stefan-alexandru.png?direct&&800 |}}

===== Software Design =====

**IDE:**
  * **PlatformIO** (typically used as an extension within Visual Studio Code). The build environment is configured via ''platformio.ini'' utilizing the Arduino framework for the ESP32 (''espressif32'' platform).

**Libraries and 3rd Party Resources:**
  * **TFT_eSPI (by Bodmer, v2.5.43):** A high-performance, hardware-accelerated graphics library specifically configured for driving the ST7789 TFT display over high-speed SPI.
  * **LVGL (Light and Versatile Graphics Library, v9.1.0):** The core UI framework powering the graphical elements, including the interactive menus, the terminal overlay, and the real-time data charting system.
  * **Adafruit BMP085 Library:** Used for interfacing with the BMP180 sensor over I2C to gather atmospheric pressure and temperature data.
  * **FS / SD / SPI (Standard ESP32 Arduino Core):** Used to interface with the SD card filesystem, manage file streaming/paging, and configure shared high-speed SPI bus communication.
  * //(Note: Adafruit GFX and Adafruit ST7789 libraries are included in dependencies for legacy support but superseded by TFT_eSPI/LVGL in the active application).//

**Algorithms and Data Structures Used (or Planned):**
  * **Linear Data Buffer (Struct Array) and Cache:** A static array ''data_buffer[MAX_SAMPLES]'' (with ''MAX_SAMPLES'' set to 500) consisting of custom ''SensorSnapshot'' structs. During active recording, it buffers live data. When inspecting loaded files from the SD card, it serves as a sliding window cache.
  * **Dynamic File Paging Algorithm:** When inspecting logs larger than RAM capacity, the graphing engine (''update_chart_source'') tracks the visible index range ''[pan_offset, pan_offset + visible_points]''. If this range is not fully contained in ''data_buffer'', it performs a cache-miss SD card read, seeking to the correct byte offset and paging in up to ''MAX_SAMPLES'' elements. This allows smooth local navigation of huge datasets.
  * **Circular Buffer (Ring Buffer):** Used within the custom PS/2 keyboard driver (''_rawBuffer'') to safely enqueue raw, asynchronous hardware scan codes triggered by GPIO interrupts without blocking the main thread.
  * **State Machines:**
    * **Keyboard Decoding:** A state machine tracks modifier keys (Shift, Ctrl, Caps Lock, Extended codes) to accurately translate complex multi-byte PS/2 scan sequences into ASCII characters.
    * **Application Flow Control:** An ''AppState'' enum (''STATE_INITIAL'', ''STATE_RUNNING'', ''STATE_INSPECT'') dictates the high-level system behavior, preventing invalid actions like zooming while actively recording data.
  * **Data Windowing and Mapping Algorithms:** The graphing engine (''update_chart_source'') calculates visible data slices using dynamic array indexing offsets (''pan_offset'') and active time windows (''scale_x_window''). It dynamically remaps sensor data into the display's visual space to accomplish real-time panning and zooming.

**Sources and Implemented Functions:**
  * **''main.cpp'':** The system orchestrator. Initializes the hardware peripherals, bridges LVGL with the display (''my_disp_flush'') and the keyboard driver (''keypad_read_cb''), maps keyboard shortcuts to system commands, and drives the main UI loop (''lv_timer_handler'').
  * **''graph.cpp'' / ''graph.h'':** Manages the LVGL charting subsystem, data acquisition, dynamic paging, and SD card file state persistence. 
    * //Key functions:// ''init_graph()'', ''recording_task()'', ''update_chart_source()'', ''cmd_start_recording()'', ''cmd_zoom()'', ''cmd_pan()'', ''cmd_save()'', ''cmd_load()''.
  * **''terminal.cpp'' / ''terminal.h'':** Implements the graphical terminal overlay and its text parser, allowing users to type and execute commands that interface with the graphing logic. 
    * //Key functions:// ''init_terminal_ui()'', ''toggle_terminal()''.
  * **''menu.cpp'' / ''menu.h'':** Controls the GUI-based settings menu, dynamic SD card directory scanning for saving/loading logs, and parameter selection submenus. 
    * //Key functions:// ''init_menu()'', ''toggle_menu()'', ''open_load_submenu()'', ''open_save_submenu()'', ''open_param_submenu()''.
  * **''ps2_keyboard.cpp'' / ''ps2_keyboard.h'':** A bare-metal PS/2 keyboard driver utilizing hardware interrupts to capture user inputs. 
    * //Key functions:// ''begin()'', ''read()'', ''handleInterrupt()''.

===== Rezultate Obţinute =====

{{:pm:prj2026:bianca.popa1106:poza_proiect_1_lupoi_stefan-alexandru.jpeg?direct&&400|}} {{:pm:prj2026:bianca.popa1106:poza_proiect_2_lupoi_stefan-alexandru.jpeg?direct&&400|}}

===== Concluzii =====

===== Download =====
  * [[https://github.com/SaboJake/ESP32-Data-Logger-and-Graphing-Terminal|github]]

===== Jurnal =====
  * **06.05.2026:** Added introduction, general description and block and electrical diagrams.
  * **19.05.2026:** Added firmware, bibliography and resources. Updated electrical diagram and block diagram.
  * **24.05.2026:** Replaced simulated EEPROM with real SD Card storage. Implemented dynamic cache-miss file paging for large files, a custom filename Save submenu, and a sensor parameter Load submenu.
  * **25.05.2026:** Updated diagrams, added photos

===== Bibliografie/Resurse =====
  * [[https://documentation.espressif.com/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf|Data sheet ESP32-WROOM-32D]]
  * [[https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf|Data sheet AT24C256]]
  * [[https://cdn-shop.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf|Data sheet BMP180]]
  * [[https://wiki.osdev.org/PS/2_Keyboard|Data sheet PS/2 keyboard]]

<html><a class="media mediafile mf_pdf" href="?do=export_pdf">Export to PDF</a></html>

