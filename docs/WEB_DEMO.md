# Two blue buzzers

Use the Bluefy page at [web/index.html](../web/index.html) on a standard iPhone 16. Reflash the board with [prototype/firmware/uno_r4_wifi/uno_r4_wifi.ino](../prototype/firmware/uno_r4_wifi/uno_r4_wifi.ino) before opening the new page. An old version 1 sketch will reject the new packets, and an old page will be rejected by this sketch. Neither mismatch sounds a side buzzer.

The page has not been run on a phone here, and the buzzers have not been heard here.

## Pins

| Cane side | Grove socket | What it does |
| --- | --- | --- |
| Left | D2 | Left buzzer. Image-left boxes, or the manual left switch. |
| None | D3 | Status output only. On during the not-ready double blip. No buzzer. |
| Right | D4 | Right buzzer. Image-right boxes, or the manual right switch. |
| None | D5 | Link indicator. Slow blip while packets arrive. Fast pattern when the 500 ms watchdog expires. No buzzer. |

Use the Grove sockets on D2 and D4. Do not use D3 as the right side. A Grove cable is SIG, VCC, and GND. The module in the photo is marked GND, VCC, NC, SIG. NC is unused. SIG goes to the digital pin through the Grove socket.

Seeed's older Grove Buzzer v1.1 page describes an onboard transistor. That description is not proof for a module marked v1.2. Before you apply power, match your module to its own Seeed page. Connect it only if SIG is a logic input to the module's own driver. Do not connect a bare motor, or a piezo that expects the GPIO to supply its current, to D2 or D4. This repository does not add a new current rating for the module.

## How a side is chosen

Section 5 still accepts only person, bicycle, car, motorcycle, bus, and truck, with a score of at least 0.5, and only when the box center is inside the middle half of the picture (from 25% to 75% of both width and height). Boxes farther out are ignored.

The control named **Looks-close box size** adds one more gate. Its default is 20. A box sounds only when its width is at least that percent of the picture width, or its height is at least that percent of the picture height. Gray dashed boxes labeled "too small" passed the class, score, and center checks but failed this size check, so they do not beep. Raise the number to ignore more distant-looking boxes. Lower it if nearby objects stay silent. A large object that is actually far away can still pass. A small object that is actually close can still fail. The phone has no meter measurement for this. You do not reflash the board for this control. Reload the page in Bluefy so it does not keep the previous file.

For those boxes, image left and image right are the two sides of the unmirrored rear-camera frame. The page does not flip the video. A center x less than half the picture width sets the left bit. Greater than half sets the right bit. Exactly half sets both bits for that one box. Several boxes are combined, so a left box and a right box can sound together. That both-sides object pattern is a steady pulse. It is not the not-ready double blip.

The not-ready double blip plays on both buzzers when the camera, model, or page is not ready, and D3 stays on. It means the pipeline is down. It does not mean objects are on both sides. Quiet buzzers are not an all-clear: watch D5. A slow blip means the link is alive. A fast pattern means the link timed out.

## Update steps

1. In Arduino IDE, select Arduino UNO R4 WiFi. Use the UNO R4 board package 1.6.0 and ArduinoBLE 2.1.0, the same versions the compile job uses.
2. Open the sketch above and upload it. Serial Monitor is 115200. The sketch prints `Cane demo controller`, `D2 left buzzer, D4 right buzzer, D3 status logic, D5 link logic.`, and `Advertising CaneDemo as a BLE peripheral.`
3. Plug the left buzzer into Grove D2 and the right buzzer into Grove D4.
4. Merge this pull request, set GitHub Pages to GitHub Actions, and open the Pages URL in Bluefy. Reload the page so Bluefy does not keep the previous file. The old page and the new sketch do not belong together.
5. On the page, try camera alone, model alone, then Bluetooth. Use the manual left and manual right switches one at a time. Then turn on section 5.

## What a spotter can check

| What you do | What should happen |
| --- | --- |
| Power the board before the phone connects | D2 and D4 quiet, D5 fast |
| Connect, leave section 5 off and manual off | Both buzzers double-blip, D3 on, D5 slow. This is not-ready, not two objects |
| Manual left only | D2 steady pulse, D4 quiet, serial `source=simulation` |
| Manual right only | D4 steady pulse, D2 quiet |
| Section 5 on, one supported box left of center | D2 steady pulse, serial `source=image-heuristic` |
| Section 5 on, boxes on both image sides | D2 and D4 steady pulse, D3 off |
| Stop the camera or leave Bluefy | Double-blip or, if the link drops, D5 fast and both buzzers quiet |

Do this with eyes uncovered. The cane you already use stays the cane.
