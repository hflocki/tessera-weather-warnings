# Changelog

## 1.1.2
- The icon sits inside its circle as one part, and an idle circle is lighter, so the grey icon reads in the light look too.
- Amber and yellow words and icons take the ink on the light look, where they were too faint to read; the dark look keeps its colours.

## 1.1.1
- Opacities are written only when they change, so the tile is not redrawn every second.

## 1.1.0
- The tint covers the whole tile with its own corners; the circle is light and the icon takes the level's colour.
- A DWD level without the details of its warnings shows as "Warning level 1", the advance warning too.
- NINA's severity is read whatever its case, for a template sensor fed by `nina.get_details` (README).
- Without a time, the tile names the source of the warning.

## 1.0.0
- First version: the strongest DWD or NINA warning on a tile in the colour of its level, and a card with every
  warning, its time, source and text.
