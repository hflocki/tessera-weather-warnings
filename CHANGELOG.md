# Changelog

## 1.3.1
- On a small screen, a tile of two rows with two warnings no longer leaves an empty line below its edge.

## 1.3.0
- The tile grows up to 4 x 3. From two rows it shows every warning with its headline in its level's colour, its time
  and its text, as the card does; one row high it stays as before.
- New option **Warning map**: the DWD's map of your region (Germany or a federal state) beside the warnings, on the
  tile from 2 x 1 and on the card. The app fetches the picture from www.dwd.de; boards without pictures (CYD) show none.
- The headlines on the card take their level's colour.

## 1.2.0
- NINA is gone: the plugin shows the DWD's warnings only. Its two NINA fields were required by the app, so the plugin
  could not be added without them (#1), and newer Home Assistant versions give NINA's sensors no text anyway.
- A warning that has ended leaves the tile and the card: the plugin reads the DWD's `warning_count` and shows only the
  slots it counts, because Home Assistant sends nothing for an attribute that is gone; a warning past its end is
  dropped too (#2).

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
