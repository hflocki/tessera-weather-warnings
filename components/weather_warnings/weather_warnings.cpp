#include "weather_warnings.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::weather_warnings {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---- Readable colours ---------------------------------------------------------------------------------------------

// A colour's luminance and the WCAG contrast of two: amber or yellow words on the light look's white card fall under
// what a person reads, so there they take the ink instead; on the dark look they keep their colour.
static float luminance(lv_color_t c) {
  auto channel = [](uint8_t v) {
    const float s = v / 255.0f;
    return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
  };
  return 0.2126f * channel(c.red) + 0.7152f * channel(c.green) + 0.0722f * channel(c.blue);
}
static bool reads_on_card(lv_color_t c) {
  const float a = luminance(c), b = luminance(ui::color(theme::CARD));
  return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f) >= 3.0f;
}
// A label (words or an icon) in a tone's or a role's colour where that reads on the card, else in the ink.
static void set_readable(lv_obj_t *label, tessera::Tone tone, theme::Role role) {
  if (reads_on_card(ui::tone_color(tone, role))) ui::set_tone(label, tone, role);
  else ui::set_color(label, theme::INK);
}

static const std::string EMPTY;

// ---- Reading the values -------------------------------------------------------------------------------------------

// Days since 1970 of a civil date (Howard Hinnant's days_from_civil).
static int32_t civil_days(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  return era * 146097 + static_cast<int32_t>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}

// "2026-10-19 18:00:00+00:00", "2026-10-19T20:00:00+02:00" or with "Z": seconds since 1970; 0 for anything else.
static uint32_t parse_time(const std::string &text) {
  int y, mo, d, h = 0, mi = 0, s = 0;
  if (sscanf(text.c_str(), "%4d-%2d-%2d", &y, &mo, &d) != 3 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  if (text.size() >= 16) sscanf(text.c_str() + 11, "%2d:%2d", &h, &mi);
  if (text.size() >= 19 && text[16] == ':') sscanf(text.c_str() + 17, "%2d", &s);
  long offset = 0;
  const size_t zone = text.find_first_of("+-Z", 19);
  if (zone != std::string::npos && text[zone] != 'Z') {
    int oh = 0, om = 0;
    sscanf(text.c_str() + zone + 1, "%2d:%2d", &oh, &om);
    offset = (oh * 60L + om) * 60L * (text[zone] == '-' ? -1 : 1);
  }
  const long long t = static_cast<long long>(civil_days(y, mo, d)) * 86400 + h * 3600 + mi * 60 + s - offset;
  return t > 0 ? static_cast<uint32_t>(t) : 0;
}

// Lowercase, umlauts as their plain vowel and ß as ss, for finding words.
static std::string fold(const std::string &text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == 0xC3 && i + 1 < text.size()) {
      const unsigned char n = static_cast<unsigned char>(text[i + 1]);
      const char *plain = n == 0xA4 || n == 0x84 ? "a" : n == 0xB6 || n == 0x96 ? "o" : n == 0xBC || n == 0x9C ? "u"
                          : n == 0x9F ? "ss" : nullptr;
      if (plain) {
        out += plain;
        ++i;
        continue;
      }
    }
    out += static_cast<char>(std::tolower(c));
  }
  return out;
}

// "STURMBÖEN" as "Sturmböen": the first letter kept, the rest lowercase, Ä Ö Ü too.
static std::string title_case(const std::string &text) {
  std::string out;
  bool first = true;
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == 0xC3 && i + 1 < text.size()) {
      unsigned char n = static_cast<unsigned char>(text[i + 1]);
      if (!first && (n == 0x84 || n == 0x96 || n == 0x9C)) n += 0x20;
      out += static_cast<char>(c);
      out += static_cast<char>(n);
      ++i;
    } else {
      out += static_cast<char>(first ? c : std::tolower(c));
    }
    first = c == ' ' || c == '-';
  }
  return out;
}

