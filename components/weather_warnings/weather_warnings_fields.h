#pragma once

// The text sensors of plugin.yaml, numbered as FIELDS in __init__.py. Generated from one list; keep them together.
namespace esphome::weather_warnings {

enum Field : int {
  F_DWD_LEVEL = 0,
  F_DWD_1_NAME = 1,
  F_DWD_1_HEADLINE = 2,
  F_DWD_1_DESCRIPTION = 3,
  F_DWD_1_LEVEL = 4,
  F_DWD_1_START = 5,
  F_DWD_1_END = 6,
  F_DWD_2_NAME = 7,
  F_DWD_2_HEADLINE = 8,
  F_DWD_2_DESCRIPTION = 9,
  F_DWD_2_LEVEL = 10,
  F_DWD_2_START = 11,
  F_DWD_2_END = 12,
  F_DWD_3_NAME = 13,
  F_DWD_3_HEADLINE = 14,
  F_DWD_3_DESCRIPTION = 15,
  F_DWD_3_LEVEL = 16,
  F_DWD_3_START = 17,
  F_DWD_3_END = 18,
  F_ADV_LEVEL = 19,
  F_ADV_1_NAME = 20,
  F_ADV_1_HEADLINE = 21,
  F_ADV_1_LEVEL = 22,
  F_ADV_1_START = 23,
  F_ADV_1_END = 24,
  F_DWD_COUNT = 25,
  F_ADV_COUNT = 26,
  FIELD_COUNT = 27
};

// The first field of a slot, and the offset of each value in it.
static constexpr int DWD_SLOT[3] = {F_DWD_1_NAME, F_DWD_2_NAME, F_DWD_3_NAME};
enum DwdPart : int { D_NAME = 0, D_HEADLINE, D_DESCRIPTION, D_LEVEL, D_START, D_END };
enum AdvPart : int { A_NAME = 0, A_HEADLINE, A_LEVEL, A_START, A_END };

}  // namespace esphome::weather_warnings
