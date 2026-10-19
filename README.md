# Weather warnings

Warnings of the DWD and NINA from Home Assistant on one tile, in the spirit of the Lovelace
[nina-dwd-card](https://github.com/timmaurice/lovelace-nina-dwd-card).

**The tile** (1 x 1 and up) shows the strongest warning in force: its icon (thunderstorm, wind, rain, snow, ice, fog,
heat ...) in a circle of its level's colour, its short name ("Sturmböen") and until when it lasts, and "+2" when there
are more. The tile itself takes the level's colour lightly: yellow for a weather warning, orange for notable weather,
red for severe weather, purple for extreme weather. NINA's severities count the same (Minor, Moderate, Severe,
Extreme). Without a warning it shows a quiet shield and "No warnings", or the DWD's advance warning, muted.

**A tap** opens a card with every warning, the strongest first and the advance warning last: a bar in its level's
colour, its icon and headline, when it is in force and where it comes from, and its text in as many lines as fit.

## Set up

1. Have the [DWD Weather Warnings](https://www.home-assistant.io/integrations/dwd_weather_warnings/) integration for
   your region (sensors `..._current_warning_level` and `..._advance_warning_level`) and, if you like, the
   [NINA](https://www.home-assistant.io/integrations/nina/) integration (binary sensors `binary_sensor.warning_<place>_1`
   and so on).
2. In Tessera, open **Plugins**, **Add with a link**, and paste the link of this repository. Add the plugin to a
   screen.
3. In the plugin's details on the screen's Plugins tab, under **When building**, choose **DWD current warning level**,
   **DWD advance warning level** and up to two **NINA warning** slots; leave out what you do not use. **Save and
   build**.
4. In **Layout**, place the **Weather warnings** tile.

## Good to know

- The screen reads the sensors and their attributes itself; a change of the chosen entities builds the screen again.
- The DWD's first three warnings, its first advance warning and two NINA slots are shown.
- NINA's headline, severity, text and times come from the attributes of its binary sensors. Home Assistant is moving
  them to the action `nina.get_details`; where a slot no longer has them, the tile says "NINA warning" in its colour.