// A description without HTML line breaks and tags, its white space as single spaces.
static std::string plain_text(const std::string &text) {
  std::string out;
  bool space = false, tag = false;
  for (char c : text) {
    if (c == '<') {
      tag = true;
      space = true;
      continue;
    }
    if (tag) {
      if (c == '>') tag = false;
      continue;
    }
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      space = true;
      continue;
    }
    if (space && !out.empty()) out += ' ';
    space = false;
    out += c;
  }
  return out;
}

// The icon of a warning by the words of its name: a thunderstorm, wind, rain, snow, ice, fog, heat and so on.
static uint32_t icon_of(const std::string &words) {
  const std::string w = fold(words);
  auto has = [&](std::initializer_list<const char *> keys) {
    for (const char *k : keys)
      if (w.find(k) != std::string::npos) return true;
    return false;
  };
  if (has({"gewitter", "thunder"})) return 0xF0593;                      // weather-lightning
  if (has({"hagel", "hail"})) return 0xF0592;                            // weather-hail
  if (has({"sturm", "orkan", "wind", "boe", "storm", "gust"})) return 0xF059D;  // weather-windy
  if (has({"hochwasser", "flut", "flood"})) return 0xF1502;              // water-alert
  if (has({"regen", "rain"})) return 0xF0596;                            // weather-pouring
  if (has({"schnee", "snow"})) return 0xF0598;                           // weather-snowy
  if (has({"glatt", "eis", "frost", "ice"})) return 0xF0717;             // snowflake
  if (has({"nebel", "fog"})) return 0xF0591;                             // weather-fog
  if (has({"hitze", "heat"})) return 0xF050F;                            // thermometer
  if (has({"uv"})) return 0xF0599;                                       // weather-sunny
  if (has({"tauwetter", "thaw"})) return 0xF058C;                        // water
  if (has({"brand", "feuer", "fire"})) return 0xF0238;                   // fire
  return 0xF0028;                                                        // alert-circle
}

// The colour of a level, in both looks: 1 yellow, 2 orange, 3 red, 4 purple.
static void paint_level(lv_obj_t *obj, int level, bool fill, theme::Role normal) {
  Tone tone = Tone::NORMAL;
  theme::Role role = normal;
  switch (level) {
    case 1: role = theme::MARK_AMBER; break;
    case 2: tone = Tone::BUSY; break;
    case 3: tone = Tone::ALERT; break;
    case 4: role = theme::MARK_PURPLE; break;
    default: break;
  }
  if (fill) ui::set_fill(obj, tone, role);
  else set_readable(obj, tone, role);
}

// An object's opacity or fill opacity, written only when it differs: a write redraws the object, so not on every tick.
static void set_opa(lv_obj_t *obj, lv_opa_t opa) {
  if (lv_obj_get_style_opa(obj, LV_PART_MAIN) != opa) lv_obj_set_style_opa(obj, opa, 0);
}
static void set_bg_opa(lv_obj_t *obj, lv_opa_t opa) {
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != opa) lv_obj_set_style_bg_opa(obj, opa, 0);
}

static void place(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_obj_set_size(obj, std::max(1, w), std::max(1, h));
  lv_obj_set_pos(obj, x, y);
}

// A label inside a circle: a child of it, so the two are one part, centred in it.
static void inside(lv_obj_t *circle, lv_obj_t *label, int w, int h, int dy = 0) {
  lv_obj_set_style_pad_all(circle, 0, 0);
  lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(label, std::max(1, w), std::max(1, h));
  lv_obj_align(label, LV_ALIGN_CENTER, 0, dy);
}

const std::string &WeatherWarnings::field(int index) const {
  const text_sensor::TextSensor *s = index >= 0 && index < FIELD_COUNT ? fields_[index] : nullptr;
  return s && s->has_state() ? s->state : EMPTY;
}

