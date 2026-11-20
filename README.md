# Weather warnings

Weather warnings of the DWD from Home Assistant on one tile, in the spirit of the Lovelace
[nina-dwd-card](https://github.com/timmaurice/lovelace-nina-dwd-card).

**The tile** (1 x 1 and up) shows the strongest warning in force: its icon (thunderstorm, wind, rain, snow, ice, fog,
heat ...) in a circle of its level's colour, its short name ("Sturmböen") and until when it lasts, and "+2" when there
are more. The tile itself takes the level's colour lightly: yellow for a weather warning, orange for notable weather,
red for severe weather, purple for extreme weather. Without a warning it shows a quiet shield and "No warnings", or the DWD's advance warning, muted.

**Larger tiles** (two rows and more) show every warning as the card does: its headline in its level's colour, when
it is in force and its text. With a **Warning map** chosen, the DWD's map of your region stands beside them, from
2 x 1 on, and on the card too. Boards without pictures (the CYD, the 3.5- and 4-inch Hosyond and Waveshare) leave the
map out.

**A tap** opens a card with every warning, the strongest first and the advance warning last: a bar in its level's
colour, its icon and headline, when it is in force and where it comes from, and its text in as many lines as fit.

## Set up

1. Have the [DWD Weather Warnings](https://www.home-assistant.io/integrations/dwd_weather_warnings/) integration for
   your region (sensors `..._current_warning_level` and `..._advance_warning_level`).
2. In Tessera, open **Plugins**, **Add with a link**, and paste the link of this repository. Add the plugin to a
   screen.
3. In the plugin's details on the screen's Plugins tab, under **When building**, choose **DWD current warning level**
   and **DWD advance warning level**. **Save and build**.
4. In **Layout**, place the **Weather warnings** tile: 1 x 1 for the strongest warning, 2 x 2 or larger for every
   warning with its text.
5. Optional: in the tile's options, choose a **Warning map** (Germany or your federal state) for the DWD's map beside
   the warnings.

## Good to know

- The screen reads the sensors and their attributes itself; a change of the chosen entities builds the screen again.
- The DWD's first three warnings and its first advance warning are shown. A warning disappears as soon as Home
  Assistant no longer counts it or its end time has passed.
- Without a time the tile names the source ("DWD"). A DWD level whose warnings have no details (yet) shows as
  "Warning level 1".


## Privacy

With a **Warning map** chosen, the Tessera app in Home Assistant fetches the map, a PNG, from `www.dwd.de` (the
Deutscher Wetterdienst) and sends it to the screen. The DWD sees the address of your Home Assistant, as any visitor of
its website; nothing else is sent. Without a map the plugin reaches no service outside your home. The maps are the
DWD's, free to use with its name as the source (`Quelle: Deutscher Wetterdienst`).

## A hobby project

This plugin is a private, non-commercial hobby project. I build and maintain it in my spare time and share it
free of charge as open source under the [MIT License](LICENSE). It is free and will stay free: there is no paid
version, no paid feature and no paid support.

Bug reports and ideas are welcome, but please keep in mind:

- There is no support and no guaranteed response time. I answer when I find the time, which can take days or weeks.
- Not every request gets built. I add what fits the plugin and what I can keep working over time.
- Pull requests are welcome; for a larger change, open an issue first so we can agree on the way.

### No warranty, no liability

The plugin is provided "as is", without any warranty and without guaranteed support, updates or fixes. You use it at
your own risk. To the extent permitted by law, I accept no liability for any damage or data loss resulting from its
use. The full terms are in the [MIT License](LICENSE).

### Buy me a coffee

If the plugin is useful to you and you would like to say thanks, you are welcome to buy me a coffee. A donation is
entirely voluntary: it is a thank-you, not a payment. It does not buy support, features, a response time or any other
service, and the plugin stays exactly the same for everyone, whether they donate or not.

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=hflocki&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://www.buymeacoffee.com/hflocki)
