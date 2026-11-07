# Supervised cane demo

This is a controlled demo for MakeUC Hackathon 2026. A sighted supervisor wears or mounts a standard iPhone 16 so its rear camera faces the direction of travel, and an Arduino UNO R4 WiFi drives four external actuators on an existing cane. The cane remains the mobility aid. This demo is not a replacement cane, not unsupervised guidance, and not a test to run with a blind traveler.

The phone warns with uncertainty. It does not invent a safe path. A quiet hazard channel is not an all-clear.

At runtime the path is the iPhone 16, then Bluetooth LE, then the UNO R4 WiFi, then the four actuator drivers. The phone processes the camera and motion on the device. No laptop and no cloud service infer a hazard. GitHub Actions can compile the unsigned app and the sketch. That compile job is not part of the running demo.

## Fixed demo requirements

- The phone is body-mounted. This repository does not design the mount.
- Four actuator channels are separate sensations. The firmware drives four logic outputs. Placement on the cane is part of the build, not a steering map.
- Destination guidance is not in this increment. A manual demo label can mark left or right on the route channel. That label does not mean the direction is clear.

## What the hardware can and cannot do

Apple's [iPhone 16 technical specifications](https://www.apple.com/iphone-16/specs/) list the sensors as Face ID, barometer, gyro, accelerometer, proximity, and ambient light. That list does not include a LiDAR scanner. The phone has a rear dual camera and motion sensors.

