# BoatSpeed v2 development environment setup

This guide takes a new contributor from a cloned repository to a starter program running on a WeMos D1 mini ESP8266. Begin with the board connected only by USB; sensor wiring comes later.

## 1 Install the tools

Install Git, Visual Studio Code, and the **PlatformIO IDE** extension in VS Code. Open Extensions with Ctrl+Shift+X, search for PlatformIO IDE, and install it. Allow its initial setup to finish and reload VS Code if prompted.

Use a USB **data** cable and a D1 mini ESP8266. A charge-only cable can power the board without creating a serial port.

## 2 Clone and open the repository

Clone the v2 repository using its remote URL, then choose **File → Open Folder** in VS Code and select the repository folder containing `.git` and `LICENSE`.

The current developer's local repository is:

```text
/home/harraz/projects/BoatSpeedV2/BoatSpeedV2
```

Other contributors should use their own clone location. Do not open the original BoatSpeed v1 repository by mistake.

An **Untitled Workspace** label means VS Code has an unsaved workspace, often containing multiple folders. It is not a Git error. Opening just the repository with File → Open Folder makes the intended project clearer.

## 3 Check the project layout

The starter project is already included in this repository. Contributors cloning it after these files are committed do not need to create a second PlatformIO project.

```text
BoatSpeedV2/
├── .git/
├── .gitignore
├── .vscode/
│   └── extensions.json
├── LICENSE
├── platformio.ini
├── docs/
│   └── development-setup.md
└── src/
    └── main.cpp
```

PlatformIO generates `.pio/` during builds. Additional `include/`, `lib/`, and `test/` folders are optional for this starter.

### Creating the project from scratch

For a new repository without PlatformIO files, use **PlatformIO Home → New Project**. Choose **WeMos D1 R2 and mini** and the **Arduino** framework. Check the resulting location carefully: the wizard can create a named project subfolder or use its default directory under `Documents/PlatformIO/Projects`.

The required result is that `platformio.ini` and `src/` are directly inside the Git repository, alongside `LICENSE`. If the wizard creates a separate project folder, copy its contents into the repository, including hidden configuration files, without replacing `.git` or `LICENSE`. Skip the generated `.pio/` folder. Verify the copied project builds before removing the original copy.

## 4 Check the configuration

Use this `platformio.ini` for the starter:

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
```

The board is an **ESP8266**, not an ESP32. The `d1_mini` environment name is used in the commands below.

Keep these generated paths out of Git:

```gitignore
.pio/
build/
.vscode/.browse.c_cpp.db*
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch/
```

Do not ignore `platformio.ini` or `src/`.

## 5 Build the starter program

The initial `src/main.cpp` is:

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BoatSpeed v2 started");
}

void loop() {
  Serial.println("D1 mini running");
  delay(1000);
}
```

Save the files. Click the PlatformIO icon in the left sidebar and select **PROJECT TASKS → d1_mini → General → Build**. The bottom-bar checkmark is another shortcut when visible.

Alternatively, run this in a terminal opened at the repository root:

```bash
pio run -e d1_mini
```

If `pio` is not on PATH, the current Linux installation provides it at:

```bash
/home/harraz/.platformio/penv/bin/platformio run -e d1_mini
```

Other Linux installations commonly use `~/.platformio/penv/bin/platformio`. PlatformIO's own terminal can also provide its command environment.

The first build can download the ESP8266 platform, compiler, and Arduino framework. A completed build ends with **SUCCESS**. This verifies compilation, not the physical board.

## 6 Detect the connected board

Connect the D1 mini with the USB data cable, then run:

```bash
pio device list
```

On Linux, a D1 mini commonly appears as `/dev/ttyUSB0`; use the actual detected port, not this example blindly.

If no ports appear:

1. Unplug and reconnect the board.
2. Try a known data cable and another USB port, preferably without a hub.
3. Run `pio device list` again.
4. If still empty, check operating-system USB detection and the USB serial driver for the board's actual bridge chip.

A lit LED proves power is reaching the board; it does not prove USB data is working. If PlatformIO prompts for a port with an empty list, press Ctrl+C and resolve detection first.

If a port is listed but access is denied, check the device's ownership and your Linux serial-access group membership. Group names vary by distribution. This is a different problem from an empty port list; avoid running the whole development environment as root.

## 7 Upload the program

Close any serial monitor already using the port. Select **PROJECT TASKS → d1_mini → General → Upload**, or run:

```bash
pio run -e d1_mini --target upload
```

If automatic port selection fails, specify the detected port:

```bash
pio run -e d1_mini --target upload --upload-port /dev/ttyUSB0
```

Uploading builds the program if necessary and writes it to the board. Wait for **SUCCESS**. If it cannot connect, verify the port and cable, disconnect external wiring, and retry. Most D1 minis enter upload mode automatically.

## 8 Verify serial output

Select **PROJECT TASKS → d1_mini → General → Monitor**, or run:

```bash
pio device monitor -b 115200 --port /dev/ttyUSB0
```

Replace the example port with the detected one. Expect:

```text
D1 mini running
D1 mini running
D1 mini running
```

Press the board's reset button to see `BoatSpeed v2 started` again. A short burst of unreadable ESP8266 boot text can precede the sketch's readable output. Persistent unreadable sketch output suggests a baud mismatch. Exit the terminal monitor with Ctrl+C before uploading again.

## 9 Save the setup in Git

Review changes from the repository root:

```bash
git status --short
```

Confirm `.pio/` and `build/` are excluded. Stage the intended starter files and guide, review them, then commit and push:

```bash
git add platformio.ini src/main.cpp .gitignore .vscode/extensions.json docs/development-setup.md
git diff --cached
git commit -m "Set up BoatSpeed v2 PlatformIO starter and development guide"
git push
```

These commands are instructions for the contributor; creating this guide does not itself commit or push anything.

## Completion checklist

- The repository root contains both `.git` and `platformio.ini`.
- The starter builds successfully.
- PlatformIO detects the D1 mini's serial port.
- Upload completes successfully.
- The serial monitor shows the repeating message at 115200 baud.

Once these checks pass, begin the FW10 connection and sensor tests as a separate step.

## Recorded project status

On November 21, 2026, the starter was created in the v2 repository and built successfully using Espressif 8266 platform 4.2.1 and Arduino ESP8266 framework 3.1.2. The device list was empty at that check, so upload and physical-board operation were not yet verified. The configuration above does not pin dependency versions; future installations may resolve different versions.
