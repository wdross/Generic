Build of Tasmota that includes seesaw soil and MCP9808 enabled

Tasmota SHA 13a61b562c48a13f19f0793974d855722800d933
 - development branch
 - Tasmota version was 0x09020003 (9.2.0.3)
   (Myrtle was originally built with 8.4.0)

platformio_override_sample.ini: must be removed/renamed/non-existent

platformio_override.ini:
 - Configure COM port for flashing:
   upload_port=COMx

tasmota\user_config_override.h:
 - #define USE_SEESAW_SOIL  // [I2cDriver51] Enable seesaw soil capacitance & temperature sensor (I2C addresses 0x36 - 0x39)
 - #define SEESAW_SOIL_PUBLISH // send only if reported moisture has changed
 - #define SEESAW_SOIL_RAW     // show raw readings on Web interface
 - #define USE_MCP9808      // [I2cDriver51] Enable MCP9808 temperature sensor (I2C addresses 0x18 - 0x1F) (+0k9 code)


The tasmota_12.0.2.bin build is stock firmware, used to flash Sonoff S31 Wifi outlets