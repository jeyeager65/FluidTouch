# Elecrow CrowPanel Advance 7" Case

A two-piece 3D printed case for the 7" CrowPanel **Advance** (it does not fit the Basic). It has a front bezel (Top) and a back shell (Bottom) that screw together at the corners. Inside there is room for a battery up to 12 mm thick, switched by a rocker switch on the side. The case has openings for the SD card, USB-C and the switch, a light pipe for the charge/power LEDs, and printed boot/reset buttons.

It was designed for FluidTouch but doesn't depend on it, so it works for any project using this display. There is also a matching [table stand](../stand/).

> **Connections:** the case works with wireless or USB connections. It has no opening for a UART cable, so it doesn't support a wired UART connection.

## Files

`CrowPanelAdvance7Case.FCStd` is the FreeCAD source model (FreeCAD 1.1). Every part is also exported as both `.3mf` and `.stl`, already in its print orientation:

| Part | Files |
|---|---|
| Front bezel | `CrowPanelAdvance7Case-Top.3mf` / `.stl` |
| Back shell | `CrowPanelAdvance7Case-Bottom.3mf` / `.stl` |
| Battery bracket | `CrowPanelAdvance7Case-BatteryBracket.3mf` / `.stl` |
| Boot/reset button, v1.4 and v1.5 boards | `CrowPanelAdvance7Case-Button-v1.4.3mf` / `.stl` |
| Boot/reset button, v1.3 boards | `CrowPanelAdvance7Case-Button-v1.3.3mf` / `.stl` |

## Printing

Printed on a Bambu Lab P1S in PLA: 0.4 mm nozzle, 0.20 mm layers, 3 walls, 30% infill. Nothing needs supports.

| Part | Qty | Orientation | Notes |
|---|---|---|---|
| Bottom | 1 | Flat side down | The screw counterbores are bridged by a single layer. Poke through it when assembling. |
| Top | 1 | Top side down | |
| Button | 2 | Top of the button down | Use the button for your board revision. Needs a brim (a 0.5 mm brim-object gap helps it come off cleanly). May need cleanup with a razor. |
| Battery Bracket | 2 | On its side | |

The v1.3 and v1.4 boards differ only in their boot/reset buttons, and v1.5 only changed the silkscreen, so the Top, Bottom and brackets are the same for all of them. v1.5 boards use the v1.4 buttons. The button fit is tight, so you may need to adjust it for your printer.

## Hardware

Screws (button or socket head):
- 4 × M3 × 20 mm to join the Top and Bottom
- 4 × M3 × 6 mm to attach the battery brackets

Battery and switch wiring:
- 1 × [15 × 10 mm rocker switch](https://www.amazon.com/dp/B0CSJTHZHR)
- 2 × [0.187" spade terminals](https://www.amazon.com/dp/B01N5APVEE) to connect to the switch, or solder the wires to it instead
- [JST PH2.0 male and female connectors](https://www.amazon.com/dp/B088NFRNZ2), or cut the battery leads and splice them
- 22 AWG wire and heat shrink tubing
- A LiPo battery up to 12 mm thick. Tested with an [8000 mAh 126090](https://www.amazon.com/dp/B0C2HN5M7M) (12 × 60 × 90 mm) and a [10000 mAh](https://www.amazon.com/dp/B093WS6C66). At least 4000 mAh is recommended.

> **Check the battery polarity.** Every battery and wire color tested so far has been reversed compared with the display's battery plug. Match the polarity printed on the display PCB, swapping the pins in the connector if needed.

## Assembly

The buttons are not attached. They sit loose in the button holes in the Bottom and are held in place between the case and the display board.

- **v1.3 board:** assemble in either order.
- **v1.4 and v1.5 boards:** the board's switches are much shorter, so place the printed buttons in the holes in the Bottom first, then fit the Top and display on top.

The v1.3 buttons are sized for that board's original 4.5 × 4.5 mm, 9 mm tall tactile switches. If you replace a switch on a v1.3 board with a shorter one, the buttons won't reach it.

## Editing the model

The model tree has one group, **Printed Parts**: Top, Bottom, both button versions and the battery bracket. The v1.4 buttons are shown by default. Each button version has a linked copy in the second hole, and the bracket has a linked copy on the second pair of posts. Each copy is the same part, moved into place.

Features are named for what they do (e.g. `TopUSBCCutout`, `BottomPowerSwitchCutout`, `LightPipePost`, `BatteryStop`). Each port opening is split between the Top and Bottom bodies.

The CrowPanel board itself is not included, to keep the file small. To check fit, import Elecrow's [STEP model of the CrowPanel Advance 7"](https://github.com/Elecrow-RD/CrowPanel-Advance-7-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/master/3D%20file) into the document.

The file is saved uncompressed so git can store changes to it efficiently. To keep it that way, set **Edit → Preferences → General → Document → Compression level** to 0 before saving.