static bool known(const std::string &s) { return !s.empty() && s != "unknown" && s != "unavailable" && s != "None"; }

bool WeatherWarnings::configured() const {
  return known(field(F_DWD_LEVEL));
}

// How many slots of a DWD sensor hold a warning: its warning_count, or all of them while the count is not known yet.
// Home Assistant sends nothing for an attribute that is removed, so a slot past the count keeps a stale warning.
static int slots_in_use(const std::string &count, int slots) {
  return known(count) ? std::clamp(atoi(count.c_str()), 0, slots) : slots;
}

// Whether a warning is over: its end is known and lies in the past (0 for an unknown clock or end).
static bool ended(uint32_t end, uint32_t now) { return end && now && end <= now; }

int WeatherWarnings::collect(Warning *out, bool advance) const {
  const uint32_t now = tessera::epoch();
  int n = 0;
  const int dwd_slots = slots_in_use(field(F_DWD_COUNT), 3);
  for (int i = 0; i < dwd_slots; ++i) {
    const int base = DWD_SLOT[i];
    if (!known(field(base + D_HEADLINE)) && !known(field(base + D_NAME))) continue;
    const uint32_t end = parse_time(field(base + D_END));
    if (ended(end, now)) continue;
    out[n++] = {Source::DWD, static_cast<uint8_t>(i), std::max(1, atoi(field(base + D_LEVEL).c_str())),
                parse_time(field(base + D_START)), end};
  }
  // The level without the details of its warnings (attributes not there yet): the level alone.
  if (n == 0 && atoi(field(F_DWD_LEVEL).c_str()) > 0)
    out[n++] = {Source::DWD, NO_SLOT, std::min(4, atoi(field(F_DWD_LEVEL).c_str())), 0, 0};
  std::stable_sort(out, out + n, [](const Warning &a, const Warning &b) { return a.level > b.level; });
  const uint32_t adv_end = parse_time(field(F_ADV_1_END));
  if (advance && slots_in_use(field(F_ADV_COUNT), 1) > 0 && !ended(adv_end, now) &&
      (known(field(F_ADV_1_HEADLINE)) || known(field(F_ADV_1_NAME))))
    out[n++] = {Source::ADVANCE, 0, std::max(1, atoi(field(F_ADV_1_LEVEL).c_str())), parse_time(field(F_ADV_1_START)),
                adv_end};
  else if (advance && atoi(field(F_ADV_LEVEL).c_str()) > 0)
    out[n++] = {Source::ADVANCE, NO_SLOT, std::min(4, atoi(field(F_ADV_LEVEL).c_str())), 0, 0};
  return n;
}

std::string WeatherWarnings::name(const Warning &w) const {
  if (w.slot == NO_SLOT) return headline(w);
  const int base = w.source == Source::DWD ? DWD_SLOT[w.slot] : F_ADV_1_NAME;
  const std::string &n = field(base + (w.source == Source::DWD ? static_cast<int>(D_NAME) : static_cast<int>(A_NAME)));
  return known(n) ? title_case(n) : headline(w);
}

std::string WeatherWarnings::headline(const Warning &w) const {
  if (w.slot == NO_SLOT) return tessera::format(text("level"), w.level);
  switch (w.source) {
    case Source::DWD: return field(DWD_SLOT[w.slot] + D_HEADLINE);
    default: return field(F_ADV_1_HEADLINE);
  }
}

std::string WeatherWarnings::description(const Warning &w) const {
  if (w.slot == NO_SLOT) return std::string();
  const std::string &d = w.source == Source::DWD ? field(DWD_SLOT[w.slot] + D_DESCRIPTION) : EMPTY;
  return known(d) ? plain_text(d) : std::string();
}

// ---- Times in words -----------------------------------------------------------------------------------------------

// "18:00" today, else "Tue 20 Oct 06:00".
static std::string moment(uint32_t t) {
  if (!t) return "";
  const std::string clock = tessera::clock_text(t);
  return tessera::days_from_today(t) == 0 ? clock : tessera::date_text(t) + " " + clock;
}

