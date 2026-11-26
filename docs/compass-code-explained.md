# How the D1 mini talks to the FW10 compass

A guided introduction to the BoatSpeed v2 compass test, for readers who know basic C. Follow the sections in order: each builds the vocabulary needed by the next. The examples describe the working IST8310 test, not the GPS inside the same module.

## 1 Start with the question our program asks

We want the D1 mini to ask the compass:

> Measure the magnetic field, then give me the results along your three sensor axes.

An axis is a direction fixed to the sensor: X, Y, or Z. The compass measures a magnetic-field component along each one. Rotating the sensor changes those components. A component can be positive or negative depending on the field's direction along that axis.

The results look like this:

```text
X: 52   Y: -71   Z: -87
```

These are raw sensor counts: integer readings before conversion or calibration. They are not degrees. The program currently proves that we can request and retrieve measurements. Calculating boat heading comes later.

There are three participants:

```text
D1 mini                    FW10 compass
runs our program  ←→       takes magnetic measurements
    |
    └── USB connection ──→ computer displays our printed messages
```

The USB messages and the compass conversation are separate. `Serial` is the Arduino interface used for computer messages. `Wire` is the Arduino interface used to communicate with the compass.

The FW10 also contains a GPS receiver. This program does not communicate with that receiver.

## 2 Meet the wires before the code

Power makes the module work. Ground, abbreviated GND, provides the common electrical reference. Two additional wires carry the compass conversation:

| Name | Full name | Purpose |
|---|---|---|
| SDA | Serial Data | Carries the information, one bit at a time |
| SCL | Serial Clock | Provides timing pulses so both devices agree when to read each bit |

The communication rules used on these two wires are called **I2C**, pronounced “eye squared see” or “eye two see.” A protocol is simply an agreed set of communication rules.

The D1 mini starts the exchanges and provides the clock. In I2C terminology it is the **controller**. The compass answers it; it is the **target**, also often called a peripheral. Older documentation may call these master and slave.

Our connections are:

| FW10 | D1 mini |
|---|---|
| SDA | D2, which is GPIO4 |
| SCL | D1, which is GPIO5 |
| GND | GND |
| 5V | Suitable 5 V supply |
| TX, RX | Not connected for this test |

GPIO means a general-purpose input/output connection on the processor. D1 and D2 are labels on the D1 mini board, not GPIO1 and GPIO2. The board definitions translate those labels for the program.

We measured the powered module's idle SDA and SCL at 3.3 V before connecting them. Its 5 V power requirement does not mean its data wires should carry 5 V. If the FW10 has a separate supply, its ground must still connect to the D1 mini's ground.

TX and RX belong to the GPS receiver's separate serial connection. They are not alternative names for SDA and SCL.

## 3 How a wire carries a number

A **bit** is one binary digit, either 0 or 1. On our data wire a low voltage represents 0 and a high voltage represents 1. Think of low as near ground and high as near 3.3 V for this test; the actual accepted voltage ranges are electrical specifications.

A **byte** contains eight bits. Interpreted as a nonnegative number, its 256 possible patterns represent values from 0 to 255.

```text
Bit position:    7   6   5   4   3   2   1   0
Place value:  128  64  32  16   8   4   2   1
Bits for 14:    0   0   0   0   1   1   1   0
                              8 + 4 + 2 = 14
```

### The problem a clock solves

SDA can show only one electrical level at a time. To send a byte, the sender must present eight bits in sequence. But how does the receiver know when one bit ends and the next begins?

Consider two consecutive 1 bits. SDA can remain high for both. There is no voltage change between them to announce “this is another bit.” Watching SDA alone would not tell us whether we had received one 1, two 1s, or a longer run of 1s.

SCL solves this by providing an agreed rhythm. Here **clock** means a sequence of electrical pulses, not the time of day. Each pulse has a low part and a high part. One pulse marks one bit position. Two high data bits accompanied by two clock pulses are two separate 1 bits, even though SDA never changed between them.

The D1 mini generates these pulses. It does this both when it sends data and when the compass sends data back. The direction of the information can change without changing who provides the timing.

### One bit from preparation to observation

For an ordinary data bit:

1. With SCL low, the sender establishes the next level on SDA.
2. SCL rises to high. The receiver observes the data level during this high interval.
3. The sender keeps SDA unchanged while SCL is high, so there is a clear, stable value to observe.
4. SCL returns low. The sender can prepare the following bit.

The receiver does not need to guess how long a bit lasts from its own timer. It follows the clock supplied on SCL. We will introduce special beginning and ending signals in Section 6; those use different SDA transitions from ordinary data bits.

### Walk through all eight pulses

Suppose the sender wants to transmit decimal 14, binary `00001110`. I2C sends the highest-place bit first:

| Clock pulse | Bit being sent | SDA during the high part of this pulse | Receiver adds to its collected bits |
|---|---|---|---|
| 1 | Bit 7 | Low | `0` |
| 2 | Bit 6 | Low | `00` |
| 3 | Bit 5 | Low | `000` |
| 4 | Bit 4 | Low | `0000` |
| 5 | Bit 3 | High | `00001` |
| 6 | Bit 2 | High | `000011` |
| 7 | Bit 1 | High | `0000111` |
| 8 | Bit 0 | Low | `00001110` |

Pulses 5, 6, and 7 all observe high SDA. They are three bits because there are three clock pulses, not three data transitions.

This diagram shows that part of the example. The vertical bars mark separate pulse positions; high/low refers to electrical levels:

```text
Pulse number:         5          6          7          8
SCL:              ___/‾‾‾\___/‾‾‾\___/‾‾‾\___/‾‾‾\___
SDA when SCL high:     1          1          1          0
```

The diagram illustrates timing rather than exact electrical rise times. At our requested clock rate of 10000 pulses per second, one full pulse cycle is nominally 100 microseconds. Extra software time and device behavior can make an actual exchange longer. Section 6 will explain the extra response pulse after the eight data bits.

The receiver is not seeing printed text. It collects timed electrical levels and interprets the resulting pattern as a byte. Wire handles this activity underneath our C++ calls.

## 4 Why we write numbers as 0x0E

We now have a byte of bits. We need a convenient way to write its value in code and documentation. This is where **hexadecimal**, abbreviated hex, helps.

### Start with familiar decimal place values

In ordinary decimal notation, a digit's position changes its contribution. In 142:

```text
142 = 1 × 100 + 4 × 10 + 2 × 1
```

Decimal uses ten possible digits, 0 through 9. After 9, we carry into the next place and write 10. Each position is worth ten times the one to its right.

Binary follows the same principle but uses two digits, 0 and 1. Its positions are worth 1, 2, 4, 8, 16, and so on. The binary value `00001110` is therefore 8 + 4 + 2 = 14.

Hex uses sixteen possible digits. We use 0–9 for the first ten, then A–F for the remaining six:

