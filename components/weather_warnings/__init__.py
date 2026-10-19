"""Weather warnings: DWD and NINA warnings on a tile coloured by their level, and a card with all of them.

plugin.yaml reads every value from Home Assistant as a text sensor; this component hands them to the C++ by the index
of FIELDS, which weather_warnings_fields.h numbers the same way. register_plugin() takes the id, version, tile memory
and the screen's texts from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]
AUTO_LOAD = ["text_sensor"]

# The order is the C++'s (weather_warnings_fields.h); never reorder one without the other.
FIELDS = ['dwd_level', 'dwd_1_name', 'dwd_1_headline', 'dwd_1_description', 'dwd_1_level', 'dwd_1_start', 'dwd_1_end', 'dwd_2_name', 'dwd_2_headline', 'dwd_2_description', 'dwd_2_level', 'dwd_2_start', 'dwd_2_end', 'dwd_3_name', 'dwd_3_headline', 'dwd_3_description', 'dwd_3_level', 'dwd_3_start', 'dwd_3_end', 'adv_level', 'adv_1_name', 'adv_1_headline', 'adv_1_level', 'adv_1_start', 'adv_1_end', 'nina_1_state', 'nina_1_headline', 'nina_1_severity', 'nina_1_description', 'nina_1_start', 'nina_1_expires', 'nina_2_state', 'nina_2_headline', 'nina_2_severity', 'nina_2_description', 'nina_2_start', 'nina_2_expires']

weather_warnings_ns = cg.esphome_ns.namespace("weather_warnings")
WeatherWarnings = weather_warnings_ns.class_("WeatherWarnings", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {cv.GenerateID(): cv.declare_id(WeatherWarnings)}
    | {cv.Optional(name): cv.use_id(text_sensor.TextSensor) for name in FIELDS}
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    for index, name in enumerate(FIELDS):
        if name in config:
            cg.add(var.set_field(index, await cg.get_variable(config[name])))
