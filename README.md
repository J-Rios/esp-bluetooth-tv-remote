# ESP Bluetooth TV Remote
ESP32 with an IR LED that allows connection from a classic bluetooth numpad and translate numpad keys into IR signals to control a LG-32LS570S TV.

### Notes
- This is a private project, the code is a mess, don't expect any support about it.

- This project uses BTStack instead ESP-IDF Bluedrid Stack and needs to be included inside ESP-IDF Components. To automatically add it as expected to ESP-iDF, build the project and then remove it from ESP-IDF, use "_make" script:
```
./_make menuconfig
./_make
./_make flash
./_make clean
```

- The "./_make" script expect that you have Platformio espressif-32 platform installed, because the script set ESP-IDF and ESP-IDF Toolchains paths according to standard platformio platform locations (the script build using Platformio internal ESPIDF).

- The project uses ESP32 Arduino core 1.0.4 (ESP-IDF 3.2), so ESP-IDF 3.2 or 3.3.X must be used (for example, ESP-IDF included in platformio platform-espressif32 version 1.11.2).

- Platformio platform-espressif32 version 1.11.2 has an invalid kconfig_new/kconfiglib.py version, change it to [this version](https://github.com/espressif/esp-idf/blob/v3.3.1/tools/kconfig_new/kconfiglib.py).
