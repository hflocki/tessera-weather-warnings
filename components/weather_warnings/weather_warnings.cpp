#include "weather_warnings.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::weather_warnings {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

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

// NINA's severity as the DWD's levels: Minor 1, Moderate 2, Severe 3, Extreme 4.
static int nina_level(const std::string &severity) {
  if (severity == "Extreme") return 4;
  if (severity == "Severe") return 3;
  if (severity == "Moderate") return 2;
  return 1;
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
  else ui::set_tone(obj, tone, role);
}

static void place(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_obj_set_size(obj, std::max(1, w), std::max(1, h));
  lv_obj_set_pos(obj, x, y);
}

const std::string &WeatherWarnings::field(int index) const {
  const text_sensor::TextSensor *s = index >= 0 && index < FIELD_COUNT ? fields_[index] : nullptr;
  return s && s->has_state() ? s->state : EMPTY;
}

static bool known(const std::string &s) { return !s.empty() && s != "unknown" && s != "unavailable" && s != "None"; }

bool WeatherWarnings::configured() const {
  return known(field(F_DWD_LEVEL)) || known(field(F_NINA_1_STATE)) || known(field(F_NINA_2_STATE));
}

int WeatherWarnings::collect(Warning *out, bool advance) const {
  int n = 0;
  for (int i = 0; i < 3; ++i) {
    const int base = DWD_SLOT[i];
    if (!known(field(base + D_HEADLINE)) && !known(field(base + D_NAME))) continue;
    out[n++] = {Source::DWD, static_cast<uint8_t>(i), std::max(1, atoi(field(base + D_LEVEL).c_str())),
                parse_time(field(base + D_START)), parse_time(field(base + D_END))};
  }
  for (int j = 0; j < 2; ++j) {
    const int base = NINA_SLOT[j];
    if (field(base + N_STATE) != "on") continue;
    out[n++] = {Source::NINA, static_cast<uint8_t>(j), nina_level(field(base + N_SEVERITY)),
                parse_time(field(base + N_START)), parse_time(field(base + N_EXPIRES))};
  }
  std::stable_sort(out, out + n, [](const Warning &a, const Warning &b) { return a.level > b.level; });
  if (advance && (known(field(F_ADV_1_HEADLINE)) || known(field(F_ADV_1_NAME))))
    out[n++] = {Source::ADVANCE, 0, std::max(1, atoi(field(F_ADV_1_LEVEL).c_str())), parse_time(field(F_ADV_1_START)),
                parse_time(field(F_ADV_1_END))};
  return n;
}

std::string WeatherWarnings::name(const Warning &w) const {
  if (w.source == Source::NINA) return headline(w);
  const int base = w.source == Source::DWD ? DWD_SLOT[w.slot] : F_ADV_1_NAME;
  const std::string &n = field(base + (w.source == Source::DWD ? static_cast<int>(D_NAME) : static_cast<int>(A_NAME)));
  return known(n) ? title_case(n) : headline(w);
}

std::string WeatherWarnings::headline(const Warning &w) const {
  switch (w.source) {
    case Source::DWD: return field(DWD_SLOT[w.slot] + D_HEADLINE);
    case Source::NINA: {
      const std::string &h = field(NINA_SLOT[w.slot] + N_HEADLINE);
      return known(h) ? h : std::string(text("nina_warning"));
    }
    default: return field(F_ADV_1_HEADLINE);
  }
}

std::string WeatherWarnings::description(const Warning &w) const {
  const std::string &d = w.source == Source::DWD    ? field(DWD_SLOT[w.slot] + D_DESCRIPTION)
                         : w.source == Source::NINA ? field(NINA_SLOT[w.slot] + N_DESCRIPTION)
                                                    : EMPTY;
  return known(d) ? plain_text(d) : std::string();
}

// ---- Times in words -----------------------------------------------------------------------------------------------

// "18:00" today, else "Tue 20 Oct 06:00".
static std::string moment(uint32_t t) {
  if (!t) return "";
  const std::string clock = tessera::clock_text(t);
  return tessera::days_from_today(t) == 0 ? clock : tessera::date_text(t) + " " + clock;
}

// ---- The tile -----------------------------------------------------------------------------------------------------