ARKit scene depth is documented as a LiDAR measurement. Apple says to call `supportsFrameSemantics(_:)` before enabling it, and that scene depth is supported only on LiDAR-capable devices: [sceneDepth](https://developer.apple.com/documentation/arkit/arconfiguration/framesemantics-swift.struct/scenedepth). This app checks that flag and does not enable scene depth, even if some other device reports support.

World tracking is a separate capability. The app calls `ARWorldTrackingConfiguration.isSupported` and requests camera permission at runtime: [Verifying device support and user permission](https://developer.apple.com/documentation/arkit/verifying-device-support-and-user-permission). Apple's [Understanding world tracking](https://developer.apple.com/documentation/arkit/understanding-world-tracking) describes a right-handed camera coordinate system. Apple's [Displaying an AR Experience with Metal](https://developer.apple.com/documentation/arkit/displaying-an-ar-experience-with-metal) sample treats a camera-relative translation of 0.2 as 0.2 meters. When tracking is normal, this demo reports the distance between consecutive ARKit camera positions as an ARKit camera step in those units. It is device motion, not an obstacle distance and not a speed. It was not calibrated on a physical iPhone 16 here.

An object detector is not used. A box in the image does not provide meters.

No monocular depth network is bundled. A relative-depth model would not be an honest meter reading, and this environment cannot validate a metric model or its license on an iPhone 16.

The app can raycast the center of the frame against ARKit's existing detected planes ([planeDetection](https://developer.apple.com/documentation/arkit/arworldtrackingconfiguration/planedetection-swift.struct)). A hit is the distance from the current camera pose to that plane, shown as context. It does not raise a hazard and it is not a clearance. No hit stays unknown. Plane detection on this iPhone was not run here.

The UNO R4 WiFi uses a Renesas RA4M1 for the headers and an ESP32-S3-MINI-1-N8 for wireless. The [datasheet](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf) says the RA4M1 GPIOs can safely handle up to 8 mA, and that higher-current devices such as servo motors need an external supply. The [cheat sheet](https://docs.arduino.cc/tutorials/uno-r4-wifi/cheat-sheet) says the ESP32-S3 provides Bluetooth LE and Wi-Fi, and that those radios share one antenna, so they are not used at the same time. [ArduinoBLE](https://docs.arduino.cc/libraries/arduinoble/) 2.1.0 documents peripheral and central modes and names the UNO R4 WiFi. Its README says that board needs ESP32-S3 firmware 0.2.0 or newer. The compile job pins Arduino core `arduino:renesas_uno` 1.6.0, whose `unor4wifi` board id is the UNO R4 WiFi. This firmware is a BLE peripheral only. It does not join Wi-Fi. The iPhone is the BLE central through [Core Bluetooth](https://developer.apple.com/documentation/corebluetooth).

## Camera path and simulation path

Every outgoing packet has one source.

The camera path can report tracking, an ARKit camera step, and an optional plane-hit context. Its hazard claim is always "no claim", and its route cue is always none. If camera permission, world tracking, or normal tracking is missing, the system-unavailable channel is on. Missing scene depth does not by itself mean unavailable, and it also does not create a distance.

The simulation path is a separate bench control on the phone. It can request the D2 hazard actuator, the D3 unavailable actuator, and a D4 left or right label. The screen says the input is simulated. Those controls do not read the camera and do not mean an obstacle was detected. The shared encoder rejects a camera-sourced hazard or a camera-sourced route cue, so a buggy packet of that shape does not move the actuators.

There is no random left or right choice. Nothing in the camera path picks a direction.

## Four channels

| Output | Pin | Meaning while the link is alive |
| --- | --- | --- |
| Hazard | D2 | Pulses only for an elevated claim. Off is not an all-clear. |
| System unavailable | D3 | Pulses when the selected source says the pipeline cannot support a claim. |
| Route demo | D4 | Pulses for a manual left or right label. The label is on the phone and in the claim. The pulse does not mean that side is safe. |
| Link | D5 | Board-owned. A short blip every 2 seconds means packets are arriving. A fast, mostly-on pattern means the watchdog expired. |

The programmed watchdog is 500 ms after the last valid packet, using the board's `millis()`. At 500 ms the link is still a heartbeat. At 501 ms it is an outage. During an outage the board forces D2, D3, and D4 off and keeps the outage pattern on D5. A rejected packet does not refresh the watchdog. Before the first valid packet, the board is already in outage.

Silence on all four outputs means the board is unpowered, the firmware is not running, or the drivers are disconnected. That is a fault, not a clear path.

The patterns are different periods in code so a supervisor can tell the channels apart. Whether a particular actuator feels distinct is a bench check, not a result recorded here.

## Actuator wiring

Each GPIO only enables an external driver. It does not power the actuator.

For each channel:

1. Use a low-side switch the builder has checked for a 5 V logic input and for the actuator's current.
2. Put the actuator between the external supply positive and the switch. Put the switch between the actuator and the common ground.
3. Connect the GPIO to the switch input through a series resistor. Add a pulldown so a reset or floating pin stays off.
4. If the actuator is inductive, put a flyback diode across it, cathode toward the supply.
5. Tie the external supply ground to a UNO ground. Do not feed actuator current through the header pin.
6. Do not connect 5 V signals to the ESP32-S3 header. Those pins are 3.3 V.

Power the actuators from a supply that can provide their current. The datasheet's 8 mA GPIO limit is for the logic pin, not a budget for the motor.

## BLE packets

Service `8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1001`.

Command characteristic `8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1002`, 18 bytes, written by the phone. The layout, CRC-16/CCITT-FALSE, and admissibility rules live in [cane_logic.h](../prototype/firmware/uno_r4_wifi/cane_logic.h). Eighteen bytes fits a 20-byte default BLE write. The link has no pairing secret. Turn the board off when the demo is finished. Do not treat the advertisement as an access-controlled safety channel.

Status characteristic `8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1003`, 8 bytes, notified by the board. The phone shows it as controller status. Until a notification arrives, the phone does not know the pin state.

The phone timer asks to send about ten times a second. That is the timer interval in [DemoController.swift](../prototype/ios/CaneDemo/CaneDemo/DemoController.swift). It is not a measured radio or actuator latency.

## What is implemented

- Host-tested assessment, packet, watchdog, and output logic.
- UNO R4 WiFi sketch that advertises `CaneDemo`, accepts command packets, and drives D2-D5.
- iPhone app source: camera preview, capability checks, ARKit world tracking, camera-step readout, existing-plane context, simulation controls, and a BLE central.
- Shared rejection of camera hazards and camera route cues.

## What is not implemented

- A meter distance to an arbitrary obstacle, a velocity, or a time to contact.
- A depth model, object detector, or use of scene depth.
- Automatic avoidance, including a random direction.
- Maps, destinations, or turn-by-turn guidance.
- A designed body mount, driver board, or chosen actuator part.
- Proof that four actuators feel distinct.
- A measured end-to-end latency.
- BLE pairing.
- An iOS binary or a flashed board. This environment has no Xcode and no Arduino toolchain.

## Checks

Host logic tests, from a machine with a C compiler:

```bash
make -C prototype test
python3 scripts/check_workflow.py
```

`make` compiles `cane_logic.c` and runs [test_cane.c](../prototype/tests/test_cane.c). Those tests do not open a camera, a radio, or a GPIO.

[prototype-build.yml](../.github/workflows/prototype-build.yml) adds two cloud compile jobs. They use the repository contents only. They do not use a signing certificate, an Apple team, or a secret.

- Unsigned iOS compile: a GitHub-hosted `macos-latest` runner runs `xcodebuild` with code signing disabled. A successful log means the sources compiled. It does not produce an app that can be installed on the iPhone.
- UNO R4 WiFi compile: a GitHub-hosted `ubuntu-latest` runner installs Arduino CLI 1.5.1, core `arduino:renesas_uno` 1.6.0, and ArduinoBLE 2.1.0, then compiles the sketch for `arduino:renesas_uno:unor4wifi`. A successful log means the sketch compiled. It does not flash the board.

## Build without owning a Mac

The unsigned iOS job is the compile check for someone who does not have a Mac. Watch that job on the pull request. A green result is a compile result. Copying the runner's build products to the phone will not install them, because the phone requires a signature from an Apple development team.

Installing the app on the iPhone 16 is a separate human step:

1. A person with a Mac and Xcode opens [CaneDemo.xcodeproj](../prototype/ios/CaneDemo/CaneDemo.xcodeproj/project.pbxproj).
2. In Signing & Capabilities, that person selects their own Apple team. This repository has no team id, certificate, or provisioning profile.
3. They change `com.makeuc2026.canedemo` if their team cannot use that bundle id.
4. They connect the standard iPhone 16, trust the computer, and enable Developer Mode.
5. They run the signed build on that phone. The iOS Simulator is not a stand-in for this phone's camera, motion, or the BLE peripheral.
6. On the phone, scene depth support should read no. If it reads yes, the device is the wrong model. This build still does not enable scene depth.

Apple Developer Program enrollment, certificate creation, and signing stay with that person. This repository does not buy or enroll an Apple account.

## Flash the UNO R4 WiFi

The firmware job only compiles. Flashing is still local:

1. Install Arduino IDE 2.
2. Install the Arduino UNO R4 Boards package at 1.6.0, or a newer package you choose after reading its notes, and select Arduino UNO R4 WiFi.
3. Install ArduinoBLE 2.1.0. The library lists this board and asks for ESP32-S3 firmware 0.2.0 or newer. Let the board package update that bridge firmware if it offers to.
4. Open [uno_r4_wifi.ino](../prototype/firmware/uno_r4_wifi/uno_r4_wifi.ino) and upload it.
5. Open Serial Monitor at 115200. Before the phone connects, the log should show the link outage pattern.

## Supervised demo

Do this with a sighted supervisor, in a clear space, with the existing cane still used as the cane. Stop if anyone would have to rely on the actuators to move safely.

1. Mount the phone so the rear camera faces travel. Connect the four drivers before powering the actuators.
2. Flash the board and confirm the link channel is in its outage pattern while the phone is disconnected.
3. Launch the app, accept camera and Bluetooth prompts, and wait until the phone says the link is ready.
4. Read the camera section. Tracking can be normal while the hazard line still says no claim. Walk a few steps and watch the camera-step line change only while tracking stays normal. That number is the phone's motion.
5. Switch the outgoing source to Simulation. The banner must say the input is simulated. Turn on the elevated hazard and confirm D2 pulses while D5 stays in the heartbeat pattern. Turn on the unavailable cue and confirm D3 can pulse at the same time. Choose a left or right label and confirm D4 pulses. The label is only a label.
6. Force the link to drop by quitting the app or moving out of range. D2, D3, and D4 should go off, and D5 should change to the fast outage pattern. Powering the board off should silence every channel. Silence is a fault.
7. To measure latency, record one video of the phone screen and the actuator, or of the screen and the serial log. Switch the simulated hazard. The latency is the time from the on-screen claim change to the actuator or log change. Write down the number you observe. This repository does not provide one.

The serial `ms=` value is the board's `millis()` when the output state changes. Packet acceptance and pin updates happen in the same loop, so that log is not the phone-to-actuator delay.
