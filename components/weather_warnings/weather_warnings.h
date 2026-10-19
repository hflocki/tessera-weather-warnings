#pragma once
#include <cstdint>
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "weather_warnings_fields.h"

namespace esphome::weather_warnings {

// Where a warning comes from.
enum class Source : uint8_t { DWD, NINA, ADVANCE };

// One warning as the plugin finds it: its source and slot (to read its words from the text sensors), its level from
// 1 (a weather warning, minor) to 4 (extreme), and when it starts and ends (seconds since 1970, 0 for not known).
struct Warning {
  Source source;
  uint8_t slot;
  int level;
  uint32_t start, end;
};

// The plugin: a tile ("status") and a card with every warning ("warnings"). It holds the text sensors plugin.yaml
// reads from Home Assistant.
class WeatherWarnings : public Component, public tessera::Plugin {
 public:
  static constexpr int MAX_WARNINGS = 6;  // three of the DWD, two of NINA, one advance warning

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_field(int index, text_sensor::TextSensor *s) {
    if (index >= 0 && index < FIELD_COUNT) fields_[index] = s;
  }
  // The text of a field, "" while it has none.
  const std::string &field(int index) const;

  // The warnings in force, the strongest first (the DWD's before NINA's at the same level); with `advance`, the
  // advance warning after them. Returns how many.
  int collect(Warning *out, bool advance) const;
  // Whether any entity was chosen at all (the DWD sensor or a NINA slot reports something).
  bool configured() const;

  // A warning's words: its short name (DWD "Sturmböen", NINA its headline), its headline and its description.
  std::string name(const Warning &w) const;
  std::string headline(const Warning &w) const;
  std::string description(const Warning &w) const;

 protected:
  text_sensor::TextSensor *fields_[FIELD_COUNT]{};
};

}  // namespace esphome::weather_warnings