| Hex digit | Decimal value | Four binary bits |
|---|---:|---|
| 0 | 0 | `0000` |
| 1 | 1 | `0001` |
| 2 | 2 | `0010` |
| 3 | 3 | `0011` |
| 4 | 4 | `0100` |
| 5 | 5 | `0101` |
| 6 | 6 | `0110` |
| 7 | 7 | `0111` |
| 8 | 8 | `1000` |
| 9 | 9 | `1001` |
| A | 10 | `1010` |
| B | 11 | `1011` |
| C | 12 | `1100` |
| D | 13 | `1101` |
| E | 14 | `1110` |
| F | 15 | `1111` |

After F, we carry to the next hex position and write 10. That means decimal 16. Hex positions are worth 1, 16, 256, and so on.

### Read the prefix and calculate the value

In C++, `0x` says “read the following digits as hexadecimal.” The prefix itself contributes no numeric value.

```text
0x0E = 0 × 16 + 14 = 14 decimal
0x10 = 1 × 16 +  0 = 16 decimal
0x2A = 2 × 16 + 10 = 42 decimal
0xFF = 15 × 16 + 15 = 255 decimal
```

Writing `0x0E`, `0xE`, or decimal `14` in our address constant produces the same numeric value. We use two hex digits to make the byte-sized pattern easy to see. This formatting does not change the transmitted information.

### Convert between hex and bits without decimal arithmetic

Sixteen is 2 to the fourth power, so one hex digit describes exactly four bits. That gives us a direct visual conversion: split a byte into two groups of four and replace each group with its hex digit.

```text
Binary byte:       0000 1110
Hex digits:           0    E
Written in C++:    0x0E

Binary byte:       0001 1100
Hex digits:           1    C
Written in C++:    0x1C
```

This is why electronics documentation favors hex. It is shorter than binary while keeping the bit groups visible. For example, `0x01` corresponds to `00000001`, clearly selecting only the rightmost bit; `0x80` corresponds to `10000000`, selecting only the leftmost bit.

### The same notation serves several different jobs

Hex numbers are not inherently commands. We must know their role:

| Value in this project | What that occurrence means |
|---|---|
| Device address `0x0E` | Which chip the controller selects |
| Register address `0x00` | Which internal location to read |
| Returned identification `0x10` | A value read from that location |
| Register address `0x0A` | Which internal control location to write |
| Written value `0x01` | An instruction interpreted by that control location |
| Mask `0x01` | A pattern used by our software to inspect one bit |

Section 5 explains addresses and registers. Notice already that the same number, `0x01`, can be a written command value in one context and a software bit-selection pattern in another.

We also use ordinary decimal values where they are easier to understand: `delay(250)` means 250 milliseconds, and the displayed `Y: -71` is a signed decimal measurement. Hex notation would not make those particular meanings clearer.

### Check your understanding

- `0x10` is decimal 16, not decimal 10.
- `0x0A` is decimal 10, because A is the digit representing ten.
- `0x1C` is binary `00011100`, or decimal 28.
- `0xFF` is one byte with every bit set; its signed interpretation is a separate question we address when decoding measurements.

The chip does not receive the characters “0”, “x”, “0”, “E”. The compiler turns our notation into a numeric value, and Wire transmits the appropriate bits.

## 5 How we choose the device and the information inside it

### Why an address is required when we have only one compass

Right now, only the compass is connected to our I2C wires. We still must send its **device address**, a number that selects which device should respond. Addressing is part of the I2C exchange format even when there is only one target.

The D1 mini cannot look at the wiring and infer “there is only one device, so send everything to it.” The compass's communication circuitry is designed to listen for its configured address before accepting the following bytes. A physical connection alone does not select it. Sending the wrong address will normally receive no response, even though the intended device is powered and connected.

Our compass answers at `0x0E`. This address is seven bits long. The chip's address configuration determines what it accepts; it is not a number our application can assign freely. The sketch's `ADDRESS` constant must match the hardware. Changing only the constant does not change the compass's address.

I2C allows devices to share the same SDA and SCL connections. That shared pair is called a **bus**. For example, a future compatible display could share it with the compass, provided they have distinct addresses and compatible electrical requirements:

```text
D1 mini SDA ──┬── compass SDA
              └── display SDA
D1 mini SCL ──┬── compass SCL
              └── display SCL
```

Both would receive the address bits, but only the matching device should answer. We are not adding that display here; this illustrates why the protocol includes selection. Two devices configured to the same address can conflict and need a different hardware arrangement.

### What our scanner established

The scanner tries possible device addresses and reports those that answer. It found `0x0E`, so we used that observed value in the next test. The scanner did not ask for a magnetic measurement or identify the chip model. We then read an identification value and compared it with the documented expectation for the IST8310.

The FW10's GPS is also physically on the module, but this program does not address it through these compass I2C connections. Its TX/RX connection is separate.

### Choose an internal register after selecting the chip

The compass also exposes numbered locations called **registers**. A register is a small hardware storage location or control interface inside the sensor. It is not a C variable in the ESP8266. Here each register location transfers one byte.

Different registers serve different purposes:

- An identification register tells us which chip answered.
- A control register accepts a setting or a request to perform an action.
- A status register tells us whether that action is finished.
- Data registers provide the result.

The chip designer defines the register numbers and their meanings. We must use the map for the IST8310, not the map for another compass.

| IST8310 register address | What our program does there |
|---|---|
| `0x00` | Read chip identification; expect contents `0x10` |
| `0x02` | Read status and check whether a measurement is ready |
| `0x03`, `0x04` | Read the two bytes representing X |
| `0x05`, `0x06` | Read the two bytes representing Y |
| `0x07`, `0x08` | Read the two bytes representing Z |
| `0x0A` | Write contents `0x01` to request one measurement |

There are three different numbers in a request to take a measurement:

```text
Device address:       0x0E   Which chip?
Register address:     0x0A   Which control inside that chip?
Value written:        0x01   What action do we request there?
```

The number `0x01` alone does not mean “measure.” That meaning comes from writing it to this particular chip's control register.

Our program also has local variables called `id` and `status`. They hold copies of bytes read from registers. Changing a local copy does not change the sensor.

Reference for this map: [PX4's IST8310 driver, hardware definitions at source lines 89–135](https://px4.github.io/Firmware-Doxygen/d3/dbd/ist8310_8cpp_source.html). The definitions and measurement request are also present in [ArduPilot's IST8310 driver](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Compass/AP_Compass_IST8310.cpp). Neither is a FW10 board schematic.

## 6 How an exchange begins and how the receiver answers

Before tracing a command, we need three more communication rules.

### How a chip lets the wire go high

Both SDA and SCL have resistors connecting them toward the signal supply, 3.3 V in our measured setup. These are called **pull-up resistors**.

