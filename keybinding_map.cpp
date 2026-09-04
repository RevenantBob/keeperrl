#include "stdafx.h"
#include "keybinding_map.h"
#include "gui_elem.h"
#include "pretty_archive.h"
#include "pretty_printing.h"
#include "steam_input.h"
#include "t_string.h"

KeybindingMap::KeybindingMap(const FilePath& defaults, const FilePath& user)
    : defaultsPath(defaults), userPath(user) {
  vector<FilePath> paths { defaults };
  if (auto error = PrettyPrinting::parseObject(this->defaults, paths, nullptr))
    USER_FATAL << "Error loading default keybindings: " << *error;
  if (user.exists())
    paths.push_back(user);
  while (true) {
    if (auto error = PrettyPrinting::parseObject(bindings, paths, nullptr)) {
      USER_INFO << "Error loading keybindings: " << *error;
      bindings.clear();
    } else
      break;
  }
}

static SDL::Uint16 getMod(SDL::Uint16 m) {
  if (m & SDL_KMOD_RCTRL)
    m = m | SDL_KMOD_LCTRL;
  if (m & SDL_KMOD_RSHIFT)
    m = m | SDL_KMOD_LSHIFT;
  if (m & SDL_KMOD_RALT)
    m = m | SDL_KMOD_LALT;
  return m & (SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT | SDL_KMOD_LALT);
}

static SDL::SDL_Keycode getEquivalent(SDL::SDL_Keycode key){
  if (key == SDLK_KP_ENTER)
    return SDLK_RETURN;
  return key;
}

optional<SDL::SDL_Keycode> KeybindingMap::getBuiltinMapping(Keybinding key) {
  static HashMap<Keybinding, SDL::SDL_Keycode> bindings {
    {Keybinding("MENU_UP"), SDLK_KP_8},
    {Keybinding("MENU_DOWN"), SDLK_KP_2},
    {Keybinding("MENU_LEFT"), SDLK_KP_4},
    {Keybinding("MENU_RIGHT"), SDLK_KP_6},
  };
  return getValueMaybe(bindings, key);
}

optional<ControllerKey> KeybindingMap::getControllerMapping(Keybinding key) {
  static HashMap<Keybinding, ControllerKey> controllerBindings {
      {Keybinding("WAIT"), C_WAIT},
      {Keybinding("CHAT"), C_CHAT},
      {Keybinding("FIRE_PROJECTILE"), C_FIRE_PROJECTILE},
      {Keybinding("SKIP_TURN"), C_SKIP_TURN},
      {Keybinding("STAND_GROUND"), C_STAND_GROUND},
      {Keybinding("IGNORE_ENEMIES"), C_IGNORE_ENEMIES},
      {Keybinding("EXIT_CONTROL_MODE"), C_EXIT_CONTROL_MODE},
      {Keybinding("TOGGLE_CONTROL_MODE"), C_TOGGLE_CONTROL_MODE},
      {Keybinding("OPEN_WORLD_MAP"), C_WORLD_MAP},
      {Keybinding("PAUSE"), C_PAUSE},
      {Keybinding("SPEED_UP"), C_SPEED_UP},
      {Keybinding("SPEED_DOWN"), C_SPEED_DOWN},
      {Keybinding("MENU_DOWN"), C_BUILDINGS_DOWN},
      {Keybinding("MENU_UP"), C_BUILDINGS_UP},
      {Keybinding("MENU_LEFT"), C_BUILDINGS_LEFT},
      {Keybinding("MENU_RIGHT"), C_BUILDINGS_RIGHT},
      {Keybinding("MENU_SELECT"), C_BUILDINGS_CONFIRM},
      {Keybinding("EXIT_MENU"), C_BUILDINGS_CANCEL},
      {Keybinding("SCROLL_Z_UP"), C_ZLEVEL_UP},
      {Keybinding("SCROLL_Z_DOWN"), C_ZLEVEL_DOWN},
  };
  return getValueMaybe(controllerBindings, key);
}

bool KeybindingMap::matches(Keybinding key, SDL::SDL_Keysym sym) {
  if (getControllerMapping(key) == (ControllerKey)sym.sym)
    return true;
  if (getBuiltinMapping(key) == sym.sym)
    return true;
  if (auto k = getReferenceMaybe(bindings, key))
    return getEquivalent(k->sym) == getEquivalent(sym.sym) && getMod(k->mod) == getMod(sym.mod);
  return false;
}