// ---- The DWD's warning map ----------------------------------------------------------------------------------------

// The address of the DWD's map of a region ("nrw", "de"), "" for none. Only the regions the manifest offers.
static std::string map_url(const char *region) {
  static const char *const REGIONS[] = {"de",  "baw", "bay", "bbb", "hes", "mvp", "nds",
                                        "nrw", "rps", "saa", "sac", "shh", "thu"};
  for (const char *r : REGIONS)
    if (region && strcmp(region, r) == 0)
      return std::string("https://www.dwd.de/DWD/warnungen/warnapp_gemeinden/json/warnungen_gemeinde_map_") + r + ".png";
  return std::string();
}

// ---- A list of warnings -------------------------------------------------------------------------------------------

// Warnings one under the other, the strongest first: a bar of its level's colour, its icon and headline in that
// colour, when it is in force and where it comes from, and its description in as many lines as its share allows.
// The card shows it over its room, a tile of two rows or more beside the map.
class WarningList {
 public:
  static constexpr int MAX_ENTRIES = WeatherWarnings::MAX_WARNINGS;

  void create(lv_obj_t *parent, int gap) {
    title_h_ = ui::line_height(Font::TITLE);
    body_h_ = ui::line_height(Font::BODY);
    icon_h_ = ui::line_height(Font::ICON_SMALL);
    gap_ = gap;
    bar_w_ = ui::px(ui::large() ? 6 : 4);
    for (Entry &e : entries_) {
      e.bar = ui::block(parent, theme::TRACK);
      e.icon = ui::label(parent, Font::ICON_SMALL, theme::MUTED);
      e.headline = ui::label(parent, Font::TITLE, theme::INK);
      e.meta = ui::label(parent, Font::BODY, theme::MUTED);
      e.text = ui::text(parent, Font::BODY, theme::INK_SOFT);
    }
  }

  // The first `n` warnings of `list` in the box x0, y0, w, h; the other entries empty.
  void paint(const WeatherWarnings *plugin, const Warning *list, int n, int x0, int y0, int w, int h, uint32_t now) {
    n = std::min(n, MAX_ENTRIES);
    const int head = std::max(title_h_, icon_h_);
    // Only as many warnings as have room for their head and time; each a share of the height.
    const int fits = std::max(1, (h + gap_) / (head + body_h_ + gap_));
    n = std::min(n, fits);
    const int share = n ? (h - (n - 1) * gap_) / n : 0;
    const int lines = std::max(0, std::min(n == 1 ? 40 : 5, (share - head - body_h_ - ui::px(4)) / body_h_));
    int y = y0;
    for (int i = 0; i < MAX_ENTRIES; ++i) {
      Entry &e = entries_[i];
      if (i >= n) {
        set_opa(e.bar, LV_OPA_TRANSP);
        ui::set_text(e.icon, "");
        ui::set_text(e.headline, "");
        ui::set_text(e.meta, "");
        ui::set_text(e.text, "");
        continue;
      }
      const Warning &wa = list[i];
      const int x = x0 + bar_w_ + ui::px(10), tw = std::max(1, w - (x - x0));
      const int text_h = lines * body_h_;
      const int eh = head + body_h_ + (text_h ? ui::px(4) + text_h : 0);
      set_opa(e.bar, LV_OPA_COVER);
      place(e.bar, x0, y, bar_w_, eh);
      paint_level(e.bar, wa.level, true, theme::TRACK);
      place(e.icon, x, y + (head - icon_h_) / 2, icon_h_, icon_h_);
      ui::set_text(e.icon, ui::icon(icon_of(plugin->name(wa) + " " + plugin->headline(wa))));
      paint_level(e.icon, wa.level, false, theme::MUTED);
      place(e.headline, x + icon_h_ + ui::px(8), y + (head - title_h_) / 2, tw - icon_h_ - ui::px(8), title_h_);
      ui::set_text(e.headline, plugin->headline(wa));
      paint_level(e.headline, wa.level, false, theme::INK);
      place(e.meta, x, y + head, tw, body_h_);
      ui::set_text(e.meta, meta(plugin, wa, now));
      ui::set_color(e.meta, theme::MUTED);
      place(e.text, x, y + head + body_h_ + ui::px(4), tw, std::max(1, text_h));
      ui::set_text(e.text, text_h ? plugin->description(wa) : "");
      ui::set_color(e.text, theme::INK_SOFT);
      y += eh + gap_;
    }
  }