Inside a chip, an electronic switch can connect the signal wire toward ground. Closing that switch pulls the voltage low. Opening the switch stops that pull toward ground; the resistor then brings the voltage high, provided the other chip is not pulling it low.

“Release the wire” means open that electronic switch. Nothing physically holds or releases the wire. This electrical arrangement is called **open-drain** signaling.

In particular, both devices can take turns using SDA without one actively driving it high while the other pulls it low. A released wire is high only if nobody else is pulling it down.

### START and STOP mark the exchange boundaries

Ordinary data bits change while SCL is low. I2C uses two special changes while SCL is high:

- **START:** SDA changes from high to low. It announces the beginning of an exchange.
- **STOP:** SDA changes from low to high. It announces the end.

START and STOP are electrical conditions, not extra bytes or ASCII words sent to the chip.

### ACK means the receiver acknowledges a byte

After eight clock pulses have transferred a byte, there is a ninth pulse. The byte's sender releases SDA for this pulse. The receiver pulls SDA low to acknowledge the byte. This is an **ACK**.

If SDA stays high on that ninth pulse, there is no acknowledgment: **NACK**. During writes it can mean the device did not answer or did not accept a byte. During reads, the controller deliberately uses NACK after the final requested byte to say “I am finished reading.” Context matters.

```text
Clock pulses:   1  2  3  4  5  6  7  8  | 9
Purpose:        eight bits from sender | receiver response
```

An ACK confirms acceptance at the communication level. It does not mean the sensor has already finished measuring or that the measurement is accurate.

