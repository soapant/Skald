#ifndef SKAPI_LUA_GENERATED_H
#define SKAPI_LUA_GENERATED_H

#include "core/skapi.h"
#include "core/lua_host.h"
#include "core/lua_struct.h"
#include "platform/lib.h"
#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include <stdint.h>
#include <string.h>

#include "helpers/helpers_hotkey.h"
static const LuaFieldDesc lf_HotkeyBinding[] = {
    {"vk", LSF_INT, offsetof(HotkeyBinding, vk), 0, 0},
    {"modifiers", LSF_INT, offsetof(HotkeyBinding, modifiers), 0, 0},
    {"window_list", LSF_STR, offsetof(HotkeyBinding, window_list), 0, 0},
    {"window_count", LSF_INT, offsetof(HotkeyBinding, window_count), 0, 0},
    {"whitelist", LSF_BOOL, offsetof(HotkeyBinding, whitelist), 0, 0},
    {"valid", LSF_BOOL, offsetof(HotkeyBinding, valid), 0, 0},
    {"hotkey_callback", LSF_CB, offsetof(HotkeyBinding, hotkey_callback), 0, 0},
    {"hotkey_callback_ud", LSF_CB, offsetof(HotkeyBinding, hotkey_callback_ud), 0, 1},
    {"should_swallow", LSF_CB, offsetof(HotkeyBinding, should_swallow), 0, 3},
    {"hotkey_condition", LSF_CB, offsetof(HotkeyBinding, hotkey_condition), 0, 3},
    {"hold", LSF_BOOL, offsetof(HotkeyBinding, hold), 0, 0},
    {"user_ud", LSF_PTR, offsetof(HotkeyBinding, user_ud), 0, 0},
};
static const LuaStructDesc lsd_HotkeyBinding = {"HotkeyBinding", sizeof(HotkeyBinding), lf_HotkeyBinding, (int)(sizeof(lf_HotkeyBinding)/sizeof(lf_HotkeyBinding[0]))};
#include "helpers/helpers_ui.h"
static const LuaFieldDesc lf_ListboxLayout[] = {
    {"bx", LSF_INT, offsetof(ListboxLayout, bx), 0, 0},
    {"bw", LSF_INT, offsetof(ListboxLayout, bw), 0, 0},
    {"fw", LSF_INT, offsetof(ListboxLayout, fw), 0, 0},
    {"fh", LSF_INT, offsetof(ListboxLayout, fh), 0, 0},
    {"y0", LSF_INT, offsetof(ListboxLayout, y0), 0, 0},
    {"item_h", LSF_INT, offsetof(ListboxLayout, item_h), 0, 0},
    {"list_h", LSF_INT, offsetof(ListboxLayout, list_h), 0, 0},
    {"visible", LSF_INT, offsetof(ListboxLayout, visible), 0, 0},
};
static const LuaStructDesc lsd_ListboxLayout = {"ListboxLayout", sizeof(ListboxLayout), lf_ListboxLayout, (int)(sizeof(lf_ListboxLayout)/sizeof(lf_ListboxLayout[0]))};
#include "helpers/helpers_ui.h"
static const LuaFieldDesc lf_SlotRect[] = {
    {"x", LSF_INT, offsetof(SlotRect, x), 0, 0},
    {"y", LSF_INT, offsetof(SlotRect, y), 0, 0},
    {"w", LSF_INT, offsetof(SlotRect, w), 0, 0},
    {"h", LSF_INT, offsetof(SlotRect, h), 0, 0},
    {"h_slot", LSF_INT, offsetof(SlotRect, h_slot), 0, 0},
    {"v_slot", LSF_INT, offsetof(SlotRect, v_slot), 0, 0},
    {"gap", LSF_INT, offsetof(SlotRect, gap), 0, 0},
    {"outward", LSF_INT, offsetof(SlotRect, outward), 0, 0},
};
static const LuaStructDesc lsd_SlotRect = {"SlotRect", sizeof(SlotRect), lf_SlotRect, (int)(sizeof(lf_SlotRect)/sizeof(lf_SlotRect[0]))};
#include "helpers/helpers_ui.h"
static const LuaFieldDesc lf_MessageSlotSpec[] = {
    {"message", LSF_STR, offsetof(MessageSlotSpec, message), 0, 0},
    {"slot", LSF_INT, offsetof(MessageSlotSpec, slot), 0, 0},
    {"max_w", LSF_INT, offsetof(MessageSlotSpec, max_w), 0, 0},
    {"max_h", LSF_INT, offsetof(MessageSlotSpec, max_h), 0, 0},
    {"prompt_gap", LSF_INT, offsetof(MessageSlotSpec, prompt_gap), 0, 0},
    {"timeout", LSF_DOUBLE, offsetof(MessageSlotSpec, timeout), 0, 0},
    {"color", LSF_INT, offsetof(MessageSlotSpec, color), 0, 0},
    {"render", LSF_CB, offsetof(MessageSlotSpec, render), 0, 1},
    {"render_data", LSF_PTR, offsetof(MessageSlotSpec, render_data), 0, 0},
    {"name", LSF_STR, offsetof(MessageSlotSpec, name), 0, 0},
    {"win_w", LSF_INT, offsetof(MessageSlotSpec, win_w), 0, 0},
    {"win_h", LSF_INT, offsetof(MessageSlotSpec, win_h), 0, 0},
    {"rounded", LSF_BOOL, offsetof(MessageSlotSpec, rounded), 0, 0},
    {"radius", LSF_INT, offsetof(MessageSlotSpec, radius), 0, 0},
    {"fps", LSF_INT, offsetof(MessageSlotSpec, fps), 0, 0},
};
static const LuaStructDesc lsd_MessageSlotSpec = {"MessageSlotSpec", sizeof(MessageSlotSpec), lf_MessageSlotSpec, (int)(sizeof(lf_MessageSlotSpec)/sizeof(lf_MessageSlotSpec[0]))};
#include "helpers/helpers_ui.h"
static const LuaFieldDesc lf_WidgetMouseState[] = {
    {"valid", LSF_BOOL, offsetof(WidgetMouseState, valid), 0, 0},
    {"mx", LSF_INT, offsetof(WidgetMouseState, mx), 0, 0},
    {"my", LSF_INT, offsetof(WidgetMouseState, my), 0, 0},
    {"down", LSF_BOOL, offsetof(WidgetMouseState, down), 0, 0},
};
static const LuaStructDesc lsd_WidgetMouseState = {"WidgetMouseState", sizeof(WidgetMouseState), lf_WidgetMouseState, (int)(sizeof(lf_WidgetMouseState)/sizeof(lf_WidgetMouseState[0]))};
#include "helpers/helpers_text.h"
static const LuaFieldDesc lf_StringList[] = {
    {"items", LSF_STR_LIST, offsetof(StringList, items), offsetof(StringList, count), 0},
    {"count", LSF_INT, offsetof(StringList, count), 0, 0},
};
static const LuaStructDesc lsd_StringList = {"StringList", sizeof(StringList), lf_StringList, (int)(sizeof(lf_StringList)/sizeof(lf_StringList[0]))};
#include "helpers/helpers_input_state.h"
static const LuaFieldDesc lf_KeyEvent[] = {
    {"vk", LSF_INT, offsetof(KeyEvent, vk), 0, 0},
    {"scancode", LSF_INT, offsetof(KeyEvent, scancode), 0, 0},
    {"mods", LSF_INT, offsetof(KeyEvent, mods), 0, 0},
    {"down", LSF_BOOL, offsetof(KeyEvent, down), 0, 0},
    {"repeat", LSF_BOOL, offsetof(KeyEvent, repeat), 0, 0},
    {"t_us", LSF_INT, offsetof(KeyEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_KeyEvent = {"KeyEvent", sizeof(KeyEvent), lf_KeyEvent, (int)(sizeof(lf_KeyEvent)/sizeof(lf_KeyEvent[0]))};
#include "helpers/helpers_input_state.h"
static const LuaFieldDesc lf_MouseEvent[] = {
    {"button", LSF_INT, offsetof(MouseEvent, button), 0, 0},
    {"x", LSF_INT, offsetof(MouseEvent, x), 0, 0},
    {"y", LSF_INT, offsetof(MouseEvent, y), 0, 0},
    {"mods", LSF_INT, offsetof(MouseEvent, mods), 0, 0},
    {"down", LSF_BOOL, offsetof(MouseEvent, down), 0, 0},
    {"repeat", LSF_BOOL, offsetof(MouseEvent, repeat), 0, 0},
    {"t_us", LSF_INT, offsetof(MouseEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_MouseEvent = {"MouseEvent", sizeof(MouseEvent), lf_MouseEvent, (int)(sizeof(lf_MouseEvent)/sizeof(lf_MouseEvent[0]))};
#include "helpers/helpers_input_state.h"
static const LuaFieldDesc lf_ScrollEvent[] = {
    {"x", LSF_INT, offsetof(ScrollEvent, x), 0, 0},
    {"y", LSF_INT, offsetof(ScrollEvent, y), 0, 0},
    {"dx", LSF_INT, offsetof(ScrollEvent, dx), 0, 0},
    {"dy", LSF_INT, offsetof(ScrollEvent, dy), 0, 0},
    {"mods", LSF_INT, offsetof(ScrollEvent, mods), 0, 0},
    {"t_us", LSF_INT, offsetof(ScrollEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_ScrollEvent = {"ScrollEvent", sizeof(ScrollEvent), lf_ScrollEvent, (int)(sizeof(lf_ScrollEvent)/sizeof(lf_ScrollEvent[0]))};
#include "helpers/helpers_monitor.h"
static const LuaFieldDesc lf_MonitorEntry[] = {
    {"x", LSF_INT, offsetof(MonitorEntry, x), 0, 0},
    {"y", LSF_INT, offsetof(MonitorEntry, y), 0, 0},
    {"w", LSF_INT, offsetof(MonitorEntry, w), 0, 0},
    {"h", LSF_INT, offsetof(MonitorEntry, h), 0, 0},
    {"is_primary", LSF_BOOL, offsetof(MonitorEntry, is_primary), 0, 0},
    {"name", LSF_STR_INLINE, offsetof(MonitorEntry, name), 128, 0},
};
static const LuaStructDesc lsd_MonitorEntry = {"MonitorEntry", sizeof(MonitorEntry), lf_MonitorEntry, (int)(sizeof(lf_MonitorEntry)/sizeof(lf_MonitorEntry[0]))};
#include "helpers/helpers_monitor.h"
static const LuaFieldDesc lf_MonitorInfo[] = {
    {"index", LSF_INT, offsetof(MonitorInfo, index), 0, 0},
    {"x", LSF_INT, offsetof(MonitorInfo, x), 0, 0},
    {"y", LSF_INT, offsetof(MonitorInfo, y), 0, 0},
    {"w", LSF_INT, offsetof(MonitorInfo, w), 0, 0},
    {"h", LSF_INT, offsetof(MonitorInfo, h), 0, 0},
    {"is_primary", LSF_BOOL, offsetof(MonitorInfo, is_primary), 0, 0},
    {"name", LSF_STR_INLINE, offsetof(MonitorInfo, name), 128, 0},
};
static const LuaStructDesc lsd_MonitorInfo = {"MonitorInfo", sizeof(MonitorInfo), lf_MonitorInfo, (int)(sizeof(lf_MonitorInfo)/sizeof(lf_MonitorInfo[0]))};
#include "core/window_manager.h"
static const LuaFieldDesc lf_PopupVTable[] = {
    {"on_show", LSF_CB, offsetof(PopupVTable, on_show), 0, 1},
    {"on_hide", LSF_CB, offsetof(PopupVTable, on_hide), 0, 1},
    {"on_render", LSF_CB, offsetof(PopupVTable, on_render), 0, 1},
    {"on_key", LSF_CB, offsetof(PopupVTable, on_key), 0, 3},
    {"on_focus_lost", LSF_CB, offsetof(PopupVTable, on_focus_lost), 0, 1},
    {"destroy", LSF_CB, offsetof(PopupVTable, destroy), 0, 1},
};
static const LuaStructDesc lsd_PopupVTable = {"PopupVTable", sizeof(PopupVTable), lf_PopupVTable, (int)(sizeof(lf_PopupVTable)/sizeof(lf_PopupVTable[0]))};
#include "core/window_manager.h"
static const LuaFieldDesc lf_Popup[] = {
    {"vtable", LSF_PTR, offsetof(Popup, vtable), 0, 0},
    {"manager", LSF_PTR, offsetof(Popup, manager), 0, 0},
    {"visible", LSF_BOOL, offsetof(Popup, visible), 0, 0},
    {"user_data", LSF_PTR, offsetof(Popup, user_data), 0, 0},
    {"type", LSF_INT, offsetof(Popup, type), 0, 0},
};
static const LuaStructDesc lsd_Popup = {"Popup", sizeof(Popup), lf_Popup, (int)(sizeof(lf_Popup)/sizeof(lf_Popup[0]))};
#include "core/config_toml.h"
static const LuaFieldDesc lf_ConfigTomlEntry[] = {
    {"section", LSF_STR_INLINE, offsetof(ConfigTomlEntry, section), 32, 0},
    {"key", LSF_STR_INLINE, offsetof(ConfigTomlEntry, key), 384, 0},
    {"type", LSF_INT, offsetof(ConfigTomlEntry, type), 0, 0},
    {"raw", LSF_STR_INLINE, offsetof(ConfigTomlEntry, raw), 1024, 0},
};
static const LuaStructDesc lsd_ConfigTomlEntry = {"ConfigTomlEntry", sizeof(ConfigTomlEntry), lf_ConfigTomlEntry, (int)(sizeof(lf_ConfigTomlEntry)/sizeof(lf_ConfigTomlEntry[0]))};
#include "core/config_toml.h"
static const LuaFieldDesc lf_ConfigTomlError[] = {
    {"status", LSF_INT, offsetof(ConfigTomlError, status), 0, 0},
    {"line", LSF_INT, offsetof(ConfigTomlError, line), 0, 0},
    {"message", LSF_STR_INLINE, offsetof(ConfigTomlError, message), 256, 0},
};
static const LuaStructDesc lsd_ConfigTomlError = {"ConfigTomlError", sizeof(ConfigTomlError), lf_ConfigTomlError, (int)(sizeof(lf_ConfigTomlError)/sizeof(lf_ConfigTomlError[0]))};
#include "core/search_index.h"
static const LuaFieldDesc lf_SearchHit[] = {
    {"source_id", LSF_INT, offsetof(SearchHit, source_id), 0, 0},
    {"kind", LSF_INT, offsetof(SearchHit, kind), 0, 0},
    {"text", LSF_STR_INLINE, offsetof(SearchHit, text), 256, 0},
    {"context", LSF_STR_INLINE, offsetof(SearchHit, context), 128, 0},
    {"widget_name", LSF_STR_INLINE, offsetof(SearchHit, widget_name), 128, 0},
    {"score", LSF_INT, offsetof(SearchHit, score), 0, 0},
};
static const LuaStructDesc lsd_SearchHit = {"SearchHit", sizeof(SearchHit), lf_SearchHit, (int)(sizeof(lf_SearchHit)/sizeof(lf_SearchHit[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsEntrySpec[] = {
    {"name", LSF_STR, offsetof(SettingsEntrySpec, name), 0, 0},
    {"icon", LSF_STR, offsetof(SettingsEntrySpec, icon), 0, 0},
    {"desc", LSF_STR, offsetof(SettingsEntrySpec, desc), 0, 0},
    {"owner", LSF_STR, offsetof(SettingsEntrySpec, owner), 0, 0},
    {"render", LSF_CB, offsetof(SettingsEntrySpec, render), 0, 0},
    {"ud", LSF_PTR, offsetof(SettingsEntrySpec, ud), 0, 0},
};
static const LuaStructDesc lsd_SettingsEntrySpec = {"SettingsEntrySpec", sizeof(SettingsEntrySpec), lf_SettingsEntrySpec, (int)(sizeof(lf_SettingsEntrySpec)/sizeof(lf_SettingsEntrySpec[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsEditboxField[] = {
    {"name", LSF_STR, offsetof(SettingsEditboxField, name), 0, 0},
    {"label", LSF_STR, offsetof(SettingsEditboxField, label), 0, 0},
    {"buffer", LSF_BUFFER, offsetof(SettingsEditboxField, buffer), offsetof(SettingsEditboxField, buffer_size), 0},
    {"buffer_size", LSF_INT, offsetof(SettingsEditboxField, buffer_size), 0, 0},
    {"width", LSF_INT, offsetof(SettingsEditboxField, width), 0, 0},
    {"config_key", LSF_STR, offsetof(SettingsEditboxField, config_key), 0, 0},
    {"default_value", LSF_STR, offsetof(SettingsEditboxField, default_value), 0, 0},
    {"type", LSF_INT, offsetof(SettingsEditboxField, type), 0, 0},
    {"min_value", LSF_FLOAT, offsetof(SettingsEditboxField, min_value), 0, 0},
    {"max_value", LSF_FLOAT, offsetof(SettingsEditboxField, max_value), 0, 0},
};
static const LuaStructDesc lsd_SettingsEditboxField = {"SettingsEditboxField", sizeof(SettingsEditboxField), lf_SettingsEditboxField, (int)(sizeof(lf_SettingsEditboxField)/sizeof(lf_SettingsEditboxField[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsEditboxSpec[] = {
    {"title", LSF_STR, offsetof(SettingsEditboxSpec, title), 0, 0},
    {"sub", LSF_STR, offsetof(SettingsEditboxSpec, sub), 0, 0},
    {"fields", LSF_PTR, offsetof(SettingsEditboxSpec, fields), 0, 0},
    {"field_count", LSF_INT, offsetof(SettingsEditboxSpec, field_count), 0, 0},
    {"x_offset", LSF_INT, offsetof(SettingsEditboxSpec, x_offset), 0, 0},
    {"on_change", LSF_CB, offsetof(SettingsEditboxSpec, on_change), 0, 0},
};
static const LuaStructDesc lsd_SettingsEditboxSpec = {"SettingsEditboxSpec", sizeof(SettingsEditboxSpec), lf_SettingsEditboxSpec, (int)(sizeof(lf_SettingsEditboxSpec)/sizeof(lf_SettingsEditboxSpec[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsEditboxGroup[] = {
    {"entry", LSF_INT, offsetof(SettingsEditboxGroup, entry), 0, 0},
    {"block", LSF_INT, offsetof(SettingsEditboxGroup, block), 0, 0},
};
static const LuaStructDesc lsd_SettingsEditboxGroup = {"SettingsEditboxGroup", sizeof(SettingsEditboxGroup), lf_SettingsEditboxGroup, (int)(sizeof(lf_SettingsEditboxGroup)/sizeof(lf_SettingsEditboxGroup[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsDropdownSpec[] = {
    {"key", LSF_STR, offsetof(SettingsDropdownSpec, key), 0, 0},
    {"title", LSF_STR, offsetof(SettingsDropdownSpec, title), 0, 0},
    {"sub", LSF_STR, offsetof(SettingsDropdownSpec, sub), 0, 0},
    {"refresh_options", LSF_CB, offsetof(SettingsDropdownSpec, refresh_options), 0, 0},
    {"initial", LSF_INT, offsetof(SettingsDropdownSpec, initial), 0, 0},
    {"on_select", LSF_CB, offsetof(SettingsDropdownSpec, on_select), 0, 0},
    {"on_hover", LSF_CB, offsetof(SettingsDropdownSpec, on_hover), 0, 0},
};
static const LuaStructDesc lsd_SettingsDropdownSpec = {"SettingsDropdownSpec", sizeof(SettingsDropdownSpec), lf_SettingsDropdownSpec, (int)(sizeof(lf_SettingsDropdownSpec)/sizeof(lf_SettingsDropdownSpec[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsCheckboxSpec[] = {
    {"key", LSF_STR, offsetof(SettingsCheckboxSpec, key), 0, 0},
    {"title", LSF_STR, offsetof(SettingsCheckboxSpec, title), 0, 0},
    {"sub", LSF_STR, offsetof(SettingsCheckboxSpec, sub), 0, 0},
    {"initial", LSF_BOOL, offsetof(SettingsCheckboxSpec, initial), 0, 0},
    {"on_click", LSF_CB, offsetof(SettingsCheckboxSpec, on_click), 0, 0},
    {"on_hover", LSF_CB, offsetof(SettingsCheckboxSpec, on_hover), 0, 0},
};
static const LuaStructDesc lsd_SettingsCheckboxSpec = {"SettingsCheckboxSpec", sizeof(SettingsCheckboxSpec), lf_SettingsCheckboxSpec, (int)(sizeof(lf_SettingsCheckboxSpec)/sizeof(lf_SettingsCheckboxSpec[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsButtonSpec[] = {
    {"key", LSF_STR, offsetof(SettingsButtonSpec, key), 0, 0},
    {"title", LSF_STR, offsetof(SettingsButtonSpec, title), 0, 0},
    {"sub", LSF_STR, offsetof(SettingsButtonSpec, sub), 0, 0},
    {"label", LSF_STR, offsetof(SettingsButtonSpec, label), 0, 0},
    {"on_click", LSF_CB, offsetof(SettingsButtonSpec, on_click), 0, 0},
    {"on_hover", LSF_CB, offsetof(SettingsButtonSpec, on_hover), 0, 0},
};
static const LuaStructDesc lsd_SettingsButtonSpec = {"SettingsButtonSpec", sizeof(SettingsButtonSpec), lf_SettingsButtonSpec, (int)(sizeof(lf_SettingsButtonSpec)/sizeof(lf_SettingsButtonSpec[0]))};
#include "core/settings_registry.h"
static const LuaFieldDesc lf_SettingsSliderSpec[] = {
    {"key", LSF_STR, offsetof(SettingsSliderSpec, key), 0, 0},
    {"title", LSF_STR, offsetof(SettingsSliderSpec, title), 0, 0},
    {"sub", LSF_STR, offsetof(SettingsSliderSpec, sub), 0, 0},
    {"min_value", LSF_FLOAT, offsetof(SettingsSliderSpec, min_value), 0, 0},
    {"max_value", LSF_FLOAT, offsetof(SettingsSliderSpec, max_value), 0, 0},
    {"initial", LSF_FLOAT, offsetof(SettingsSliderSpec, initial), 0, 0},
    {"on_change", LSF_CB, offsetof(SettingsSliderSpec, on_change), 0, 0},
    {"on_hover", LSF_CB, offsetof(SettingsSliderSpec, on_hover), 0, 0},
};
static const LuaStructDesc lsd_SettingsSliderSpec = {"SettingsSliderSpec", sizeof(SettingsSliderSpec), lf_SettingsSliderSpec, (int)(sizeof(lf_SettingsSliderSpec)/sizeof(lf_SettingsSliderSpec[0]))};
#include "core/owner_registry.h"
static const LuaFieldDesc lf_OwnerTag[] = {
    {"owner", LSF_STR, offsetof(OwnerTag, owner), 0, 0},
    {"gen", LSF_INT, offsetof(OwnerTag, gen), 0, 0},
};
static const LuaStructDesc lsd_OwnerTag = {"OwnerTag", sizeof(OwnerTag), lf_OwnerTag, (int)(sizeof(lf_OwnerTag)/sizeof(lf_OwnerTag[0]))};
#include "core/theme.h"
static const LuaFieldDesc lf_ThemeEntry[] = {
    {"name", LSF_STR, offsetof(ThemeEntry, name), 0, 0},
    {"bg", LSF_INT, offsetof(ThemeEntry, bg), 0, 0},
    {"text", LSF_INT, offsetof(ThemeEntry, text), 0, 0},
    {"selected_text", LSF_INT, offsetof(ThemeEntry, selected_text), 0, 0},
    {"cursor", LSF_INT, offsetof(ThemeEntry, cursor), 0, 0},
    {"border", LSF_INT, offsetof(ThemeEntry, border), 0, 0},
};
static const LuaStructDesc lsd_ThemeEntry = {"ThemeEntry", sizeof(ThemeEntry), lf_ThemeEntry, (int)(sizeof(lf_ThemeEntry)/sizeof(lf_ThemeEntry[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkSize[] = {
    {"w", LSF_INT, offsetof(SkSize, w), 0, 0},
    {"h", LSF_INT, offsetof(SkSize, h), 0, 0},
};
static const LuaStructDesc lsd_SkSize = {"SkSize", sizeof(SkSize), lf_SkSize, (int)(sizeof(lf_SkSize)/sizeof(lf_SkSize[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkFeature[] = {
    {"name", LSF_STR, offsetof(SkFeature, name), 0, 0},
    {"callback", LSF_CB, offsetof(SkFeature, callback), 0, 0},
    {"icon", LSF_STR, offsetof(SkFeature, icon), 0, 0},
};
static const LuaStructDesc lsd_SkFeature = {"SkFeature", sizeof(SkFeature), lf_SkFeature, (int)(sizeof(lf_SkFeature)/sizeof(lf_SkFeature[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkFeatureList[] = {
    {"items", LSF_PTR, offsetof(SkFeatureList, items), 0, 0},
    {"count", LSF_INT, offsetof(SkFeatureList, count), 0, 0},
};
static const LuaStructDesc lsd_SkFeatureList = {"SkFeatureList", sizeof(SkFeatureList), lf_SkFeatureList, (int)(sizeof(lf_SkFeatureList)/sizeof(lf_SkFeatureList[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkMonitorInfo[] = {
    {"index", LSF_INT, offsetof(SkMonitorInfo, index), 0, 0},
    {"name", LSF_STR_INLINE, offsetof(SkMonitorInfo, name), 128, 0},
    {"x", LSF_INT, offsetof(SkMonitorInfo, x), 0, 0},
    {"y", LSF_INT, offsetof(SkMonitorInfo, y), 0, 0},
    {"w", LSF_INT, offsetof(SkMonitorInfo, w), 0, 0},
    {"h", LSF_INT, offsetof(SkMonitorInfo, h), 0, 0},
    {"is_primary", LSF_BOOL, offsetof(SkMonitorInfo, is_primary), 0, 0},
};
static const LuaStructDesc lsd_SkMonitorInfo = {"SkMonitorInfo", sizeof(SkMonitorInfo), lf_SkMonitorInfo, (int)(sizeof(lf_SkMonitorInfo)/sizeof(lf_SkMonitorInfo[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkDirScanSpec[] = {
    {"dir_path", LSF_STR, offsetof(SkDirScanSpec, dir_path), 0, 0},
    {"allowed_extensions", LSF_STR, offsetof(SkDirScanSpec, allowed_extensions), 0, 0},
    {"out", LSF_PTR, offsetof(SkDirScanSpec, out), 0, 0},
    {"names_to_ignore", LSF_STR, offsetof(SkDirScanSpec, names_to_ignore), 0, 0},
    {"strip_extension", LSF_BOOL, offsetof(SkDirScanSpec, strip_extension), 0, 0},
    {"max_depth", LSF_INT, offsetof(SkDirScanSpec, max_depth), 0, 0},
};
static const LuaStructDesc lsd_SkDirScanSpec = {"SkDirScanSpec", sizeof(SkDirScanSpec), lf_SkDirScanSpec, (int)(sizeof(lf_SkDirScanSpec)/sizeof(lf_SkDirScanSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkPopulateListboxSpec[] = {
    {"lb", LSF_PTR, offsetof(SkPopulateListboxSpec, lb), 0, 0},
    {"filter", LSF_STR, offsetof(SkPopulateListboxSpec, filter), 0, 0},
    {"items", LSF_STR_LIST, offsetof(SkPopulateListboxSpec, items), offsetof(SkPopulateListboxSpec, count), 0},
    {"count", LSF_INT, offsetof(SkPopulateListboxSpec, count), 0, 0},
    {"filter_match", LSF_CB, offsetof(SkPopulateListboxSpec, filter_match), 0, 3},
};
static const LuaStructDesc lsd_SkPopulateListboxSpec = {"SkPopulateListboxSpec", sizeof(SkPopulateListboxSpec), lf_SkPopulateListboxSpec, (int)(sizeof(lf_SkPopulateListboxSpec)/sizeof(lf_SkPopulateListboxSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkEditPopupSpec[] = {
    {"initial_text", LSF_STR, offsetof(SkEditPopupSpec, initial_text), 0, 0},
    {"x", LSF_INT, offsetof(SkEditPopupSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkEditPopupSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkEditPopupSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkEditPopupSpec, h), 0, 0},
    {"lb", LSF_PTR, offsetof(SkEditPopupSpec, lb), 0, 0},
    {"out_index", LSF_PTR, offsetof(SkEditPopupSpec, out_index), 0, 0},
};
static const LuaStructDesc lsd_SkEditPopupSpec = {"SkEditPopupSpec", sizeof(SkEditPopupSpec), lf_SkEditPopupSpec, (int)(sizeof(lf_SkEditPopupSpec)/sizeof(lf_SkEditPopupSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkMessageSpec[] = {
    {"message", LSF_STR, offsetof(SkMessageSpec, message), 0, 0},
    {"slot", LSF_INT, offsetof(SkMessageSpec, slot), 0, 0},
    {"x", LSF_INT, offsetof(SkMessageSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkMessageSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkMessageSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkMessageSpec, h), 0, 0},
    {"mw", LSF_INT, offsetof(SkMessageSpec, mw), 0, 0},
    {"mh", LSF_INT, offsetof(SkMessageSpec, mh), 0, 0},
    {"timeout", LSF_DOUBLE, offsetof(SkMessageSpec, timeout), 0, 0},
    {"prompt_gap", LSF_INT, offsetof(SkMessageSpec, prompt_gap), 0, 0},
    {"color_text", LSF_INT, offsetof(SkMessageSpec, color_text), 0, 0},
    {"hover_key", LSF_STR, offsetof(SkMessageSpec, hover_key), 0, 0},
    {"render", LSF_CB, offsetof(SkMessageSpec, render), 0, 1},
    {"render_data", LSF_PTR, offsetof(SkMessageSpec, render_data), 0, 0},
    {"fps", LSF_INT, offsetof(SkMessageSpec, fps), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(SkMessageSpec, no_bg), 0, 0},
    {"color_bg", LSF_INT, offsetof(SkMessageSpec, color_bg), 0, 0},
    {"name", LSF_STR, offsetof(SkMessageSpec, name), 0, 0},
    {"close_on_keypress", LSF_BOOL, offsetof(SkMessageSpec, close_on_keypress), 0, 0},
    {"rounded", LSF_BOOL, offsetof(SkMessageSpec, rounded), 0, 0},
    {"radius", LSF_INT, offsetof(SkMessageSpec, radius), 0, 0},
};
static const LuaStructDesc lsd_SkMessageSpec = {"SkMessageSpec", sizeof(SkMessageSpec), lf_SkMessageSpec, (int)(sizeof(lf_SkMessageSpec)/sizeof(lf_SkMessageSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkHoverPopupCtx[] = {
    {"render", LSF_CB, offsetof(SkHoverPopupCtx, render), 0, 1},
    {"render_data", LSF_PTR, offsetof(SkHoverPopupCtx, render_data), 0, 0},
    {"w", LSF_INT, offsetof(SkHoverPopupCtx, w), 0, 0},
    {"h", LSF_INT, offsetof(SkHoverPopupCtx, h), 0, 0},
    {"frame", LSF_INT, offsetof(SkHoverPopupCtx, frame), 0, 0},
    {"pad", LSF_INT, offsetof(SkHoverPopupCtx, pad), 0, 0},
};
static const LuaStructDesc lsd_SkHoverPopupCtx = {"SkHoverPopupCtx", sizeof(SkHoverPopupCtx), lf_SkHoverPopupCtx, (int)(sizeof(lf_SkHoverPopupCtx)/sizeof(lf_SkHoverPopupCtx[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextSpec[] = {
    {"text", LSF_STR, offsetof(SkTextSpec, text), 0, 0},
    {"x", LSF_INT, offsetof(SkTextSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkTextSpec, y), 0, 0},
    {"color", LSF_INT, offsetof(SkTextSpec, color), 0, 0},
    {"font_size", LSF_INT, offsetof(SkTextSpec, font_size), 0, 0},
    {"align", LSF_STR, offsetof(SkTextSpec, align), 0, 0},
    {"pad", LSF_INT, offsetof(SkTextSpec, pad), 0, 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextSpec, no_hscroll), 0, 0},
    {"angle", LSF_FLOAT, offsetof(SkTextSpec, angle), 0, 0},
};
static const LuaStructDesc lsd_SkTextSpec = {"SkTextSpec", sizeof(SkTextSpec), lf_SkTextSpec, (int)(sizeof(lf_SkTextSpec)/sizeof(lf_SkTextSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextCenteredSpec[] = {
    {"x", LSF_INT, offsetof(SkTextCenteredSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkTextCenteredSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkTextCenteredSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkTextCenteredSpec, h), 0, 0},
    {"text", LSF_STR, offsetof(SkTextCenteredSpec, text), 0, 0},
    {"color", LSF_INT, offsetof(SkTextCenteredSpec, color), 0, 0},
    {"font_size", LSF_INT, offsetof(SkTextCenteredSpec, font_size), 0, 0},
};
static const LuaStructDesc lsd_SkTextCenteredSpec = {"SkTextCenteredSpec", sizeof(SkTextCenteredSpec), lf_SkTextCenteredSpec, (int)(sizeof(lf_SkTextCenteredSpec)/sizeof(lf_SkTextCenteredSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkMeasureSpec[] = {
    {"x", LSF_INT, offsetof(SkMeasureSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkMeasureSpec, y), 0, 0},
    {"max_width", LSF_INT, offsetof(SkMeasureSpec, max_width), 0, 0},
    {"line_height", LSF_INT, offsetof(SkMeasureSpec, line_height), 0, 0},
    {"text", LSF_STR, offsetof(SkMeasureSpec, text), 0, 0},
    {"font_size", LSF_INT, offsetof(SkMeasureSpec, font_size), 0, 0},
    {"out_positions", LSF_PTR, offsetof(SkMeasureSpec, out_positions), 0, 0},
};
static const LuaStructDesc lsd_SkMeasureSpec = {"SkMeasureSpec", sizeof(SkMeasureSpec), lf_SkMeasureSpec, (int)(sizeof(lf_SkMeasureSpec)/sizeof(lf_SkMeasureSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextBoundsSpec[] = {
    {"x", LSF_INT, offsetof(SkTextBoundsSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkTextBoundsSpec, y), 0, 0},
    {"text", LSF_STR, offsetof(SkTextBoundsSpec, text), 0, 0},
    {"font_size", LSF_INT, offsetof(SkTextBoundsSpec, font_size), 0, 0},
    {"align", LSF_STR, offsetof(SkTextBoundsSpec, align), 0, 0},
    {"pad", LSF_INT, offsetof(SkTextBoundsSpec, pad), 0, 0},
};
static const LuaStructDesc lsd_SkTextBoundsSpec = {"SkTextBoundsSpec", sizeof(SkTextBoundsSpec), lf_SkTextBoundsSpec, (int)(sizeof(lf_SkTextBoundsSpec)/sizeof(lf_SkTextBoundsSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkImageSpec[] = {
    {"path", LSF_STR, offsetof(SkImageSpec, path), 0, 0},
    {"x", LSF_INT, offsetof(SkImageSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkImageSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkImageSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkImageSpec, h), 0, 0},
};
static const LuaStructDesc lsd_SkImageSpec = {"SkImageSpec", sizeof(SkImageSpec), lf_SkImageSpec, (int)(sizeof(lf_SkImageSpec)/sizeof(lf_SkImageSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextSegment[] = {
    {"text", LSF_STR, offsetof(SkTextSegment, text), 0, 0},
    {"color", LSF_INT, offsetof(SkTextSegment, color), 0, 0},
};
static const LuaStructDesc lsd_SkTextSegment = {"SkTextSegment", sizeof(SkTextSegment), lf_SkTextSegment, (int)(sizeof(lf_SkTextSegment)/sizeof(lf_SkTextSegment[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextSegmentsSpec[] = {
    {"segments", LSF_PTR, offsetof(SkTextSegmentsSpec, segments), 0, 0},
    {"count", LSF_INT, offsetof(SkTextSegmentsSpec, count), 0, 0},
    {"x", LSF_INT, offsetof(SkTextSegmentsSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkTextSegmentsSpec, y), 0, 0},
    {"color", LSF_INT, offsetof(SkTextSegmentsSpec, color), 0, 0},
    {"font_size", LSF_INT, offsetof(SkTextSegmentsSpec, font_size), 0, 0},
    {"align", LSF_STR, offsetof(SkTextSegmentsSpec, align), 0, 0},
    {"pad", LSF_INT, offsetof(SkTextSegmentsSpec, pad), 0, 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextSegmentsSpec, no_hscroll), 0, 0},
    {"angle", LSF_FLOAT, offsetof(SkTextSegmentsSpec, angle), 0, 0},
};
static const LuaStructDesc lsd_SkTextSegmentsSpec = {"SkTextSegmentsSpec", sizeof(SkTextSegmentsSpec), lf_SkTextSegmentsSpec, (int)(sizeof(lf_SkTextSegmentsSpec)/sizeof(lf_SkTextSegmentsSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkTextWrappedSpec[] = {
    {"text", LSF_STR, offsetof(SkTextWrappedSpec, text), 0, 0},
    {"segments", LSF_PTR, offsetof(SkTextWrappedSpec, segments), 0, 0},
    {"count", LSF_INT, offsetof(SkTextWrappedSpec, count), 0, 0},
    {"x", LSF_INT, offsetof(SkTextWrappedSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkTextWrappedSpec, y), 0, 0},
    {"max_width", LSF_INT, offsetof(SkTextWrappedSpec, max_width), 0, 0},
    {"line_height", LSF_INT, offsetof(SkTextWrappedSpec, line_height), 0, 0},
    {"color", LSF_INT, offsetof(SkTextWrappedSpec, color), 0, 0},
    {"font_size", LSF_INT, offsetof(SkTextWrappedSpec, font_size), 0, 0},
    {"align", LSF_STR, offsetof(SkTextWrappedSpec, align), 0, 0},
    {"pad", LSF_INT, offsetof(SkTextWrappedSpec, pad), 0, 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextWrappedSpec, no_hscroll), 0, 0},
    {"angle", LSF_FLOAT, offsetof(SkTextWrappedSpec, angle), 0, 0},
    {"measure_only", LSF_BOOL, offsetof(SkTextWrappedSpec, measure_only), 0, 0},
};
static const LuaStructDesc lsd_SkTextWrappedSpec = {"SkTextWrappedSpec", sizeof(SkTextWrappedSpec), lf_SkTextWrappedSpec, (int)(sizeof(lf_SkTextWrappedSpec)/sizeof(lf_SkTextWrappedSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkScrollSpec[] = {
    {"name", LSF_STR, offsetof(SkScrollSpec, name), 0, 0},
    {"x", LSF_INT, offsetof(SkScrollSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkScrollSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkScrollSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkScrollSpec, h), 0, 0},
    {"content_w", LSF_INT, offsetof(SkScrollSpec, content_w), 0, 0},
    {"content_h", LSF_INT, offsetof(SkScrollSpec, content_h), 0, 0},
    {"off_x", LSF_PTR, offsetof(SkScrollSpec, off_x), 0, 0},
    {"off_y", LSF_PTR, offsetof(SkScrollSpec, off_y), 0, 0},
    {"wheel_px", LSF_INT, offsetof(SkScrollSpec, wheel_px), 0, 0},
};
static const LuaStructDesc lsd_SkScrollSpec = {"SkScrollSpec", sizeof(SkScrollSpec), lf_SkScrollSpec, (int)(sizeof(lf_SkScrollSpec)/sizeof(lf_SkScrollSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkFlag[] = {
    {"name", LSF_STR_INLINE, offsetof(SkFlag, name), 64, 0},
    {"value", LSF_STR_INLINE, offsetof(SkFlag, value), 256, 0},
    {"has_value", LSF_BOOL, offsetof(SkFlag, has_value), 0, 0},
};
static const LuaStructDesc lsd_SkFlag = {"SkFlag", sizeof(SkFlag), lf_SkFlag, (int)(sizeof(lf_SkFlag)/sizeof(lf_SkFlag[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkFlagList[] = {
    {"flags", LSF_PTR, offsetof(SkFlagList, flags), 0, 0},
    {"count", LSF_INT, offsetof(SkFlagList, count), 0, 0},
};
static const LuaStructDesc lsd_SkFlagList = {"SkFlagList", sizeof(SkFlagList), lf_SkFlagList, (int)(sizeof(lf_SkFlagList)/sizeof(lf_SkFlagList[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkHashMap[] = {
    {"key", LSF_STR, offsetof(SkHashMap, key), 0, 0},
    {"value", LSF_INT, offsetof(SkHashMap, value), 0, 0},
};
static const LuaStructDesc lsd_SkHashMap = {"SkHashMap", sizeof(SkHashMap), lf_SkHashMap, (int)(sizeof(lf_SkHashMap)/sizeof(lf_SkHashMap[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkKeyEvent[] = {
    {"vk", LSF_INT, offsetof(SkKeyEvent, vk), 0, 0},
    {"scancode", LSF_INT, offsetof(SkKeyEvent, scancode), 0, 0},
    {"mods", LSF_INT, offsetof(SkKeyEvent, mods), 0, 0},
    {"down", LSF_BOOL, offsetof(SkKeyEvent, down), 0, 0},
    {"repeat", LSF_BOOL, offsetof(SkKeyEvent, repeat), 0, 0},
    {"t_us", LSF_INT, offsetof(SkKeyEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_SkKeyEvent = {"SkKeyEvent", sizeof(SkKeyEvent), lf_SkKeyEvent, (int)(sizeof(lf_SkKeyEvent)/sizeof(lf_SkKeyEvent[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkMouseEvent[] = {
    {"button", LSF_INT, offsetof(SkMouseEvent, button), 0, 0},
    {"x", LSF_INT, offsetof(SkMouseEvent, x), 0, 0},
    {"y", LSF_INT, offsetof(SkMouseEvent, y), 0, 0},
    {"mods", LSF_INT, offsetof(SkMouseEvent, mods), 0, 0},
    {"down", LSF_BOOL, offsetof(SkMouseEvent, down), 0, 0},
    {"repeat", LSF_BOOL, offsetof(SkMouseEvent, repeat), 0, 0},
    {"t_us", LSF_INT, offsetof(SkMouseEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_SkMouseEvent = {"SkMouseEvent", sizeof(SkMouseEvent), lf_SkMouseEvent, (int)(sizeof(lf_SkMouseEvent)/sizeof(lf_SkMouseEvent[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkScrollEvent[] = {
    {"x", LSF_INT, offsetof(SkScrollEvent, x), 0, 0},
    {"y", LSF_INT, offsetof(SkScrollEvent, y), 0, 0},
    {"dx", LSF_INT, offsetof(SkScrollEvent, dx), 0, 0},
    {"dy", LSF_INT, offsetof(SkScrollEvent, dy), 0, 0},
    {"mods", LSF_INT, offsetof(SkScrollEvent, mods), 0, 0},
    {"t_us", LSF_INT, offsetof(SkScrollEvent, t_us), 0, 0},
};
static const LuaStructDesc lsd_SkScrollEvent = {"SkScrollEvent", sizeof(SkScrollEvent), lf_SkScrollEvent, (int)(sizeof(lf_SkScrollEvent)/sizeof(lf_SkScrollEvent[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkConfigHotkeySpec[] = {
    {"name", LSF_STR, offsetof(SkConfigHotkeySpec, name), 0, 0},
    {"hotkey", LSF_STR, offsetof(SkConfigHotkeySpec, hotkey), 0, 0},
    {"cb", LSF_CB, offsetof(SkConfigHotkeySpec, cb), 0, 1},
    {"ud", LSF_PTR, offsetof(SkConfigHotkeySpec, ud), 0, 0},
};
static const LuaStructDesc lsd_SkConfigHotkeySpec = {"SkConfigHotkeySpec", sizeof(SkConfigHotkeySpec), lf_SkConfigHotkeySpec, (int)(sizeof(lf_SkConfigHotkeySpec)/sizeof(lf_SkConfigHotkeySpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkHotkeySpec[] = {
    {"hotkey", LSF_STR, offsetof(SkHotkeySpec, hotkey), 0, 0},
    {"cb", LSF_CB, offsetof(SkHotkeySpec, cb), 0, 0},
    {"cb_ud", LSF_CB, offsetof(SkHotkeySpec, cb_ud), 0, 1},
    {"should_swallow", LSF_CB, offsetof(SkHotkeySpec, should_swallow), 0, 3},
    {"whitelist", LSF_STR_ARRAY, offsetof(SkHotkeySpec, whitelist), 16, 0},
    {"blacklist", LSF_STR_ARRAY, offsetof(SkHotkeySpec, blacklist), 16, 0},
    {"swallow", LSF_BOOL, offsetof(SkHotkeySpec, swallow), 0, 0},
    {"ud", LSF_PTR, offsetof(SkHotkeySpec, ud), 0, 0},
};
static const LuaStructDesc lsd_SkHotkeySpec = {"SkHotkeySpec", sizeof(SkHotkeySpec), lf_SkHotkeySpec, (int)(sizeof(lf_SkHotkeySpec)/sizeof(lf_SkHotkeySpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkConfigHotkeyRegisterSpec[] = {
    {"name", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, name), 0, 0},
    {"key", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, key), 0, 0},
    {"default_hotkey", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, default_hotkey), 0, 0},
    {"cb", LSF_CB, offsetof(SkConfigHotkeyRegisterSpec, cb), 0, 0},
    {"cb_ud", LSF_CB, offsetof(SkConfigHotkeyRegisterSpec, cb_ud), 0, 1},
    {"should_swallow", LSF_CB, offsetof(SkConfigHotkeyRegisterSpec, should_swallow), 0, 3},
    {"whitelist", LSF_STR_ARRAY, offsetof(SkConfigHotkeyRegisterSpec, whitelist), 16, 0},
    {"blacklist", LSF_STR_ARRAY, offsetof(SkConfigHotkeyRegisterSpec, blacklist), 16, 0},
    {"ud", LSF_PTR, offsetof(SkConfigHotkeyRegisterSpec, ud), 0, 0},
};
static const LuaStructDesc lsd_SkConfigHotkeyRegisterSpec = {"SkConfigHotkeyRegisterSpec", sizeof(SkConfigHotkeyRegisterSpec), lf_SkConfigHotkeyRegisterSpec, (int)(sizeof(lf_SkConfigHotkeyRegisterSpec)/sizeof(lf_SkConfigHotkeyRegisterSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkRemappingSpec[] = {
    {"src", LSF_STR, offsetof(SkRemappingSpec, src), 0, 0},
    {"dst", LSF_STR, offsetof(SkRemappingSpec, dst), 0, 0},
    {"whitelist", LSF_STR_ARRAY, offsetof(SkRemappingSpec, whitelist), 16, 0},
    {"blacklist", LSF_STR_ARRAY, offsetof(SkRemappingSpec, blacklist), 16, 0},
};
static const LuaStructDesc lsd_SkRemappingSpec = {"SkRemappingSpec", sizeof(SkRemappingSpec), lf_SkRemappingSpec, (int)(sizeof(lf_SkRemappingSpec)/sizeof(lf_SkRemappingSpec[0]))};
#include "core/skapi.h"
static const LuaFieldDesc lf_SkSnippetSpec[] = {
    {"trigger", LSF_STR, offsetof(SkSnippetSpec, trigger), 0, 0},
    {"text", LSF_STR, offsetof(SkSnippetSpec, text), 0, 0},
    {"fn", LSF_CB, offsetof(SkSnippetSpec, fn), 0, 0},
    {"whitelist", LSF_STR_ARRAY, offsetof(SkSnippetSpec, whitelist), 16, 0},
    {"blacklist", LSF_STR_ARRAY, offsetof(SkSnippetSpec, blacklist), 16, 0},
};
static const LuaStructDesc lsd_SkSnippetSpec = {"SkSnippetSpec", sizeof(SkSnippetSpec), lf_SkSnippetSpec, (int)(sizeof(lf_SkSnippetSpec)/sizeof(lf_SkSnippetSpec[0]))};
#include "core/manifest.h"
static const LuaFieldDesc lf_FeatureManifest[] = {
    {"name", LSF_STR_INLINE, offsetof(FeatureManifest, name), 64, 0},
    {"description", LSF_STR_INLINE, offsetof(FeatureManifest, description), 256, 0},
    {"deps", LSF_STR_INLINE, offsetof(FeatureManifest, deps), sizeof(((FeatureManifest *)0)->deps)/sizeof(((FeatureManifest *)0)->deps[0]), 0},
    {"dep_count", LSF_INT, offsetof(FeatureManifest, dep_count), 0, 0},
    {"provides", LSF_STR_INLINE, offsetof(FeatureManifest, provides), sizeof(((FeatureManifest *)0)->provides)/sizeof(((FeatureManifest *)0)->provides[0]), 0},
    {"provide_count", LSF_INT, offsetof(FeatureManifest, provide_count), 0, 0},
    {"valid", LSF_BOOL, offsetof(FeatureManifest, valid), 0, 0},
};
static const LuaStructDesc lsd_FeatureManifest = {"FeatureManifest", sizeof(FeatureManifest), lf_FeatureManifest, (int)(sizeof(lf_FeatureManifest)/sizeof(lf_FeatureManifest[0]))};
#include "core/hover_widget.h"
static const LuaFieldDesc lf_HoverWidgetShowSpec[] = {
    {"key", LSF_STR, offsetof(HoverWidgetShowSpec, key), 0, 0},
    {"render", LSF_CB, offsetof(HoverWidgetShowSpec, render), 0, 1},
    {"ud", LSF_PTR, offsetof(HoverWidgetShowSpec, ud), 0, 0},
    {"owner_src", LSF_PTR, offsetof(HoverWidgetShowSpec, owner_src), 0, 0},
    {"x", LSF_INT, offsetof(HoverWidgetShowSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(HoverWidgetShowSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(HoverWidgetShowSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(HoverWidgetShowSpec, h), 0, 0},
    {"fps", LSF_INT, offsetof(HoverWidgetShowSpec, fps), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(HoverWidgetShowSpec, no_bg), 0, 0},
    {"bg_hex", LSF_INT, offsetof(HoverWidgetShowSpec, bg_hex), 0, 0},
    {"release", LSF_CB, offsetof(HoverWidgetShowSpec, release), 0, 1},
    {"release_ud", LSF_PTR, offsetof(HoverWidgetShowSpec, release_ud), 0, 0},
};
static const LuaStructDesc lsd_HoverWidgetShowSpec = {"HoverWidgetShowSpec", sizeof(HoverWidgetShowSpec), lf_HoverWidgetShowSpec, (int)(sizeof(lf_HoverWidgetShowSpec)/sizeof(lf_HoverWidgetShowSpec[0]))};
#include "core/types.h"
static const LuaFieldDesc lf_ImageBuffer[] = {
    {"w", LSF_INT, offsetof(ImageBuffer, w), 0, 0},
    {"h", LSF_INT, offsetof(ImageBuffer, h), 0, 0},
    {"pixels", LSF_STR, offsetof(ImageBuffer, pixels), 0, 0},
    {"platform", LSF_PTR, offsetof(ImageBuffer, platform), 0, 0},
};
static const LuaStructDesc lsd_ImageBuffer = {"ImageBuffer", sizeof(ImageBuffer), lf_ImageBuffer, (int)(sizeof(lf_ImageBuffer)/sizeof(lf_ImageBuffer[0]))};
#include "core/context.h"
static const LuaFieldDesc lf_SubwindowInfo[] = {
    {"in_enter", LSF_BOOL, offsetof(SubwindowInfo, in_enter), 0, 0},
    {"pending", LSF_BOOL, offsetof(SubwindowInfo, pending), 0, 0},
    {"created", LSF_BOOL, offsetof(SubwindowInfo, created), 0, 0},
};
static const LuaStructDesc lsd_SubwindowInfo = {"SubwindowInfo", sizeof(SubwindowInfo), lf_SubwindowInfo, (int)(sizeof(lf_SubwindowInfo)/sizeof(lf_SubwindowInfo[0]))};
#include "core/context.h"
static const LuaFieldDesc lf_HeadlessDispatch[] = {
    {"active", LSF_BOOL, offsetof(HeadlessDispatch, active), 0, 0},
    {"capturing", LSF_BOOL, offsetof(HeadlessDispatch, capturing), 0, 0},
    {"spec_valid", LSF_BOOL, offsetof(HeadlessDispatch, spec_valid), 0, 0},
    {"state", LSF_PTR, offsetof(HeadlessDispatch, state), 0, 0},
    {"filter", LSF_STR, offsetof(HeadlessDispatch, filter), 0, 0},
};
static const LuaStructDesc lsd_HeadlessDispatch = {"HeadlessDispatch", sizeof(HeadlessDispatch), lf_HeadlessDispatch, (int)(sizeof(lf_HeadlessDispatch)/sizeof(lf_HeadlessDispatch[0]))};
#include "core/context.h"
static const LuaFieldDesc lf_WindowContext[] = {
    {"selection_complete", LSF_BOOL, offsetof(WindowContext, selection_complete), 0, 0},
    {"auto_enter_pending", LSF_BOOL, offsetof(WindowContext, auto_enter_pending), 0, 0},
    {"window_override_in_progress", LSF_BOOL, offsetof(WindowContext, window_override_in_progress), 0, 0},
    {"initial_text_pending", LSF_STR, offsetof(WindowContext, initial_text_pending), 0, 0},
    {"editbox_placed", LSF_BOOL, offsetof(WindowContext, editbox_placed), 0, 0},
    {"listbox_placed", LSF_BOOL, offsetof(WindowContext, listbox_placed), 0, 0},
    {"band_active", LSF_BOOL, offsetof(WindowContext, band_active), 0, 0},
    {"output_buffer", LSF_STR_INLINE, offsetof(WindowContext, output_buffer), 1024, 0},
    {"current_feature", LSF_STR, offsetof(WindowContext, current_feature), 0, 0},
    {"title", LSF_STR, offsetof(WindowContext, title), 0, 0},
    {"last_popup_pop_ms", LSF_INT, offsetof(WindowContext, last_popup_pop_ms), 0, 0},
};
static const LuaStructDesc lsd_WindowContext = {"WindowContext", sizeof(WindowContext), lf_WindowContext, (int)(sizeof(lf_WindowContext)/sizeof(lf_WindowContext[0]))};
#include "core/context.h"
static const LuaFieldDesc lf_SkaldContext[] = {
};
static const LuaStructDesc lsd_SkaldContext = {"SkaldContext", sizeof(SkaldContext), lf_SkaldContext, (int)(sizeof(lf_SkaldContext)/sizeof(lf_SkaldContext[0]))};
#include "core/clipboard_history.h"
static const LuaFieldDesc lf_ClipboardEntry[] = {
    {"type", LSF_INT, offsetof(ClipboardEntry, type), 0, 0},
    {"timestamp", LSF_INT, offsetof(ClipboardEntry, timestamp), 0, 0},
    {"text", LSF_STR, offsetof(ClipboardEntry, text), 0, 0},
    {"image_path", LSF_STR, offsetof(ClipboardEntry, image_path), 0, 0},
};
static const LuaStructDesc lsd_ClipboardEntry = {"ClipboardEntry", sizeof(ClipboardEntry), lf_ClipboardEntry, (int)(sizeof(lf_ClipboardEntry)/sizeof(lf_ClipboardEntry[0]))};
#include "core/clipboard_history.h"
static const LuaFieldDesc lf_ClipboardHistory[] = {
    {"entries", LSF_PTR, offsetof(ClipboardHistory, entries), 0, 0},
    {"count", LSF_INT, offsetof(ClipboardHistory, count), 0, 0},
    {"capacity", LSF_INT, offsetof(ClipboardHistory, capacity), 0, 0},
};
static const LuaStructDesc lsd_ClipboardHistory = {"ClipboardHistory", sizeof(ClipboardHistory), lf_ClipboardHistory, (int)(sizeof(lf_ClipboardHistory)/sizeof(lf_ClipboardHistory[0]))};
#include "core/checkbox.h"
static const LuaFieldDesc lf_PlaceState[] = {
    {"name", LSF_STR, offsetof(PlaceState, name), 0, 0},
    {"kind", LSF_INT, offsetof(PlaceState, kind), 0, 0},
    {"hovered", LSF_BOOL, offsetof(PlaceState, hovered), 0, 0},
    {"checked", LSF_BOOL, offsetof(PlaceState, checked), 0, 0},
    {"clicked", LSF_BOOL, offsetof(PlaceState, clicked), 0, 0},
};
static const LuaStructDesc lsd_PlaceState = {"PlaceState", sizeof(PlaceState), lf_PlaceState, (int)(sizeof(lf_PlaceState)/sizeof(lf_PlaceState[0]))};
#include "core/checkbox.h"
static const LuaFieldDesc lf_CheckboxSpec[] = {
    {"name", LSF_STR, offsetof(CheckboxSpec, name), 0, 0},
    {"text", LSF_STR, offsetof(CheckboxSpec, text), 0, 0},
    {"font_size", LSF_INT, offsetof(CheckboxSpec, font_size), 0, 0},
    {"x", LSF_INT, offsetof(CheckboxSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(CheckboxSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(CheckboxSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(CheckboxSpec, h), 0, 0},
    {"checked", LSF_BOOL, offsetof(CheckboxSpec, checked), 0, 0},
    {"button", LSF_BOOL, offsetof(CheckboxSpec, button), 0, 0},
    {"checkbox", LSF_BOOL, offsetof(CheckboxSpec, checkbox), 0, 0},
    {"hovered", LSF_BOOL, offsetof(CheckboxSpec, hovered), 0, 0},
    {"was_down", LSF_BOOL, offsetof(CheckboxSpec, was_down), 0, 0},
    {"clicked", LSF_BOOL, offsetof(CheckboxSpec, clicked), 0, 0},
    {"focused", LSF_BOOL, offsetof(CheckboxSpec, focused), 0, 0},
    {"thickness", LSF_INT, offsetof(CheckboxSpec, thickness), 0, 0},
    {"text_color", LSF_INT, offsetof(CheckboxSpec, text_color), 0, 0},
    {"hovered_color", LSF_INT, offsetof(CheckboxSpec, hovered_color), 0, 0},
    {"clicked_color", LSF_INT, offsetof(CheckboxSpec, clicked_color), 0, 0},
    {"bg_color", LSF_INT, offsetof(CheckboxSpec, bg_color), 0, 0},
    {"border", LSF_BOOL, offsetof(CheckboxSpec, border), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(CheckboxSpec, no_bg), 0, 0},
    {"on_click", LSF_CB, offsetof(CheckboxSpec, on_click), 0, 0},
    {"on_hover", LSF_CB, offsetof(CheckboxSpec, on_hover), 0, 0},
    {"render", LSF_CB, offsetof(CheckboxSpec, render), 0, 0},
};
static const LuaStructDesc lsd_CheckboxSpec = {"CheckboxSpec", sizeof(CheckboxSpec), lf_CheckboxSpec, (int)(sizeof(lf_CheckboxSpec)/sizeof(lf_CheckboxSpec[0]))};
#include "core/http.h"
static const LuaFieldDesc lf_HttpTransport[] = {
    {"save_to_file", LSF_CB, offsetof(HttpTransport, save_to_file), 0, 3},
    {"atomic_rename", LSF_CB, offsetof(HttpTransport, atomic_rename), 0, 3},
};
static const LuaStructDesc lsd_HttpTransport = {"HttpTransport", sizeof(HttpTransport), lf_HttpTransport, (int)(sizeof(lf_HttpTransport)/sizeof(lf_HttpTransport[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_Pos[] = {
    {"x", LSF_INT, offsetof(Pos, x), 0, 0},
    {"y", LSF_INT, offsetof(Pos, y), 0, 0},
};
static const LuaStructDesc lsd_Pos = {"Pos", sizeof(Pos), lf_Pos, (int)(sizeof(lf_Pos)/sizeof(lf_Pos[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_Metrics[] = {
    {"edit_cursor_width", LSF_INT, offsetof(Metrics, edit_cursor_width), 0, 0},
    {"edit_cursor_height", LSF_INT, offsetof(Metrics, edit_cursor_height), 0, 0},
    {"editbox_padding", LSF_INT, offsetof(Metrics, editbox_padding), 0, 0},
    {"listbox_padding", LSF_INT, offsetof(Metrics, listbox_padding), 0, 0},
    {"listbox_item_height", LSF_INT, offsetof(Metrics, listbox_item_height), 0, 0},
};
static const LuaStructDesc lsd_Metrics = {"Metrics", sizeof(Metrics), lf_Metrics, (int)(sizeof(lf_Metrics)/sizeof(lf_Metrics[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_EditBox[] = {
    {"filter", LSF_STR_INLINE, offsetof(EditBox, filter), 512, 0},
    {"cursor", LSF_INT, offsetof(EditBox, cursor), 0, 0},
    {"sel_start", LSF_INT, offsetof(EditBox, sel_start), 0, 0},
    {"sel_end", LSF_INT, offsetof(EditBox, sel_end), 0, 0},
    {"scroll_offset", LSF_INT, offsetof(EditBox, scroll_offset), 0, 0},
    {"placeholder", LSF_STR, offsetof(EditBox, placeholder), 0, 0},
};
static const LuaStructDesc lsd_EditBox = {"EditBox", sizeof(EditBox), lf_EditBox, (int)(sizeof(lf_EditBox)/sizeof(lf_EditBox[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_EditboxWrapCache[] = {
    {"key", LSF_INT, offsetof(EditboxWrapCache, key), 0, 0},
    {"len", LSF_INT, offsetof(EditboxWrapCache, len), 0, 0},
    {"line_h", LSF_INT, offsetof(EditboxWrapCache, line_h), 0, 0},
    {"max_width", LSF_INT, offsetof(EditboxWrapCache, max_width), 0, 0},
    {"count", LSF_INT, offsetof(EditboxWrapCache, count), 0, 0},
    {"positions", LSF_PTR, offsetof(EditboxWrapCache, positions), 0, 0},
    {"positions_cap", LSF_INT, offsetof(EditboxWrapCache, positions_cap), 0, 0},
};
static const LuaStructDesc lsd_EditboxWrapCache = {"EditboxWrapCache", sizeof(EditboxWrapCache), lf_EditboxWrapCache, (int)(sizeof(lf_EditboxWrapCache)/sizeof(lf_EditboxWrapCache[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_PromptEditbox[] = {
    {"name", LSF_STR_INLINE, offsetof(PromptEditbox, name), 128, 0},
    {"buffer", LSF_BUFFER, offsetof(PromptEditbox, buffer), offsetof(PromptEditbox, buffer_size), 0},
    {"buffer_size", LSF_INT, offsetof(PromptEditbox, buffer_size), 0, 0},
    {"x", LSF_INT, offsetof(PromptEditbox, x), 0, 0},
    {"y", LSF_INT, offsetof(PromptEditbox, y), 0, 0},
    {"w", LSF_INT, offsetof(PromptEditbox, w), 0, 0},
    {"h", LSF_INT, offsetof(PromptEditbox, h), 0, 0},
    {"placed", LSF_BOOL, offsetof(PromptEditbox, placed), 0, 0},
    {"was_down", LSF_BOOL, offsetof(PromptEditbox, was_down), 0, 0},
    {"primary", LSF_BOOL, offsetof(PromptEditbox, primary), 0, 0},
    {"no_tab", LSF_BOOL, offsetof(PromptEditbox, no_tab), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(PromptEditbox, wrapped), 0, 0},
    {"font_size", LSF_INT, offsetof(PromptEditbox, font_size), 0, 0},
};
static const LuaStructDesc lsd_PromptEditbox = {"PromptEditbox", sizeof(PromptEditbox), lf_PromptEditbox, (int)(sizeof(lf_PromptEditbox)/sizeof(lf_PromptEditbox[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_ListBox[] = {
    {"items", LSF_STR, offsetof(ListBox, items), 0, 0},
    {"items_cap", LSF_INT, offsetof(ListBox, items_cap), 0, 0},
    {"item_count", LSF_INT, offsetof(ListBox, item_count), 0, 0},
    {"matching_indices", LSF_PTR, offsetof(ListBox, matching_indices), 0, 0},
    {"matches_cap", LSF_INT, offsetof(ListBox, matches_cap), 0, 0},
    {"total_matches", LSF_INT, offsetof(ListBox, total_matches), 0, 0},
    {"selected_index", LSF_INT, offsetof(ListBox, selected_index), 0, 0},
    {"selected_text", LSF_STR, offsetof(ListBox, selected_text), 0, 0},
    {"view_offset", LSF_INT, offsetof(ListBox, view_offset), 0, 0},
    {"text_hscroll", LSF_INT, offsetof(ListBox, text_hscroll), 0, 0},
    {"last_filter", LSF_STR_INLINE, offsetof(ListBox, last_filter), 256, 0},
};
static const LuaStructDesc lsd_ListBox = {"ListBox", sizeof(ListBox), lf_ListBox, (int)(sizeof(lf_ListBox)/sizeof(lf_ListBox[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_PromptState[] = {
    {"width", LSF_INT, offsetof(PromptState, width), 0, 0},
    {"height", LSF_INT, offsetof(PromptState, height), 0, 0},
    {"window_title", LSF_STR, offsetof(PromptState, window_title), 0, 0},
    {"eb", LSF_PTR, offsetof(PromptState, eb), 0, 0},
    {"default_eb", LSF_PTR, offsetof(PromptState, default_eb), 0, 0},
    {"editbox_count", LSF_INT, offsetof(PromptState, editbox_count), 0, 0},
    {"active_editbox", LSF_INT, offsetof(PromptState, active_editbox), 0, 0},
    {"lb", LSF_PTR, offsetof(PromptState, lb), 0, 0},
    {"callbacks", LSF_PTR, offsetof(PromptState, callbacks), 0, 0},
    {"render_data", LSF_PTR, offsetof(PromptState, render_data), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(PromptState, no_bg), 0, 0},
    {"password_mode", LSF_BOOL, offsetof(PromptState, password_mode), 0, 0},
    {"bg_color", LSF_INT, offsetof(PromptState, bg_color), 0, 0},
    {"no_separators", LSF_BOOL, offsetof(PromptState, no_separators), 0, 0},
    {"delta_time", LSF_DOUBLE, offsetof(PromptState, delta_time), 0, 0},
    {"last_frame_ticks", LSF_INT, offsetof(PromptState, last_frame_ticks), 0, 0},
};
static const LuaStructDesc lsd_PromptState = {"PromptState", sizeof(PromptState), lf_PromptState, (int)(sizeof(lf_PromptState)/sizeof(lf_PromptState[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_PromptCallbacks[] = {
    {"refresh", LSF_CB, offsetof(PromptCallbacks, refresh), 0, 1},
    {"enter", LSF_CB, offsetof(PromptCallbacks, enter), 0, 2},
    {"ctrl_enter", LSF_CB, offsetof(PromptCallbacks, ctrl_enter), 0, 2},
    {"shift_enter", LSF_CB, offsetof(PromptCallbacks, shift_enter), 0, 2},
    {"ctrl_shift_enter", LSF_CB, offsetof(PromptCallbacks, ctrl_shift_enter), 0, 2},
    {"ctrl_alt_enter", LSF_CB, offsetof(PromptCallbacks, ctrl_alt_enter), 0, 2},
    {"shift_alt_enter", LSF_CB, offsetof(PromptCallbacks, shift_alt_enter), 0, 2},
    {"help", LSF_CB, offsetof(PromptCallbacks, help), 0, 0},
    {"render", LSF_CB, offsetof(PromptCallbacks, render), 0, 1},
    {"edit", LSF_CB, offsetof(PromptCallbacks, edit), 0, 1},
    {"filter", LSF_CB, offsetof(PromptCallbacks, filter), 0, 0},
};
static const LuaStructDesc lsd_PromptCallbacks = {"PromptCallbacks", sizeof(PromptCallbacks), lf_PromptCallbacks, (int)(sizeof(lf_PromptCallbacks)/sizeof(lf_PromptCallbacks[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_PromptSpec[] = {
    {"title", LSF_STR, offsetof(PromptSpec, title), 0, 0},
    {"items", LSF_STR, offsetof(PromptSpec, items), 0, 0},
    {"refresh", LSF_CB, offsetof(PromptSpec, refresh), 0, 1},
    {"enter", LSF_CB, offsetof(PromptSpec, enter), 0, 2},
    {"ctrl_enter", LSF_CB, offsetof(PromptSpec, ctrl_enter), 0, 2},
    {"shift_enter", LSF_CB, offsetof(PromptSpec, shift_enter), 0, 2},
    {"ctrl_shift_enter", LSF_CB, offsetof(PromptSpec, ctrl_shift_enter), 0, 2},
    {"ctrl_alt_enter", LSF_CB, offsetof(PromptSpec, ctrl_alt_enter), 0, 2},
    {"shift_alt_enter", LSF_CB, offsetof(PromptSpec, shift_alt_enter), 0, 2},
    {"render", LSF_CB, offsetof(PromptSpec, render), 0, 1},
    {"edit", LSF_CB, offsetof(PromptSpec, edit), 0, 1},
    {"filter", LSF_CB, offsetof(PromptSpec, filter), 0, 0},
    {"help", LSF_CB, offsetof(PromptSpec, help), 0, 0},
    {"on_close", LSF_CB, offsetof(PromptSpec, on_close), 0, 0},
    {"render_data", LSF_PTR, offsetof(PromptSpec, render_data), 0, 0},
    {"fps", LSF_INT, offsetof(PromptSpec, fps), 0, 0},
    {"auto_scale", LSF_BOOL, offsetof(PromptSpec, auto_scale), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(PromptSpec, no_bg), 0, 0},
    {"password_mode", LSF_BOOL, offsetof(PromptSpec, password_mode), 0, 0},
    {"bg_color", LSF_INT, offsetof(PromptSpec, bg_color), 0, 0},
    {"w", LSF_INT, offsetof(PromptSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(PromptSpec, h), 0, 0},
    {"x", LSF_INT, offsetof(PromptSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(PromptSpec, y), 0, 0},
    {"subwindow", LSF_BOOL, offsetof(PromptSpec, subwindow), 0, 0},
    {"no_separators", LSF_BOOL, offsetof(PromptSpec, no_separators), 0, 0},
    {"anchor_y_to_prompt_h", LSF_BOOL, offsetof(PromptSpec, anchor_y_to_prompt_h), 0, 0},
    {"rounded", LSF_BOOL, offsetof(PromptSpec, rounded), 0, 0},
    {"radius", LSF_INT, offsetof(PromptSpec, radius), 0, 0},
};
static const LuaStructDesc lsd_PromptSpec = {"PromptSpec", sizeof(PromptSpec), lf_PromptSpec, (int)(sizeof(lf_PromptSpec)/sizeof(lf_PromptSpec[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_PopupSpec[] = {
    {"initial_text", LSF_STR, offsetof(PopupSpec, initial_text), 0, 0},
    {"placeholder", LSF_STR, offsetof(PopupSpec, placeholder), 0, 0},
    {"title", LSF_STR, offsetof(PopupSpec, title), 0, 0},
    {"password_mode", LSF_BOOL, offsetof(PopupSpec, password_mode), 0, 0},
    {"x", LSF_INT, offsetof(PopupSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(PopupSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(PopupSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(PopupSpec, h), 0, 0},
};
static const LuaStructDesc lsd_PopupSpec = {"PopupSpec", sizeof(PopupSpec), lf_PopupSpec, (int)(sizeof(lf_PopupSpec)/sizeof(lf_PopupSpec[0]))};
#include "core/types/prompt.h"
static const LuaFieldDesc lf_SortOption[] = {
    {"name", LSF_STR, offsetof(SortOption, name), 0, 0},
    {"func", LSF_CB, offsetof(SortOption, func), 0, 0},
};
static const LuaStructDesc lsd_SortOption = {"SortOption", sizeof(SortOption), lf_SortOption, (int)(sizeof(lf_SortOption)/sizeof(lf_SortOption[0]))};
#include "core/types/keys.h"
static const LuaFieldDesc lf_HotkeyKey[] = {
    {"vk", LSF_INT, offsetof(HotkeyKey, vk), 0, 0},
    {"mods", LSF_INT, offsetof(HotkeyKey, mods), 0, 0},
};
static const LuaStructDesc lsd_HotkeyKey = {"HotkeyKey", sizeof(HotkeyKey), lf_HotkeyKey, (int)(sizeof(lf_HotkeyKey)/sizeof(lf_HotkeyKey[0]))};
#include "core/types/keys.h"
static const LuaFieldDesc lf_PopupKeyEvent[] = {
    {"vk", LSF_INT, offsetof(PopupKeyEvent, vk), 0, 0},
    {"ctrl", LSF_BOOL, offsetof(PopupKeyEvent, ctrl), 0, 0},
    {"shift", LSF_BOOL, offsetof(PopupKeyEvent, shift), 0, 0},
    {"alt", LSF_BOOL, offsetof(PopupKeyEvent, alt), 0, 0},
    {"text", LSF_STR_INLINE, offsetof(PopupKeyEvent, text), 32, 0},
    {"text_len", LSF_INT, offsetof(PopupKeyEvent, text_len), 0, 0},
};
static const LuaStructDesc lsd_PopupKeyEvent = {"PopupKeyEvent", sizeof(PopupKeyEvent), lf_PopupKeyEvent, (int)(sizeof(lf_PopupKeyEvent)/sizeof(lf_PopupKeyEvent[0]))};
#include "core/types/skapi_types.h"
static const LuaFieldDesc lf_SkEditSpec[] = {
    {"name", LSF_STR, offsetof(SkEditSpec, name), 0, 0},
    {"buffer", LSF_BUFFER, offsetof(SkEditSpec, buffer), offsetof(SkEditSpec, buffer_size), 0},
    {"buffer_size", LSF_INT, offsetof(SkEditSpec, buffer_size), 0, 0},
    {"x", LSF_INT, offsetof(SkEditSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SkEditSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SkEditSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SkEditSpec, h), 0, 0},
    {"placeholder", LSF_STR, offsetof(SkEditSpec, placeholder), 0, 0},
    {"initial_text", LSF_STR, offsetof(SkEditSpec, initial_text), 0, 0},
    {"color_bg", LSF_INT, offsetof(SkEditSpec, color_bg), 0, 0},
    {"color_text", LSF_INT, offsetof(SkEditSpec, color_text), 0, 0},
    {"font_size", LSF_INT, offsetof(SkEditSpec, font_size), 0, 0},
    {"caret_h", LSF_INT, offsetof(SkEditSpec, caret_h), 0, 0},
    {"caret_w", LSF_INT, offsetof(SkEditSpec, caret_w), 0, 0},
    {"caret_x", LSF_INT, offsetof(SkEditSpec, caret_x), 0, 0},
    {"caret_y", LSF_INT, offsetof(SkEditSpec, caret_y), 0, 0},
    {"caret_pos", LSF_INT, offsetof(SkEditSpec, caret_pos), 0, 0},
    {"text_y", LSF_INT, offsetof(SkEditSpec, text_y), 0, 0},
    {"auto_focus", LSF_BOOL, offsetof(SkEditSpec, auto_focus), 0, 0},
    {"hide_caret", LSF_BOOL, offsetof(SkEditSpec, hide_caret), 0, 0},
    {"primary", LSF_BOOL, offsetof(SkEditSpec, primary), 0, 0},
    {"no_tab", LSF_BOOL, offsetof(SkEditSpec, no_tab), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(SkEditSpec, wrapped), 0, 0},
};
static const LuaStructDesc lsd_SkEditSpec = {"SkEditSpec", sizeof(SkEditSpec), lf_SkEditSpec, (int)(sizeof(lf_SkEditSpec)/sizeof(lf_SkEditSpec[0]))};
#include "core/types/skapi_types.h"
static const LuaFieldDesc lf_SkListItemCtx[] = {
    {"idx", LSF_INT, offsetof(SkListItemCtx, idx), 0, 0},
    {"selected", LSF_BOOL, offsetof(SkListItemCtx, selected), 0, 0},
    {"x", LSF_INT, offsetof(SkListItemCtx, x), 0, 0},
    {"y", LSF_INT, offsetof(SkListItemCtx, y), 0, 0},
    {"w", LSF_INT, offsetof(SkListItemCtx, w), 0, 0},
    {"h", LSF_INT, offsetof(SkListItemCtx, h), 0, 0},
    {"text", LSF_STR, offsetof(SkListItemCtx, text), 0, 0},
};
static const LuaStructDesc lsd_SkListItemCtx = {"SkListItemCtx", sizeof(SkListItemCtx), lf_SkListItemCtx, (int)(sizeof(lf_SkListItemCtx)/sizeof(lf_SkListItemCtx[0]))};
#include "core/types/skapi_types.h"
static const LuaFieldDesc lf_SkRunAfterPayload[] = {
    {"seconds", LSF_DOUBLE, offsetof(SkRunAfterPayload, seconds), 0, 0},
    {"callback", LSF_CB, offsetof(SkRunAfterPayload, callback), 0, 0},
    {"owner", LSF_STR, offsetof(SkRunAfterPayload, owner), 0, 0},
};
static const LuaStructDesc lsd_SkRunAfterPayload = {"SkRunAfterPayload", sizeof(SkRunAfterPayload), lf_SkRunAfterPayload, (int)(sizeof(lf_SkRunAfterPayload)/sizeof(lf_SkRunAfterPayload[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_WindowLayout[] = {
    {"x", LSF_INT, offsetof(WindowLayout, x), 0, 0},
    {"y", LSF_INT, offsetof(WindowLayout, y), 0, 0},
    {"w", LSF_INT, offsetof(WindowLayout, w), 0, 0},
    {"h", LSF_INT, offsetof(WindowLayout, h), 0, 0},
    {"item_height", LSF_INT, offsetof(WindowLayout, item_height), 0, 0},
};
static const LuaStructDesc lsd_WindowLayout = {"WindowLayout", sizeof(WindowLayout), lf_WindowLayout, (int)(sizeof(lf_WindowLayout)/sizeof(lf_WindowLayout[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_WindowData[] = {
    {"title", LSF_STR_INLINE, offsetof(WindowData, title), 256, 0},
    {"exe", LSF_STR_INLINE, offsetof(WindowData, exe), 256, 0},
};
static const LuaStructDesc lsd_WindowData = {"WindowData", sizeof(WindowData), lf_WindowData, (int)(sizeof(lf_WindowData)/sizeof(lf_WindowData[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_RgbColor[] = {
    {"r", LSF_INT, offsetof(RgbColor, r), 0, 0},
    {"g", LSF_INT, offsetof(RgbColor, g), 0, 0},
    {"b", LSF_INT, offsetof(RgbColor, b), 0, 0},
};
static const LuaStructDesc lsd_RgbColor = {"RgbColor", sizeof(RgbColor), lf_RgbColor, (int)(sizeof(lf_RgbColor)/sizeof(lf_RgbColor[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_DrawRect[] = {
    {"x", LSF_INT, offsetof(DrawRect, x), 0, 0},
    {"y", LSF_INT, offsetof(DrawRect, y), 0, 0},
    {"w", LSF_INT, offsetof(DrawRect, w), 0, 0},
    {"h", LSF_INT, offsetof(DrawRect, h), 0, 0},
    {"color", LSF_INT, offsetof(DrawRect, color), 0, 0},
    {"thickness", LSF_FLOAT, offsetof(DrawRect, thickness), 0, 0},
    {"transparent", LSF_BOOL, offsetof(DrawRect, transparent), 0, 0},
    {"filled", LSF_BOOL, offsetof(DrawRect, filled), 0, 0},
    {"radius", LSF_INT, offsetof(DrawRect, radius), 0, 0},
    {"rounded", LSF_BOOL, offsetof(DrawRect, rounded), 0, 0},
    {"angle", LSF_FLOAT, offsetof(DrawRect, angle), 0, 0},
    {"border", LSF_BOOL, offsetof(DrawRect, border), 0, 0},
};
static const LuaStructDesc lsd_DrawRect = {"DrawRect", sizeof(DrawRect), lf_DrawRect, (int)(sizeof(lf_DrawRect)/sizeof(lf_DrawRect[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_Line[] = {
    {"x1", LSF_INT, offsetof(Line, x1), 0, 0},
    {"y1", LSF_INT, offsetof(Line, y1), 0, 0},
    {"x2", LSF_INT, offsetof(Line, x2), 0, 0},
    {"y2", LSF_INT, offsetof(Line, y2), 0, 0},
    {"color", LSF_INT, offsetof(Line, color), 0, 0},
    {"thickness", LSF_FLOAT, offsetof(Line, thickness), 0, 0},
};
static const LuaStructDesc lsd_Line = {"Line", sizeof(Line), lf_Line, (int)(sizeof(lf_Line)/sizeof(lf_Line[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_Circle[] = {
    {"x", LSF_INT, offsetof(Circle, x), 0, 0},
    {"y", LSF_INT, offsetof(Circle, y), 0, 0},
    {"radius", LSF_INT, offsetof(Circle, radius), 0, 0},
    {"color", LSF_INT, offsetof(Circle, color), 0, 0},
    {"thickness", LSF_FLOAT, offsetof(Circle, thickness), 0, 0},
};
static const LuaStructDesc lsd_Circle = {"Circle", sizeof(Circle), lf_Circle, (int)(sizeof(lf_Circle)/sizeof(lf_Circle[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_WindowList[] = {
    {"windows", LSF_STR, offsetof(WindowList, windows), 0, 0},
    {"count", LSF_INT, offsetof(WindowList, count), 0, 0},
};
static const LuaStructDesc lsd_WindowList = {"WindowList", sizeof(WindowList), lf_WindowList, (int)(sizeof(lf_WindowList)/sizeof(lf_WindowList[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_Timer[] = {
    {"ticks", LSF_INT, offsetof(Timer, ticks), 0, 0},
};
static const LuaStructDesc lsd_Timer = {"Timer", sizeof(Timer), lf_Timer, (int)(sizeof(lf_Timer)/sizeof(lf_Timer[0]))};
#include "core/types/window.h"
static const LuaFieldDesc lf_ListSpec[] = {
    {"lb", LSF_PTR, offsetof(ListSpec, lb), 0, 0},
    {"items", LSF_STR_LIST, offsetof(ListSpec, items), offsetof(ListSpec, count), 0},
    {"count", LSF_INT, offsetof(ListSpec, count), 0, 0},
    {"x", LSF_INT, offsetof(ListSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(ListSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(ListSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(ListSpec, h), 0, 0},
    {"item_height", LSF_INT, offsetof(ListSpec, item_height), 0, 0},
};
static const LuaStructDesc lsd_ListSpec = {"ListSpec", sizeof(ListSpec), lf_ListSpec, (int)(sizeof(lf_ListSpec)/sizeof(lf_ListSpec[0]))};
#include "core/types/file.h"
static const LuaFieldDesc lf_FileItem[] = {
    {"name", LSF_STR_INLINE, offsetof(FileItem, name), 256, 0},
    {"path", LSF_STR_INLINE, offsetof(FileItem, path), sizeof(((FileItem *)0)->path)/sizeof(((FileItem *)0)->path[0]), 0},
};
static const LuaStructDesc lsd_FileItem = {"FileItem", sizeof(FileItem), lf_FileItem, (int)(sizeof(lf_FileItem)/sizeof(lf_FileItem[0]))};
#include "core/types/file.h"
static const LuaFieldDesc lf_ListOfFiles[] = {
    {"files", LSF_PTR, offsetof(ListOfFiles, files), 0, 0},
    {"count", LSF_INT, offsetof(ListOfFiles, count), 0, 0},
    {"capacity", LSF_INT, offsetof(ListOfFiles, capacity), 0, 0},
};
static const LuaStructDesc lsd_ListOfFiles = {"ListOfFiles", sizeof(ListOfFiles), lf_ListOfFiles, (int)(sizeof(lf_ListOfFiles)/sizeof(lf_ListOfFiles[0]))};
#include "core/edit_state.h"
static const LuaFieldDesc lf_EditSelection[] = {
    {"active", LSF_BOOL, offsetof(EditSelection, active), 0, 0},
    {"anchor", LSF_INT, offsetof(EditSelection, anchor), 0, 0},
    {"head", LSF_INT, offsetof(EditSelection, head), 0, 0},
};
static const LuaStructDesc lsd_EditSelection = {"EditSelection", sizeof(EditSelection), lf_EditSelection, (int)(sizeof(lf_EditSelection)/sizeof(lf_EditSelection[0]))};
#include "core/snippet_manager.h"
static const LuaFieldDesc lf_SnippetBinding[] = {
    {"trigger", LSF_STR, offsetof(SnippetBinding, trigger), 0, 0},
    {"text", LSF_STR, offsetof(SnippetBinding, text), 0, 0},
    {"func", LSF_CB, offsetof(SnippetBinding, func), 0, 0},
    {"whitelist", LSF_BOOL, offsetof(SnippetBinding, whitelist), 0, 0},
    {"windows", LSF_STR, offsetof(SnippetBinding, windows), 0, 0},
    {"window_count", LSF_INT, offsetof(SnippetBinding, window_count), 0, 0},
};
static const LuaStructDesc lsd_SnippetBinding = {"SnippetBinding", sizeof(SnippetBinding), lf_SnippetBinding, (int)(sizeof(lf_SnippetBinding)/sizeof(lf_SnippetBinding[0]))};
#include "core/slider.h"
static const LuaFieldDesc lf_SliderState[] = {
    {"name", LSF_STR, offsetof(SliderState, name), 0, 0},
    {"orientation", LSF_INT, offsetof(SliderState, orientation), 0, 0},
    {"hovered", LSF_BOOL, offsetof(SliderState, hovered), 0, 0},
    {"dragging", LSF_BOOL, offsetof(SliderState, dragging), 0, 0},
    {"clicked", LSF_BOOL, offsetof(SliderState, clicked), 0, 0},
    {"value", LSF_FLOAT, offsetof(SliderState, value), 0, 0},
    {"fraction", LSF_FLOAT, offsetof(SliderState, fraction), 0, 0},
};
static const LuaStructDesc lsd_SliderState = {"SliderState", sizeof(SliderState), lf_SliderState, (int)(sizeof(lf_SliderState)/sizeof(lf_SliderState[0]))};
#include "core/slider.h"
static const LuaFieldDesc lf_SliderSpec[] = {
    {"name", LSF_STR, offsetof(SliderSpec, name), 0, 0},
    {"x", LSF_INT, offsetof(SliderSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(SliderSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(SliderSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(SliderSpec, h), 0, 0},
    {"orientation", LSF_INT, offsetof(SliderSpec, orientation), 0, 0},
    {"min_value", LSF_FLOAT, offsetof(SliderSpec, min_value), 0, 0},
    {"max_value", LSF_FLOAT, offsetof(SliderSpec, max_value), 0, 0},
    {"step", LSF_FLOAT, offsetof(SliderSpec, step), 0, 0},
    {"value", LSF_FLOAT, offsetof(SliderSpec, value), 0, 0},
    {"thickness", LSF_INT, offsetof(SliderSpec, thickness), 0, 0},
    {"fill_color", LSF_INT, offsetof(SliderSpec, fill_color), 0, 0},
    {"thumb_color", LSF_INT, offsetof(SliderSpec, thumb_color), 0, 0},
    {"bg_color", LSF_INT, offsetof(SliderSpec, bg_color), 0, 0},
    {"hovered_color", LSF_INT, offsetof(SliderSpec, hovered_color), 0, 0},
    {"clicked_color", LSF_INT, offsetof(SliderSpec, clicked_color), 0, 0},
    {"border", LSF_BOOL, offsetof(SliderSpec, border), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(SliderSpec, no_bg), 0, 0},
    {"hovered", LSF_BOOL, offsetof(SliderSpec, hovered), 0, 0},
    {"dragging", LSF_BOOL, offsetof(SliderSpec, dragging), 0, 0},
    {"clicked", LSF_BOOL, offsetof(SliderSpec, clicked), 0, 0},
    {"was_down", LSF_BOOL, offsetof(SliderSpec, was_down), 0, 0},
    {"focused", LSF_BOOL, offsetof(SliderSpec, focused), 0, 0},
    {"on_input", LSF_CB, offsetof(SliderSpec, on_input), 0, 0},
    {"on_change", LSF_CB, offsetof(SliderSpec, on_change), 0, 0},
    {"on_hover", LSF_CB, offsetof(SliderSpec, on_hover), 0, 0},
    {"render", LSF_CB, offsetof(SliderSpec, render), 0, 0},
};
static const LuaStructDesc lsd_SliderSpec = {"SliderSpec", sizeof(SliderSpec), lf_SliderSpec, (int)(sizeof(lf_SliderSpec)/sizeof(lf_SliderSpec[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxSpec[] = {
    {"text", LSF_STR, offsetof(EditboxSpec, text), 0, 0},
    {"text_len", LSF_INT, offsetof(EditboxSpec, text_len), 0, 0},
    {"cursor", LSF_INT, offsetof(EditboxSpec, cursor), 0, 0},
    {"password_mode", LSF_BOOL, offsetof(EditboxSpec, password_mode), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(EditboxSpec, wrapped), 0, 0},
    {"draw_bg", LSF_BOOL, offsetof(EditboxSpec, draw_bg), 0, 0},
    {"sel_start", LSF_INT, offsetof(EditboxSpec, sel_start), 0, 0},
    {"sel_end", LSF_INT, offsetof(EditboxSpec, sel_end), 0, 0},
    {"has_selection", LSF_BOOL, offsetof(EditboxSpec, has_selection), 0, 0},
    {"sel_mode", LSF_INT, offsetof(EditboxSpec, sel_mode), 0, 0},
    {"x", LSF_INT, offsetof(EditboxSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(EditboxSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(EditboxSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(EditboxSpec, h), 0, 0},
    {"inset", LSF_INT, offsetof(EditboxSpec, inset), 0, 0},
    {"visible_w", LSF_INT, offsetof(EditboxSpec, visible_w), 0, 0},
    {"scroll", LSF_INT, offsetof(EditboxSpec, scroll), 0, 0},
    {"cursor_x", LSF_INT, offsetof(EditboxSpec, cursor_x), 0, 0},
    {"text_w", LSF_INT, offsetof(EditboxSpec, text_w), 0, 0},
    {"text_y", LSF_INT, offsetof(EditboxSpec, text_y), 0, 0},
    {"caret_y", LSF_INT, offsetof(EditboxSpec, caret_y), 0, 0},
    {"align", LSF_STR, offsetof(EditboxSpec, align), 0, 0},
    {"placeholder_dy", LSF_INT, offsetof(EditboxSpec, placeholder_dy), 0, 0},
    {"placeholder", LSF_STR, offsetof(EditboxSpec, placeholder), 0, 0},
    {"text_color", LSF_INT, offsetof(EditboxSpec, text_color), 0, 0},
    {"bg_color", LSF_INT, offsetof(EditboxSpec, bg_color), 0, 0},
    {"font_size", LSF_INT, offsetof(EditboxSpec, font_size), 0, 0},
    {"metrics", LSF_PTR, offsetof(EditboxSpec, metrics), 0, 0},
    {"caret_h", LSF_INT, offsetof(EditboxSpec, caret_h), 0, 0},
    {"caret_w", LSF_INT, offsetof(EditboxSpec, caret_w), 0, 0},
    {"caret_x", LSF_INT, offsetof(EditboxSpec, caret_x), 0, 0},
    {"caret_pos", LSF_INT, offsetof(EditboxSpec, caret_pos), 0, 0},
    {"measure", LSF_CB, offsetof(EditboxSpec, measure), 0, 0},
    {"measure_user", LSF_PTR, offsetof(EditboxSpec, measure_user), 0, 0},
    {"wrap_cache", LSF_PTR, offsetof(EditboxSpec, wrap_cache), 0, 0},
};
static const LuaStructDesc lsd_EditboxSpec = {"EditboxSpec", sizeof(EditboxSpec), lf_EditboxSpec, (int)(sizeof(lf_EditboxSpec)/sizeof(lf_EditboxSpec[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxLayout[] = {
    {"scroll", LSF_INT, offsetof(EditboxLayout, scroll), 0, 0},
    {"draw_x", LSF_INT, offsetof(EditboxLayout, draw_x), 0, 0},
    {"cursor_draw_x", LSF_INT, offsetof(EditboxLayout, cursor_draw_x), 0, 0},
    {"sel_x0", LSF_INT, offsetof(EditboxLayout, sel_x0), 0, 0},
    {"sel_x1", LSF_INT, offsetof(EditboxLayout, sel_x1), 0, 0},
    {"draw_selection", LSF_BOOL, offsetof(EditboxLayout, draw_selection), 0, 0},
    {"caret_x", LSF_INT, offsetof(EditboxLayout, caret_x), 0, 0},
    {"caret_w", LSF_INT, offsetof(EditboxLayout, caret_w), 0, 0},
    {"caret_y", LSF_INT, offsetof(EditboxLayout, caret_y), 0, 0},
    {"caret_text_y", LSF_INT, offsetof(EditboxLayout, caret_text_y), 0, 0},
    {"scroll_y", LSF_INT, offsetof(EditboxLayout, scroll_y), 0, 0},
    {"line_count", LSF_INT, offsetof(EditboxLayout, line_count), 0, 0},
    {"caret_line", LSF_INT, offsetof(EditboxLayout, caret_line), 0, 0},
};
static const LuaStructDesc lsd_EditboxLayout = {"EditboxLayout", sizeof(EditboxLayout), lf_EditboxLayout, (int)(sizeof(lf_EditboxLayout)/sizeof(lf_EditboxLayout[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxInit[] = {
    {"state", LSF_PTR, offsetof(EditboxInit, state), 0, 0},
    {"text", LSF_STR, offsetof(EditboxInit, text), 0, 0},
    {"text_len", LSF_INT, offsetof(EditboxInit, text_len), 0, 0},
    {"cursor", LSF_INT, offsetof(EditboxInit, cursor), 0, 0},
    {"x", LSF_INT, offsetof(EditboxInit, x), 0, 0},
    {"y", LSF_INT, offsetof(EditboxInit, y), 0, 0},
    {"w", LSF_INT, offsetof(EditboxInit, w), 0, 0},
    {"h", LSF_INT, offsetof(EditboxInit, h), 0, 0},
    {"cursor_x", LSF_INT, offsetof(EditboxInit, cursor_x), 0, 0},
    {"text_w", LSF_INT, offsetof(EditboxInit, text_w), 0, 0},
    {"ascent", LSF_INT, offsetof(EditboxInit, ascent), 0, 0},
    {"descent", LSF_INT, offsetof(EditboxInit, descent), 0, 0},
    {"text_color", LSF_INT, offsetof(EditboxInit, text_color), 0, 0},
    {"bg_color", LSF_INT, offsetof(EditboxInit, bg_color), 0, 0},
    {"placeholder", LSF_STR, offsetof(EditboxInit, placeholder), 0, 0},
    {"font_size", LSF_INT, offsetof(EditboxInit, font_size), 0, 0},
    {"caret_h", LSF_INT, offsetof(EditboxInit, caret_h), 0, 0},
    {"caret_w", LSF_INT, offsetof(EditboxInit, caret_w), 0, 0},
    {"caret_x", LSF_INT, offsetof(EditboxInit, caret_x), 0, 0},
    {"caret_y", LSF_INT, offsetof(EditboxInit, caret_y), 0, 0},
    {"caret_pos", LSF_INT, offsetof(EditboxInit, caret_pos), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(EditboxInit, wrapped), 0, 0},
};
static const LuaStructDesc lsd_EditboxInit = {"EditboxInit", sizeof(EditboxInit), lf_EditboxInit, (int)(sizeof(lf_EditboxInit)/sizeof(lf_EditboxInit[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxPlace[] = {
    {"state", LSF_PTR, offsetof(EditboxPlace, state), 0, 0},
    {"buf", LSF_STR, offsetof(EditboxPlace, buf), 0, 0},
    {"text_len", LSF_INT, offsetof(EditboxPlace, text_len), 0, 0},
    {"cursor", LSF_INT, offsetof(EditboxPlace, cursor), 0, 0},
    {"x", LSF_INT, offsetof(EditboxPlace, x), 0, 0},
    {"y", LSF_INT, offsetof(EditboxPlace, y), 0, 0},
    {"w", LSF_INT, offsetof(EditboxPlace, w), 0, 0},
    {"h", LSF_INT, offsetof(EditboxPlace, h), 0, 0},
    {"y_raw", LSF_INT, offsetof(EditboxPlace, y_raw), 0, 0},
    {"font_size", LSF_INT, offsetof(EditboxPlace, font_size), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(EditboxPlace, wrapped), 0, 0},
};
static const LuaStructDesc lsd_EditboxPlace = {"EditboxPlace", sizeof(EditboxPlace), lf_EditboxPlace, (int)(sizeof(lf_EditboxPlace)/sizeof(lf_EditboxPlace[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxUiSpec[] = {
    {"color_text", LSF_INT, offsetof(EditboxUiSpec, color_text), 0, 0},
    {"color_bg", LSF_INT, offsetof(EditboxUiSpec, color_bg), 0, 0},
    {"placeholder", LSF_STR, offsetof(EditboxUiSpec, placeholder), 0, 0},
    {"caret_h", LSF_INT, offsetof(EditboxUiSpec, caret_h), 0, 0},
    {"caret_w", LSF_INT, offsetof(EditboxUiSpec, caret_w), 0, 0},
    {"caret_x", LSF_INT, offsetof(EditboxUiSpec, caret_x), 0, 0},
    {"caret_y", LSF_INT, offsetof(EditboxUiSpec, caret_y), 0, 0},
    {"caret_pos", LSF_INT, offsetof(EditboxUiSpec, caret_pos), 0, 0},
};
static const LuaStructDesc lsd_EditboxUiSpec = {"EditboxUiSpec", sizeof(EditboxUiSpec), lf_EditboxUiSpec, (int)(sizeof(lf_EditboxUiSpec)/sizeof(lf_EditboxUiSpec[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxPlaceSpec[] = {
    {"buffer", LSF_STR, offsetof(EditboxPlaceSpec, buffer), 0, 0},
    {"x", LSF_INT, offsetof(EditboxPlaceSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(EditboxPlaceSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(EditboxPlaceSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(EditboxPlaceSpec, h), 0, 0},
    {"font_size", LSF_INT, offsetof(EditboxPlaceSpec, font_size), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(EditboxPlaceSpec, wrapped), 0, 0},
};
static const LuaStructDesc lsd_EditboxPlaceSpec = {"EditboxPlaceSpec", sizeof(EditboxPlaceSpec), lf_EditboxPlaceSpec, (int)(sizeof(lf_EditboxPlaceSpec)/sizeof(lf_EditboxPlaceSpec[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxMetrics[] = {
    {"text_w", LSF_INT, offsetof(EditboxMetrics, text_w), 0, 0},
    {"cursor_x", LSF_INT, offsetof(EditboxMetrics, cursor_x), 0, 0},
    {"ascent", LSF_INT, offsetof(EditboxMetrics, ascent), 0, 0},
    {"descent", LSF_INT, offsetof(EditboxMetrics, descent), 0, 0},
};
static const LuaStructDesc lsd_EditboxMetrics = {"EditboxMetrics", sizeof(EditboxMetrics), lf_EditboxMetrics, (int)(sizeof(lf_EditboxMetrics)/sizeof(lf_EditboxMetrics[0]))};
#include "core/editbox.h"
static const LuaFieldDesc lf_EditboxInitFromSpec[] = {
    {"state", LSF_PTR, offsetof(EditboxInitFromSpec, state), 0, 0},
    {"buf", LSF_STR, offsetof(EditboxInitFromSpec, buf), 0, 0},
    {"text_len", LSF_INT, offsetof(EditboxInitFromSpec, text_len), 0, 0},
    {"cursor", LSF_INT, offsetof(EditboxInitFromSpec, cursor), 0, 0},
    {"x", LSF_INT, offsetof(EditboxInitFromSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(EditboxInitFromSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(EditboxInitFromSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(EditboxInitFromSpec, h), 0, 0},
    {"cursor_x", LSF_INT, offsetof(EditboxInitFromSpec, cursor_x), 0, 0},
    {"text_w", LSF_INT, offsetof(EditboxInitFromSpec, text_w), 0, 0},
    {"ascent", LSF_INT, offsetof(EditboxInitFromSpec, ascent), 0, 0},
    {"descent", LSF_INT, offsetof(EditboxInitFromSpec, descent), 0, 0},
    {"font_size", LSF_INT, offsetof(EditboxInitFromSpec, font_size), 0, 0},
    {"text_color", LSF_INT, offsetof(EditboxInitFromSpec, text_color), 0, 0},
    {"bg_color", LSF_INT, offsetof(EditboxInitFromSpec, bg_color), 0, 0},
    {"placeholder", LSF_STR, offsetof(EditboxInitFromSpec, placeholder), 0, 0},
    {"caret_h", LSF_INT, offsetof(EditboxInitFromSpec, caret_h), 0, 0},
    {"caret_w", LSF_INT, offsetof(EditboxInitFromSpec, caret_w), 0, 0},
    {"caret_x", LSF_INT, offsetof(EditboxInitFromSpec, caret_x), 0, 0},
    {"caret_y", LSF_INT, offsetof(EditboxInitFromSpec, caret_y), 0, 0},
    {"caret_pos", LSF_INT, offsetof(EditboxInitFromSpec, caret_pos), 0, 0},
    {"wrapped", LSF_BOOL, offsetof(EditboxInitFromSpec, wrapped), 0, 0},
};
static const LuaStructDesc lsd_EditboxInitFromSpec = {"EditboxInitFromSpec", sizeof(EditboxInitFromSpec), lf_EditboxInitFromSpec, (int)(sizeof(lf_EditboxInitFromSpec)/sizeof(lf_EditboxInitFromSpec[0]))};
#include "core/draw_state.h"
static const LuaFieldDesc lf_DrawCanvas[] = {
    {"off_x", LSF_INT, offsetof(DrawCanvas, off_x), 0, 0},
    {"off_y", LSF_INT, offsetof(DrawCanvas, off_y), 0, 0},
    {"bg_override", LSF_INT, offsetof(DrawCanvas, bg_override), 0, 0},
    {"transparent", LSF_BOOL, offsetof(DrawCanvas, transparent), 0, 0},
    {"measuring", LSF_BOOL, offsetof(DrawCanvas, measuring), 0, 0},
    {"meas_x0", LSF_INT, offsetof(DrawCanvas, meas_x0), 0, 0},
    {"meas_y0", LSF_INT, offsetof(DrawCanvas, meas_y0), 0, 0},
    {"meas_x1", LSF_INT, offsetof(DrawCanvas, meas_x1), 0, 0},
    {"meas_y1", LSF_INT, offsetof(DrawCanvas, meas_y1), 0, 0},
    {"meas_origin_x", LSF_INT, offsetof(DrawCanvas, meas_origin_x), 0, 0},
    {"meas_origin_y", LSF_INT, offsetof(DrawCanvas, meas_origin_y), 0, 0},
    {"win32_widget_surface", LSF_BOOL, offsetof(DrawCanvas, win32_widget_surface), 0, 0},
    {"win32_listbox_surface", LSF_BOOL, offsetof(DrawCanvas, win32_listbox_surface), 0, 0},
    {"listbox_rows", LSF_BOOL, offsetof(DrawCanvas, listbox_rows), 0, 0},
    {"suppress_separators", LSF_BOOL, offsetof(DrawCanvas, suppress_separators), 0, 0},
    {"rendering", LSF_BOOL, offsetof(DrawCanvas, rendering), 0, 0},
    {"repaint_pending", LSF_BOOL, offsetof(DrawCanvas, repaint_pending), 0, 0},
};
static const LuaStructDesc lsd_DrawCanvas = {"DrawCanvas", sizeof(DrawCanvas), lf_DrawCanvas, (int)(sizeof(lf_DrawCanvas)/sizeof(lf_DrawCanvas[0]))};
#include "core/cc_registry.h"
static const LuaFieldDesc lf_CcEntrySpec[] = {
    {"display_name", LSF_STR, offsetof(CcEntrySpec, display_name), 0, 0},
    {"internal_name", LSF_STR, offsetof(CcEntrySpec, internal_name), 0, 0},
    {"on_enter", LSF_CB, offsetof(CcEntrySpec, on_enter), 0, 3},
    {"on_ctrl_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_enter), 0, 3},
    {"on_shift_enter", LSF_CB, offsetof(CcEntrySpec, on_shift_enter), 0, 3},
    {"on_ctrl_shift_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_shift_enter), 0, 3},
    {"on_ctrl_alt_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_alt_enter), 0, 3},
    {"on_shift_alt_enter", LSF_CB, offsetof(CcEntrySpec, on_shift_alt_enter), 0, 3},
    {"on_tab", LSF_CB, offsetof(CcEntrySpec, on_tab), 0, 3},
    {"on_action", LSF_CB, offsetof(CcEntrySpec, on_action), 0, 0},
    {"icon", LSF_STR, offsetof(CcEntrySpec, icon), 0, 0},
    {"type", LSF_STR, offsetof(CcEntrySpec, type), 0, 0},
};
static const LuaStructDesc lsd_CcEntrySpec = {"CcEntrySpec", sizeof(CcEntrySpec), lf_CcEntrySpec, (int)(sizeof(lf_CcEntrySpec)/sizeof(lf_CcEntrySpec[0]))};
#include "core/scroll_area.h"
static const LuaFieldDesc lf_ScrollAreaSpec[] = {
    {"name", LSF_STR, offsetof(ScrollAreaSpec, name), 0, 0},
    {"x", LSF_INT, offsetof(ScrollAreaSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(ScrollAreaSpec, y), 0, 0},
    {"w", LSF_INT, offsetof(ScrollAreaSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(ScrollAreaSpec, h), 0, 0},
    {"content_w", LSF_INT, offsetof(ScrollAreaSpec, content_w), 0, 0},
    {"content_h", LSF_INT, offsetof(ScrollAreaSpec, content_h), 0, 0},
    {"off_x", LSF_PTR, offsetof(ScrollAreaSpec, off_x), 0, 0},
    {"off_y", LSF_PTR, offsetof(ScrollAreaSpec, off_y), 0, 0},
    {"wheel_px", LSF_INT, offsetof(ScrollAreaSpec, wheel_px), 0, 0},
    {"owner", LSF_STR, offsetof(ScrollAreaSpec, owner), 0, 0},
};
static const LuaStructDesc lsd_ScrollAreaSpec = {"ScrollAreaSpec", sizeof(ScrollAreaSpec), lf_ScrollAreaSpec, (int)(sizeof(lf_ScrollAreaSpec)/sizeof(lf_ScrollAreaSpec[0]))};
#include "core/widget_registry.h"
static const LuaFieldDesc lf_WidgetKey[] = {
    {"vk", LSF_INT, offsetof(WidgetKey, vk), 0, 0},
    {"mods", LSF_INT, offsetof(WidgetKey, mods), 0, 0},
    {"down", LSF_BOOL, offsetof(WidgetKey, down), 0, 0},
    {"repeat", LSF_BOOL, offsetof(WidgetKey, repeat), 0, 0},
};
static const LuaStructDesc lsd_WidgetKey = {"WidgetKey", sizeof(WidgetKey), lf_WidgetKey, (int)(sizeof(lf_WidgetKey)/sizeof(lf_WidgetKey[0]))};
#include "core/widget_registry.h"
static const LuaFieldDesc lf_WidgetSpec[] = {
    {"name", LSF_STR, offsetof(WidgetSpec, name), 0, 0},
    {"w", LSF_INT, offsetof(WidgetSpec, w), 0, 0},
    {"h", LSF_INT, offsetof(WidgetSpec, h), 0, 0},
    {"slot", LSF_INT, offsetof(WidgetSpec, slot), 0, 0},
    {"order", LSF_INT, offsetof(WidgetSpec, order), 0, 0},
    {"gap", LSF_INT, offsetof(WidgetSpec, gap), 0, 0},
    {"pad", LSF_INT, offsetof(WidgetSpec, pad), 0, 0},
    {"fps", LSF_INT, offsetof(WidgetSpec, fps), 0, 0},
    {"color", LSF_INT, offsetof(WidgetSpec, color), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(WidgetSpec, no_bg), 0, 0},
    {"capture_keys", LSF_BOOL, offsetof(WidgetSpec, capture_keys), 0, 0},
    {"on_click", LSF_CB, offsetof(WidgetSpec, on_click), 0, 0},
    {"on_hover", LSF_CB, offsetof(WidgetSpec, on_hover), 0, 0},
    {"render", LSF_CB, offsetof(WidgetSpec, render), 0, 1},
    {"x", LSF_INT, offsetof(WidgetSpec, x), 0, 0},
    {"y", LSF_INT, offsetof(WidgetSpec, y), 0, 0},
    {"monitor", LSF_INT, offsetof(WidgetSpec, monitor), 0, 0},
    {"monitor_name", LSF_STR, offsetof(WidgetSpec, monitor_name), 0, 0},
};
static const LuaStructDesc lsd_WidgetSpec = {"WidgetSpec", sizeof(WidgetSpec), lf_WidgetSpec, (int)(sizeof(lf_WidgetSpec)/sizeof(lf_WidgetSpec[0]))};
#include "core/widget_registry.h"
static const LuaFieldDesc lf_WidgetState[] = {
    {"width", LSF_INT, offsetof(WidgetState, width), 0, 0},
    {"height", LSF_INT, offsetof(WidgetState, height), 0, 0},
    {"frame", LSF_INT, offsetof(WidgetState, frame), 0, 0},
    {"elapsed", LSF_DOUBLE, offsetof(WidgetState, elapsed), 0, 0},
    {"hovered", LSF_BOOL, offsetof(WidgetState, hovered), 0, 0},
    {"widget", LSF_PTR, offsetof(WidgetState, widget), 0, 0},
};
static const LuaStructDesc lsd_WidgetState = {"WidgetState", sizeof(WidgetState), lf_WidgetState, (int)(sizeof(lf_WidgetState)/sizeof(lf_WidgetState[0]))};
#include "core/widget_registry.h"
static const LuaFieldDesc lf_Widget[] = {
    {"name", LSF_STR_INLINE, offsetof(Widget, name), sizeof(((Widget *)0)->name)/sizeof(((Widget *)0)->name[0]), 0},
    {"w", LSF_INT, offsetof(Widget, w), 0, 0},
    {"h", LSF_INT, offsetof(Widget, h), 0, 0},
    {"gap_override", LSF_INT, offsetof(Widget, gap_override), 0, 0},
    {"fps", LSF_INT, offsetof(Widget, fps), 0, 0},
    {"slot", LSF_INT, offsetof(Widget, slot), 0, 0},
    {"order", LSF_INT, offsetof(Widget, order), 0, 0},
    {"seq", LSF_INT, offsetof(Widget, seq), 0, 0},
    {"order_set", LSF_BOOL, offsetof(Widget, order_set), 0, 0},
    {"free_pos", LSF_BOOL, offsetof(Widget, free_pos), 0, 0},
    {"x", LSF_INT, offsetof(Widget, x), 0, 0},
    {"y", LSF_INT, offsetof(Widget, y), 0, 0},
    {"auto_w", LSF_BOOL, offsetof(Widget, auto_w), 0, 0},
    {"auto_h", LSF_BOOL, offsetof(Widget, auto_h), 0, 0},
    {"origin_x", LSF_INT, offsetof(Widget, origin_x), 0, 0},
    {"origin_y", LSF_INT, offsetof(Widget, origin_y), 0, 0},
    {"bg_color", LSF_INT, offsetof(Widget, bg_color), 0, 0},
    {"has_bg", LSF_BOOL, offsetof(Widget, has_bg), 0, 0},
    {"no_bg", LSF_BOOL, offsetof(Widget, no_bg), 0, 0},
    {"pad", LSF_INT, offsetof(Widget, pad), 0, 0},
    {"mapped", LSF_BOOL, offsetof(Widget, mapped), 0, 0},
    {"dead", LSF_INT, offsetof(Widget, dead), 0, 0},
    {"hover_running", LSF_BOOL, offsetof(Widget, hover_running), 0, 0},
    {"hover_thread", LSF_PTR, offsetof(Widget, hover_thread), 0, 0},
    {"hover_epoch", LSF_INT, offsetof(Widget, hover_epoch), 0, 0},
    {"win", LSF_PTR, offsetof(Widget, win), 0, 0},
    {"render", LSF_CB, offsetof(Widget, render), 0, 1},
    {"frame", LSF_INT, offsetof(Widget, frame), 0, 0},
    {"monitor", LSF_INT, offsetof(Widget, monitor), 0, 0},
    {"on_click", LSF_CB, offsetof(Widget, on_click), 0, 0},
    {"on_hover", LSF_CB, offsetof(Widget, on_hover), 0, 0},
    {"clickable", LSF_BOOL, offsetof(Widget, clickable), 0, 0},
    {"hovered", LSF_BOOL, offsetof(Widget, hovered), 0, 0},
    {"capture_keys", LSF_BOOL, offsetof(Widget, capture_keys), 0, 0},
    {"key_hovered", LSF_BOOL, offsetof(Widget, key_hovered), 0, 0},
    {"pinned_keys", LSF_BOOL, offsetof(Widget, pinned_keys), 0, 0},
    {"prev_click_down", LSF_BOOL, offsetof(Widget, prev_click_down), 0, 0},
    {"key_head", LSF_INT, offsetof(Widget, key_head), 0, 0},
    {"key_tail", LSF_INT, offsetof(Widget, key_tail), 0, 0},
    {"key_dropped", LSF_INT, offsetof(Widget, key_dropped), 0, 0},
};
static const LuaStructDesc lsd_Widget = {"Widget", sizeof(Widget), lf_Widget, (int)(sizeof(lf_Widget)/sizeof(lf_Widget[0]))};
#include "wm/wm.h"
static const LuaFieldDesc lf_SnapRect[] = {
    {"x", LSF_INT, offsetof(SnapRect, x), 0, 0},
    {"y", LSF_INT, offsetof(SnapRect, y), 0, 0},
    {"w", LSF_INT, offsetof(SnapRect, w), 0, 0},
    {"h", LSF_INT, offsetof(SnapRect, h), 0, 0},
};
static const LuaStructDesc lsd_SnapRect = {"SnapRect", sizeof(SnapRect), lf_SnapRect, (int)(sizeof(lf_SnapRect)/sizeof(lf_SnapRect[0]))};
static const LuaStructDesc *lua_generated_structs[] = {
    &lsd_HotkeyBinding,
    &lsd_ListboxLayout,
    &lsd_SlotRect,
    &lsd_MessageSlotSpec,
    &lsd_WidgetMouseState,
    &lsd_StringList,
    &lsd_KeyEvent,
    &lsd_MouseEvent,
    &lsd_ScrollEvent,
    &lsd_MonitorEntry,
    &lsd_MonitorInfo,
    &lsd_PopupVTable,
    &lsd_Popup,
    &lsd_ConfigTomlEntry,
    &lsd_ConfigTomlError,
    &lsd_SearchHit,
    &lsd_SettingsEntrySpec,
    &lsd_SettingsEditboxField,
    &lsd_SettingsEditboxSpec,
    &lsd_SettingsEditboxGroup,
    &lsd_SettingsDropdownSpec,
    &lsd_SettingsCheckboxSpec,
    &lsd_SettingsButtonSpec,
    &lsd_SettingsSliderSpec,
    &lsd_OwnerTag,
    &lsd_ThemeEntry,
    &lsd_SkSize,
    &lsd_SkFeature,
    &lsd_SkFeatureList,
    &lsd_SkMonitorInfo,
    &lsd_SkDirScanSpec,
    &lsd_SkPopulateListboxSpec,
    &lsd_SkEditPopupSpec,
    &lsd_SkMessageSpec,
    &lsd_SkHoverPopupCtx,
    &lsd_SkTextSpec,
    &lsd_SkTextCenteredSpec,
    &lsd_SkMeasureSpec,
    &lsd_SkTextBoundsSpec,
    &lsd_SkImageSpec,
    &lsd_SkTextSegment,
    &lsd_SkTextSegmentsSpec,
    &lsd_SkTextWrappedSpec,
    &lsd_SkScrollSpec,
    &lsd_SkFlag,
    &lsd_SkFlagList,
    &lsd_SkHashMap,
    &lsd_SkKeyEvent,
    &lsd_SkMouseEvent,
    &lsd_SkScrollEvent,
    &lsd_SkConfigHotkeySpec,
    &lsd_SkHotkeySpec,
    &lsd_SkConfigHotkeyRegisterSpec,
    &lsd_SkRemappingSpec,
    &lsd_SkSnippetSpec,
    &lsd_FeatureManifest,
    &lsd_HoverWidgetShowSpec,
    &lsd_ImageBuffer,
    &lsd_SubwindowInfo,
    &lsd_HeadlessDispatch,
    &lsd_WindowContext,
    &lsd_SkaldContext,
    &lsd_ClipboardEntry,
    &lsd_ClipboardHistory,
    &lsd_PlaceState,
    &lsd_CheckboxSpec,
    &lsd_HttpTransport,
    &lsd_Pos,
    &lsd_Metrics,
    &lsd_EditBox,
    &lsd_EditboxWrapCache,
    &lsd_PromptEditbox,
    &lsd_ListBox,
    &lsd_PromptState,
    &lsd_PromptCallbacks,
    &lsd_PromptSpec,
    &lsd_PopupSpec,
    &lsd_SortOption,
    &lsd_HotkeyKey,
    &lsd_PopupKeyEvent,
    &lsd_SkEditSpec,
    &lsd_SkListItemCtx,
    &lsd_SkRunAfterPayload,
    &lsd_WindowLayout,
    &lsd_WindowData,
    &lsd_RgbColor,
    &lsd_DrawRect,
    &lsd_Line,
    &lsd_Circle,
    &lsd_WindowList,
    &lsd_Timer,
    &lsd_ListSpec,
    &lsd_FileItem,
    &lsd_ListOfFiles,
    &lsd_EditSelection,
    &lsd_SnippetBinding,
    &lsd_SliderState,
    &lsd_SliderSpec,
    &lsd_EditboxSpec,
    &lsd_EditboxLayout,
    &lsd_EditboxInit,
    &lsd_EditboxPlace,
    &lsd_EditboxUiSpec,
    &lsd_EditboxPlaceSpec,
    &lsd_EditboxMetrics,
    &lsd_EditboxInitFromSpec,
    &lsd_DrawCanvas,
    &lsd_CcEntrySpec,
    &lsd_ScrollAreaSpec,
    &lsd_WidgetKey,
    &lsd_WidgetSpec,
    &lsd_WidgetState,
    &lsd_Widget,
    &lsd_SnapRect,
};
static const char *lua_alias_from[] = {
    "SkStringList",
    "SkKey",
    "SkWindowHandle",
    "SkImage",
    "SkColor",
    "SkMetrics",
    "SkEvent",
    "SkEventCallback",
    "SkWidget",
    "SkWidgetSpec",
    "SkWidgetKey",
    "SkWidgetState",
    "SkWidgetRenderFn",
    "SkRect",
    "SkLine",
    "SkCircle",
    "SkPos",
    "SkListSpec",
    "SkWindowList",
    "SkCheckboxSpec",
    "SkSliderSpec",
    "SkSettingsEntrySpec",
    "SkPlaceState",
    "SkClipboardEntry",
    "SkSearchKind",
    "SkSearchHit",
    "SkSearchIndex",
    "SkSearchRenderFn",
    "SkSearchActivateFn",
    "SkClipboardHistory",
    "SkPromptSpec",
    "SkLibrary",
    "SkThread",
    "SkMutex",
    "SkCcEntrySpec",
    "SkSettingsEntry",
    "SkSettingsCheckboxSpec",
    "SkSettingsButtonSpec",
    "SkSettingsSliderSpec",
    "SkSettingsDropdownSpec",
    "SkSettingsEditboxChangeFn",
    "SkSettingsEditboxType",
    "SkSettingsEditboxField",
    "SkSettingsEditboxSpec",
    "SkSettingsEditboxGroup",
    "ColorRef",
    "SkTimer",
};
static const char *lua_alias_to[] = {
    "StringList",
    "HotkeyKey",
    "WindowHandle",
    "ImageBuffer",
    "RgbColor",
    "Metrics",
    "Event",
    "EventCallback",
    "Widget",
    "WidgetSpec",
    "WidgetKey",
    "WidgetState",
    "WidgetRenderFn",
    "DrawRect",
    "Line",
    "Circle",
    "Pos",
    "ListSpec",
    "WindowList",
    "CheckboxSpec",
    "SliderSpec",
    "SettingsEntrySpec",
    "PlaceState",
    "ClipboardEntry",
    "SearchKind",
    "SearchHit",
    "SearchIndex",
    "SearchRenderFn",
    "SearchActivateFn",
    "ClipboardHistory",
    "PromptSpec",
    "void",
    "void",
    "void",
    "CcEntrySpec",
    "SettingsEntry",
    "SettingsCheckboxSpec",
    "SettingsButtonSpec",
    "SettingsSliderSpec",
    "SettingsDropdownSpec",
    "SettingsEditboxChangeFn",
    "SettingsEditboxType",
    "SettingsEditboxField",
    "SettingsEditboxSpec",
    "SettingsEditboxGroup",
    "uint32_t",
    "Timer",
};
static const LuaStructDesc *slua_struct_find(const char *name) {
  for (int pass = 0; pass < 8; pass++) {
    const char *next = name;
    for (size_t i = 0; i < sizeof(lua_alias_from)/sizeof(lua_alias_from[0]); i++)
      if (strcmp(next, lua_alias_from[i]) == 0) {
        next = lua_alias_to[i];
        break;
      }
    if (strcmp(next, name) == 0)
      break;
    name = next;
  }
  for (size_t i = 0; i < sizeof(lua_generated_structs)/sizeof(lua_generated_structs[0]); i++)
    if (strcmp(name, lua_generated_structs[i]->name) == 0)
      return lua_generated_structs[i];
  return NULL;
}

const char *slua_cb_struct_name(int kind) {
  switch (kind) {
  case 1: return "PromptState";
  case 2: return "PromptState";
  default: return NULL;
  }
}

static inline int lthunk_sk_copy_to_clipboard(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_copy_to_clipboard");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_copy_to_clipboard"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_copy_file_path(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_copy_file_path");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_copy_file_path"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_paste_from_clipboard(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_paste_from_clipboard");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_paste_from_clipboard"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_paste(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_paste");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_paste"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_clipboard_history_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_clipboard_history_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_clipboard_history_count"); }
  typedef size_t (*fn_t)(void);
  size_t r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_clipboard_clear_history(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_clipboard_clear_history");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_clipboard_clear_history"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_net_status(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_net_status");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_net_status"); }
  typedef SkNetStatus (*fn_t)(void);
  SkNetStatus r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_net_rx_bytes(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_net_rx_bytes");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_net_rx_bytes"); }
  typedef unsigned long long (*fn_t)(void);
  unsigned long long r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_net_rx_bytes_over(lua_State *L) {
  slua_owner_from_state(L);
  double a0 = lua_isnoneornil(L, 1) ? 0 : lua_tonumber(L, 1);
  void *sym = Platform_GetMainSymbol("sk_net_rx_bytes_over");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_net_rx_bytes_over"); }
  typedef unsigned long long (*fn_t)(double);
  unsigned long long r = ((fn_t)sym)((double)a0);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_load_file_list(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_load_file_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_load_file_list"); }
  typedef SkStringList (*fn_t)(const char *);
  SkStringList r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkStringList", &r);
  return 1;
}
static inline int lthunk_sk_array_free(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_array_free");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_array_free"); }
  typedef void (*fn_t)(void *);
  ((fn_t)sym)((void *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_write_string_to_file(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_write_string_to_file");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_write_string_to_file"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_file_exists(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_file_exists");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_file_exists"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_append_unique_line(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_append_unique_line");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_append_unique_line"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_remove_line(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_remove_line");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_remove_line"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_remove_line_contains(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_remove_line_contains");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_remove_line_contains"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_prewarm_image(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_prewarm_image");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_prewarm_image"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_append_to_file(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_append_to_file");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_append_to_file"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_ski_store_set(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("ski_store_set");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_store_set"); }
  typedef bool (*fn_t)(const char *, const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_ski_store_append(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("ski_store_append");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_store_append"); }
  typedef bool (*fn_t)(const char *, const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_ski_store_get_all(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("ski_store_get_all");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_store_get_all"); }
  typedef SkStringList (*fn_t)(const char *, const char *);
  SkStringList r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  slua_push_struct(L, "SkStringList", &r);
  return 1;
}
static inline int lthunk_ski_store_replace_all(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  char **a2 = (char **)slua_string_array(L, 3);
  long a3 = lua_isnoneornil(L, 4) ? 0 : lua_tointeger(L, 4);
  void *sym = Platform_GetMainSymbol("ski_store_replace_all");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_store_replace_all"); }
  typedef bool (*fn_t)(const char *, const char *, const char *const *, int);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1, (const char *const *)a2, (int)a3);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_ski_store_clear(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("ski_store_clear");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_store_clear"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_get_icon_size(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_get_icon_size");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_icon_size"); }
  typedef int (*fn_t)(const char *);
  int r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_free_string_list(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_free_string_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_free_string_list"); }
  typedef void (*fn_t)(SkStringList *);
  ((fn_t)sym)((SkStringList *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_free_files_list(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_free_files_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_free_files_list"); }
  typedef void (*fn_t)(ListOfFiles *);
  ((fn_t)sym)((ListOfFiles *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_free_window_list(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_free_window_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_free_window_list"); }
  typedef bool (*fn_t)(SkWindowList *);
  bool r = ((fn_t)sym)((SkWindowList *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_list_free(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_string_list_free");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_list_free"); }
  typedef void (*fn_t)(StringList *);
  ((fn_t)sym)((StringList *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_launch_with_args(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_launch_with_args");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_launch_with_args"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_launch_with_args_as_admin(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_launch_with_args_as_admin");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_launch_with_args_as_admin"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_launch_terminal_at_path(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_launch_terminal_at_path");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_launch_terminal_at_path"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_launch_detached(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_launch_detached");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_launch_detached"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_terminate_process_by_name(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_terminate_process_by_name");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_terminate_process_by_name"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_process_running(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_process_running");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_process_running"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_get_feature_list(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_feature_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_feature_list"); }
  typedef SkFeatureList (*fn_t)(void);
  SkFeatureList r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkFeatureList", &r);
  return 1;
}
static inline int lthunk_sk_free_feature_list(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_free_feature_list");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_free_feature_list"); }
  typedef void (*fn_t)(SkFeatureList *);
  ((fn_t)sym)((SkFeatureList *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_init_all_hotkeys(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_init_all_hotkeys");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_init_all_hotkeys"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_send_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_send_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_send_hotkey"); }
  typedef void (*fn_t)(const char *);
  ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_hotkey_from_string(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_hotkey_from_string");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_from_string"); }
  typedef SkKey (*fn_t)(const char *);
  SkKey r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkKey", &r);
  return 1;
}
static inline int lthunk_sk_hotkey_down(lua_State *L) {
  slua_owner_from_state(L);
  SkKey a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkKey", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_hotkey_down");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_down"); }
  typedef bool (*fn_t)(SkKey);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_launch_browser(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_launch_browser");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_launch_browser"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_query_browser(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_query_browser");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_query_browser"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_window_by_title(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_focus_window_by_title");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_window_by_title"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_close_active_window(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_close_active_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_close_active_window"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_dismiss_message(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_dismiss_message");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_dismiss_message"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_message_active(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_message_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_message_active"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_toggle_fullscreen_active(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_toggle_fullscreen_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_toggle_fullscreen_active"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_to_monitor(lua_State *L) {
  slua_owner_from_state(L);
  bool a0 = lua_toboolean(L, 1) != 0;
  void *sym = Platform_GetMainSymbol("sk_move_active_window_to_monitor");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_to_monitor"); }
  typedef bool (*fn_t)(bool);
  bool r = ((fn_t)sym)((bool)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_left(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_move_active_window_left");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_left"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_up(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_move_active_window_up");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_up"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_down(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_move_active_window_down");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_down"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_right(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_move_active_window_right");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_right"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_window_left(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_window_left");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_window_left"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_window_up(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_window_up");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_window_up"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_window_down(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_window_down");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_window_down"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_window_right(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_window_right");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_window_right"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_toggle_minimized_active(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_toggle_minimized_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_toggle_minimized_active"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_toggle_maximize_active(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_toggle_maximize_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_toggle_maximize_active"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_toggle_always_on_top_active(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_toggle_always_on_top_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_toggle_always_on_top_active"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_raise_active_window(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_raise_active_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_raise_active_window"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_lower_active_window(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_lower_active_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_lower_active_window"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_center_active_window(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_center_active_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_center_active_window"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_snap_active_window(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_snap_active_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_snap_active_window"); }
  typedef bool (*fn_t)(SnapPreset);
  bool r = ((fn_t)sym)((SnapPreset)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_get_active_window_pid(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_active_window_pid");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_active_window_pid"); }
  typedef long (*fn_t)(void);
  long r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_switch_to_workspace(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_switch_to_workspace");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_switch_to_workspace"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_active_window_to_workspace(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_move_active_window_to_workspace");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_active_window_to_workspace"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_set_active_window_opacity(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_set_active_window_opacity");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_active_window_opacity"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_close_windows_by_title(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_close_windows_by_title");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_close_windows_by_title"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_close_windows_by_id(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_close_windows_by_id");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_close_windows_by_id"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_reload_config(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_reload_config");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_reload_config"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_save_config(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_save_config");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_save_config"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_refresh_ui(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_refresh_ui");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_refresh_ui"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_request_refresh(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_request_refresh");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_request_refresh"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_commit_config(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_commit_config");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_commit_config"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_set_theme(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_set_theme");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_theme"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_theme_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_theme_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_theme_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_screen_width(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_screen_width");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_screen_width"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_screen_height(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_screen_height");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_screen_height"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_monitor_geometry(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_monitor_geometry");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_monitor_geometry"); }
  typedef SkRect (*fn_t)(void);
  SkRect r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkRect", &r);
  return 1;
}
static inline int lthunk_sk_string_starts_with(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_starts_with");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_starts_with"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_starts_with_ci(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_starts_with_ci");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_starts_with_ci"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_ends_with(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_ends_with");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_ends_with"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_equals_ci(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_equals_ci");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_equals_ci"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_split(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_split");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_split"); }
  typedef SkStringList (*fn_t)(const char *, const char *);
  SkStringList r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  slua_push_struct(L, "SkStringList", &r);
  return 1;
}
static inline int lthunk_sk_is_selected(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_is_selected");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_selected"); }
  typedef bool (*fn_t)(ListBox *);
  bool r = ((fn_t)sym)((ListBox *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_set_edit_text(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_set_edit_text");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_edit_text"); }
  typedef void (*fn_t)(const char *);
  ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_set_edit_cursor(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_set_edit_cursor");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_edit_cursor"); }
  typedef void (*fn_t)(int);
  ((fn_t)sym)((int)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_get_edit_cursor(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_edit_cursor");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_edit_cursor"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_replace_line_in_file(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("sk_replace_line_in_file");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_replace_line_in_file"); }
  typedef bool (*fn_t)(const char *, int, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (int)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_image_from_window_title(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_image_from_window_title");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_image_from_window_title"); }
  typedef SkImage (*fn_t)(const char *);
  SkImage r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkImage", &r);
  return 1;
}
static inline int lthunk_sk_image_from_window_id(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_image_from_window_id");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_image_from_window_id"); }
  typedef SkImage (*fn_t)(const char *);
  SkImage r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkImage", &r);
  return 1;
}
static inline int lthunk_sk_image_free(lua_State *L) {
  slua_owner_from_state(L);
  SkImage a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkImage", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_image_free");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_image_free"); }
  typedef void (*fn_t)(SkImage);
  ((fn_t)sym)(a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_ski_get_image_size(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  int a1 = 0;
  int a2 = 0;
  void *sym = Platform_GetMainSymbol("ski_get_image_size");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_get_image_size"); }
  typedef SkSize (*fn_t)(const char *, int *, int *);
  SkSize r = ((fn_t)sym)((const char *)a0, &a1, &a2);
  slua_owner_clear();
  slua_push_struct(L, "SkSize", &r);
  lua_pushinteger(L, (lua_Integer)a1);
  lua_pushinteger(L, (lua_Integer)a2);
  return 3;
}
static inline int lthunk_sk_editbox_set_text(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_editbox_set_text");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_editbox_set_text"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_editbox_set_active(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_editbox_set_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_editbox_set_active"); }
  typedef void (*fn_t)(const char *);
  ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_checkbox_is_checked(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_checkbox_is_checked");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_checkbox_is_checked"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_checkbox_is_hovered(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_checkbox_is_hovered");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_checkbox_is_hovered"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_checkbox_is_clicked(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_checkbox_is_clicked");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_checkbox_is_clicked"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_button_is_checked(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_button_is_checked");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_button_is_checked"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_button_is_hovered(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_button_is_hovered");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_button_is_hovered"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_button_is_clicked(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_button_is_clicked");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_button_is_clicked"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_slider_is_hovered(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_is_hovered");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_is_hovered"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_slider_is_clicked(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_is_clicked");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_is_clicked"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_slider_is_dragging(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_is_dragging");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_is_dragging"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_slider_value(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_value");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_value"); }
  typedef float (*fn_t)(const char *);
  float r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_slider_fraction(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_fraction");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_fraction"); }
  typedef float (*fn_t)(const char *);
  float r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_slider_percent(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_slider_percent");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_percent"); }
  typedef int (*fn_t)(const char *);
  int r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_slider_set_value(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  double a1 = lua_isnoneornil(L, 2) ? 0 : lua_tonumber(L, 2);
  void *sym = Platform_GetMainSymbol("sk_slider_set_value");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_slider_set_value"); }
  typedef bool (*fn_t)(const char *, float);
  bool r = ((fn_t)sym)((const char *)a0, (float)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_kind(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_kind");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_kind"); }
  typedef SkFocusKind (*fn_t)(void);
  SkFocusKind r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_focus_is(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_focus_is");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_is"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_has_entries(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_has_entries");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_has_entries"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_move_next(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_move_next");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_move_next"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_move_before(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_move_before");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_move_before"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_move_dir(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_focus_move_dir");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_move_dir"); }
  typedef bool (*fn_t)(SkFocusDir);
  bool r = ((fn_t)sym)((SkFocusDir)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_activate_focused(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_activate_focused");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_activate_focused"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_adjust(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_focus_adjust");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_adjust"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_set_focus(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_set_focus");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_focus"); }
  typedef bool (*fn_t)(SkFocusKind);
  bool r = ((fn_t)sym)((SkFocusKind)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_focus_clear(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_focus_clear");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_focus_clear"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_listbox_next(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_listbox_next");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_listbox_next"); }
  typedef bool (*fn_t)(SkListItemCtx *);
  bool r = ((fn_t)sym)((SkListItemCtx *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_listbox_end(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_listbox_end");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_listbox_end"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_draw_text_auto(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_draw_text_auto");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_draw_text_auto"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_place_editbox_auto(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("sk_place_editbox_auto");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_place_editbox_auto"); }
  typedef bool (*fn_t)(int, int, const char *);
  bool r = ((fn_t)sym)((int)a0, (int)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_draw_image_auto(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("sk_draw_image_auto");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_draw_image_auto"); }
  typedef bool (*fn_t)(int, int, const char *);
  bool r = ((fn_t)sym)((int)a0, (int)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_monitor_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_monitor_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_monitor_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_monitor_at(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *a1 = (void *)(uintptr_t)(lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2));
  void *sym = Platform_GetMainSymbol("sk_monitor_at");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_monitor_at"); }
  typedef bool (*fn_t)(int, SkMonitorInfo *);
  bool r = ((fn_t)sym)((int)a0, (SkMonitorInfo *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_widget_valid(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_widget_valid");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_widget_valid"); }
  typedef bool (*fn_t)(SkWidget *);
  bool r = ((fn_t)sym)((SkWidget *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_widget_poll_key(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *a1 = (void *)(uintptr_t)(lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2));
  void *sym = Platform_GetMainSymbol("sk_widget_poll_key");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_widget_poll_key"); }
  typedef bool (*fn_t)(SkWidget *, SkWidgetKey *);
  bool r = ((fn_t)sym)((SkWidget *)a0, (SkWidgetKey *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_get_widget_pos(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_get_widget_pos");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_widget_pos"); }
  typedef SkPos (*fn_t)(const char *);
  SkPos r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkPos", &r);
  return 1;
}
static inline int lthunk_sk_widget_show(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_widget_show");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_widget_show"); }
  typedef void (*fn_t)(SkWidget *);
  ((fn_t)sym)((SkWidget *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_widget_hide(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_widget_hide");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_widget_hide"); }
  typedef void (*fn_t)(SkWidget *);
  ((fn_t)sym)((SkWidget *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_widget_destroy(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_widget_destroy");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_widget_destroy"); }
  typedef void (*fn_t)(SkWidget *);
  ((fn_t)sym)((SkWidget *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_rand_num_between(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_rand_num_between");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_rand_num_between"); }
  typedef int (*fn_t)(int, int);
  int r = ((fn_t)sym)((int)a0, (int)a1);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_text_width(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_get_text_width");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_text_width"); }
  typedef int (*fn_t)(const char *, int);
  int r = ((fn_t)sym)((const char *)a0, (int)a1);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_text_height(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_get_text_height");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_text_height"); }
  typedef int (*fn_t)(int);
  int r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_color_lerp(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  double a2 = lua_isnoneornil(L, 3) ? 0 : lua_tonumber(L, 3);
  void *sym = Platform_GetMainSymbol("sk_color_lerp");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_color_lerp"); }
  typedef ColorRef (*fn_t)(ColorRef, ColorRef, float);
  ColorRef r = ((fn_t)sym)((ColorRef)a0, (ColorRef)a1, (float)a2);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_set_draw_offset(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_set_draw_offset");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_set_draw_offset"); }
  typedef void (*fn_t)(SkPos);
  ((fn_t)sym)(a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_get_draw_offset(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_draw_offset");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_draw_offset"); }
  typedef SkPos (*fn_t)(void);
  SkPos r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkPos", &r);
  return 1;
}
static inline int lthunk_sk_scroll_end(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_scroll_end");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_scroll_end"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_http_download(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_http_download");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_http_download"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_http_get_async(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *a1 = slua_callback(L, 2, 0);
  void *a2 = (void *)(uintptr_t)(lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3));
  void *sym = Platform_GetMainSymbol("sk_http_get_async");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_http_get_async"); }
  typedef void (*fn_t)(const char *, SkHttpCallback, void *);
  ((fn_t)sym)((const char *)a0, (SkHttpCallback)a1, (void *)a2);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_parse_flags(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_parse_flags");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_parse_flags"); }
  typedef SkFlagList (*fn_t)(const char *);
  SkFlagList r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  slua_push_struct(L, "SkFlagList", &r);
  return 1;
}
static inline int lthunk_sk_is_flag_prefix(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_is_flag_prefix");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_flag_prefix"); }
  typedef bool (*fn_t)(char);
  bool r = ((fn_t)sym)((char)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_next_flag(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_next_flag");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_next_flag"); }
  typedef bool (*fn_t)(char *);
  bool r = ((fn_t)sym)((char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_free_flags(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_free_flags");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_free_flags"); }
  typedef void (*fn_t)(SkFlagList *);
  ((fn_t)sym)((SkFlagList *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_timer_start(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_timer_start");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_timer_start"); }
  typedef SkTimer (*fn_t)(void);
  SkTimer r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkTimer", &r);
  return 1;
}
static inline int lthunk_sk_timer_end(lua_State *L) {
  slua_owner_from_state(L);
  SkTimer a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkTimer", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_timer_end");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_timer_end"); }
  typedef double (*fn_t)(SkTimer);
  double r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_profile_start(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_profile_start");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_profile_start"); }
  typedef SkTimer (*fn_t)(void);
  SkTimer r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkTimer", &r);
  return 1;
}
static inline int lthunk_sk_profile_end(lua_State *L) {
  slua_owner_from_state(L);
  SkTimer a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkTimer", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_profile_end");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_profile_end"); }
  typedef void (*fn_t)(SkTimer);
  ((fn_t)sym)(a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_run_after_time(lua_State *L) {
  slua_owner_from_state(L);
  double a0 = lua_isnoneornil(L, 1) ? 0 : lua_tonumber(L, 1);
  void *sym = Platform_GetMainSymbol("sk_run_after_time");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_run_after_time"); }
  typedef void (*fn_t)(double);
  ((fn_t)sym)((double)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_string_contains(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_contains");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_contains"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_string_contains_ci(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_string_contains_ci");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_string_contains_ci"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_is_root(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_is_root");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_root"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_run_every(lua_State *L) {
  slua_owner_from_state(L);
  double a0 = lua_isnoneornil(L, 1) ? 0 : lua_tonumber(L, 1);
  void *sym = Platform_GetMainSymbol("sk_run_every");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_run_every"); }
  typedef void (*fn_t)(double);
  ((fn_t)sym)((double)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_hashmap_get(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  long a2 = lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3);
  void *sym = Platform_GetMainSymbol("sk_hashmap_get");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hashmap_get"); }
  typedef long (*fn_t)(SkHashMap *, const char *, long);
  long r = ((fn_t)sym)((SkHashMap *)a0, (const char *)a1, (long)a2);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_hashmap_has(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_hashmap_has");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hashmap_has"); }
  typedef bool (*fn_t)(SkHashMap *, const char *);
  bool r = ((fn_t)sym)((SkHashMap *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_hashmap_len(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_hashmap_len");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hashmap_len"); }
  typedef size_t (*fn_t)(SkHashMap *);
  size_t r = ((fn_t)sym)((SkHashMap *)a0);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_all_window_titles(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_all_window_titles");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_all_window_titles"); }
  typedef SkWindowList (*fn_t)(void);
  SkWindowList r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkWindowList", &r);
  return 1;
}
static inline int lthunk_sk_get_all_window_ids(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_all_window_ids");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_all_window_ids"); }
  typedef SkWindowList (*fn_t)(void);
  SkWindowList r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkWindowList", &r);
  return 1;
}
static inline int lthunk_sk_is_window_open(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_is_window_open");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_window_open"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_get_pixel_color(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_get_pixel_color");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_pixel_color"); }
  typedef SkColor (*fn_t)(SkPos);
  SkColor r = ((fn_t)sym)(a0);
  slua_owner_clear();
  slua_push_struct(L, "SkColor", &r);
  return 1;
}
static inline int lthunk_sk_color_matches(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  SkColor a1; memset(&a1, 0, sizeof(a1));
  slua_fill_struct(L, "SkColor", 2, &a1);
  long a2 = lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3);
  void *sym = Platform_GetMainSymbol("sk_color_matches");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_color_matches"); }
  typedef bool (*fn_t)(SkPos, SkColor, int);
  bool r = ((fn_t)sym)(a0, a1, (int)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_wait_pixel(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  SkColor a1; memset(&a1, 0, sizeof(a1));
  slua_fill_struct(L, "SkColor", 2, &a1);
  long a2 = lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3);
  double a3 = lua_isnoneornil(L, 4) ? 0 : lua_tonumber(L, 4);
  void *sym = Platform_GetMainSymbol("sk_wait_pixel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_wait_pixel"); }
  typedef bool (*fn_t)(SkPos, SkColor, int, double);
  bool r = ((fn_t)sym)(a0, a1, (int)a2, (double)a3);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_move_to_pos(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_move_to_pos");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_to_pos"); }
  typedef bool (*fn_t)(SkPos);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_click_at(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_click_at");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_click_at"); }
  typedef bool (*fn_t)(SkPos);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_left_click(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_left_click");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_left_click"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_right_click(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_right_click");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_right_click"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_emit_mouse_button(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  bool a1 = lua_toboolean(L, 2) != 0;
  void *sym = Platform_GetMainSymbol("sk_emit_mouse_button");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_emit_mouse_button"); }
  typedef bool (*fn_t)(int, bool);
  bool r = ((fn_t)sym)((int)a0, (bool)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_emit_mouse_button_at(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  bool a2 = lua_toboolean(L, 3) != 0;
  void *sym = Platform_GetMainSymbol("sk_emit_mouse_button_at");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_emit_mouse_button_at"); }
  typedef bool (*fn_t)(SkPos, int, bool);
  bool r = ((fn_t)sym)(a0, (int)a1, (bool)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_click_button_at(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_click_button_at");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_click_button_at"); }
  typedef bool (*fn_t)(SkPos, int);
  bool r = ((fn_t)sym)(a0, (int)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_mouse_scroll(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_mouse_scroll");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_mouse_scroll"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_emit_key(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  bool a1 = lua_toboolean(L, 2) != 0;
  void *sym = Platform_GetMainSymbol("sk_emit_key");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_emit_key"); }
  typedef void (*fn_t)(int, bool);
  ((fn_t)sym)((int)a0, (bool)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_move_to_pos_rel(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_move_to_pos_rel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_move_to_pos_rel"); }
  typedef bool (*fn_t)(SkPos);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_click_at_rel(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_click_at_rel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_click_at_rel"); }
  typedef bool (*fn_t)(SkPos);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_emit_mouse_button_at_rel(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  bool a2 = lua_toboolean(L, 3) != 0;
  void *sym = Platform_GetMainSymbol("sk_emit_mouse_button_at_rel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_emit_mouse_button_at_rel"); }
  typedef bool (*fn_t)(SkPos, int, bool);
  bool r = ((fn_t)sym)(a0, (int)a1, (bool)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_click_button_at_rel(lua_State *L) {
  slua_owner_from_state(L);
  SkPos a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkPos", 1, &a0);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_click_button_at_rel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_click_button_at_rel"); }
  typedef bool (*fn_t)(SkPos, int);
  bool r = ((fn_t)sym)(a0, (int)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_type_text(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_type_text");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_type_text"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_sleep(lua_State *L) {
  slua_owner_from_state(L);
  double a0 = lua_isnoneornil(L, 1) ? 0 : lua_tonumber(L, 1);
  void *sym = Platform_GetMainSymbol("sk_sleep");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_sleep"); }
  typedef void (*fn_t)(double);
  ((fn_t)sym)((double)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_is_key_down(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_is_key_down");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_key_down"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_is_key_pressed(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_is_key_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_key_pressed"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_is_key_released(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_is_key_released");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_is_key_released"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_ctrl_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_ctrl_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_ctrl_pressed"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_shift_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_shift_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_shift_pressed"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_alt_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_alt_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_alt_pressed"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_super_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_super_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_super_pressed"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_on_key(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_on_key");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_on_key"); }
  typedef void (*fn_t)(void *);
  ((fn_t)sym)((void *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_get_key_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_key_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_key_pressed"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_char_pressed(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_char_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_char_pressed"); }
  typedef char (*fn_t)(void);
  char r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_poll_mouse(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_poll_mouse");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_poll_mouse"); }
  typedef bool (*fn_t)(SkMouseEvent *);
  bool r = ((fn_t)sym)((SkMouseEvent *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_poll_scroll(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_poll_scroll");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_poll_scroll"); }
  typedef bool (*fn_t)(SkScrollEvent *);
  bool r = ((fn_t)sym)((SkScrollEvent *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_scroll_queue_len(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_scroll_queue_len");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_scroll_queue_len"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_clear_scroll_queue(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_clear_scroll_queue");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_clear_scroll_queue"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_mouse_queue_len(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_mouse_queue_len");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_mouse_queue_len"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_mouse_queue_dropped(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_mouse_queue_dropped");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_mouse_queue_dropped"); }
  typedef unsigned long (*fn_t)(void);
  unsigned long r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_clear_mouse_queue(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_clear_mouse_queue");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_clear_mouse_queue"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_get_cursor_pos(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_cursor_pos");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_cursor_pos"); }
  typedef SkPos (*fn_t)(void);
  SkPos r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkPos", &r);
  return 1;
}
static inline int lthunk_sk_get_cursor_pos_rel(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_cursor_pos_rel");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_cursor_pos_rel"); }
  typedef SkPos (*fn_t)(void);
  SkPos r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkPos", &r);
  return 1;
}
static inline int lthunk_sk_mouse_pressed(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *sym = Platform_GetMainSymbol("sk_mouse_pressed");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_mouse_pressed"); }
  typedef bool (*fn_t)(int);
  bool r = ((fn_t)sym)((int)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_poll_key(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = (void *)(uintptr_t)(lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1));
  void *sym = Platform_GetMainSymbol("sk_poll_key");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_poll_key"); }
  typedef bool (*fn_t)(SkKeyEvent *);
  bool r = ((fn_t)sym)((SkKeyEvent *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_key_queue_len(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_key_queue_len");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_key_queue_len"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_key_queue_dropped(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_key_queue_dropped");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_key_queue_dropped"); }
  typedef unsigned long (*fn_t)(void);
  unsigned long r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_clear_key_queue(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_clear_key_queue");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_clear_key_queue"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_config_set_setting(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_setting");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_setting"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_set_setting_int(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_setting_int");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_setting_int"); }
  typedef bool (*fn_t)(const char *, int);
  bool r = ((fn_t)sym)((const char *)a0, (int)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_set_setting_float(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  double a1 = lua_isnoneornil(L, 2) ? 0 : lua_tonumber(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_setting_float");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_setting_float"); }
  typedef bool (*fn_t)(const char *, float);
  bool r = ((fn_t)sym)((const char *)a0, (float)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_get_setting_string(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  long a2 = lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3);
  const char *a3 = lua_isnoneornil(L, 4) ? "" : luaL_checkstring(L, 4);
  void *sym = Platform_GetMainSymbol("sk_config_get_setting_string");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_get_setting_string"); }
  typedef bool (*fn_t)(const char *, char *, size_t, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (char *)a1, (size_t)a2, (const char *)a3);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_get_setting_bool(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_config_get_setting_bool");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_get_setting_bool"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_get_setting_int(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_get_setting_int");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_get_setting_int"); }
  typedef int (*fn_t)(const char *, int);
  int r = ((fn_t)sym)((const char *)a0, (int)a1);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_config_get_setting_float(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_config_get_setting_float");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_get_setting_float"); }
  typedef float (*fn_t)(const char *);
  float r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_config_get_setting_float_or(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  double a1 = lua_isnoneornil(L, 2) ? 0 : lua_tonumber(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_get_setting_float_or");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_get_setting_float_or"); }
  typedef float (*fn_t)(const char *, float);
  float r = ((fn_t)sym)((const char *)a0, (float)a1);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_parse_int(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_parse_int");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_parse_int"); }
  typedef int (*fn_t)(const char *, int);
  int r = ((fn_t)sym)((const char *)a0, (int)a1);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_parse_float(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  double a1 = lua_isnoneornil(L, 2) ? 0 : lua_tonumber(L, 2);
  void *sym = Platform_GetMainSymbol("sk_parse_float");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_parse_float"); }
  typedef float (*fn_t)(const char *, float);
  float r = ((fn_t)sym)((const char *)a0, (float)a1);
  slua_owner_clear();
  lua_pushnumber(L, (double)r);
  return 1;
}
static inline int lthunk_sk_config_has_setting(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_config_has_setting");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_has_setting"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_setting_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_setting_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_setting_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_settings_editbox_set(lua_State *L) {
  slua_owner_from_state(L);
  SkSettingsEditboxGroup a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkSettingsEditboxGroup", 1, &a0);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  const char *a2 = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
  void *sym = Platform_GetMainSymbol("sk_settings_editbox_set");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_settings_editbox_set"); }
  typedef bool (*fn_t)(SkSettingsEditboxGroup, const char *, const char *);
  bool r = ((fn_t)sym)(a0, (const char *)a1, (const char *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_settings_editbox_right_offset(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_settings_editbox_right_offset");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_settings_editbox_right_offset"); }
  typedef int (*fn_t)(int, int);
  int r = ((fn_t)sym)((int)a0, (int)a1);
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_register_alias(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_register_alias");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_register_alias"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_remove_alias(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_remove_alias");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_remove_alias"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_init(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_init");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_init"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_config_set_alias(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_alias");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_alias"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_clear_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_config_clear_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_clear_hotkey"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_bind_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *a1 = slua_callback(L, 2, 1);
  void *a2 = (void *)(uintptr_t)(lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3));
  void *sym = Platform_GetMainSymbol("sk_config_bind_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_bind_hotkey"); }
  typedef bool (*fn_t)(const char *, SkConfigFn, void *);
  bool r = ((fn_t)sym)((const char *)a0, (SkConfigFn)a1, (void *)a2);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_hotkey_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_hotkey_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_hotkey_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_config_clear_alias(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_config_clear_alias");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_clear_alias"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_alias_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_alias_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_alias_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_config_quicklink_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_quicklink_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_quicklink_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_config_set_quicklink(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_quicklink");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_quicklink"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_config_feature_shortcut_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_config_feature_shortcut_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_feature_shortcut_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_config_set_feature_shortcut(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("sk_config_set_feature_shortcut");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_config_set_feature_shortcut"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_run_feature_shortcut(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_run_feature_shortcut");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_run_feature_shortcut"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_cc_entry_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_cc_entry_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_cc_entry_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_settings_entry_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_settings_entry_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_settings_entry_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_settings_scroll_to_block(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_settings_scroll_to_block");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_settings_scroll_to_block"); }
  typedef void (*fn_t)(const char *, int);
  ((fn_t)sym)((const char *)a0, (int)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_ski_add_dep(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  const char *a1 = lua_isnoneornil(L, 2) ? "" : luaL_checkstring(L, 2);
  void *sym = Platform_GetMainSymbol("ski_add_dep");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_add_dep"); }
  typedef bool (*fn_t)(const char *, const char *);
  bool r = ((fn_t)sym)((const char *)a0, (const char *)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_unregister_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_unregister_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_unregister_hotkey"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_disable_global_hotkeys(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_disable_global_hotkeys");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_disable_global_hotkeys"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_enable_global_hotkeys(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_enable_global_hotkeys");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_enable_global_hotkeys"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_hotkey_capture_begin(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_hotkey_capture_begin");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_capture_begin"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_hotkey_capture_poll(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *sym = Platform_GetMainSymbol("sk_hotkey_capture_poll");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_capture_poll"); }
  typedef bool (*fn_t)(char *, size_t);
  bool r = ((fn_t)sym)((char *)a0, (size_t)a1);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_hotkey_capture_active(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_hotkey_capture_active");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_capture_active"); }
  typedef bool (*fn_t)(void);
  bool r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_hotkey_capture_end(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_hotkey_capture_end");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_capture_end"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_hotkey_count(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_hotkey_count");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_count"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_hotkey_taken(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_hotkey_taken");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_hotkey_taken"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_register_deferred_init(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_register_deferred_init");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_register_deferred_init"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_replay_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  SkKey a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkKey", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_replay_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_replay_hotkey"); }
  typedef bool (*fn_t)(SkKey);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_execute_hotkey(lua_State *L) {
  slua_owner_from_state(L);
  SkKey a0; memset(&a0, 0, sizeof(a0));
  slua_fill_struct(L, "SkKey", 1, &a0);
  void *sym = Platform_GetMainSymbol("sk_execute_hotkey");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_execute_hotkey"); }
  typedef bool (*fn_t)(SkKey);
  bool r = ((fn_t)sym)(a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_unregister_snippet(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_unregister_snippet");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_unregister_snippet"); }
  typedef bool (*fn_t)(const char *);
  bool r = ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  lua_pushboolean(L, r != 0);
  return 1;
}
static inline int lthunk_sk_reload_feature(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *sym = Platform_GetMainSymbol("sk_reload_feature");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_reload_feature"); }
  typedef void (*fn_t)(const char *);
  ((fn_t)sym)((const char *)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_reload_all_features(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_reload_all_features");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_reload_all_features"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_get_pid(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_pid");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_pid"); }
  typedef int (*fn_t)(void);
  int r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_cap_register_abi(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  long a1 = lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2);
  void *a2 = (void *)(uintptr_t)(lua_isnoneornil(L, 3) ? 0 : lua_tointeger(L, 3));
  void *sym = Platform_GetMainSymbol("sk_cap_register_abi");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_cap_register_abi"); }
  typedef void (*fn_t)(const char *, unsigned, void *);
  ((fn_t)sym)((const char *)a0, (unsigned)a1, (void *)a2);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_ski_cap_register(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *a1 = (void *)(uintptr_t)(lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2));
  void *sym = Platform_GetMainSymbol("ski_cap_register");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "ski_cap_register"); }
  typedef void (*fn_t)(const char *, void *);
  ((fn_t)sym)((const char *)a0, (void *)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_cap_generation(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_cap_generation");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_cap_generation"); }
  typedef unsigned (*fn_t)(void);
  unsigned r = ((fn_t)sym)();
  slua_owner_clear();
  lua_pushinteger(L, (lua_Integer)r);
  return 1;
}
static inline int lthunk_sk_cap_bind_declare(lua_State *L) {
  slua_owner_from_state(L);
  const char *a0 = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
  void *a1 = (void *)(uintptr_t)(lua_isnoneornil(L, 2) ? 0 : lua_tointeger(L, 2));
  void *sym = Platform_GetMainSymbol("sk_cap_bind_declare");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_cap_bind_declare"); }
  typedef void (*fn_t)(const char *, void *);
  ((fn_t)sym)((const char *)a0, (void *)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_register_event(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *a1 = slua_callback(L, 2, 0);
  void *sym = Platform_GetMainSymbol("sk_register_event");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_register_event"); }
  typedef void (*fn_t)(SkEvent, SkEventCallback);
  ((fn_t)sym)((SkEvent)a0, (SkEventCallback)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_unregister_event(lua_State *L) {
  slua_owner_from_state(L);
  long a0 = lua_isnoneornil(L, 1) ? 0 : lua_tointeger(L, 1);
  void *a1 = slua_callback(L, 2, 0);
  void *sym = Platform_GetMainSymbol("sk_unregister_event");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_unregister_event"); }
  typedef void (*fn_t)(SkEvent, SkEventCallback);
  ((fn_t)sym)((SkEvent)a0, (SkEventCallback)a1);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_register_key_interceptor(lua_State *L) {
  slua_owner_from_state(L);
  void *a0 = slua_callback(L, 1, 2);
  void *sym = Platform_GetMainSymbol("sk_register_key_interceptor");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_register_key_interceptor"); }
  typedef void (*fn_t)(SkKeyInterceptorFn);
  ((fn_t)sym)((SkKeyInterceptorFn)a0);
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_close_window(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_close_window");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_close_window"); }
  typedef void (*fn_t)(void);
  ((fn_t)sym)();
  slua_owner_clear();
  return 0;
}
static inline int lthunk_sk_get_window_bounds(lua_State *L) {
  slua_owner_from_state(L);
  void *sym = Platform_GetMainSymbol("sk_get_window_bounds");
  if (!sym) { slua_owner_clear(); return luaL_error(L, "unknown skapi function '%s'", "sk_get_window_bounds"); }
  typedef SkRect (*fn_t)(void);
  SkRect r = ((fn_t)sym)();
  slua_owner_clear();
  slua_push_struct(L, "SkRect", &r);
  return 1;
}
static inline void slua_register_thunks(void *Lp) {
  lua_State *L = (lua_State *)Lp;
  lua_pushcfunction(L, lthunk_sk_copy_to_clipboard); lua_setfield(L, -2, "sk_copy_to_clipboard");
  lua_pushcfunction(L, lthunk_sk_copy_file_path); lua_setfield(L, -2, "sk_copy_file_path");
  lua_pushcfunction(L, lthunk_sk_paste_from_clipboard); lua_setfield(L, -2, "sk_paste_from_clipboard");
  lua_pushcfunction(L, lthunk_sk_paste); lua_setfield(L, -2, "sk_paste");
  lua_pushcfunction(L, lthunk_sk_clipboard_history_count); lua_setfield(L, -2, "sk_clipboard_history_count");
  lua_pushcfunction(L, lthunk_sk_clipboard_clear_history); lua_setfield(L, -2, "sk_clipboard_clear_history");
  lua_pushcfunction(L, lthunk_sk_net_status); lua_setfield(L, -2, "sk_net_status");
  lua_pushcfunction(L, lthunk_sk_net_rx_bytes); lua_setfield(L, -2, "sk_net_rx_bytes");
  lua_pushcfunction(L, lthunk_sk_net_rx_bytes_over); lua_setfield(L, -2, "sk_net_rx_bytes_over");
  lua_pushcfunction(L, lthunk_sk_load_file_list); lua_setfield(L, -2, "sk_load_file_list");
  lua_pushcfunction(L, lthunk_sk_array_free); lua_setfield(L, -2, "sk_array_free");
  lua_pushcfunction(L, lthunk_sk_write_string_to_file); lua_setfield(L, -2, "sk_write_string_to_file");
  lua_pushcfunction(L, lthunk_sk_file_exists); lua_setfield(L, -2, "sk_file_exists");
  lua_pushcfunction(L, lthunk_sk_append_unique_line); lua_setfield(L, -2, "sk_append_unique_line");
  lua_pushcfunction(L, lthunk_sk_remove_line); lua_setfield(L, -2, "sk_remove_line");
  lua_pushcfunction(L, lthunk_sk_remove_line_contains); lua_setfield(L, -2, "sk_remove_line_contains");
  lua_pushcfunction(L, lthunk_sk_prewarm_image); lua_setfield(L, -2, "sk_prewarm_image");
  lua_pushcfunction(L, lthunk_sk_append_to_file); lua_setfield(L, -2, "sk_append_to_file");
  lua_pushcfunction(L, lthunk_ski_store_set); lua_setfield(L, -2, "ski_store_set");
  lua_pushcfunction(L, lthunk_ski_store_append); lua_setfield(L, -2, "ski_store_append");
  lua_pushcfunction(L, lthunk_ski_store_get_all); lua_setfield(L, -2, "ski_store_get_all");
  lua_pushcfunction(L, lthunk_ski_store_replace_all); lua_setfield(L, -2, "ski_store_replace_all");
  lua_pushcfunction(L, lthunk_ski_store_clear); lua_setfield(L, -2, "ski_store_clear");
  lua_pushcfunction(L, lthunk_sk_get_icon_size); lua_setfield(L, -2, "sk_get_icon_size");
  lua_pushcfunction(L, lthunk_sk_free_string_list); lua_setfield(L, -2, "sk_free_string_list");
  lua_pushcfunction(L, lthunk_sk_free_files_list); lua_setfield(L, -2, "sk_free_files_list");
  lua_pushcfunction(L, lthunk_sk_free_window_list); lua_setfield(L, -2, "sk_free_window_list");
  lua_pushcfunction(L, lthunk_sk_string_list_free); lua_setfield(L, -2, "sk_string_list_free");
  lua_pushcfunction(L, lthunk_sk_launch_with_args); lua_setfield(L, -2, "sk_launch_with_args");
  lua_pushcfunction(L, lthunk_sk_launch_with_args_as_admin); lua_setfield(L, -2, "sk_launch_with_args_as_admin");
  lua_pushcfunction(L, lthunk_sk_launch_terminal_at_path); lua_setfield(L, -2, "sk_launch_terminal_at_path");
  lua_pushcfunction(L, lthunk_sk_launch_detached); lua_setfield(L, -2, "sk_launch_detached");
  lua_pushcfunction(L, lthunk_sk_terminate_process_by_name); lua_setfield(L, -2, "sk_terminate_process_by_name");
  lua_pushcfunction(L, lthunk_sk_process_running); lua_setfield(L, -2, "sk_process_running");
  lua_pushcfunction(L, lthunk_sk_get_feature_list); lua_setfield(L, -2, "sk_get_feature_list");
  lua_pushcfunction(L, lthunk_sk_free_feature_list); lua_setfield(L, -2, "sk_free_feature_list");
  lua_pushcfunction(L, lthunk_sk_init_all_hotkeys); lua_setfield(L, -2, "sk_init_all_hotkeys");
  lua_pushcfunction(L, lthunk_sk_send_hotkey); lua_setfield(L, -2, "sk_send_hotkey");
  lua_pushcfunction(L, lthunk_sk_hotkey_from_string); lua_setfield(L, -2, "sk_hotkey_from_string");
  lua_pushcfunction(L, lthunk_sk_hotkey_down); lua_setfield(L, -2, "sk_hotkey_down");
  lua_pushcfunction(L, lthunk_sk_launch_browser); lua_setfield(L, -2, "sk_launch_browser");
  lua_pushcfunction(L, lthunk_sk_query_browser); lua_setfield(L, -2, "sk_query_browser");
  lua_pushcfunction(L, lthunk_sk_focus_window_by_title); lua_setfield(L, -2, "sk_focus_window_by_title");
  lua_pushcfunction(L, lthunk_sk_close_active_window); lua_setfield(L, -2, "sk_close_active_window");
  lua_pushcfunction(L, lthunk_sk_dismiss_message); lua_setfield(L, -2, "sk_dismiss_message");
  lua_pushcfunction(L, lthunk_sk_message_active); lua_setfield(L, -2, "sk_message_active");
  lua_pushcfunction(L, lthunk_sk_toggle_fullscreen_active); lua_setfield(L, -2, "sk_toggle_fullscreen_active");
  lua_pushcfunction(L, lthunk_sk_move_active_window_to_monitor); lua_setfield(L, -2, "sk_move_active_window_to_monitor");
  lua_pushcfunction(L, lthunk_sk_move_active_window_left); lua_setfield(L, -2, "sk_move_active_window_left");
  lua_pushcfunction(L, lthunk_sk_move_active_window_up); lua_setfield(L, -2, "sk_move_active_window_up");
  lua_pushcfunction(L, lthunk_sk_move_active_window_down); lua_setfield(L, -2, "sk_move_active_window_down");
  lua_pushcfunction(L, lthunk_sk_move_active_window_right); lua_setfield(L, -2, "sk_move_active_window_right");
  lua_pushcfunction(L, lthunk_sk_focus_window_left); lua_setfield(L, -2, "sk_focus_window_left");
  lua_pushcfunction(L, lthunk_sk_focus_window_up); lua_setfield(L, -2, "sk_focus_window_up");
  lua_pushcfunction(L, lthunk_sk_focus_window_down); lua_setfield(L, -2, "sk_focus_window_down");
  lua_pushcfunction(L, lthunk_sk_focus_window_right); lua_setfield(L, -2, "sk_focus_window_right");
  lua_pushcfunction(L, lthunk_sk_toggle_minimized_active); lua_setfield(L, -2, "sk_toggle_minimized_active");
  lua_pushcfunction(L, lthunk_sk_toggle_maximize_active); lua_setfield(L, -2, "sk_toggle_maximize_active");
  lua_pushcfunction(L, lthunk_sk_toggle_always_on_top_active); lua_setfield(L, -2, "sk_toggle_always_on_top_active");
  lua_pushcfunction(L, lthunk_sk_raise_active_window); lua_setfield(L, -2, "sk_raise_active_window");
  lua_pushcfunction(L, lthunk_sk_lower_active_window); lua_setfield(L, -2, "sk_lower_active_window");
  lua_pushcfunction(L, lthunk_sk_center_active_window); lua_setfield(L, -2, "sk_center_active_window");
  lua_pushcfunction(L, lthunk_sk_snap_active_window); lua_setfield(L, -2, "sk_snap_active_window");
  lua_pushcfunction(L, lthunk_sk_get_active_window_pid); lua_setfield(L, -2, "sk_get_active_window_pid");
  lua_pushcfunction(L, lthunk_sk_switch_to_workspace); lua_setfield(L, -2, "sk_switch_to_workspace");
  lua_pushcfunction(L, lthunk_sk_move_active_window_to_workspace); lua_setfield(L, -2, "sk_move_active_window_to_workspace");
  lua_pushcfunction(L, lthunk_sk_set_active_window_opacity); lua_setfield(L, -2, "sk_set_active_window_opacity");
  lua_pushcfunction(L, lthunk_sk_close_windows_by_title); lua_setfield(L, -2, "sk_close_windows_by_title");
  lua_pushcfunction(L, lthunk_sk_close_windows_by_id); lua_setfield(L, -2, "sk_close_windows_by_id");
  lua_pushcfunction(L, lthunk_sk_reload_config); lua_setfield(L, -2, "sk_reload_config");
  lua_pushcfunction(L, lthunk_sk_save_config); lua_setfield(L, -2, "sk_save_config");
  lua_pushcfunction(L, lthunk_sk_refresh_ui); lua_setfield(L, -2, "sk_refresh_ui");
  lua_pushcfunction(L, lthunk_sk_request_refresh); lua_setfield(L, -2, "sk_request_refresh");
  lua_pushcfunction(L, lthunk_sk_commit_config); lua_setfield(L, -2, "sk_commit_config");
  lua_pushcfunction(L, lthunk_sk_set_theme); lua_setfield(L, -2, "sk_set_theme");
  lua_pushcfunction(L, lthunk_sk_theme_count); lua_setfield(L, -2, "sk_theme_count");
  lua_pushcfunction(L, lthunk_sk_get_screen_width); lua_setfield(L, -2, "sk_get_screen_width");
  lua_pushcfunction(L, lthunk_sk_get_screen_height); lua_setfield(L, -2, "sk_get_screen_height");
  lua_pushcfunction(L, lthunk_sk_get_monitor_geometry); lua_setfield(L, -2, "sk_get_monitor_geometry");
  lua_pushcfunction(L, lthunk_sk_string_starts_with); lua_setfield(L, -2, "sk_string_starts_with");
  lua_pushcfunction(L, lthunk_sk_string_starts_with_ci); lua_setfield(L, -2, "sk_string_starts_with_ci");
  lua_pushcfunction(L, lthunk_sk_string_ends_with); lua_setfield(L, -2, "sk_string_ends_with");
  lua_pushcfunction(L, lthunk_sk_string_equals_ci); lua_setfield(L, -2, "sk_string_equals_ci");
  lua_pushcfunction(L, lthunk_sk_string_split); lua_setfield(L, -2, "sk_string_split");
  lua_pushcfunction(L, lthunk_sk_is_selected); lua_setfield(L, -2, "sk_is_selected");
  lua_pushcfunction(L, lthunk_sk_set_edit_text); lua_setfield(L, -2, "sk_set_edit_text");
  lua_pushcfunction(L, lthunk_sk_set_edit_cursor); lua_setfield(L, -2, "sk_set_edit_cursor");
  lua_pushcfunction(L, lthunk_sk_get_edit_cursor); lua_setfield(L, -2, "sk_get_edit_cursor");
  lua_pushcfunction(L, lthunk_sk_replace_line_in_file); lua_setfield(L, -2, "sk_replace_line_in_file");
  lua_pushcfunction(L, lthunk_sk_image_from_window_title); lua_setfield(L, -2, "sk_image_from_window_title");
  lua_pushcfunction(L, lthunk_sk_image_from_window_id); lua_setfield(L, -2, "sk_image_from_window_id");
  lua_pushcfunction(L, lthunk_sk_image_free); lua_setfield(L, -2, "sk_image_free");
  lua_pushcfunction(L, lthunk_ski_get_image_size); lua_setfield(L, -2, "ski_get_image_size");
  lua_pushcfunction(L, lthunk_sk_editbox_set_text); lua_setfield(L, -2, "sk_editbox_set_text");
  lua_pushcfunction(L, lthunk_sk_editbox_set_active); lua_setfield(L, -2, "sk_editbox_set_active");
  lua_pushcfunction(L, lthunk_sk_checkbox_is_checked); lua_setfield(L, -2, "sk_checkbox_is_checked");
  lua_pushcfunction(L, lthunk_sk_checkbox_is_hovered); lua_setfield(L, -2, "sk_checkbox_is_hovered");
  lua_pushcfunction(L, lthunk_sk_checkbox_is_clicked); lua_setfield(L, -2, "sk_checkbox_is_clicked");
  lua_pushcfunction(L, lthunk_sk_button_is_checked); lua_setfield(L, -2, "sk_button_is_checked");
  lua_pushcfunction(L, lthunk_sk_button_is_hovered); lua_setfield(L, -2, "sk_button_is_hovered");
  lua_pushcfunction(L, lthunk_sk_button_is_clicked); lua_setfield(L, -2, "sk_button_is_clicked");
  lua_pushcfunction(L, lthunk_sk_slider_is_hovered); lua_setfield(L, -2, "sk_slider_is_hovered");
  lua_pushcfunction(L, lthunk_sk_slider_is_clicked); lua_setfield(L, -2, "sk_slider_is_clicked");
  lua_pushcfunction(L, lthunk_sk_slider_is_dragging); lua_setfield(L, -2, "sk_slider_is_dragging");
  lua_pushcfunction(L, lthunk_sk_slider_value); lua_setfield(L, -2, "sk_slider_value");
  lua_pushcfunction(L, lthunk_sk_slider_fraction); lua_setfield(L, -2, "sk_slider_fraction");
  lua_pushcfunction(L, lthunk_sk_slider_percent); lua_setfield(L, -2, "sk_slider_percent");
  lua_pushcfunction(L, lthunk_sk_slider_set_value); lua_setfield(L, -2, "sk_slider_set_value");
  lua_pushcfunction(L, lthunk_sk_focus_kind); lua_setfield(L, -2, "sk_focus_kind");
  lua_pushcfunction(L, lthunk_sk_focus_is); lua_setfield(L, -2, "sk_focus_is");
  lua_pushcfunction(L, lthunk_sk_focus_has_entries); lua_setfield(L, -2, "sk_focus_has_entries");
  lua_pushcfunction(L, lthunk_sk_focus_move_next); lua_setfield(L, -2, "sk_focus_move_next");
  lua_pushcfunction(L, lthunk_sk_focus_move_before); lua_setfield(L, -2, "sk_focus_move_before");
  lua_pushcfunction(L, lthunk_sk_focus_move_dir); lua_setfield(L, -2, "sk_focus_move_dir");
  lua_pushcfunction(L, lthunk_sk_activate_focused); lua_setfield(L, -2, "sk_activate_focused");
  lua_pushcfunction(L, lthunk_sk_focus_adjust); lua_setfield(L, -2, "sk_focus_adjust");
  lua_pushcfunction(L, lthunk_sk_set_focus); lua_setfield(L, -2, "sk_set_focus");
  lua_pushcfunction(L, lthunk_sk_focus_clear); lua_setfield(L, -2, "sk_focus_clear");
  lua_pushcfunction(L, lthunk_sk_listbox_next); lua_setfield(L, -2, "sk_listbox_next");
  lua_pushcfunction(L, lthunk_sk_listbox_end); lua_setfield(L, -2, "sk_listbox_end");
  lua_pushcfunction(L, lthunk_sk_draw_text_auto); lua_setfield(L, -2, "sk_draw_text_auto");
  lua_pushcfunction(L, lthunk_sk_place_editbox_auto); lua_setfield(L, -2, "sk_place_editbox_auto");
  lua_pushcfunction(L, lthunk_sk_draw_image_auto); lua_setfield(L, -2, "sk_draw_image_auto");
  lua_pushcfunction(L, lthunk_sk_monitor_count); lua_setfield(L, -2, "sk_monitor_count");
  lua_pushcfunction(L, lthunk_sk_monitor_at); lua_setfield(L, -2, "sk_monitor_at");
  lua_pushcfunction(L, lthunk_sk_widget_valid); lua_setfield(L, -2, "sk_widget_valid");
  lua_pushcfunction(L, lthunk_sk_widget_poll_key); lua_setfield(L, -2, "sk_widget_poll_key");
  lua_pushcfunction(L, lthunk_sk_get_widget_pos); lua_setfield(L, -2, "sk_get_widget_pos");
  lua_pushcfunction(L, lthunk_sk_widget_show); lua_setfield(L, -2, "sk_widget_show");
  lua_pushcfunction(L, lthunk_sk_widget_hide); lua_setfield(L, -2, "sk_widget_hide");
  lua_pushcfunction(L, lthunk_sk_widget_destroy); lua_setfield(L, -2, "sk_widget_destroy");
  lua_pushcfunction(L, lthunk_sk_rand_num_between); lua_setfield(L, -2, "sk_rand_num_between");
  lua_pushcfunction(L, lthunk_sk_get_text_width); lua_setfield(L, -2, "sk_get_text_width");
  lua_pushcfunction(L, lthunk_sk_get_text_height); lua_setfield(L, -2, "sk_get_text_height");
  lua_pushcfunction(L, lthunk_sk_color_lerp); lua_setfield(L, -2, "sk_color_lerp");
  lua_pushcfunction(L, lthunk_sk_set_draw_offset); lua_setfield(L, -2, "sk_set_draw_offset");
  lua_pushcfunction(L, lthunk_sk_get_draw_offset); lua_setfield(L, -2, "sk_get_draw_offset");
  lua_pushcfunction(L, lthunk_sk_scroll_end); lua_setfield(L, -2, "sk_scroll_end");
  lua_pushcfunction(L, lthunk_sk_http_download); lua_setfield(L, -2, "sk_http_download");
  lua_pushcfunction(L, lthunk_sk_http_get_async); lua_setfield(L, -2, "sk_http_get_async");
  lua_pushcfunction(L, lthunk_sk_parse_flags); lua_setfield(L, -2, "sk_parse_flags");
  lua_pushcfunction(L, lthunk_sk_is_flag_prefix); lua_setfield(L, -2, "sk_is_flag_prefix");
  lua_pushcfunction(L, lthunk_sk_next_flag); lua_setfield(L, -2, "sk_next_flag");
  lua_pushcfunction(L, lthunk_sk_free_flags); lua_setfield(L, -2, "sk_free_flags");
  lua_pushcfunction(L, lthunk_sk_timer_start); lua_setfield(L, -2, "sk_timer_start");
  lua_pushcfunction(L, lthunk_sk_timer_end); lua_setfield(L, -2, "sk_timer_end");
  lua_pushcfunction(L, lthunk_sk_profile_start); lua_setfield(L, -2, "sk_profile_start");
  lua_pushcfunction(L, lthunk_sk_profile_end); lua_setfield(L, -2, "sk_profile_end");
  lua_pushcfunction(L, lthunk_sk_run_after_time); lua_setfield(L, -2, "sk_run_after_time");
  lua_pushcfunction(L, lthunk_sk_string_contains); lua_setfield(L, -2, "sk_string_contains");
  lua_pushcfunction(L, lthunk_sk_string_contains_ci); lua_setfield(L, -2, "sk_string_contains_ci");
  lua_pushcfunction(L, lthunk_sk_is_root); lua_setfield(L, -2, "sk_is_root");
  lua_pushcfunction(L, lthunk_sk_run_every); lua_setfield(L, -2, "sk_run_every");
  lua_pushcfunction(L, lthunk_sk_hashmap_get); lua_setfield(L, -2, "sk_hashmap_get");
  lua_pushcfunction(L, lthunk_sk_hashmap_has); lua_setfield(L, -2, "sk_hashmap_has");
  lua_pushcfunction(L, lthunk_sk_hashmap_len); lua_setfield(L, -2, "sk_hashmap_len");
  lua_pushcfunction(L, lthunk_sk_get_all_window_titles); lua_setfield(L, -2, "sk_get_all_window_titles");
  lua_pushcfunction(L, lthunk_sk_get_all_window_ids); lua_setfield(L, -2, "sk_get_all_window_ids");
  lua_pushcfunction(L, lthunk_sk_is_window_open); lua_setfield(L, -2, "sk_is_window_open");
  lua_pushcfunction(L, lthunk_sk_get_pixel_color); lua_setfield(L, -2, "sk_get_pixel_color");
  lua_pushcfunction(L, lthunk_sk_color_matches); lua_setfield(L, -2, "sk_color_matches");
  lua_pushcfunction(L, lthunk_sk_wait_pixel); lua_setfield(L, -2, "sk_wait_pixel");
  lua_pushcfunction(L, lthunk_sk_move_to_pos); lua_setfield(L, -2, "sk_move_to_pos");
  lua_pushcfunction(L, lthunk_sk_click_at); lua_setfield(L, -2, "sk_click_at");
  lua_pushcfunction(L, lthunk_sk_left_click); lua_setfield(L, -2, "sk_left_click");
  lua_pushcfunction(L, lthunk_sk_right_click); lua_setfield(L, -2, "sk_right_click");
  lua_pushcfunction(L, lthunk_sk_emit_mouse_button); lua_setfield(L, -2, "sk_emit_mouse_button");
  lua_pushcfunction(L, lthunk_sk_emit_mouse_button_at); lua_setfield(L, -2, "sk_emit_mouse_button_at");
  lua_pushcfunction(L, lthunk_sk_click_button_at); lua_setfield(L, -2, "sk_click_button_at");
  lua_pushcfunction(L, lthunk_sk_mouse_scroll); lua_setfield(L, -2, "sk_mouse_scroll");
  lua_pushcfunction(L, lthunk_sk_emit_key); lua_setfield(L, -2, "sk_emit_key");
  lua_pushcfunction(L, lthunk_sk_move_to_pos_rel); lua_setfield(L, -2, "sk_move_to_pos_rel");
  lua_pushcfunction(L, lthunk_sk_click_at_rel); lua_setfield(L, -2, "sk_click_at_rel");
  lua_pushcfunction(L, lthunk_sk_emit_mouse_button_at_rel); lua_setfield(L, -2, "sk_emit_mouse_button_at_rel");
  lua_pushcfunction(L, lthunk_sk_click_button_at_rel); lua_setfield(L, -2, "sk_click_button_at_rel");
  lua_pushcfunction(L, lthunk_sk_type_text); lua_setfield(L, -2, "sk_type_text");
  lua_pushcfunction(L, lthunk_sk_sleep); lua_setfield(L, -2, "sk_sleep");
  lua_pushcfunction(L, lthunk_sk_is_key_down); lua_setfield(L, -2, "sk_is_key_down");
  lua_pushcfunction(L, lthunk_sk_is_key_pressed); lua_setfield(L, -2, "sk_is_key_pressed");
  lua_pushcfunction(L, lthunk_sk_is_key_released); lua_setfield(L, -2, "sk_is_key_released");
  lua_pushcfunction(L, lthunk_sk_ctrl_pressed); lua_setfield(L, -2, "sk_ctrl_pressed");
  lua_pushcfunction(L, lthunk_sk_shift_pressed); lua_setfield(L, -2, "sk_shift_pressed");
  lua_pushcfunction(L, lthunk_sk_alt_pressed); lua_setfield(L, -2, "sk_alt_pressed");
  lua_pushcfunction(L, lthunk_sk_super_pressed); lua_setfield(L, -2, "sk_super_pressed");
  lua_pushcfunction(L, lthunk_sk_on_key); lua_setfield(L, -2, "sk_on_key");
  lua_pushcfunction(L, lthunk_sk_get_key_pressed); lua_setfield(L, -2, "sk_get_key_pressed");
  lua_pushcfunction(L, lthunk_sk_get_char_pressed); lua_setfield(L, -2, "sk_get_char_pressed");
  lua_pushcfunction(L, lthunk_sk_poll_mouse); lua_setfield(L, -2, "sk_poll_mouse");
  lua_pushcfunction(L, lthunk_sk_poll_scroll); lua_setfield(L, -2, "sk_poll_scroll");
  lua_pushcfunction(L, lthunk_sk_scroll_queue_len); lua_setfield(L, -2, "sk_scroll_queue_len");
  lua_pushcfunction(L, lthunk_sk_clear_scroll_queue); lua_setfield(L, -2, "sk_clear_scroll_queue");
  lua_pushcfunction(L, lthunk_sk_mouse_queue_len); lua_setfield(L, -2, "sk_mouse_queue_len");
  lua_pushcfunction(L, lthunk_sk_mouse_queue_dropped); lua_setfield(L, -2, "sk_mouse_queue_dropped");
  lua_pushcfunction(L, lthunk_sk_clear_mouse_queue); lua_setfield(L, -2, "sk_clear_mouse_queue");
  lua_pushcfunction(L, lthunk_sk_get_cursor_pos); lua_setfield(L, -2, "sk_get_cursor_pos");
  lua_pushcfunction(L, lthunk_sk_get_cursor_pos_rel); lua_setfield(L, -2, "sk_get_cursor_pos_rel");
  lua_pushcfunction(L, lthunk_sk_mouse_pressed); lua_setfield(L, -2, "sk_mouse_pressed");
  lua_pushcfunction(L, lthunk_sk_poll_key); lua_setfield(L, -2, "sk_poll_key");
  lua_pushcfunction(L, lthunk_sk_key_queue_len); lua_setfield(L, -2, "sk_key_queue_len");
  lua_pushcfunction(L, lthunk_sk_key_queue_dropped); lua_setfield(L, -2, "sk_key_queue_dropped");
  lua_pushcfunction(L, lthunk_sk_clear_key_queue); lua_setfield(L, -2, "sk_clear_key_queue");
  lua_pushcfunction(L, lthunk_sk_config_set_setting); lua_setfield(L, -2, "sk_config_set_setting");
  lua_pushcfunction(L, lthunk_sk_config_set_setting_int); lua_setfield(L, -2, "sk_config_set_setting_int");
  lua_pushcfunction(L, lthunk_sk_config_set_setting_float); lua_setfield(L, -2, "sk_config_set_setting_float");
  lua_pushcfunction(L, lthunk_sk_config_get_setting_string); lua_setfield(L, -2, "sk_config_get_setting_string");
  lua_pushcfunction(L, lthunk_sk_config_get_setting_bool); lua_setfield(L, -2, "sk_config_get_setting_bool");
  lua_pushcfunction(L, lthunk_sk_config_get_setting_int); lua_setfield(L, -2, "sk_config_get_setting_int");
  lua_pushcfunction(L, lthunk_sk_config_get_setting_float); lua_setfield(L, -2, "sk_config_get_setting_float");
  lua_pushcfunction(L, lthunk_sk_config_get_setting_float_or); lua_setfield(L, -2, "sk_config_get_setting_float_or");
  lua_pushcfunction(L, lthunk_sk_parse_int); lua_setfield(L, -2, "sk_parse_int");
  lua_pushcfunction(L, lthunk_sk_parse_float); lua_setfield(L, -2, "sk_parse_float");
  lua_pushcfunction(L, lthunk_sk_config_has_setting); lua_setfield(L, -2, "sk_config_has_setting");
  lua_pushcfunction(L, lthunk_sk_config_setting_count); lua_setfield(L, -2, "sk_config_setting_count");
  lua_pushcfunction(L, lthunk_sk_settings_editbox_set); lua_setfield(L, -2, "sk_settings_editbox_set");
  lua_pushcfunction(L, lthunk_sk_settings_editbox_right_offset); lua_setfield(L, -2, "sk_settings_editbox_right_offset");
  lua_pushcfunction(L, lthunk_sk_register_alias); lua_setfield(L, -2, "sk_register_alias");
  lua_pushcfunction(L, lthunk_sk_remove_alias); lua_setfield(L, -2, "sk_remove_alias");
  lua_pushcfunction(L, lthunk_sk_config_init); lua_setfield(L, -2, "sk_config_init");
  lua_pushcfunction(L, lthunk_sk_config_set_alias); lua_setfield(L, -2, "sk_config_set_alias");
  lua_pushcfunction(L, lthunk_sk_config_clear_hotkey); lua_setfield(L, -2, "sk_config_clear_hotkey");
  lua_pushcfunction(L, lthunk_sk_config_bind_hotkey); lua_setfield(L, -2, "sk_config_bind_hotkey");
  lua_pushcfunction(L, lthunk_sk_config_hotkey_count); lua_setfield(L, -2, "sk_config_hotkey_count");
  lua_pushcfunction(L, lthunk_sk_config_clear_alias); lua_setfield(L, -2, "sk_config_clear_alias");
  lua_pushcfunction(L, lthunk_sk_config_alias_count); lua_setfield(L, -2, "sk_config_alias_count");
  lua_pushcfunction(L, lthunk_sk_config_quicklink_count); lua_setfield(L, -2, "sk_config_quicklink_count");
  lua_pushcfunction(L, lthunk_sk_config_set_quicklink); lua_setfield(L, -2, "sk_config_set_quicklink");
  lua_pushcfunction(L, lthunk_sk_config_feature_shortcut_count); lua_setfield(L, -2, "sk_config_feature_shortcut_count");
  lua_pushcfunction(L, lthunk_sk_config_set_feature_shortcut); lua_setfield(L, -2, "sk_config_set_feature_shortcut");
  lua_pushcfunction(L, lthunk_sk_run_feature_shortcut); lua_setfield(L, -2, "sk_run_feature_shortcut");
  lua_pushcfunction(L, lthunk_sk_cc_entry_count); lua_setfield(L, -2, "sk_cc_entry_count");
  lua_pushcfunction(L, lthunk_sk_settings_entry_count); lua_setfield(L, -2, "sk_settings_entry_count");
  lua_pushcfunction(L, lthunk_sk_settings_scroll_to_block); lua_setfield(L, -2, "sk_settings_scroll_to_block");
  lua_pushcfunction(L, lthunk_ski_add_dep); lua_setfield(L, -2, "ski_add_dep");
  lua_pushcfunction(L, lthunk_sk_unregister_hotkey); lua_setfield(L, -2, "sk_unregister_hotkey");
  lua_pushcfunction(L, lthunk_sk_disable_global_hotkeys); lua_setfield(L, -2, "sk_disable_global_hotkeys");
  lua_pushcfunction(L, lthunk_sk_enable_global_hotkeys); lua_setfield(L, -2, "sk_enable_global_hotkeys");
  lua_pushcfunction(L, lthunk_sk_hotkey_capture_begin); lua_setfield(L, -2, "sk_hotkey_capture_begin");
  lua_pushcfunction(L, lthunk_sk_hotkey_capture_poll); lua_setfield(L, -2, "sk_hotkey_capture_poll");
  lua_pushcfunction(L, lthunk_sk_hotkey_capture_active); lua_setfield(L, -2, "sk_hotkey_capture_active");
  lua_pushcfunction(L, lthunk_sk_hotkey_capture_end); lua_setfield(L, -2, "sk_hotkey_capture_end");
  lua_pushcfunction(L, lthunk_sk_hotkey_count); lua_setfield(L, -2, "sk_hotkey_count");
  lua_pushcfunction(L, lthunk_sk_hotkey_taken); lua_setfield(L, -2, "sk_hotkey_taken");
  lua_pushcfunction(L, lthunk_sk_register_deferred_init); lua_setfield(L, -2, "sk_register_deferred_init");
  lua_pushcfunction(L, lthunk_sk_replay_hotkey); lua_setfield(L, -2, "sk_replay_hotkey");
  lua_pushcfunction(L, lthunk_sk_execute_hotkey); lua_setfield(L, -2, "sk_execute_hotkey");
  lua_pushcfunction(L, lthunk_sk_unregister_snippet); lua_setfield(L, -2, "sk_unregister_snippet");
  lua_pushcfunction(L, lthunk_sk_reload_feature); lua_setfield(L, -2, "sk_reload_feature");
  lua_pushcfunction(L, lthunk_sk_reload_all_features); lua_setfield(L, -2, "sk_reload_all_features");
  lua_pushcfunction(L, lthunk_sk_get_pid); lua_setfield(L, -2, "sk_get_pid");
  lua_pushcfunction(L, lthunk_sk_cap_register_abi); lua_setfield(L, -2, "sk_cap_register_abi");
  lua_pushcfunction(L, lthunk_ski_cap_register); lua_setfield(L, -2, "ski_cap_register");
  lua_pushcfunction(L, lthunk_sk_cap_generation); lua_setfield(L, -2, "sk_cap_generation");
  lua_pushcfunction(L, lthunk_sk_cap_bind_declare); lua_setfield(L, -2, "sk_cap_bind_declare");
  lua_pushcfunction(L, lthunk_sk_register_event); lua_setfield(L, -2, "sk_register_event");
  lua_pushcfunction(L, lthunk_sk_unregister_event); lua_setfield(L, -2, "sk_unregister_event");
  lua_pushcfunction(L, lthunk_sk_register_key_interceptor); lua_setfield(L, -2, "sk_register_key_interceptor");
  lua_pushcfunction(L, lthunk_sk_close_window); lua_setfield(L, -2, "sk_close_window");
  lua_pushcfunction(L, lthunk_sk_get_window_bounds); lua_setfield(L, -2, "sk_get_window_bounds");
}

#endif
