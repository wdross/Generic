SONOFF S31:
- Connect pins to flash unit directly:
  - Remove grey cover over the WiFi button
  - slide out 2 corner covers
  - Remove 3 Phillips screws
  - lift out circuit board assembly
  - reference PinOut.png to make clamp connections to circuit board pads
  - reference "My FTDI cable" to ensure power jumper is set to 3.3v  === NOT 5V! ===

- Use Windows C:\Generic\TasmotaConfigs\SONOFF\tasmotizer-1.2.exe to flash Tasmota:
  - Run executable
  - chose the binary to flash: C:/Generic/TasmotaConfigs/Firmware/tasmota_12.0.2.bin
  - press-and-hold the WiFi button on the S31 while connecting the USB power
    o hold for ~5s after USB
  - Select the COM port the USB FTDI adapter connects as (COM8)
  - press Tasmotize!

- Get on the WiFi:
  - reboot by unplugging and plugging in the USB
  - use phone to connect to new temporary 'access point'
  - tell S31 to associate with internalWiFi/pw
  - after reboot, install standard config:
    o IP available from Tasmotizer's "Get IP" button
    o Configuration ->
      Restore Configuration ->
      C:/Generic/TasmotaConfigs/Firmware/DefaultConfig_12.0.2.dmp
  
  Looks like these are the elements configured in there:
    o # Put hostname and IP on web page
    o SetOption53 1
    o # Home Assistant discovery
    o SetOption19 0
    Backlog SetOption53 1; SetOption19 0


- wrap up
  - disconnect USB
  - disconnect clips
  - reassemble S31 in opposite order as disassembly

- Should show up in Home Assistant ==> Settings ==> Integrations ==> Tasmota

2023-01-17:
 - looking to increase the responsiveness after tea kettle is turned on to have the current update in HA
 - implemented two things:
   'powerlow 2' will report if the wattage drops to under 2w
   'powerhigh 700' will report if the wattage jumps over 700w (our kettle is ~1400w)
2023-12-23:
 - "Water Garden" was configured to turn off after one hour in case Home Assistant didn't send OFF
 - Rule1 ON Power1#state=1 DO RuleTimer1 3600 ENDON ON Rules#Timer=1 DO Power off ENDON
 - During Christmas Season (to prevent this rule):
   "Rule1 0" is sent via "Christmas On" Automation
 - In summer (to enforce this rule):
   "Rule1 1" is sent via "Water Garden" Automation

 'ampres' configures the resolution of reported current (amperage, default 3 digits)
 'wattres' watt resolution (default 0)
 'voltres' (default 0)
 'powerdelta 800' will report if the wattage changes by 700 watts (700+100); our kettle is ~1400w
