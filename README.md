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