The rules in Sections 3 and 6 follow [NXP's I2C specification, Sections 3.1.1–3.1.6](https://www.nxp.com/docs/en/user-guide/UM10204.pdf), which include timing drawings. The worked compass examples below apply those rules to our actual register calls.

### Analogy One shared walkie talkie and a bell

Imagine the D1 mini and compass sharing one walkie-talkie. Only one person speaks at a time. The D1 mini also rings a bell that both can hear.

- The shared walkie-talkie represents SDA, the data wire.
- The bell represents SCL, the clock wire.
- Each spoken digit is one bit: 0 or 1.
- Eight digits form one byte.

Before starting, both agree on the turn-taking rule: **the sender speaks for eight bell rings; the receiver answers on the ninth**. In the real devices this agreement is implemented by their communication software and circuitry, not negotiated in English.

When the D1 mini sends a byte:

```text
Bell ring:       1 2 3 4 5 6 7 8 | 9
D1 mini:         sends eight bits | listens
Compass:         listens         | gives ACK or NACK
```

When the compass returns a measurement byte:

```text
Bell ring:       1 2 3 4 5 6 7 8 | 9
Compass:         sends eight bits | listens
D1 mini:         listens          | gives ACK or NACK
```

The D1 mini supplies the clock in both cases. Creating timing pulses is separate from supplying data. The SDA wire has one assigned sender at a time.

### Why the compass does not interrupt

The compass does not decide to speak whenever it has a measurement. It waits until the D1 mini addresses it for reading. It then sends bytes in the agreed time slots. When the D1 mini is writing, the compass listens during the eight data pulses and answers only during the ninth.

Both follow the same sequence of clock pulses. Before the response pulse, the sender stops pulling SDA down so the receiver can answer. After that pulse, the receiver stops pulling it down so the next sender can take its turn. This is what changing direction on the shared wire means.

The communication is **half-duplex**: information can travel either way, but the two devices take turns. It is not full-duplex, which would allow independent messages in both directions simultaneously. SCL is a timing path, not a second data path.

### The bell is not continuously ringing

SCL is not an endlessly running background clock. The D1 mini generates pulses when performing an exchange and can pause between exchanges. While our program waits 250 milliseconds before its next measurement request, it is not sending a continuous stream of I2C clock pulses.

The receiver does not need a guess about when our program will next run. START marks a new exchange, the address selects the recipient and direction, and the following pulses organize the bits and response turns.

### The ninth bit does not always mean done

The ninth pulse belongs to the receiver of the preceding byte:

| Transfer | Receiver's response | Meaning |
|---|---|---|
| D1 mini sends an address or command byte | Compass ACK | The address or byte was accepted; not proof the measurement is complete |
| Compass sends a measurement byte before the last one | D1 mini ACK | The read can continue with another byte |
| Compass sends the final requested measurement byte | D1 mini NACK | The controller is ending this read |

There is a response after **every byte**, not just one response after the whole measurement. For six returned bytes, our library generates five ACK responses and a final NACK, followed by STOP.

### What if both try to send

Normal operation avoids this through the agreed turns. If faulty hardware, incorrect software, or a wiring problem makes both control SDA unexpectedly, the data can be corrupted or the exchange can fail. Either device pulling SDA low makes the shared line low; there is no automatic mixing of two independent messages into something readable.

The walkie-talkie analogy explains turn-taking, not the electrical details. On the real wire, a device sends high by stopping its pull toward ground and letting the pull-up resistor raise the voltage. A listening device likewise avoids pulling the data wire down except during its assigned response turn.

## 7 Example one Request a measurement

Here is the software request:

```cpp
writeRegister(0x0A, 0x01);
```

The helper uses device address `0x0E`. The library adds one direction bit: 0 for writing to the device, 1 for reading from it. This makes an eight-bit address-and-direction byte on the wire.

```text
7-bit address:          0001110       = 0x0E
Address plus write:    0001110 0      = 0x1C
Address plus read:     0001110 1      = 0x1D
```

Pass **0x0E** to Wire. Do not pass 0x1C or 0x1D: Wire constructs the direction bit. A measurement instrument used to inspect the wires may display either the 7-bit address or the combined byte, depending on its settings.

The complete successful write looks like this:

```text
D1 mini: START
D1 mini: 0x1C    → compass ACK
D1 mini: 0x0A    → compass ACK
D1 mini: 0x01    → compass ACK
D1 mini: STOP
```

Read this aloud as:

> Start an exchange. Address the compass for writing. Select its measurement control register. Write the value requesting one measurement. End the exchange.

Here is exactly who controls SDA during those bytes:

| Part | Eight data pulses | Ninth pulse | Meaning |
|---|---|---|---|
| Address and write direction | D1 mini sends `00011100` | Compass acknowledges | “I am talking to you, and will write.” |
| Register selection | D1 mini sends `00001010` | Compass acknowledges | “Use register 0x0A.” |
| Command contents | D1 mini sends `00000001` | Compass acknowledges | “Put 0x01 there.” |

The D1 mini supplies the clock pulses throughout. Three bytes require 27 clock pulses, including acknowledgments. At our requested 10000 Hz clock, that is nominally about 2.7 ms plus boundary timing and software overhead. The command starts a measurement; the sensor still needs time to produce its result.

### Match that exchange to our helper

```cpp
Wire.beginTransmission(ADDRESS);
Wire.write(reg);
Wire.write(value);
uint8_t error = Wire.endTransmission();
```

The first three calls prepare the destination and put two bytes into a temporary software holding area, called a **buffer**. The complete write is sent when `endTransmission()` runs. Thus `Wire.write(reg)` alone does not immediately put that byte on the physical wire.

`endTransmission()` returns a result code. Our helper prints it and returns true only for result 0. This true/false return lets the caller decide whether to continue.

This behavior can be inspected in [ESP8266 Wire.cpp version 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Wire/Wire.cpp), in `beginTransmission`, `write`, and `endTransmission`.

## 8 Example two Ask whether the result is ready

We cannot assume the result is ready just because the request was accepted. We read status register `0x02`.

A register read has two parts: first select the register, then receive its contents. For illustration, assume the returned status byte is `0x01`. This example value means the data-ready bit is set.

```text
D1 mini: START
D1 mini: 0x1C    → compass ACK   select device for writing
D1 mini: 0x02    → compass ACK   select status register
D1 mini: repeated START
D1 mini: 0x1D    → compass ACK   select device for reading
Compass: 0x01   → D1 mini NACK  receive one status byte, then finish
D1 mini: STOP
```

A **repeated START** begins the next addressed part without first ending the overall exchange with STOP. We still send the device address again, this time with the read direction. The compass now sends the data byte; the D1 mini still supplies the clock.

Why does a read start with writing? We are not asking the compass to measure again. We are telling it which register we want to read. Register-selection and measurement-request are different operations.

### Match this to readRegisters

```cpp
Wire.beginTransmission(ADDRESS);
Wire.write(reg);
if (Wire.endTransmission(false) != 0) return false;
if (Wire.requestFrom(ADDRESS, count) != count) return false;
```

`endTransmission(false)` sends the selected register without STOP. `requestFrom` performs the read and puts received bytes in Wire's buffer. It returns how many bytes arrived. The default read finishes with STOP.

Then our helper copies those bytes into the caller's storage:

```cpp
for (uint8_t i = 0; i < count; i++) {
  data[i] = Wire.read();
}
```

Here `Wire.read()` takes a byte from the software buffer; it does not create another entire register exchange. The data was received during `requestFrom`.

The transfer and buffer behavior is visible in [Wire.cpp, requestFrom and read](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Wire/Wire.cpp).

### Understand the status test

A status byte can hold several yes/no flags, one per bit. We only need bit 0, the rightmost bit, which tells us whether a measurement is ready. A **mask** is a bit pattern used to select particular bits. Our mask is `0x01`:

```text
Example status:   00000011
Mask 0x01:        00000001
Bitwise AND:      00000001  → nonzero, so bit 0 is set
```

The C++ expression `status & 0x01` performs that bitwise AND. In an `if`, a nonzero result counts as true. It differs from `status == 0x01`, which would require every other bit to be zero too.

If the bit is not set, the program waits one millisecond and asks again. Repeatedly checking is called **polling**. The code gives up after approximately 100 ms rather than waiting forever. A communication failure also ends the wait.

## 9 A complete measurement conversation

Let us tell the whole story in everyday language, then put the exact bytes beside it. The compass does not understand English; the dialogue explains what our numeric exchanges mean.

### Before speaking Know where the answers will be

The compass puts its results into six numbered byte locations. Those locations are its data registers. We know their layout from the driver documentation, not because the compass announces a message length.

In the [PX4 IST8310 driver](https://px4.github.io/Firmware-Doxygen/d3/dbd/ist8310_8cpp_source.html), find displayed source lines **117–122**, or search for `ADDR_DATA_OUT_X_LSB`. The six definitions identify this layout:

| Register | Contains | Place in our received array |
|---|---|---|
| `0x03` | X lower byte | `data[0]` |
| `0x04` | X upper byte | `data[1]` |
| `0x05` | Y lower byte | `data[2]` |
| `0x06` | Y upper byte | `data[3]` |
| `0x07` | Z lower byte | `data[4]` |
| `0x08` | Z upper byte | `data[5]` |

Each axis needs two bytes to represent its signed 16-bit result. Three axes therefore occupy six bytes. The register numbers run consecutively, so starting at `0x03` lets us read all six in one sequence.

The word “lower” describes which part of the number the byte holds, not a low voltage. Both lower and upper bytes contain their own mixture of 0 and 1 bits.

This is why our code says:

```cpp
uint8_t data[6];                 // Reserve six bytes in the D1 mini.
readRegisters(0x03, data, 6);     // Starting register, storage, byte count.
```

The number six is an instruction to **our Wire library** about how many bytes to receive. We do not send a separate command byte containing six to the compass. The D1 mini keeps count and ends the read after six bytes.

### Part A Ask the compass to do the work

**D1 mini:** “Compass at address 0x0E, I want to send you something.”

**Compass:** “I acknowledge my address.”

**D1 mini:** “Choose your measurement-control register, 0x0A.”

**Compass:** “I acknowledge that byte.”

**D1 mini:** “Put 0x01 there. For this register, that means take one measurement.”

**Compass:** “I acknowledge that byte.”

**D1 mini:** “I am ending this exchange.”

The actual byte sequence is the write from Section 7: address plus write direction `0x1C`, register `0x0A`, and value `0x01`. The compass acknowledges each byte. Our corresponding call is:

```cpp
writeRegister(0x0A, 0x01);
```

The compass then takes a measurement. It has accepted our request, but we still need to check whether the answer is ready. An acknowledgment is not a completed measurement.

### Part B Check whether the work is finished

**D1 mini:** “Compass at 0x0E, I am selecting your status register, 0x02.”

The compass acknowledges the address and the register-selection byte separately.

**D1 mini:** “Now send me the contents of that register.”

The D1 mini sends the address again with read direction; the compass acknowledges it and sends one status byte.

**Compass:** “Here is the status byte.”

**D1 mini:** “That is the one byte I wanted. End this read.”

The last response is NACK followed by STOP, a normal ending for a read. The compass does not literally say “ready” or “not ready.” The D1 mini inspects the returned byte's bit 0. If it is zero, it waits briefly and repeats this status read. If it is one, it proceeds to the measurement registers.

Our corresponding call is:

```cpp
readRegisters(0x02, &status, 1);
```

Selecting `0x02` only selects where to read information. It does not start another measurement.

### Part C Select the beginning of the answer

We now know a measurement is ready. We want all three axes, whose first byte is stored at `0x03`.

**D1 mini:** “Compass at 0x0E, I want to send you a register selection.”

**Compass:** “I acknowledge my address.”

**D1 mini:** “Start with register 0x03.”

**Compass:** “I acknowledge that byte.”

At this point we have selected the starting location. We have not asked the compass to perform another measurement, and we have not yet received X.

**D1 mini:** “Now I am switching to reading from you.”

The D1 mini generates a repeated START and sends the compass address with read direction.

**Compass:** “I acknowledge the read address. I will send the contents starting at the location you selected.”

The actual setup is:

```text
START
D1 mini sends 0x1C → compass ACK    address 0x0E, write direction
D1 mini sends 0x03 → compass ACK    select the first data register
repeated START
D1 mini sends 0x1D → compass ACK    address 0x0E, read direction
```

The compass now sends data on SDA. The D1 mini continues supplying clock pulses on SCL. This is a change in who supplies the information, not a change in who supplies its timing.

### Part D Receive the answer one byte at a time

Use example bytes that produce X=52, Y=-71, Z=-87. The conversion is explained in Section 10. These example bytes are calculated from those readings, not a recorded capture of the wires.

**Compass:** “The byte from register 0x03 is 0x34.”

**D1 mini:** “Received; I am accepting another byte.” **ACK number 1.**

The compass automatically advances its internal read position to `0x04`.

**Compass:** “The byte from register 0x04 is 0x00.”

**D1 mini:** “Received; I am accepting another byte.” **ACK number 2.**

The compass advances to `0x05`. That location contains the first Y byte, so Y naturally comes next. The acknowledgment did not request Y by name.

**Compass:** “The byte from register 0x05 is 0xB9.”

**D1 mini:** “Received; I am accepting another byte.” **ACK number 3.**

**Compass:** “The byte from register 0x06 is 0xFF.”

**D1 mini:** “Received; I am accepting another byte.” **ACK number 4.**

**Compass:** “The byte from register 0x07 is 0xA9.”

**D1 mini:** “Received; I am accepting another byte.” **ACK number 5.**

**Compass:** “The byte from register 0x08 is 0xFF.”

The D1 mini has now counted six received bytes.

**D1 mini:** “That was my final byte; do not send another.” **NACK**, followed by **STOP**.

There are five ACK responses and one final NACK during these six returned bytes. There were also compass ACK responses to the address and register bytes before the data began.

The statements “received” and “accepting another byte” translate a single response bit. They are not new command bytes. After every eight data bits, the receiver uses the ninth clock pulse to give its response. The low/high mechanism is the one explained in Section 6.

### Why Y and Z follow X without separate requests

The IST8310's consecutive register-read behavior supplies the next numbered location as the same read continues. Our program relies on that chip-specific behavior:

```text
Selected start:  03
Next locations:  04 → 05 → 06 → 07 → 08
Contents:        X     Y     Y     Z     Z
                 upper lower upper lower upper
```

We selected where to start, and the controller kept count. We did not send separate “send Y” and “send Z” commands. An ACK does not select an axis; it is the receiver's response after the current byte. A different sensor might organize its results differently, so we must check its own register layout and read behavior.

### Match the conversation to our read helper

For `readRegisters(0x03, data, 6)`, the arguments become `reg = 0x03` and `count = 6`:

| Our helper line | What it contributes |
|---|---|
| `Wire.beginTransmission(ADDRESS);` | Prepare to address the compass for writing |
| `Wire.write(reg);` | Queue the selected starting register, 0x03 |
| `Wire.endTransmission(false);` | Send the selection without STOP |
| `Wire.requestFrom(ADDRESS, count);` | Read six bytes; the library handles the read address, five ACKs, final NACK, and STOP |
| `data[i] = Wire.read();` in a loop | Copy each already-received byte from Wire's buffer into our array |

The chip never sends the text “X: 52.” After the read ends, our program combines the byte pairs, assigns them to X/Y/Z, and prints that human-readable text to the computer through Serial. That is a different connection from the compass exchange.

### A short way to narrate the whole cycle

> First, our D1 mini tells the compass to take a measurement. It checks a status location until the result is ready. Then it selects the location holding the first X byte. We already know from the register map that the complete X, Y, Z result occupies six consecutive bytes. The compass sends those bytes in order, and our D1 mini acknowledges each one until the last. It then ends the read, combines the byte pairs, and prints three numbers.

## 10 Turn those bytes into numbers and choose their types

Before stepping through the remaining code, we need to explain why its variables have different integer types. The type controls the range of numbers a variable can represent.

In `uint16_t`, the `u` means unsigned, `int` means integer, and `16` specifies exactly 16 bits. The `_t` is part of the type name. Unsigned means no negative values. Without `u`, an `int16_t` is signed and can represent negatives.

| Type | Range | Use in this sketch |
|---|---|---|
| `uint8_t` | 0 to 255 | Individual bytes, addresses, status, identification, small counts |
| `uint16_t` | 0 to 65535 | Combine two bytes while preserving every bit pattern |
| `int16_t` | -32768 to 32767 | Final signed magnetic readings |
| `int32_t` | -2147483648 to 2147483647 | Intermediate subtraction when decoding negatives |
| `bool` | false or true | Yes/no results and state flags |
| `unsigned long` | 0 to 4294967295 on this ESP8266 | Millisecond timer values |

The exact-width names are defined by the C/C++ integer type facilities. See the [C++ standard draft, fixed-width types](https://eel.is/c++draft/cstdint.syn). These choices match the data format; small types are not automatically faster on a 32-bit processor.

### Combine low and high bytes

Our helper is:

```cpp
int16_t signedValue(uint8_t low, uint8_t high) {
  uint16_t value = uint16_t(low) | (uint16_t(high) << 8);
  return value < 32768 ? int16_t(value)
                       : int16_t(int32_t(value) - 65536);
}
```

`uint16_t(high)` converts the high byte to the named type. `<< 8` moves its bits eight places left. The bitwise OR operator `|` combines those bits with the low byte.

For X:

```text
High 0x00 shifted left 8:  00000000 00000000
Low  0x34:                00000000 00110100
Combined:                 00000000 00110100 = 52
```

An unsigned 16-bit variable is appropriate for the combined pattern because all patterns from 0 through 65535 must fit before interpreting the sign. C++ promotes small operands during arithmetic; on this ESP8266 the expression uses promoted integer operands before assignment. A cast does not force the processor to execute an operation in a 16-bit hardware register.

The sensor encodes negative values using **two's complement**. For this 16-bit format, patterns below 32768 represent nonnegative values. Patterns at or above 32768 represent their unsigned value minus 65536.

```text
X: high 0x00, low 0x34 → 0x0034 → 52
Y: high 0xFF, low 0xB9 → 0xFFB9 → 65465 − 65536 = -71
Z: high 0xFF, low 0xA9 → 0xFFA9 → 65449 − 65536 = -87
```

The expression `condition ? first : second` chooses the first result when the condition is true, otherwise the second. The `int32_t` intermediate explicitly accommodates the unsigned combined value and the subtraction. Only the final result is converted to `int16_t`, where it fits.

### Why not int or float for everything

On the ESP8266 an ordinary `int` is 32 bits; on a classic Uno it is 16 bits. Explicit widths document the sensor format across boards. They also distinguish unsigned transmitted bytes from signed measurement results.

The sensor sends integer counts, so no floating-point type such as `float` is needed yet. A later calculation of angles or physical units may use one.

## 11 Follow startup in the order it runs

Arduino calls `setup()` once, then calls `loop()` repeatedly. Helper functions are defined earlier in the file so the program can call them; their position does not mean they execute before setup.

### Load the interfaces and define shared values

```cpp
#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t ADDRESS = 0x0E;
bool ready = false;
```

The include lines make the Arduino and Wire declarations available. Arduino supplies timing, serial printing, and board pin names; Wire supplies I2C operations.

`constexpr` says ADDRESS is fixed at compile time. A 7-bit address fits in `uint8_t`. `ready` is a global flag, meaning it is accessible to setup and loop and persists between function calls. It starts false.

### Open serial output and initialize the two I2C pins

```cpp
Serial.begin(115200);
delay(2000);
Wire.begin(D2, D1);
Wire.setClock(10000);
```

Serial Monitor must use 115200 baud, the selected serial bit rate. `delay(2000)` waits two seconds. It gives the powered module startup time but cannot turn on its power.

`Wire.begin` takes SDA first, SCL second. `setClock(10000)` requests a 10000 Hz I2C clock. This is the slow diagnostic setting in our working sketch. It is neither the USB serial speed nor the measurement rate. See [ESP8266 Core I2C documentation](https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#i2c-wire-library).

### Check the address

```cpp
Wire.beginTransmission(ADDRESS);
uint8_t result = Wire.endTransmission();
Serial.printf("Address 0x%02X: result %u\n", ADDRESS, result);
```

This sends the address for writing but no register or contents. It is an address probe, the same basic operation used by our scanner. It tests whether something acknowledges, not which chip it is.

`result` is an unsigned byte because Wire returns a small nonnegative code. In our ESP8266 implementation, 0 means success, 2 means no address acknowledgment, and 4 is a general communication error. These are library result codes, not compass measurement values. Our earlier errors disappeared after powering the FW10.

The `printf` format string controls appearance: `%02X` means uppercase hexadecimal with at least two digits and leading zeroes; `%u` means unsigned decimal; `\n` ends the line. The characters `0x` inside the quotes are printed literally.

```cpp
if (result != 0) {
  Serial.println("Address check failed.");
  return;
}
```

A failure ends setup early. `ready` remains false, so the later loop will not attempt measurements. Returning from setup does not shut down Arduino; loop still runs.

### Identify the chip

```cpp
uint8_t id;
if (!readRegisters(0x00, &id, 1)) {
  Serial.println("Chip ID read failed.");
  return;
}
```

`id` holds one byte. `&id` means the address of this variable in the D1 mini's memory, allowing the helper to fill it. This memory address is unrelated to I2C device or register addresses.

The `!` means logical NOT: execute the failure block if the helper returns false. The helper's parameter `uint8_t *data` is a pointer, a value referring to byte storage. Here it points to `id`; during a six-byte read it points to an array. The caller must supply enough storage for the requested count.

Next the code prints the ID and checks it against `0x10`. Only a successful read with the expected value leads to:

```cpp
ready = true;
Serial.println("IST8310 identified.");
```

`ready` means initialization passed. It is not the sensor's measurement-ready bit. This saved sketch does not issue the compass reset command used in an earlier version.

## 12 Follow one pass through loop

### Check initialization and send the request

```cpp
if (!ready) {
  delay(1000);
  return;
}
```

If setup failed, loop waits one second and returns. Arduino calls it again. Initialization is not automatically retried; reset the board after correcting the problem.

```cpp
if (!writeRegister(0x0A, 0x01)) {
  Serial.println("Measurement request failed.");
  delay(500);
  return;
}
```

This invokes the complete measurement-request exchange from Section 7. Its arguments `reg` and `value` are unsigned bytes because each is sent as one byte. A failed request ends this loop pass; the next pass tries again.

The helper prints its register number and result before returning `error == 0`, a Boolean expression meaning “was the result zero?”

### Wait for the sensor

```cpp
uint8_t status = 0;
bool measured = false;
unsigned long started = millis();

while (millis() - started < 100) {
  if (!readRegisters(0x02, &status, 1)) break;
  if (status & 0x01) {
    measured = true;
    break;
  }
  delay(1);
}
```

`status` is one copied register byte. `measured` is a local Boolean flag, initially false for each loop pass. `millis()` reports elapsed milliseconds since startup. `started` matches its `unsigned long` return type; see the [Arduino millis reference](https://docs.arduino.cc/language-reference/en/functions/time/millis/).

Each pass through while reads status, checks the bit explained in Section 8, and either finishes or waits one millisecond. `break` exits the while loop, whereas `return` exits the entire current function call.

`millis() - started` calculates elapsed time. Unsigned subtraction handles the timer wrapping around for this short interval. On the ESP8266 the counter wraps after roughly 49.7 days; a 100 ms interval remains unambiguous. Individual library calls add execution time, so this is a polling timeout rather than an exact overall timing guarantee.

### Read, decode, and print

```cpp
uint8_t data[6];
if (measured && readRegisters(0x03, data, 6)) {
  int16_t x = signedValue(data[0], data[1]);
  int16_t y = signedValue(data[2], data[3]);
  int16_t z = signedValue(data[4], data[5]);

  Serial.printf("X: %d   Y: %d   Z: %d\n", x, y, z);
} else {
  Serial.println("Measurement timed out or read failed.");
}
delay(250);
```

`data` is six bytes of storage. Passing an array this way supplies a pointer to its first element. In the read helper, `count` and the loop index `i` are `uint8_t`; our one- and six-byte transfers fit easily. A general-purpose large-transfer helper would need a different capacity design.

`&&` means logical AND and uses short-circuit evaluation: if `measured` is false, the register-read function is not called. The bytes are decoded only when both conditions succeed. That prevents unfilled array contents from becoming fake measurements.

The `%d` format prints signed decimal integers. Negative readings are expected; they are not error codes. Finally, `delay(250)` waits a quarter second. Measurement, communication, and printing also take time, so the actual cycle is slower than four measurements per second.

The combined failure message does not distinguish a wait timeout from a failed read. More detailed diagnostics could separate those cases later.

## 13 Put the whole conversation together

One successful cycle now has a clear meaning:

1. Write `0x01` to the compass control register: “Take one measurement.”
2. Read status repeatedly: “Is the result ready?”
3. Read six bytes: “Give me X low/high, Y low/high, and Z low/high.”
4. Convert the bytes into three signed numbers.
5. Print them through USB to the computer.
6. Wait briefly and repeat.

The compass does not spontaneously send the printed line. It answers byte requests. Our program constructs the readable line from the returned values.

For a video demonstration, show the physical module and wires first, then the request/read diagrams, then rotate the board while the printed values change. Explain the angle calculation only in a later demonstration: this one establishes what information the sensor actually supplies.

## 14 What this test does not establish

We have observed working communication and changing readings during rotation. That does not establish calibrated compass accuracy. The sensor's axes still need mapping to the boat, magnetic interference needs checking, and heading behavior during heel remains an open topic.

No GPS reception is required for this compass test. Metal, magnets, and nearby electrical equipment can influence magnetic readings indoors or outdoors.

There is no heading calculation, tilt correction, tack reference, display code, or automatic recovery from failed initialization in this sketch. The pauses are useful for learning but will need revisiting when multiple project functions run together.

## 15 Reference copy of the complete working sketch

The line numbers below are for reading, not part of the program. Earlier explanations follow execution order instead of forcing you to understand every helper before learning what it does.

```text
  1: #include <Arduino.h>
  2: #include <Wire.h>
  3: 
  4: constexpr uint8_t ADDRESS = 0x0E;
  5: bool ready = false;
  6: 
  7: bool writeRegister(uint8_t reg, uint8_t value) {
  8:   Wire.beginTransmission(ADDRESS);
  9:   Wire.write(reg);
 10:   Wire.write(value);
 11: 
 12:   uint8_t error = Wire.endTransmission();
 13:   Serial.printf("Write register 0x%02X: result %u\n", reg, error);
 14: 
 15:   return error == 0;
 16: }
 17: 
 18: bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
 19:   Wire.beginTransmission(ADDRESS);
 20:   Wire.write(reg);
 21: 
 22:   if (Wire.endTransmission(false) != 0) return false;
 23:   if (Wire.requestFrom(ADDRESS, count) != count) return false;
 24: 
 25:   for (uint8_t i = 0; i < count; i++) {
 26:     data[i] = Wire.read();
 27:   }
 28:   return true;
 29: }
 30: 
 31: int16_t signedValue(uint8_t low, uint8_t high) {
 32:   uint16_t value = uint16_t(low) | (uint16_t(high) << 8);
 33:   return value < 32768 ? int16_t(value)
 34:                        : int16_t(int32_t(value) - 65536);
 35: }
 36: 
 37: void setup() {
 38:   Serial.begin(115200);
 39:   delay(2000);
 40: 
 41:   Wire.begin(D2, D1); // SDA, SCL
 42:   Wire.setClock(10000); // Slower speed for diagnosis
 43: 
 44:   // First check whether the address responds.
 45:   Wire.beginTransmission(ADDRESS);
 46:   uint8_t result = Wire.endTransmission();
 47:   Serial.printf("Address 0x%02X: result %u\n", ADDRESS, result);
 48: 
 49:   if (result != 0) {
 50:     Serial.println("Address check failed.");
 51:     return;
 52:   }
 53: 
 54:   // Read identification without issuing a reset.
 55:   uint8_t id;
 56:   if (!readRegisters(0x00, &id, 1)) {
 57:     Serial.println("Chip ID read failed.");
 58:     return;
 59:   }
 60: 
 61:   Serial.printf("Chip ID: 0x%02X (expected 0x10)\n", id);
 62: 
 63:   if (id != 0x10) {
 64:     Serial.println("Unexpected chip ID.");
 65:     return;
 66:   }
 67: 
 68:   ready = true;
 69:   Serial.println("IST8310 identified.");
 70: }
 71: 
 72: void loop() {
 73:   if (!ready) {
 74:     delay(1000);
 75:     return;
 76:   }
 77: 
 78:   // Request one magnetic measurement.
 79:   if (!writeRegister(0x0A, 0x01)) {
 80:     Serial.println("Measurement request failed.");
 81:     delay(500);
 82:     return;
 83:   }
 84: 
 85:   uint8_t status = 0;
 86:   bool measured = false;
 87:   unsigned long started = millis();
 88: 
 89:   while (millis() - started < 100) {
 90:     if (!readRegisters(0x02, &status, 1)) break;
 91:     if (status & 0x01) {
 92:       measured = true;
 93:       break;
 94:     }
 95:     delay(1);
 96:   }
 97: 
 98:   uint8_t data[6];
 99:   if (measured && readRegisters(0x03, data, 6)) {
100:     int16_t x = signedValue(data[0], data[1]);
101:     int16_t y = signedValue(data[2], data[3]);
102:     int16_t z = signedValue(data[4], data[5]);
103: 
104:     Serial.printf("X: %d   Y: %d   Z: %d\n", x, y, z);
105:   } else {
106:     Serial.println("Measurement timed out or read failed.");
107:   }
108: 
109:   delay(250);
110: }
```

## 16 How to find the information in open source code

A link alone is not enough to teach someone how to investigate a sensor. This section shows which questions we asked, what names to search for, and how the findings became our small test.

A **driver** is code that knows how to operate a particular chip. A **library** is a collection of reusable code. A chip driver can be part of a much larger library or application. PX4 and ArduPilot are flight-control projects with IST8310 drivers; they are not Arduino libraries that we installed in PlatformIO.

Our sketch uses the ESP8266's existing Wire library and small helper functions written for this test. We consulted the flight-control drivers to learn the chip operations; we did not paste an entire flight-control driver into the sketch.

### Step 1 Establish the exact chip name

We first had a listing describing a QMC5883L, but the corrected product information named **IST8310**. That change matters because compatible wires do not imply compatible register maps.

The useful starting evidence was the scanner result, `0x0E`, followed by the correct chip name. A source for a similarly named chip is not enough. Neither a product title nor an address by itself proves the full identity.

### Step 2 Open a driver maintained by the project that uses it

Open the [PX4 IST8310 source](https://px4.github.io/Firmware-Doxygen/d3/dbd/ist8310_8cpp_source.html). This page is a formatted copy of C++ source, with the original source line numbers displayed beside the code.

Use your browser's Find command, Ctrl+F. Search for these exact names. Read the nearby definitions and comments rather than relying on a search-result excerpt.

| Question | Search in the PX4 file | What it reveals | Where it appears in our sketch |
|---|---|---|---|
| Which I2C address is used? | `IST8310_BUS_I2C_ADDR` | `0xE`, equivalent to `0x0E` | `ADDRESS` |
| Which register identifies it? | `ADDR_WAI` and `WAI_EXPECTED_VALUE` | Register 0, expected contents `0x10` | `readRegisters(0x00, &id, 1)` and the ID comparison |
| Where is measurement status? | `ADDR_STAT1` | Register `0x02` | Status read in the while loop |
| Which bit means ready? | `STAT1_DRDY` | Its bit position is zero | `status & 0x01` |
| Where does X start? | `ADDR_DATA_OUT_X_LSB` | Register `0x03` | First register of our six-byte read |
| How are the bytes laid out? | `ADDR_DATA_OUT_X_MSB`, then Y and Z definitions | Consecutive low/high register pairs through `0x08` | Pairing elements of `data[6]` |
| Where is the measurement control? | `ADDR_CTRL1` and `CTRL1_MODE_SINGLE` | Register `0x0A`, single mode value 1 | `writeRegister(0x0A, 0x01)` |

In these names, WAI means “Who Am I,” DRDY means “data ready,” LSB labels the least-significant or lower-place byte, and MSB labels the most-significant or higher-place byte. The labels are clues, but checking their definitions and use is what establishes the meaning.

A line such as `#define ADDR_STAT1 0x02` gives a readable name to a constant. It does not perform a hardware operation. The operation occurs later when a function uses that constant in a read or write. This is the distinction between finding a register map and understanding how to operate it.

### Step 3 Follow a constant to the code that uses it

Open the [ArduPilot IST8310 driver](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Compass/AP_Compass_IST8310.cpp). Search for `CNTL1_REG`. Near the definitions it gives the control register and the value named `CNTL1_VAL_SINGLE_MEASUREMENT_MODE`.

Search for the same names again to find an actual call to `write_register`. That call is the evidence that the driver writes the defined value to the defined control register to request a measurement. Our equivalent is `writeRegister(0x0A, 0x01)`, using our own Wire-based helper.

Next search for `OUTPUT_X_L_REG` and follow it to a `read_registers` call. Check the requested byte count and the code that interprets the returned values. That supplies more evidence than the register names alone.

Finally, inspect `init()` to see how identification is checked. It reads `WAI_REG` and compares the returned byte with `DEVICE_ID`. Our setup makes the same kind of check.

The ArduPilot initialization also handles reset, averaging, and other settings. Our learning sketch is intentionally smaller. A working minimal read does not mean we have implemented every reliability feature in that driver.

### Step 4 Learn what the library is doing underneath our helpers

Now open [ESP8266 Wire.cpp at version 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Wire/Wire.cpp). This version matches the framework used for the initial project build.

Search for `TwoWire::beginTransmission`: it records the address and resets the outgoing buffer indexes. Then search for `TwoWire::write`: in controller-transmit mode it puts bytes in the outgoing buffer. Finally search for `TwoWire::endTransmission`: it calls a lower-level transfer routine to send that buffer.

That is how we established the statement “Wire.write queues bytes; endTransmission sends them.” We followed the implementation rather than assuming that a method named write always acts on the pins immediately.

Search for `TwoWire::requestFrom` and `TwoWire::read`. The first calls the receiving routine and fills the receive buffer; the second retrieves a byte from that buffer. `read()` returns an `int` so it can also return -1 when no byte is available. Our helper checks the received count before copying bytes into unsigned-byte storage.

The Wire file delegates the actual clock and data signaling to lower-level functions whose names begin with `twi_`. If investigating an error code, follow the called function to its implementation instead of assuming the high-level wrapper explains the cause. The return value describes the stage of communication that failed, not necessarily the physical reason.

### Step 5 Separate universal rules from chip behavior

Use the [NXP I2C specification](https://www.nxp.com/docs/en/user-guide/UM10204.pdf) for the shared communication rules: clock/data timing, START/STOP, acknowledgment, and address direction. Use the IST8310 driver or a valid chip datasheet for register locations and measurement commands.

These are different questions. Knowing I2C does not tell us which compass register starts a measurement. Knowing the register map does not explain how bytes travel electrically. Our tutorial joins the two.

### Step 6 Separate evidence from our design choices

| Item | Basis |
|---|---|
| Device answered at `0x0E` | Observed with our scanner; consistent with PX4's driver |
| Identification value and measurement register map | Checked in the named IST8310 drivers |
| X/Y/Z changed during rotation | Observed on our hardware |
| 10000 Hz diagnostic clock | A choice in our sketch, not claimed as a manufacturer requirement |
| 100 ms wait limit and 250 ms pause | Choices that make the test understandable and avoid an endless wait |
| Byte arrays and signed conversion helper | Our implementation of the format, with explicit C++ types |
| Example wire bytes for X=52, Y=-71, Z=-87 | Calculated teaching example, not a captured electrical trace |
| Accurate heading on the boat | Not established by this raw-reading test |

### Step 7 Check reuse terms and preserve traceability

Open source means you can inspect the implementation, but permission to copy and redistribute depends on its license. Read the license notice before copying code into a project or showing substantial extracts in teaching material. The driver source files include license notices. Our walkthrough uses their names and operations as references; it is not a claim that all code in those projects has identical reuse terms.

For a teaching recording, show the repository name, file path, and the searched constant on screen, then show the corresponding line in our sketch. GitHub's permanent-link feature can preserve a specific revision for the video description. Ordinary links to a development branch can change over time; search names are more reliable than memorizing current line numbers.

### Reference index

| Reference | Purpose |
|---|---|
| [NXP UM10204](https://www.nxp.com/docs/en/user-guide/UM10204.pdf) | I2C rules; Sections 3.1.1–3.1.6 and 3.1.10 |
| [PX4 IST8310 driver](https://px4.github.io/Firmware-Doxygen/d3/dbd/ist8310_8cpp_source.html) | Address, status flags, and register definitions |
| [ArduPilot IST8310 driver](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Compass/AP_Compass_IST8310.cpp) | Identification, setup, measurement request, and reading code |
| [ESP8266 Wire 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Wire/Wire.cpp) | How our I2C API calls are implemented |
| [ESP8266 Core I2C documentation](https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#i2c-wire-library) | SDA/SCL initialization |
| [C++ fixed-width integer types](https://eel.is/c++draft/cstdint.syn) | Meaning of the integer type families |
| [Arduino millis reference](https://docs.arduino.cc/language-reference/en/functions/time/millis/) | Elapsed-time interface |

The manufacturer datasheet URL previously supplied returned 404. We do not treat it as an accessible source. These working drivers document the chip operations; they do not establish the complete FW10 board's signal-voltage circuitry.

## 17 Reading and printing

Use Ctrl+Shift+V in VS Code to open the Markdown preview. For paper, use a Markdown viewer or export tool with print support. Check print preview for clipped code before printing; available controls depend on your viewer. Landscape orientation can help with the full code listing.

For teaching, Sections 1–9 form the hardware conversation lesson; Sections 10–13 connect that understanding to C++. The full listing and references are available afterward without interrupting the explanation.

This guide describes the saved sketch on November 22, 2026. Later source changes can alter line numbers and behavior.

Source snapshot SHA256: `ebd2e18e3933bf99357cea13687a3f7b46702fe8deb99b883bfc1bda8aa75dc0`.