// The strongest warning: its icon in a circle of its level's colour, its short name, until when it lasts and how many
// more there are; the tile tinted in the level's colour. Without a warning a quiet shield and "No warnings". A tap
// opens every warning.
class StatusTile : public tessera::Tile {
 public:
  explicit StatusTile(const WeatherWarnings *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    tile_ = c.tile;
    snprintf(name_, sizeof name_, "%s", c.name && *c.name ? c.name : plugin_->text("title"));
    advance_ = c.options["advance"] | true;

    tint_ = ui::block(c.parent, theme::CARD);
    place(tint_, 0, 0, width_, height_);
    lv_obj_set_style_bg_opa(tint_, LV_OPA_TRANSP, 0);

    const int title_h = ui::line_height(Font::TITLE), body_h = ui::line_height(Font::BODY);
    dia_ = std::min({ui::mm(12), height_ - ui::px(8), width_ / 3});
    dia_ = std::max(dia_, ui::line_height(Font::ICON_SMALL) + ui::px(8));
    big_icon_ = dia_ >= ui::line_height(Font::ICON) + ui::px(8);
    const int cy = height_ / 2;
    circle_ = ui::block(c.parent, theme::TRACK);
    lv_obj_set_style_radius(circle_, LV_RADIUS_CIRCLE, 0);
    place(circle_, 0, cy - dia_ / 2, dia_, dia_);
    icon_ = ui::label(c.parent, big_icon_ ? Font::ICON : Font::ICON_SMALL, theme::MUTED);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    const int ih = ui::line_height(big_icon_ ? Font::ICON : Font::ICON_SMALL);
    place(icon_, 0, cy - ih / 2, dia_, ih);

    const int x = dia_ + ui::px(10), w = width_ - x;
    title_ = ui::label(c.parent, Font::TITLE, theme::INK);
    place(title_, x, cy - (title_h + body_h) / 2, w, title_h);
    sub_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    place(sub_, x, cy - (title_h + body_h) / 2 + title_h, w, body_h);
  }

  void on_tick(uint32_t now) override {
    Warning list[WeatherWarnings::MAX_WARNINGS];
    const int n = plugin_->collect(list, false);
    if (n == 0) {
      lv_obj_set_style_bg_opa(tint_, LV_OPA_TRANSP, 0);
      ui::set_fill(circle_, Tone::NORMAL, theme::TRACK);
      // Without a warning in force, an advance warning shows muted, else a quiet shield.
      Warning adv[WeatherWarnings::MAX_WARNINGS];
      const int a = advance_ ? plugin_->collect(adv, true) : 0;
      if (a > 0) {
        ui::set_text(icon_, ui::icon(icon_of(plugin_->name(adv[0]) + " " + plugin_->headline(adv[0]))));
        paint_level(icon_, adv[0].level, false, theme::MUTED);
        ui::set_text(title_, plugin_->name(adv[0]));
        ui::set_text(sub_, tessera::fill(plugin_->text("advance_from"), "time", moment(adv[0].start)));
      } else {
        ui::set_text(icon_, ui::icon(0xF0498));  // shield
        ui::set_color(icon_, theme::MUTED);
        ui::set_text(title_, name_);
        ui::set_text(sub_, plugin_->text(plugin_->configured() ? "none" : "choose"));
      }
      ui::set_color(title_, theme::INK);
      ui::set_color(sub_, theme::MUTED);
      return;
    }
    const Warning &w = list[0];
    // The tile takes the level's colour, lightly; the circle fully.
    lv_obj_set_style_bg_opa(tint_, LV_OPA_20, 0);
    paint_level(tint_, w.level, true, theme::CARD);
    paint_level(circle_, w.level, true, theme::TRACK);
    ui::set_text(icon_, ui::icon(icon_of(plugin_->name(w) + " " + plugin_->headline(w))));
    ui::set_color(icon_, theme::ON_ACCENT);
    ui::set_text(title_, plugin_->name(w));
    ui::set_color(title_, theme::INK);
    std::string sub;
    if (now && w.start > now) sub = tessera::fill(plugin_->text("from"), "time", moment(w.start));
    else if (w.end) sub = tessera::fill(plugin_->text("until"), "time", moment(w.end));
    if (n > 1) sub += (sub.empty() ? "" : " · ") + tessera::format(plugin_->text("more"), n - 1);
    ui::set_text(sub_, sub);
    ui::set_color(sub_, theme::INK_SOFT);
  }

  void on_theme() override { on_tick(tessera::epoch()); }

  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "warnings", "", tile_); }

 private:
  const WeatherWarnings *plugin_;
  char name_[48]{};
  int width_ = 0, height_ = 0, tile_ = -1, dia_ = 0;
  bool advance_ = true, big_icon_ = false;
  lv_obj_t *tint_{}, *circle_{}, *icon_{}, *title_{}, *sub_{};
};

// ---- The card -----------------------------------------------------------------------------------------------------

