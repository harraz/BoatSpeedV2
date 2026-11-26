# GPS connection and Port button update

The controller is the Hosyond D1 mini ESP8266. GPS and compass are separate interfaces on the FW10: GPS uses UART serial; compass uses I2C.

## Connections

| Device connection | D1 mini connection |
| --- | --- |
| FW10 TX | D6 / GPIO12 (GPS receive input) |
| FW10 RX | Unconnected; this sketch sends no GPS commands |
| FW10 5V | Existing 5V supply |
| FW10 GND | Common GND |
| FW10 SDA | D2 / GPIO4 |
| FW10 SCL | D1 / GPIO5 |
| Port button | A0, through the circuit below |
| Starboard button | D0 / GPIO16, existing external 10k pull-up |
| Reference button | D3 / GPIO0, internal pull-up; release during reset |
| TFT SCL | D5 / GPIO14 |
| TFT SDA | D7 / GPIO13 |
| TFT DC | D4 / GPIO2 |
| TFT CS | D8 / GPIO15 |
| TFT RST | Board RST |
| TFT VCC / GND / BL | Existing 5V / GND / 3.3V connections |
| TFT SDA-O | Unconnected |

The GPS TX signal must use 3.3V-compatible logic; the FW10's 5V power supply does not mean its serial signal should be 5V.

```text
3.3V -- Port button -- 10k ohm --+-- A0
                                |
                             10k ohm
                                |
                               GND
```

Remove the old Port button connection to D6. The two resistors above are additional to the Starboard button resistor. Released A0 is near 0V. Pressed is approximately 1.6V (the board's own input divider loads it slightly). The product listing specifies A0 up to 3.2V: https://www.amazon.com/gp/product/B09SPYY61L . This analog voltage is read with analogRead(), rather than digitalRead().

## Code

- src/main.cpp: Port button now uses A0. Readings at or above 350 mean pressed; at or below 150 mean released. Between those thresholds the previous state is retained, then the normal 25ms debounce applies. Analog samples are at least 5ms apart.
- src/gps.cpp and include/gps.h: receive-only SoftwareSerial on D6; TinyGPSPlus parses checksum-verified NMEA sentences. The receiver tries 115200, 9600, 38400, 57600, and 19200 baud, five seconds per rate. Two valid sentences lock the rate. Ten seconds without a valid sentence restarts detection. This changes only the ESP's receive setting, not the FW10 configuration.
- src/display.cpp and include/display.h: display SOG in knots only when both position and speed are valid and less than three seconds old. Otherwise show --.-, with NO GPS DATA, CHECK GPS LINK, or WAITING FIX. Zero speed is a valid value; it does not mean GPS data is missing.
- platformio.ini: adds TinyGPSPlus 1.1.0. SoftwareSerial is supplied by the installed ESP8266 framework.

GPS processing runs between screen transfers and compass work. The compass continues to supply heading; GPS supplies speed over ground. No GPS course is substituted for compass heading.

## First hardware check

1. Upload the new firmware. No serial watcher is started automatically.
2. Check the Port button selects Port and Reference still saves the reference.
3. With the GPS powered, look for GPS: NMEA detected at ... baud in the 115200-baud USB serial monitor. Detection does not require a satellite fix.
4. Outdoors with an open sky, wait for GPS FIX and a numeric SOG. At rest some small speed variation is normal.
5. Disconnect only FW10 TX from D6: after about three seconds the screen should clear the speed and show NO GPS DATA. The compass and buttons should continue to work. Reconnect and allow detection to recover.

Serial diagnostics print every two seconds, including checksum and receive overflow counters. Increasing overflows indicate that receive timing/buffering needs attention, especially at high baud rates. If there are bytes but no valid NMEA sentences across all rates, check the GPS output protocol: this version reads NMEA, not binary UBX. No baud rate or protocol has been confirmed on the physical FW10 yet.

## Implementation references

- TinyGPSPlus parser: https://github.com/mikalhart/TinyGPSPlus/tree/v1.1.0/src (isValid(), age(), speed.knots(), encode(), and checksum counters).
- EspSoftwareSerial: installed ESP8266 framework libraries/SoftwareSerial/src/SoftwareSerial.h (receive-only begin() with TX pin -1, byte/edge buffer sizes, overflow()). Upstream: https://github.com/plerup/espsoftwareserial .

The build checks compilation. Actual baud detection, reception alongside TFT drawing, and analog button thresholds still need the hardware checks above.