static const map<string, SDL::SDL_Keycode> keycodes {
  {"A", SDLK_A},
  {"B", SDLK_B},
  {"C", SDLK_C},
  {"D", SDLK_D},
  {"E", SDLK_E},
  {"F", SDLK_F},
  {"G", SDLK_G},
  {"H", SDLK_H},
  {"I", SDLK_I},
  {"J", SDLK_J},
  {"K", SDLK_K},
  {"L", SDLK_L},
  {"M", SDLK_M},
  {"N", SDLK_N},
  {"O", SDLK_O},
  {"P", SDLK_P},
  {"Q", SDLK_Q},
  {"R", SDLK_R},
  {"S", SDLK_S},
  {"T", SDLK_T},
  {"U", SDLK_U},
  {"V", SDLK_V},
  {"W", SDLK_W},
  {"X", SDLK_X},
  {"Y", SDLK_Y},
  {"Z", SDLK_Z},
  {"0", SDLK_0},
  {"1", SDLK_1},
  {"2", SDLK_2},
  {"3", SDLK_3},
  {"4", SDLK_4},
  {"5", SDLK_5},
  {"6", SDLK_6},
  {"7", SDLK_7},
  {"8", SDLK_8},
  {"9", SDLK_9},
  {"KEYPAD0", SDLK_KP_0},
  {"KEYPAD1", SDLK_KP_1},
  {"KEYPAD2", SDLK_KP_2},
  {"KEYPAD3", SDLK_KP_3},
  {"KEYPAD4", SDLK_KP_4},
  {"KEYPAD5", SDLK_KP_5},
  {"KEYPAD6", SDLK_KP_6},
  {"KEYPAD7", SDLK_KP_7},
  {"KEYPAD8", SDLK_KP_8},
  {"KEYPAD9", SDLK_KP_9},
  {"SPACE", SDLK_SPACE},
  {"COMMA", SDLK_COMMA},
  {"DELETE", SDLK_DELETE},
  {"SLASH", SDLK_SLASH},
  {"BACKSLASH", SDLK_BACKSLASH},
  {"SEMICOLON", SDLK_SEMICOLON},
  {"PAGEUP", SDLK_PAGEUP},
  {"PAGEDOWN", SDLK_PAGEDOWN},
  {"PERIOD", SDLK_PERIOD},
  {"UP", SDLK_UP},
  {"DOWN", SDLK_DOWN},
  {"LEFT", SDLK_LEFT},
  {"RIGHT", SDLK_RIGHT},
  {"ESCAPE", SDLK_ESCAPE},
  {"ENTER", SDLK_RETURN},
};

TString KeybindingMap::getText(SDL::SDL_Keysym sym, string delimiter) {
  static unordered_map<SDL::SDL_Keycode, TString> keys = [] {
    unordered_map<SDL::SDL_Keycode, TString> ret;
    for (auto& elem : keycodes)
      ret[elem.second] = elem.first;
    return ret;
  }();
  TString ret = keys.at(sym.sym);
  if (sym.mod & SDL_KMOD_LCTRL)
    ret = TSentence("KEY_MODIFIER", TString("Ctrl"_s), ret);
  if (sym.mod & SDL_KMOD_LSHIFT)
    ret = TSentence("KEY_MODIFIER", TString("Shift"_s), ret);
  if (sym.mod & SDL_KMOD_LALT)
    ret = TSentence("KEY_MODIFIER", TString("Alt"_s), ret);
  return ret;
}

optional<TString> KeybindingMap::getText(Keybinding key) {
  if (auto ks = getReferenceMaybe(bindings, key))
    return getText(*ks);
  return none;
}

SGuiElem KeybindingMap::getGlyph(SGuiElem label, GuiFactory* f, optional<ControllerKey> key,
    optional<TString> alternative) {
  SGuiElem add;
  auto steamInput = f->getSteamInput();
  if (steamInput && !steamInput->controllers.empty()) {
    if (key)
      add = f->steamInputGlyph(*key);
  } else if (alternative)
    add = f->label(TSentence("GLYPH_LABEL", *alternative));
  if (add)
    label = f->getListBuilder()
        .addElemAuto(std::move(label))
        .addSpace(10)
        .addElemAuto(std::move(add))
        .buildHorizontalList();
  return label;
}

SGuiElem KeybindingMap::getGlyph(SGuiElem label, GuiFactory* f, Keybinding key) {
  optional<TString> alternative;
  if (auto k = getReferenceMaybe(bindings, key))
    alternative = getText(*k);
  return getGlyph(std::move(label), f, getControllerMapping(key), std::move(alternative));
}

string KeybindingMap::getString(SDL::SDL_Keysym sym) {
  static unordered_map<SDL::SDL_Keycode, string> keys = [] {
    unordered_map<SDL::SDL_Keycode, string> ret;
    for (auto& elem : keycodes)
      ret[elem.second] = elem.first;
    return ret;
  }();
  string ret = keys.at(sym.sym);
  if (sym.mod & SDL_KMOD_LCTRL)
    ret = "ctrl " + ret;
  if (sym.mod & SDL_KMOD_LSHIFT)
    ret = "shift " + ret;
  if (sym.mod & SDL_KMOD_LALT)
    ret = "alt " + ret;
  return ret;
}

static bool equal(SDL::SDL_Keysym s1, SDL::SDL_Keysym s2) {
  return s1.sym == s2.sym && s1.mod == s2.mod;
}

void KeybindingMap::save() {
  ofstream out(userPath.getPath());
  for (auto& elem : bindings)
    if (!defaults.count(elem.first) || !equal(defaults.at(elem.first), elem.second)) {
    out << elem.first.data() << " ";
    if (defaults.count(elem.first))
      out << "modify ";
    out << getString(elem.second) << endl;
  }
}

void KeybindingMap::reset() {
  if (auto error = PrettyPrinting::parseObject(bindings, {defaultsPath}, nullptr)) {
    USER_INFO << "Error loading default keybindings: " << *error;
    bindings.clear();
  }
  save();
}

bool KeybindingMap::set(Keybinding k, SDL::SDL_Keysym s) {
  for (auto& elem : keycodes)
    if (elem.second == s.sym) {
      bindings[k] = s;
      save();
      return true;
    }
  return false;
}

void serialize(PrettyInputArchive& ar, SDL::SDL_Keysym& sym) {
  sym.mod = 0;
  while (true) {
    string s;
    ar.readText(s);
    if (lowercase(s) == "ctrl")
      sym.mod = sym.mod | SDL_KMOD_LCTRL;
    else if (lowercase(s) == "shift")
      sym.mod = sym.mod | SDL_KMOD_LSHIFT;
    else if (lowercase(s) == "alt")
      sym.mod = sym.mod | SDL_KMOD_LALT;
    else if (auto code = getValueMaybe(keycodes, s)) {
      sym.sym = *code;
      break;
    } else
      ar.error("Unknown key code: \"" + s + "\"");
  }
}