// Every warning, the strongest first and the advance warning last: a bar of its level's colour, its icon and headline,
// when it is in force and where it comes from, and its description in as many lines as the room allows.
class WarningsCard : public tessera::Card {
 public:
  explicit WarningsCard(const WeatherWarnings *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    width_ = c.width;
    height_ = c.height;
    title_h_ = ui::line_height(Font::TITLE);
    body_h_ = ui::line_height(Font::BODY);
    icon_h_ = ui::line_height(Font::ICON_SMALL);
    gap_ = ui::px(ui::large() ? 16 : 10);
    bar_w_ = ui::px(ui::large() ? 6 : 4);
    for (Entry &e : entries_) {
      e.bar = ui::block(c.parent, theme::TRACK);
      e.icon = ui::label(c.parent, Font::ICON_SMALL, theme::MUTED);
      e.headline = ui::label(c.parent, Font::TITLE, theme::INK);
      e.meta = ui::label(c.parent, Font::BODY, theme::MUTED);
      e.text = ui::text(c.parent, Font::BODY, theme::INK_SOFT);
    }
    note_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    place(note_, 0, 0, width_, ui::line_height(Font::BODY_LARGE));
    on_tick(tessera::epoch());
  }

  void on_tick(uint32_t now) override {
    Warning list[WeatherWarnings::MAX_WARNINGS];
    const int n = plugin_->collect(list, true);
    ui::set_text(note_, n ? "" : plugin_->text("none"));
    // Each warning a share of the height: its head, its time and source, and its description in whole lines.
    const int share = n ? (height_ - (n - 1) * gap_) / n : 0;
    const int head = std::max(title_h_, icon_h_);
    const int lines = std::max(0, std::min(n == 1 ? 40 : 5, (share - head - body_h_ - ui::px(4)) / body_h_));
    int y = 0;
    for (int i = 0; i < MAX_ENTRIES; ++i) {
      Entry &e = entries_[i];
      if (i >= n) {
        lv_obj_set_style_opa(e.bar, LV_OPA_TRANSP, 0);
        ui::set_text(e.icon, "");
        ui::set_text(e.headline, "");
        ui::set_text(e.meta, "");
        ui::set_text(e.text, "");
        continue;
      }
      const Warning &w = list[i];
      const int x = bar_w_ + ui::px(10);
      const int text_h = lines * body_h_;
      const int h = head + body_h_ + (text_h ? ui::px(4) + text_h : 0);
      lv_obj_set_style_opa(e.bar, LV_OPA_COVER, 0);
      place(e.bar, 0, y, bar_w_, h);
      paint_level(e.bar, w.level, true, theme::TRACK);
      place(e.icon, x, y + (head - icon_h_) / 2, icon_h_, icon_h_);
      ui::set_text(e.icon, ui::icon(icon_of(plugin_->name(w) + " " + plugin_->headline(w))));
      paint_level(e.icon, w.level, false, theme::MUTED);
      place(e.headline, x + icon_h_ + ui::px(8), y + (head - title_h_) / 2, width_ - x - icon_h_ - ui::px(8), title_h_);
      ui::set_text(e.headline, plugin_->headline(w));
      ui::set_color(e.headline, theme::INK);
      place(e.meta, x, y + head, width_ - x, body_h_);
      ui::set_text(e.meta, meta(w, now));
      ui::set_color(e.meta, theme::MUTED);
      place(e.text, x, y + head + body_h_ + ui::px(4), width_ - x, std::max(1, text_h));
      ui::set_text(e.text, text_h ? plugin_->description(w) : "");
      ui::set_color(e.text, theme::INK_SOFT);
      y += h + gap_;
    }
  }

  void on_theme() override { on_tick(tessera::epoch()); }

 private:
  static constexpr int MAX_ENTRIES = WeatherWarnings::MAX_WARNINGS;
  struct Entry {
    lv_obj_t *bar{}, *icon{}, *headline{}, *meta{}, *text{};
  };

  // "Mon 18:00 – Tue 06:00 · DWD", "until 22:00 · NINA", "Advance warning · from Wed 06:00".
  std::string meta(const Warning &w, uint32_t now) const {
    std::string when;
    if (w.start && w.end && (!now || w.start > now))
      when = moment(w.start) + " – " + moment(w.end);
    else if (w.end)
      when = tessera::fill(plugin_->text("until"), "time", moment(w.end));
    else if (w.start)
      when = tessera::fill(plugin_->text("from"), "time", moment(w.start));
    const char *source = w.source == Source::DWD ? "dwd" : w.source == Source::NINA ? "nina" : "advance";
    return when.empty() ? std::string(plugin_->text(source)) : when + " · " + plugin_->text(source);
  }

  const WeatherWarnings *plugin_;
  int width_ = 0, height_ = 0, title_h_ = 0, body_h_ = 0, icon_h_ = 0, gap_ = 0, bar_w_ = 0;
  Entry entries_[MAX_ENTRIES];
  lv_obj_t *note_{};
};

// ---- The plugin ---------------------------------------------------------------------------------------------------

void WeatherWarnings::setup() {
  add_tile("status", [this]() { return new StatusTile(this); });
  add_card("warnings", [this]() { return new WarningsCard(this); }, true);
}

}  // namespace esphome::weather_warnings
