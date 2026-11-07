# Bluefy page

The page is [web/index.html](../web/index.html). It is the phone runtime for this increment. A standard iPhone 16, with Bluefy installed by you, opens the page. The page uses the rear camera, runs COCO-SSD in the browser, and writes the existing 18-byte Bluetooth packet to the UNO R4 WiFi. A laptop is not part of that runtime.

This repository has not run that combination on a phone. A green test log is not a Bluefy result.

## What the packet means

The native camera path still cannot send an elevated hazard. The web page uses a separate source, image heuristic, value 3.

- D2 pulses only for an elevated claim. From the web page that means a supported box is in the center of the picture, or you turned on the manual D2 switch. It is not a measured collision.
- D3 pulses when the camera, model, or page is not ready. Quiet D3 is not an all-clear.
- D4 pulses only for a manual left or right label. That label is not a clear direction.
- D5 is driven by the board. A short blip means packets are arriving. A fast pattern means the 500 ms watchdog expired. The page tries to send every 200 ms while it is connected.

The image-heuristic packet is rejected if it contains a distance or a route. The model does not report meters, speed, or time to contact. If the model fails, the page shows the error and does not invent a warning from brightness.

## Software already in the repo

- Sketch: [prototype/firmware/uno_r4_wifi/uno_r4_wifi.ino](../prototype/firmware/uno_r4_wifi/uno_r4_wifi.ino)
- The compile job that passed on the previous pull request used Arduino CLI 1.5.1, board package `arduino:renesas_uno` 1.6.0, and ArduinoBLE 2.1.0. This page keeps those versions.
- Pick the board Arduino UNO R4 WiFi. The sketch calls `Serial.begin(115200)` and prints `Cane demo controller`, `Advertising CaneDemo as a BLE peripheral.`, `accepted source=`, and `rejected packet`.
- Pinned browser downloads, only when you tap Load object model: TensorFlow.js 4.22.0 and COCO-SSD 2.2.3 from jsDelivr, both Apache-2.0. The weights are the library's `lite_mobilenet_v2` files at `https://storage.googleapis.com/tfjs-models/savedmodel/ssdlite_mobilenet_v2/model.json`. The camera picture is not sent anywhere.

## What only you can do

1. Merge the pull request.
2. In the repository Settings, open Pages, and set the source to GitHub Actions. The workflow [pages.yml](../.github/workflows/pages.yml) does not run for a pull request, and it cannot switch that setting. Until that source is selected, there is no published page. `https://pschmeln.github.io/agent-workflow/` returned 404 when this branch was prepared.
3. After the Pages job on `main` succeeds, open the URL printed by that job in Bluefy on the iPhone 16. Do not type a guessed address.
4. Install Bluefy yourself. Flash the sketch. Wire four external drivers to D2, D3, D4, and D5. Do not power actuators from the header pins.
5. With a sighted spotter, use the page sections one at a time, then the combined switch. Background the app and confirm D5 changes to the fast pattern. Do not cover anyone's eyes.

## Actuator check

| What you do | What should move |
| --- | --- |
| Phone not connected | D5 fast, D2 D3 D4 off |
| Connected, no manual switch and no combined warning | D3 unavailable pattern, D5 slow blip |
| Manual D2 | D2 pulses, serial `source=simulation` |
| Manual D4 left or right label | D4 pulses. The label is not a safe turn |
| Combined warning on, supported object in the center box | D2 pulses, serial `source=image-heuristic` |
| Combined warning on, nothing in the center | D2 off. The page says this is not an all-clear |
| Leave Bluefy or disconnect | D5 fast within about a second |
