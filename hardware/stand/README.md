# Elecrow CrowPanel Advance 7" Stand

A table-edge stand for the [CrowPanel Advance 7" case](../case/). The case drops into the cradle, which holds it angled back. The stand can be screwed to the top of a table or to the front face of one.

## Files

| File | What it is |
|---|---|
| `CrowPanelAdvance7Stand.FCStd` | FreeCAD source model (FreeCAD 1.1) |
| `CrowPanelAdvance7Stand.3mf` / `.stl` | The stand, already in print orientation |

## Printing

Print 1, base down, with no supports. It was printed on a Bambu Lab P1S in PLA: 0.4 mm nozzle, 0.20 mm layers, 3 walls, 25% infill.

## Mounting

There are two pairs of counterbored screw holes. They are 4 mm holes with 12 mm counterbores, sized to take whatever screws suit your table, such as #6–#8 wood screws or M3–M4 screws.

- **Base holes:** for screwing the stand to the top of a table.
- **Back holes:** for screwing it to the front face of a table. You will probably need a spacer block behind it so the stand sits far enough out from the table.

Front side holders make the case harder to knock off the stand.

The case can sit in the stand either way round. Side holes at both ends let you reach the power switch, and a channel inside each end keeps the switch from being flipped as you drop the case in.

## Editing the model

The model is a single body. One half is modelled and mirrored (`MirrorHalf`), so changes to one end apply to both. Features are named for what they do (e.g. `FrontHolder`, `SwitchChannel`, `BackScrewHole`, `BaseScrewCounterbore`).

The file is saved uncompressed so git can store changes to it efficiently. To keep it that way, set **Edit → Preferences → General → Document → Compression level** to 0 before saving.