 private:
  struct Entry {
    lv_obj_t *bar{}, *icon{}, *headline{}, *meta{}, *text{};
  };

  // "Mon 18:00 – Tue 06:00 · DWD", "until 22:00 · DWD", "from Wed 06:00 · Advance warning (DWD)".
  static std::string meta(const WeatherWarnings *plugin, const Warning &w, uint32_t now) {
    std::string when;
    if (w.start && w.end && (!now || w.start > now))
      when = moment(w.start) + " – " + moment(w.end);
    else if (w.end)
      when = tessera::fill(plugin->text("until"), "time", moment(w.end));
    else if (w.start)
      when = tessera::fill(plugin->text("from"), "time", moment(w.start));
    const char *source = w.source == Source::DWD ? "dwd" : "advance";
    return when.empty() ? std::string(plugin->text(source)) : when + " · " + plugin->text(source);
  }

  int title_h_ = 0, body_h_ = 0, icon_h_ = 0, gap_ = 0, bar_w_ = 0;
  Entry entries_[MAX_ENTRIES];
};

// ---- The tile -----------------------------------------------------------------------------------------------------

// One row high: the strongest warning, its icon in a circle of its level's colour, its short name, until when it
// lasts and how many more there are; the tile tinted in the level's colour. Two rows or more: every warning with its
// text. From two columns, the DWD's map of the chosen region beside it. Without a warning a quiet shield and
// "No warnings". A tap opens every warning.
class StatusTile : public tessera::Tile {
 public:
  explicit StatusTile(const WeatherWarnings *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    tile_ = c.tile;
    snprintf(name_, sizeof name_, "%s", c.name && *c.name ? c.name : plugin_->text("title"));
    advance_ = c.options["advance"] | true;
    snprintf(region_, sizeof region_, "%s", c.options["map"] | "");
    big_ = c.rows >= 2;
    const std::string url = map_url(region_);

    // The tint covers the whole tile, its padding too, with the tile's own corners. Floating: it never makes the
    // tile scroll.
    const int pl = lv_obj_get_style_pad_left(c.parent, LV_PART_MAIN), pt = lv_obj_get_style_pad_top(c.parent, LV_PART_MAIN);
    const int pr = lv_obj_get_style_pad_right(c.parent, LV_PART_MAIN);
    const int pb = lv_obj_get_style_pad_bottom(c.parent, LV_PART_MAIN);
    tint_ = ui::block(c.parent, theme::CARD);
    lv_obj_add_flag(tint_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(tint_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(tint_, lv_obj_get_style_radius(c.parent, LV_PART_MAIN), 0);
    place(tint_, -pl, -pt, width_ + pl + pr, height_ + pt + pb);
    lv_obj_set_style_bg_opa(tint_, LV_OPA_TRANSP, 0);

    // The map at the right: as high as a tile of one row, else up to two fifths of the width.
    const int gap = ui::px(ui::large() ? 12 : 8);
    if (!url.empty() && c.columns >= 2 && ui::pictures()) {
      const int side = big_ ? std::min(width_ * 2 / 5, height_) : std::min(height_, width_ / 2);
      map_ = ui::picture(c.parent);
      place(map_, width_ - side, big_ ? 0 : (height_ - side) / 2, side, side);
      ui::set_picture(map_, url, tessera::Fit::CONTAIN);
      left_w_ = width_ - side - gap;
    } else {
      left_w_ = width_;
    }

    const int title_h = ui::line_height(Font::TITLE), body_h = ui::line_height(Font::BODY);
    dia_ = std::min({ui::mm(12), height_ - ui::px(8), left_w_ / 3});
    dia_ = std::max(dia_, ui::line_height(Font::ICON_SMALL) + ui::px(8));
    big_icon_ = dia_ >= ui::line_height(Font::ICON) + ui::px(8);
    const int cy = height_ / 2;
    circle_ = ui::block(c.parent, theme::TRACK);
    lv_obj_set_style_radius(circle_, LV_RADIUS_CIRCLE, 0);
    place(circle_, 0, cy - dia_ / 2, dia_, dia_);
    icon_ = ui::label(circle_, big_icon_ ? Font::ICON : Font::ICON_SMALL, theme::MUTED);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    const int ih = ui::line_height(big_icon_ ? Font::ICON : Font::ICON_SMALL);
    inside(circle_, icon_, dia_, ih);

    const int x = dia_ + ui::px(10), w = left_w_ - x;
    title_ = ui::label(c.parent, Font::TITLE, theme::INK);
    place(title_, x, cy - (title_h + body_h) / 2, w, title_h);
    sub_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    place(sub_, x, cy - (title_h + body_h) / 2 + title_h, w, body_h);

    if (big_) list_.create(c.parent, ui::px(ui::large() ? 12 : 8));
  }

  void on_tick(uint32_t now) override {
    Warning list[WeatherWarnings::MAX_WARNINGS];
    if (big_) {
      const int n = plugin_->collect(list, advance_);
      list_.paint(plugin_, list, n, 0, 0, left_w_, height_, now);
      if (n > 0) {
        // The head of the small tile stays empty; the tint follows the strongest warning in force.
        set_opa(circle_, LV_OPA_TRANSP);
        ui::set_text(title_, "");
        ui::set_text(sub_, "");
        if (list[0].source == Source::DWD) {
          paint_level(tint_, list[0].level, true, theme::CARD);
          set_bg_opa(tint_, LV_OPA_10);
        } else {
          set_bg_opa(tint_, LV_OPA_TRANSP);
        }
        return;
      }
      set_opa(circle_, LV_OPA_COVER);
      quiet();
      return;
    }
    const int n = plugin_->collect(list, false);
    if (n == 0) {
      // Without a warning in force, an advance warning shows muted, else a quiet shield.
      Warning adv[WeatherWarnings::MAX_WARNINGS];
      const int a = advance_ ? plugin_->collect(adv, true) : 0;
      if (a > 0) {
        set_bg_opa(tint_, LV_OPA_TRANSP);
        ui::set_fill(circle_, Tone::NORMAL, theme::TRACK);
        set_bg_opa(circle_, LV_OPA_60);
        ui::set_text(icon_, ui::icon(icon_of(plugin_->name(adv[0]) + " " + plugin_->headline(adv[0]))));
        paint_level(icon_, adv[0].level, false, theme::MUTED);
        ui::set_text(title_, plugin_->name(adv[0]));
        ui::set_text(sub_, tessera::fill(plugin_->text("advance_from"), "time", moment(adv[0].start)));
        ui::set_color(title_, theme::INK);
        ui::set_color(sub_, theme::MUTED);
      } else {
        quiet();
      }
      return;
    }
    const Warning &w = list[0];
    // The tile takes the level's colour lightly, the circle a little more, the icon fully.
    paint_level(tint_, w.level, true, theme::CARD);
    set_bg_opa(tint_, LV_OPA_10);
    paint_level(circle_, w.level, true, theme::TRACK);
    set_bg_opa(circle_, LV_OPA_30);
    ui::set_text(icon_, ui::icon(icon_of(plugin_->name(w) + " " + plugin_->headline(w))));
    paint_level(icon_, w.level, false, theme::INK);
    ui::set_text(title_, plugin_->name(w));
    ui::set_color(title_, theme::INK);
    std::string sub;
    if (now && w.start > now) sub = tessera::fill(plugin_->text("from"), "time", moment(w.start));
    else if (w.end) sub = tessera::fill(plugin_->text("until"), "time", moment(w.end));
    if (sub.empty()) sub = plugin_->text("dwd");
    if (n > 1) sub += " · " + tessera::format(plugin_->text("more"), n - 1);
    ui::set_text(sub_, sub);
    ui::set_color(sub_, theme::INK_SOFT);
  }

  void on_theme() override { on_tick(tessera::epoch()); }

  void on_tap() override {
    plugin_->set_region(region_);
    tessera::open_card(plugin_->plugin_id(), "warnings", "", tile_);
  }

 private:
  // No warning: the shield, the tile's name and "No warnings" (or what to choose).
  void quiet() {
    set_bg_opa(tint_, LV_OPA_TRANSP);
    ui::set_fill(circle_, Tone::NORMAL, theme::TRACK);
    set_bg_opa(circle_, LV_OPA_60);
    ui::set_text(icon_, ui::icon(0xF0498));  // shield
    ui::set_color(icon_, theme::MUTED);
    ui::set_text(title_, name_);
    ui::set_text(sub_, plugin_->text(plugin_->configured() ? "none" : "choose"));
    ui::set_color(title_, theme::INK);
    ui::set_color(sub_, theme::MUTED);
  }

  const WeatherWarnings *plugin_;
  char name_[48]{}, region_[8]{};
  int width_ = 0, height_ = 0, tile_ = -1, dia_ = 0, left_w_ = 0;
  bool advance_ = true, big_icon_ = false, big_ = false;
  lv_obj_t *tint_{}, *circle_{}, *icon_{}, *title_{}, *sub_{}, *map_{};
  WarningList list_;
};

// ---- The card -----------------------------------------------------------------------------------------------------

// Every warning, the strongest first and the advance warning last, and the DWD's map of the region the tile that
// opened it shows, at the right.
class WarningsCard : public tessera::Card {
 public:
  explicit WarningsCard(const WeatherWarnings *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    width_ = c.width;
    height_ = c.height;
    list_w_ = width_;
    const int gap = ui::px(ui::large() ? 16 : 10);
    const std::string url = map_url(plugin_->region());
    if (!url.empty() && ui::pictures()) {
      const int side = std::min(width_ * 2 / 5, height_);
      map_ = ui::picture(c.parent);
      place(map_, width_ - side, 0, side, side);
      ui::set_picture(map_, url, tessera::Fit::CONTAIN);
      list_w_ = width_ - side - gap;
    }
    list_.create(c.parent, gap);
    note_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    place(note_, 0, 0, list_w_, ui::line_height(Font::BODY_LARGE));
    on_tick(tessera::epoch());
  }

  void on_tick(uint32_t now) override {
    Warning list[WeatherWarnings::MAX_WARNINGS];
    const int n = plugin_->collect(list, true);
    ui::set_text(note_, n ? "" : plugin_->text("none"));
    list_.paint(plugin_, list, n, 0, 0, list_w_, height_, now);
  }

  void on_theme() override { on_tick(tessera::epoch()); }

 private:
  const WeatherWarnings *plugin_;
  int width_ = 0, height_ = 0, list_w_ = 0;
  WarningList list_;
  lv_obj_t *note_{}, *map_{};
};

// ---- The plugin ---------------------------------------------------------------------------------------------------

void WeatherWarnings::setup() {
  add_tile("status", [this]() { return new StatusTile(this); });
  add_card("warnings", [this]() { return new WarningsCard(this); }, true);
}

}  // namespace esphome::weather_warnings
