Hardware:
- Huzzah breakout (https://www.adafruit.com/product/2471)                        $9.95
- Non-Latching MiniRelay (https://www.adafruit.com/product/2895)     $7.95ea    $15.90
  Relay good for up to 60 Watts (About 5A @ 12VDC; 0.6A @ 120VAC)
- Feather stacking header (https://www.adafruit.com/product/2830)                $1.25
- Feather female header (https://www.adafruit.com/product/2886)                  $0.95
- MCP9808 breakout board (https://www.adafruit.com/product/1782)                 $4.95
- Adjustable buck converter (http://amazon.com/gp/product/B018JWCX8M) 1 of 6pcs  $1.33
- 2x5 IDC Sockets (http://amazon.com/gp/product/B00DE5CTMK)          1 of 50pcs  $0.19
Miscellaneous wire and solder
                                                                                ------
                                                                                $34.52
Firmware built on Tasmota 9.2.0.1
from https://github.com/arendst/Tasmota (SHA 668448a)

The only customization required is to enable support for the MCP9808:
Place this in user_config_override.h: << EOF
#ifndef USE_MCP9808
#define USE_MCP9808      // [I2cDriver51] Enable MCP9808 temperature sensor (I2C addresses 0x18 - 0x1F) (+0k9 code)
#endif

EOF


Then get the device on your WiFi (rosshome is current)

After that the following items have been entered via the Console:
# Set up the possible I/Os:
Template {"NAME":"Thermostat","GPIO":[255,0,255,0,255,255,0,0,255,255,255,255,255],"FLAG":15,"BASE":18}
# Restart:
Module 0
# Sets up I2C bus and can find all temperature sensors MCP9808 (and report in Fahrenheit)
Backlog GPIO4 640; GPIO5 608; SetOption8 1
# Configure desired hostname on network; display hostname and IP on web page:
Backlog hostname ShopThermometer; SetOption53 1



                       >>> Dad left off here <<<



# Configure your Mqtt server
# - substitute your IP Address for _IP_ADDRESS_
#                mqtt Username for _USER_
#                mqtt password for _PASSWORD_
Backlog MqttHost _IP_ADDRESS_; MqttUser _USER_; MqttPassword _PASSWORD_
# Home Assistant discovery
SetOption19 1
# temperature updates every 10 seconds (default is 300 seconds)
TelePeriod 10
# Ensure upon boot that the relay is OFF until commanded by HomeAssistant:
PowerOnState 0
# Ensure relay is on for a maximum amount 180 seconds (in case HA turns on the furnace then stops processing);
# the value in PulseTime is desired_max_time+100 so 180 seconds is 280:
PulseTime 280
