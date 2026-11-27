#ifndef SKAPI_LUA_GENERATED_H
#define SKAPI_LUA_GENERATED_H

#include "core/skapi.h"
#include "core/lua_struct.h"
#include <string.h>

static const LuaFieldDesc lf_SkSize[] = {
    {"w", LSF_INT, offsetof(SkSize, w), 0},
    {"h", LSF_INT, offsetof(SkSize, h), 0},
};
static const LuaStructDesc lsd_SkSize = {"SkSize", sizeof(SkSize), lf_SkSize, (int)(sizeof(lf_SkSize)/sizeof(lf_SkSize[0]))};
static const LuaFieldDesc lf_SkFeature[] = {
    {"name", LSF_STR, offsetof(SkFeature, name), 0},
    {"callback", LSF_CB, offsetof(SkFeature, callback), 0},
    {"icon", LSF_STR, offsetof(SkFeature, icon), 0},
};
static const LuaStructDesc lsd_SkFeature = {"SkFeature", sizeof(SkFeature), lf_SkFeature, (int)(sizeof(lf_SkFeature)/sizeof(lf_SkFeature[0]))};
static const LuaFieldDesc lf_SkFeatureList[] = {
    {"count", LSF_INT, offsetof(SkFeatureList, count), 0},
};
static const LuaStructDesc lsd_SkFeatureList = {"SkFeatureList", sizeof(SkFeatureList), lf_SkFeatureList, (int)(sizeof(lf_SkFeatureList)/sizeof(lf_SkFeatureList[0]))};
static const LuaFieldDesc lf_SkMonitorInfo[] = {
    {"index", LSF_INT, offsetof(SkMonitorInfo, index), 0},
    {"h", LSF_INT, offsetof(SkMonitorInfo, h), 0},
    {"is_primary", LSF_BOOL, offsetof(SkMonitorInfo, is_primary), 0},
};
static const LuaStructDesc lsd_SkMonitorInfo = {"SkMonitorInfo", sizeof(SkMonitorInfo), lf_SkMonitorInfo, (int)(sizeof(lf_SkMonitorInfo)/sizeof(lf_SkMonitorInfo[0]))};
static const LuaFieldDesc lf_SkDirScanSpec[] = {
    {"dir_path", LSF_STR, offsetof(SkDirScanSpec, dir_path), 0},
    {"allowed_extensions", LSF_STR, offsetof(SkDirScanSpec, allowed_extensions), 0},
    {"names_to_ignore", LSF_STR, offsetof(SkDirScanSpec, names_to_ignore), 0},
    {"strip_extension", LSF_BOOL, offsetof(SkDirScanSpec, strip_extension), 0},
    {"max_depth", LSF_INT, offsetof(SkDirScanSpec, max_depth), 0},
};
static const LuaStructDesc lsd_SkDirScanSpec = {"SkDirScanSpec", sizeof(SkDirScanSpec), lf_SkDirScanSpec, (int)(sizeof(lf_SkDirScanSpec)/sizeof(lf_SkDirScanSpec[0]))};
static const LuaFieldDesc lf_SkPopulateListboxSpec[] = {
    {"filter", LSF_STR, offsetof(SkPopulateListboxSpec, filter), 0},
    {"count", LSF_INT, offsetof(SkPopulateListboxSpec, count), 0},
};
static const LuaStructDesc lsd_SkPopulateListboxSpec = {"SkPopulateListboxSpec", sizeof(SkPopulateListboxSpec), lf_SkPopulateListboxSpec, (int)(sizeof(lf_SkPopulateListboxSpec)/sizeof(lf_SkPopulateListboxSpec[0]))};
static const LuaFieldDesc lf_SkEditPopupSpec[] = {
    {"initial_text", LSF_STR, offsetof(SkEditPopupSpec, initial_text), 0},
    {"x", LSF_INT, offsetof(SkEditPopupSpec, x), 0},
    {"y", LSF_INT, offsetof(SkEditPopupSpec, y), 0},
    {"w", LSF_INT, offsetof(SkEditPopupSpec, w), 0},
    {"h", LSF_INT, offsetof(SkEditPopupSpec, h), 0},
};
static const LuaStructDesc lsd_SkEditPopupSpec = {"SkEditPopupSpec", sizeof(SkEditPopupSpec), lf_SkEditPopupSpec, (int)(sizeof(lf_SkEditPopupSpec)/sizeof(lf_SkEditPopupSpec[0]))};
static const LuaFieldDesc lf_SkMessageSpec[] = {
    {"message", LSF_STR, offsetof(SkMessageSpec, message), 0},
    {"slot", LSF_INT, offsetof(SkMessageSpec, slot), 0},
    {"x", LSF_INT, offsetof(SkMessageSpec, x), 0},
    {"y", LSF_INT, offsetof(SkMessageSpec, y), 0},
    {"w", LSF_INT, offsetof(SkMessageSpec, w), 0},
    {"h", LSF_INT, offsetof(SkMessageSpec, h), 0},
    {"timeout", LSF_DOUBLE, offsetof(SkMessageSpec, timeout), 0},
    {"color_text", LSF_INT, offsetof(SkMessageSpec, color_text), 0},
    {"hover_key", LSF_STR, offsetof(SkMessageSpec, hover_key), 0},
    {"render", LSF_CB, offsetof(SkMessageSpec, render), 1},
    {"no_bg", LSF_BOOL, offsetof(SkMessageSpec, no_bg), 0},
    {"color_bg", LSF_INT, offsetof(SkMessageSpec, color_bg), 0},
    {"name", LSF_STR, offsetof(SkMessageSpec, name), 0},
    {"close_on_keypress", LSF_BOOL, offsetof(SkMessageSpec, close_on_keypress), 0},
    {"rounded", LSF_BOOL, offsetof(SkMessageSpec, rounded), 0},
    {"radius", LSF_INT, offsetof(SkMessageSpec, radius), 0},
};
static const LuaStructDesc lsd_SkMessageSpec = {"SkMessageSpec", sizeof(SkMessageSpec), lf_SkMessageSpec, (int)(sizeof(lf_SkMessageSpec)/sizeof(lf_SkMessageSpec[0]))};
static const LuaFieldDesc lf_SkHoverPopupCtx[] = {
    {"render", LSF_CB, offsetof(SkHoverPopupCtx, render), 1},
    {"w", LSF_INT, offsetof(SkHoverPopupCtx, w), 0},
    {"h", LSF_INT, offsetof(SkHoverPopupCtx, h), 0},
};
static const LuaStructDesc lsd_SkHoverPopupCtx = {"SkHoverPopupCtx", sizeof(SkHoverPopupCtx), lf_SkHoverPopupCtx, (int)(sizeof(lf_SkHoverPopupCtx)/sizeof(lf_SkHoverPopupCtx[0]))};
static const LuaFieldDesc lf_SkTextSpec[] = {
    {"text", LSF_STR, offsetof(SkTextSpec, text), 0},
    {"x", LSF_INT, offsetof(SkTextSpec, x), 0},
    {"y", LSF_INT, offsetof(SkTextSpec, y), 0},
    {"color", LSF_INT, offsetof(SkTextSpec, color), 0},
    {"font_size", LSF_INT, offsetof(SkTextSpec, font_size), 0},
    {"align", LSF_STR, offsetof(SkTextSpec, align), 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextSpec, no_hscroll), 0},
    {"angle", LSF_FLOAT, offsetof(SkTextSpec, angle), 0},
};
static const LuaStructDesc lsd_SkTextSpec = {"SkTextSpec", sizeof(SkTextSpec), lf_SkTextSpec, (int)(sizeof(lf_SkTextSpec)/sizeof(lf_SkTextSpec[0]))};
static const LuaFieldDesc lf_SkTextCenteredSpec[] = {
    {"x", LSF_INT, offsetof(SkTextCenteredSpec, x), 0},
    {"y", LSF_INT, offsetof(SkTextCenteredSpec, y), 0},
    {"w", LSF_INT, offsetof(SkTextCenteredSpec, w), 0},
    {"h", LSF_INT, offsetof(SkTextCenteredSpec, h), 0},
    {"text", LSF_STR, offsetof(SkTextCenteredSpec, text), 0},
    {"color", LSF_INT, offsetof(SkTextCenteredSpec, color), 0},
    {"font_size", LSF_INT, offsetof(SkTextCenteredSpec, font_size), 0},
};
static const LuaStructDesc lsd_SkTextCenteredSpec = {"SkTextCenteredSpec", sizeof(SkTextCenteredSpec), lf_SkTextCenteredSpec, (int)(sizeof(lf_SkTextCenteredSpec)/sizeof(lf_SkTextCenteredSpec[0]))};
static const LuaFieldDesc lf_SkMeasureSpec[] = {
    {"x", LSF_INT, offsetof(SkMeasureSpec, x), 0},
    {"y", LSF_INT, offsetof(SkMeasureSpec, y), 0},
    {"max_width", LSF_INT, offsetof(SkMeasureSpec, max_width), 0},
    {"line_height", LSF_INT, offsetof(SkMeasureSpec, line_height), 0},
    {"text", LSF_STR, offsetof(SkMeasureSpec, text), 0},
    {"font_size", LSF_INT, offsetof(SkMeasureSpec, font_size), 0},
};
static const LuaStructDesc lsd_SkMeasureSpec = {"SkMeasureSpec", sizeof(SkMeasureSpec), lf_SkMeasureSpec, (int)(sizeof(lf_SkMeasureSpec)/sizeof(lf_SkMeasureSpec[0]))};
static const LuaFieldDesc lf_SkTextBoundsSpec[] = {
    {"x", LSF_INT, offsetof(SkTextBoundsSpec, x), 0},
    {"y", LSF_INT, offsetof(SkTextBoundsSpec, y), 0},
    {"text", LSF_STR, offsetof(SkTextBoundsSpec, text), 0},
    {"font_size", LSF_INT, offsetof(SkTextBoundsSpec, font_size), 0},
    {"align", LSF_STR, offsetof(SkTextBoundsSpec, align), 0},
};
static const LuaStructDesc lsd_SkTextBoundsSpec = {"SkTextBoundsSpec", sizeof(SkTextBoundsSpec), lf_SkTextBoundsSpec, (int)(sizeof(lf_SkTextBoundsSpec)/sizeof(lf_SkTextBoundsSpec[0]))};
static const LuaFieldDesc lf_SkImageSpec[] = {
    {"path", LSF_STR, offsetof(SkImageSpec, path), 0},
    {"x", LSF_INT, offsetof(SkImageSpec, x), 0},
    {"y", LSF_INT, offsetof(SkImageSpec, y), 0},
    {"w", LSF_INT, offsetof(SkImageSpec, w), 0},
    {"h", LSF_INT, offsetof(SkImageSpec, h), 0},
};
static const LuaStructDesc lsd_SkImageSpec = {"SkImageSpec", sizeof(SkImageSpec), lf_SkImageSpec, (int)(sizeof(lf_SkImageSpec)/sizeof(lf_SkImageSpec[0]))};
static const LuaFieldDesc lf_SkTextSegment[] = {
    {"text", LSF_STR, offsetof(SkTextSegment, text), 0},
    {"color", LSF_INT, offsetof(SkTextSegment, color), 0},
};
static const LuaStructDesc lsd_SkTextSegment = {"SkTextSegment", sizeof(SkTextSegment), lf_SkTextSegment, (int)(sizeof(lf_SkTextSegment)/sizeof(lf_SkTextSegment[0]))};
static const LuaFieldDesc lf_SkTextSegmentsSpec[] = {
    {"count", LSF_INT, offsetof(SkTextSegmentsSpec, count), 0},
    {"x", LSF_INT, offsetof(SkTextSegmentsSpec, x), 0},
    {"y", LSF_INT, offsetof(SkTextSegmentsSpec, y), 0},
    {"color", LSF_INT, offsetof(SkTextSegmentsSpec, color), 0},
    {"font_size", LSF_INT, offsetof(SkTextSegmentsSpec, font_size), 0},
    {"align", LSF_STR, offsetof(SkTextSegmentsSpec, align), 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextSegmentsSpec, no_hscroll), 0},
    {"angle", LSF_FLOAT, offsetof(SkTextSegmentsSpec, angle), 0},
};
static const LuaStructDesc lsd_SkTextSegmentsSpec = {"SkTextSegmentsSpec", sizeof(SkTextSegmentsSpec), lf_SkTextSegmentsSpec, (int)(sizeof(lf_SkTextSegmentsSpec)/sizeof(lf_SkTextSegmentsSpec[0]))};
static const LuaFieldDesc lf_SkTextWrappedSpec[] = {
    {"text", LSF_STR, offsetof(SkTextWrappedSpec, text), 0},
    {"count", LSF_INT, offsetof(SkTextWrappedSpec, count), 0},
    {"x", LSF_INT, offsetof(SkTextWrappedSpec, x), 0},
    {"y", LSF_INT, offsetof(SkTextWrappedSpec, y), 0},
    {"max_width", LSF_INT, offsetof(SkTextWrappedSpec, max_width), 0},
    {"line_height", LSF_INT, offsetof(SkTextWrappedSpec, line_height), 0},
    {"color", LSF_INT, offsetof(SkTextWrappedSpec, color), 0},
    {"font_size", LSF_INT, offsetof(SkTextWrappedSpec, font_size), 0},
    {"align", LSF_STR, offsetof(SkTextWrappedSpec, align), 0},
    {"no_hscroll", LSF_BOOL, offsetof(SkTextWrappedSpec, no_hscroll), 0},
    {"angle", LSF_FLOAT, offsetof(SkTextWrappedSpec, angle), 0},
    {"measure_only", LSF_BOOL, offsetof(SkTextWrappedSpec, measure_only), 0},
};
static const LuaStructDesc lsd_SkTextWrappedSpec = {"SkTextWrappedSpec", sizeof(SkTextWrappedSpec), lf_SkTextWrappedSpec, (int)(sizeof(lf_SkTextWrappedSpec)/sizeof(lf_SkTextWrappedSpec[0]))};
static const LuaFieldDesc lf_SkScrollSpec[] = {
    {"name", LSF_STR, offsetof(SkScrollSpec, name), 0},
    {"h", LSF_INT, offsetof(SkScrollSpec, h), 0},
    {"content_h", LSF_INT, offsetof(SkScrollSpec, content_h), 0},
    {"off_x", LSF_INT, offsetof(SkScrollSpec, off_x), 0},
    {"off_y", LSF_INT, offsetof(SkScrollSpec, off_y), 0},
    {"wheel_px", LSF_INT, offsetof(SkScrollSpec, wheel_px), 0},
};
static const LuaStructDesc lsd_SkScrollSpec = {"SkScrollSpec", sizeof(SkScrollSpec), lf_SkScrollSpec, (int)(sizeof(lf_SkScrollSpec)/sizeof(lf_SkScrollSpec[0]))};
static const LuaFieldDesc lf_SkFlag[] = {
    {"has_value", LSF_BOOL, offsetof(SkFlag, has_value), 0},
};
static const LuaStructDesc lsd_SkFlag = {"SkFlag", sizeof(SkFlag), lf_SkFlag, (int)(sizeof(lf_SkFlag)/sizeof(lf_SkFlag[0]))};
static const LuaFieldDesc lf_SkFlagList[] = {
    {"count", LSF_INT, offsetof(SkFlagList, count), 0},
};
static const LuaStructDesc lsd_SkFlagList = {"SkFlagList", sizeof(SkFlagList), lf_SkFlagList, (int)(sizeof(lf_SkFlagList)/sizeof(lf_SkFlagList[0]))};
static const LuaFieldDesc lf_SkHashMap[] = {
    {"key", LSF_STR, offsetof(SkHashMap, key), 0},
    {"value", LSF_INT, offsetof(SkHashMap, value), 0},
};
static const LuaStructDesc lsd_SkHashMap = {"SkHashMap", sizeof(SkHashMap), lf_SkHashMap, (int)(sizeof(lf_SkHashMap)/sizeof(lf_SkHashMap[0]))};
static const LuaFieldDesc lf_SkKeyEvent[] = {
    {"vk", LSF_INT, offsetof(SkKeyEvent, vk), 0},
    {"scancode", LSF_INT, offsetof(SkKeyEvent, scancode), 0},
    {"mods", LSF_INT, offsetof(SkKeyEvent, mods), 0},
    {"down", LSF_BOOL, offsetof(SkKeyEvent, down), 0},
    {"repeat", LSF_BOOL, offsetof(SkKeyEvent, repeat), 0},
    {"t_us", LSF_INT, offsetof(SkKeyEvent, t_us), 0},
};
static const LuaStructDesc lsd_SkKeyEvent = {"SkKeyEvent", sizeof(SkKeyEvent), lf_SkKeyEvent, (int)(sizeof(lf_SkKeyEvent)/sizeof(lf_SkKeyEvent[0]))};
static const LuaFieldDesc lf_SkMouseEvent[] = {
    {"button", LSF_INT, offsetof(SkMouseEvent, button), 0},
    {"x", LSF_INT, offsetof(SkMouseEvent, x), 0},
    {"y", LSF_INT, offsetof(SkMouseEvent, y), 0},
    {"mods", LSF_INT, offsetof(SkMouseEvent, mods), 0},
    {"down", LSF_BOOL, offsetof(SkMouseEvent, down), 0},
    {"repeat", LSF_BOOL, offsetof(SkMouseEvent, repeat), 0},
    {"t_us", LSF_INT, offsetof(SkMouseEvent, t_us), 0},
};
static const LuaStructDesc lsd_SkMouseEvent = {"SkMouseEvent", sizeof(SkMouseEvent), lf_SkMouseEvent, (int)(sizeof(lf_SkMouseEvent)/sizeof(lf_SkMouseEvent[0]))};
static const LuaFieldDesc lf_SkScrollEvent[] = {
    {"x", LSF_INT, offsetof(SkScrollEvent, x), 0},
    {"y", LSF_INT, offsetof(SkScrollEvent, y), 0},
    {"dx", LSF_INT, offsetof(SkScrollEvent, dx), 0},
    {"dy", LSF_INT, offsetof(SkScrollEvent, dy), 0},
    {"mods", LSF_INT, offsetof(SkScrollEvent, mods), 0},
    {"t_us", LSF_INT, offsetof(SkScrollEvent, t_us), 0},
};
static const LuaStructDesc lsd_SkScrollEvent = {"SkScrollEvent", sizeof(SkScrollEvent), lf_SkScrollEvent, (int)(sizeof(lf_SkScrollEvent)/sizeof(lf_SkScrollEvent[0]))};
static const LuaFieldDesc lf_SkConfigHotkeySpec[] = {
    {"name", LSF_STR, offsetof(SkConfigHotkeySpec, name), 0},
    {"hotkey", LSF_STR, offsetof(SkConfigHotkeySpec, hotkey), 0},
    {"cb", LSF_CB, offsetof(SkConfigHotkeySpec, cb), 1},
};
static const LuaStructDesc lsd_SkConfigHotkeySpec = {"SkConfigHotkeySpec", sizeof(SkConfigHotkeySpec), lf_SkConfigHotkeySpec, (int)(sizeof(lf_SkConfigHotkeySpec)/sizeof(lf_SkConfigHotkeySpec[0]))};
static const LuaFieldDesc lf_SkHotkeySpec[] = {
    {"hotkey", LSF_STR, offsetof(SkHotkeySpec, hotkey), 0},
    {"cb", LSF_CB, offsetof(SkHotkeySpec, cb), 0},
    {"should_swallow", LSF_CB, offsetof(SkHotkeySpec, should_swallow), 3},
    {"swallow", LSF_BOOL, offsetof(SkHotkeySpec, swallow), 0},
};
static const LuaStructDesc lsd_SkHotkeySpec = {"SkHotkeySpec", sizeof(SkHotkeySpec), lf_SkHotkeySpec, (int)(sizeof(lf_SkHotkeySpec)/sizeof(lf_SkHotkeySpec[0]))};
static const LuaFieldDesc lf_SkConfigHotkeyRegisterSpec[] = {
    {"name", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, name), 0},
    {"key", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, key), 0},
    {"default_hotkey", LSF_STR, offsetof(SkConfigHotkeyRegisterSpec, default_hotkey), 0},
    {"cb", LSF_CB, offsetof(SkConfigHotkeyRegisterSpec, cb), 0},
    {"should_swallow", LSF_CB, offsetof(SkConfigHotkeyRegisterSpec, should_swallow), 3},
};
static const LuaStructDesc lsd_SkConfigHotkeyRegisterSpec = {"SkConfigHotkeyRegisterSpec", sizeof(SkConfigHotkeyRegisterSpec), lf_SkConfigHotkeyRegisterSpec, (int)(sizeof(lf_SkConfigHotkeyRegisterSpec)/sizeof(lf_SkConfigHotkeyRegisterSpec[0]))};
static const LuaFieldDesc lf_SkRemappingSpec[] = {
    {"src", LSF_STR, offsetof(SkRemappingSpec, src), 0},
    {"dst", LSF_STR, offsetof(SkRemappingSpec, dst), 0},
};
static const LuaStructDesc lsd_SkRemappingSpec = {"SkRemappingSpec", sizeof(SkRemappingSpec), lf_SkRemappingSpec, (int)(sizeof(lf_SkRemappingSpec)/sizeof(lf_SkRemappingSpec[0]))};
static const LuaFieldDesc lf_SkSnippetSpec[] = {
    {"trigger", LSF_STR, offsetof(SkSnippetSpec, trigger), 0},
    {"text", LSF_STR, offsetof(SkSnippetSpec, text), 0},
    {"fn", LSF_CB, offsetof(SkSnippetSpec, fn), 0},
};
static const LuaStructDesc lsd_SkSnippetSpec = {"SkSnippetSpec", sizeof(SkSnippetSpec), lf_SkSnippetSpec, (int)(sizeof(lf_SkSnippetSpec)/sizeof(lf_SkSnippetSpec[0]))};
static const LuaFieldDesc lf_Pos[] = {
    {"x", LSF_INT, offsetof(Pos, x), 0},
    {"y", LSF_INT, offsetof(Pos, y), 0},
};
static const LuaStructDesc lsd_Pos = {"Pos", sizeof(Pos), lf_Pos, (int)(sizeof(lf_Pos)/sizeof(lf_Pos[0]))};
static const LuaFieldDesc lf_Metrics[] = {
    {"edit_cursor_width", LSF_INT, offsetof(Metrics, edit_cursor_width), 0},
    {"edit_cursor_height", LSF_INT, offsetof(Metrics, edit_cursor_height), 0},
    {"editbox_padding", LSF_INT, offsetof(Metrics, editbox_padding), 0},
    {"listbox_padding", LSF_INT, offsetof(Metrics, listbox_padding), 0},
    {"listbox_item_height", LSF_INT, offsetof(Metrics, listbox_item_height), 0},
};
static const LuaStructDesc lsd_Metrics = {"Metrics", sizeof(Metrics), lf_Metrics, (int)(sizeof(lf_Metrics)/sizeof(lf_Metrics[0]))};
static const LuaFieldDesc lf_EditBox[] = {
    {"cursor", LSF_INT, offsetof(EditBox, cursor), 0},
    {"sel_start", LSF_INT, offsetof(EditBox, sel_start), 0},
    {"sel_end", LSF_INT, offsetof(EditBox, sel_end), 0},
    {"scroll_offset", LSF_INT, offsetof(EditBox, scroll_offset), 0},
    {"placeholder", LSF_STR, offsetof(EditBox, placeholder), 0},
};
static const LuaStructDesc lsd_EditBox = {"EditBox", sizeof(EditBox), lf_EditBox, (int)(sizeof(lf_EditBox)/sizeof(lf_EditBox[0]))};
static const LuaFieldDesc lf_EditboxWrapCache[] = {
    {"key", LSF_INT, offsetof(EditboxWrapCache, key), 0},
    {"len", LSF_INT, offsetof(EditboxWrapCache, len), 0},
    {"line_h", LSF_INT, offsetof(EditboxWrapCache, line_h), 0},
    {"max_width", LSF_INT, offsetof(EditboxWrapCache, max_width), 0},
    {"count", LSF_INT, offsetof(EditboxWrapCache, count), 0},
    {"positions_cap", LSF_INT, offsetof(EditboxWrapCache, positions_cap), 0},
};
static const LuaStructDesc lsd_EditboxWrapCache = {"EditboxWrapCache", sizeof(EditboxWrapCache), lf_EditboxWrapCache, (int)(sizeof(lf_EditboxWrapCache)/sizeof(lf_EditboxWrapCache[0]))};
static const LuaFieldDesc lf_PromptEditbox[] = {
    {"h", LSF_INT, offsetof(PromptEditbox, h), 0},
    {"placed", LSF_BOOL, offsetof(PromptEditbox, placed), 0},
    {"primary", LSF_BOOL, offsetof(PromptEditbox, primary), 0},
    {"no_tab", LSF_BOOL, offsetof(PromptEditbox, no_tab), 0},
    {"wrapped", LSF_BOOL, offsetof(PromptEditbox, wrapped), 0},
    {"font_size", LSF_INT, offsetof(PromptEditbox, font_size), 0},
};
static const LuaStructDesc lsd_PromptEditbox = {"PromptEditbox", sizeof(PromptEditbox), lf_PromptEditbox, (int)(sizeof(lf_PromptEditbox)/sizeof(lf_PromptEditbox[0]))};
static const LuaFieldDesc lf_PromptSpec[] = {
    {"title", LSF_STR, offsetof(PromptSpec, title), 0},
    {"refresh", LSF_CB, offsetof(PromptSpec, refresh), 1},
    {"enter", LSF_CB, offsetof(PromptSpec, enter), 2},
    {"ctrl_enter", LSF_CB, offsetof(PromptSpec, ctrl_enter), 2},
    {"shift_enter", LSF_CB, offsetof(PromptSpec, shift_enter), 2},
    {"ctrl_shift_enter", LSF_CB, offsetof(PromptSpec, ctrl_shift_enter), 2},
    {"ctrl_alt_enter", LSF_CB, offsetof(PromptSpec, ctrl_alt_enter), 2},
    {"shift_alt_enter", LSF_CB, offsetof(PromptSpec, shift_alt_enter), 2},
    {"render", LSF_CB, offsetof(PromptSpec, render), 1},
    {"edit", LSF_CB, offsetof(PromptSpec, edit), 1},
    {"help", LSF_CB, offsetof(PromptSpec, help), 0},
    {"on_close", LSF_CB, offsetof(PromptSpec, on_close), 0},
    {"auto_scale", LSF_BOOL, offsetof(PromptSpec, auto_scale), 0},
    {"no_bg", LSF_BOOL, offsetof(PromptSpec, no_bg), 0},
    {"password_mode", LSF_BOOL, offsetof(PromptSpec, password_mode), 0},
    {"bg_color", LSF_INT, offsetof(PromptSpec, bg_color), 0},
    {"w", LSF_INT, offsetof(PromptSpec, w), 0},
    {"h", LSF_INT, offsetof(PromptSpec, h), 0},
    {"x", LSF_INT, offsetof(PromptSpec, x), 0},
    {"y", LSF_INT, offsetof(PromptSpec, y), 0},
    {"subwindow", LSF_BOOL, offsetof(PromptSpec, subwindow), 0},
    {"no_separators", LSF_BOOL, offsetof(PromptSpec, no_separators), 0},
    {"rounded", LSF_BOOL, offsetof(PromptSpec, rounded), 0},
    {"radius", LSF_INT, offsetof(PromptSpec, radius), 0},
};
static const LuaStructDesc lsd_PromptSpec = {"PromptSpec", sizeof(PromptSpec), lf_PromptSpec, (int)(sizeof(lf_PromptSpec)/sizeof(lf_PromptSpec[0]))};
static const LuaFieldDesc lf_PopupSpec[] = {
    {"initial_text", LSF_STR, offsetof(PopupSpec, initial_text), 0},
    {"placeholder", LSF_STR, offsetof(PopupSpec, placeholder), 0},
    {"title", LSF_STR, offsetof(PopupSpec, title), 0},
    {"password_mode", LSF_BOOL, offsetof(PopupSpec, password_mode), 0},
    {"x", LSF_INT, offsetof(PopupSpec, x), 0},
    {"y", LSF_INT, offsetof(PopupSpec, y), 0},
    {"w", LSF_INT, offsetof(PopupSpec, w), 0},
    {"h", LSF_INT, offsetof(PopupSpec, h), 0},
};
static const LuaStructDesc lsd_PopupSpec = {"PopupSpec", sizeof(PopupSpec), lf_PopupSpec, (int)(sizeof(lf_PopupSpec)/sizeof(lf_PopupSpec[0]))};
static const LuaFieldDesc lf_SortOption[] = {
    {"name", LSF_STR, offsetof(SortOption, name), 0},
};
static const LuaStructDesc lsd_SortOption = {"SortOption", sizeof(SortOption), lf_SortOption, (int)(sizeof(lf_SortOption)/sizeof(lf_SortOption[0]))};
static const LuaFieldDesc lf_WindowLayout[] = {
    {"h", LSF_INT, offsetof(WindowLayout, h), 0},
    {"item_height", LSF_INT, offsetof(WindowLayout, item_height), 0},
};
static const LuaStructDesc lsd_WindowLayout = {"WindowLayout", sizeof(WindowLayout), lf_WindowLayout, (int)(sizeof(lf_WindowLayout)/sizeof(lf_WindowLayout[0]))};
static const LuaFieldDesc lf_WindowData[] = {
};
static const LuaStructDesc lsd_WindowData = {"WindowData", sizeof(WindowData), lf_WindowData, (int)(sizeof(lf_WindowData)/sizeof(lf_WindowData[0]))};
static const LuaFieldDesc lf_RgbColor[] = {
    {"b", LSF_INT, offsetof(RgbColor, b), 0},
};
static const LuaStructDesc lsd_RgbColor = {"RgbColor", sizeof(RgbColor), lf_RgbColor, (int)(sizeof(lf_RgbColor)/sizeof(lf_RgbColor[0]))};
static const LuaFieldDesc lf_DrawRect[] = {
    {"h", LSF_INT, offsetof(DrawRect, h), 0},
    {"color", LSF_INT, offsetof(DrawRect, color), 0},
    {"thickness", LSF_FLOAT, offsetof(DrawRect, thickness), 0},
    {"transparent", LSF_BOOL, offsetof(DrawRect, transparent), 0},
    {"filled", LSF_BOOL, offsetof(DrawRect, filled), 0},
    {"radius", LSF_INT, offsetof(DrawRect, radius), 0},
    {"rounded", LSF_BOOL, offsetof(DrawRect, rounded), 0},
    {"angle", LSF_FLOAT, offsetof(DrawRect, angle), 0},
    {"border", LSF_BOOL, offsetof(DrawRect, border), 0},
};
static const LuaStructDesc lsd_DrawRect = {"DrawRect", sizeof(DrawRect), lf_DrawRect, (int)(sizeof(lf_DrawRect)/sizeof(lf_DrawRect[0]))};
static const LuaFieldDesc lf_Line[] = {
    {"y2", LSF_INT, offsetof(Line, y2), 0},
    {"color", LSF_INT, offsetof(Line, color), 0},
    {"thickness", LSF_FLOAT, offsetof(Line, thickness), 0},
};
static const LuaStructDesc lsd_Line = {"Line", sizeof(Line), lf_Line, (int)(sizeof(lf_Line)/sizeof(lf_Line[0]))};
static const LuaFieldDesc lf_Circle[] = {
    {"radius", LSF_INT, offsetof(Circle, radius), 0},
    {"color", LSF_INT, offsetof(Circle, color), 0},
    {"thickness", LSF_FLOAT, offsetof(Circle, thickness), 0},
};
static const LuaStructDesc lsd_Circle = {"Circle", sizeof(Circle), lf_Circle, (int)(sizeof(lf_Circle)/sizeof(lf_Circle[0]))};
static const LuaFieldDesc lf_WindowList[] = {
    {"windows", LSF_STR, offsetof(WindowList, windows), 0},
    {"count", LSF_INT, offsetof(WindowList, count), 0},
};
static const LuaStructDesc lsd_WindowList = {"WindowList", sizeof(WindowList), lf_WindowList, (int)(sizeof(lf_WindowList)/sizeof(lf_WindowList[0]))};
static const LuaFieldDesc lf_Timer[] = {
};
static const LuaStructDesc lsd_Timer = {"Timer", sizeof(Timer), lf_Timer, (int)(sizeof(lf_Timer)/sizeof(lf_Timer[0]))};
static const LuaFieldDesc lf_ListSpec[] = {
    {"count", LSF_INT, offsetof(ListSpec, count), 0},
    {"x", LSF_INT, offsetof(ListSpec, x), 0},
    {"y", LSF_INT, offsetof(ListSpec, y), 0},
    {"w", LSF_INT, offsetof(ListSpec, w), 0},
    {"h", LSF_INT, offsetof(ListSpec, h), 0},
    {"item_height", LSF_INT, offsetof(ListSpec, item_height), 0},
};
static const LuaStructDesc lsd_ListSpec = {"ListSpec", sizeof(ListSpec), lf_ListSpec, (int)(sizeof(lf_ListSpec)/sizeof(lf_ListSpec[0]))};
static const LuaFieldDesc lf_SkEditSpec[] = {
    {"name", LSF_STR, offsetof(SkEditSpec, name), 0},
    {"x", LSF_INT, offsetof(SkEditSpec, x), 0},
    {"y", LSF_INT, offsetof(SkEditSpec, y), 0},
    {"w", LSF_INT, offsetof(SkEditSpec, w), 0},
    {"h", LSF_INT, offsetof(SkEditSpec, h), 0},
    {"placeholder", LSF_STR, offsetof(SkEditSpec, placeholder), 0},
    {"initial_text", LSF_STR, offsetof(SkEditSpec, initial_text), 0},
    {"color_bg", LSF_INT, offsetof(SkEditSpec, color_bg), 0},
    {"color_text", LSF_INT, offsetof(SkEditSpec, color_text), 0},
    {"font_size", LSF_INT, offsetof(SkEditSpec, font_size), 0},
    {"caret_h", LSF_INT, offsetof(SkEditSpec, caret_h), 0},
    {"caret_w", LSF_INT, offsetof(SkEditSpec, caret_w), 0},
    {"primary", LSF_BOOL, offsetof(SkEditSpec, primary), 0},
    {"no_tab", LSF_BOOL, offsetof(SkEditSpec, no_tab), 0},
    {"wrapped", LSF_BOOL, offsetof(SkEditSpec, wrapped), 0},
};
static const LuaStructDesc lsd_SkEditSpec = {"SkEditSpec", sizeof(SkEditSpec), lf_SkEditSpec, (int)(sizeof(lf_SkEditSpec)/sizeof(lf_SkEditSpec[0]))};
static const LuaFieldDesc lf_SkListItemCtx[] = {
    {"idx", LSF_INT, offsetof(SkListItemCtx, idx), 0},
    {"selected", LSF_BOOL, offsetof(SkListItemCtx, selected), 0},
    {"x", LSF_INT, offsetof(SkListItemCtx, x), 0},
    {"y", LSF_INT, offsetof(SkListItemCtx, y), 0},
    {"w", LSF_INT, offsetof(SkListItemCtx, w), 0},
    {"h", LSF_INT, offsetof(SkListItemCtx, h), 0},
    {"text", LSF_STR, offsetof(SkListItemCtx, text), 0},
};
static const LuaStructDesc lsd_SkListItemCtx = {"SkListItemCtx", sizeof(SkListItemCtx), lf_SkListItemCtx, (int)(sizeof(lf_SkListItemCtx)/sizeof(lf_SkListItemCtx[0]))};
static const LuaFieldDesc lf_PlaceState[] = {
    {"name", LSF_STR, offsetof(PlaceState, name), 0},
    {"kind", LSF_INT, offsetof(PlaceState, kind), 0},
    {"checked", LSF_BOOL, offsetof(PlaceState, checked), 0},
};
static const LuaStructDesc lsd_PlaceState = {"PlaceState", sizeof(PlaceState), lf_PlaceState, (int)(sizeof(lf_PlaceState)/sizeof(lf_PlaceState[0]))};
static const LuaFieldDesc lf_CheckboxSpec[] = {
    {"name", LSF_STR, offsetof(CheckboxSpec, name), 0},
    {"text", LSF_STR, offsetof(CheckboxSpec, text), 0},
    {"font_size", LSF_INT, offsetof(CheckboxSpec, font_size), 0},
    {"x", LSF_INT, offsetof(CheckboxSpec, x), 0},
    {"y", LSF_INT, offsetof(CheckboxSpec, y), 0},
    {"w", LSF_INT, offsetof(CheckboxSpec, w), 0},
    {"h", LSF_INT, offsetof(CheckboxSpec, h), 0},
    {"checked", LSF_BOOL, offsetof(CheckboxSpec, checked), 0},
    {"button", LSF_BOOL, offsetof(CheckboxSpec, button), 0},
    {"checkbox", LSF_BOOL, offsetof(CheckboxSpec, checkbox), 0},
    {"thickness", LSF_INT, offsetof(CheckboxSpec, thickness), 0},
    {"text_color", LSF_INT, offsetof(CheckboxSpec, text_color), 0},
    {"hovered_color", LSF_INT, offsetof(CheckboxSpec, hovered_color), 0},
    {"clicked_color", LSF_INT, offsetof(CheckboxSpec, clicked_color), 0},
    {"bg_color", LSF_INT, offsetof(CheckboxSpec, bg_color), 0},
    {"border", LSF_BOOL, offsetof(CheckboxSpec, border), 0},
    {"no_bg", LSF_BOOL, offsetof(CheckboxSpec, no_bg), 0},
    {"on_click", LSF_CB, offsetof(CheckboxSpec, on_click), 0},
    {"on_hover", LSF_CB, offsetof(CheckboxSpec, on_hover), 0},
    {"render", LSF_CB, offsetof(CheckboxSpec, render), 0},
};
static const LuaStructDesc lsd_CheckboxSpec = {"CheckboxSpec", sizeof(CheckboxSpec), lf_CheckboxSpec, (int)(sizeof(lf_CheckboxSpec)/sizeof(lf_CheckboxSpec[0]))};
static const LuaFieldDesc lf_CcEntrySpec[] = {
    {"display_name", LSF_STR, offsetof(CcEntrySpec, display_name), 0},
    {"internal_name", LSF_STR, offsetof(CcEntrySpec, internal_name), 0},
    {"on_enter", LSF_CB, offsetof(CcEntrySpec, on_enter), 3},
    {"on_ctrl_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_enter), 3},
    {"on_shift_enter", LSF_CB, offsetof(CcEntrySpec, on_shift_enter), 3},
    {"on_ctrl_shift_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_shift_enter), 3},
    {"on_ctrl_alt_enter", LSF_CB, offsetof(CcEntrySpec, on_ctrl_alt_enter), 3},
    {"on_shift_alt_enter", LSF_CB, offsetof(CcEntrySpec, on_shift_alt_enter), 3},
    {"on_tab", LSF_CB, offsetof(CcEntrySpec, on_tab), 3},
    {"on_action", LSF_CB, offsetof(CcEntrySpec, on_action), 0},
    {"icon", LSF_STR, offsetof(CcEntrySpec, icon), 0},
    {"type", LSF_STR, offsetof(CcEntrySpec, type), 0},
};
static const LuaStructDesc lsd_CcEntrySpec = {"CcEntrySpec", sizeof(CcEntrySpec), lf_CcEntrySpec, (int)(sizeof(lf_CcEntrySpec)/sizeof(lf_CcEntrySpec[0]))};
static const LuaStructDesc *lua_generated_structs[] = {
    &lsd_CcEntrySpec, &lsd_SkHotkeySpec, &lsd_PromptSpec,
    &lsd_SkTextSpec, &lsd_DrawRect,
    &lsd_CheckboxSpec, &lsd_SkMessageSpec, &lsd_SkEditSpec,
    &lsd_ListSpec,
};
static const char *lua_alias_from[] = {
    "SkStringList",
    "SkKey",
    "SkWindowHandle",
    "PlatformWidget",
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
    "ListBox",
    "PromptState",
    "PromptCallbacks",
    "Display",
    "XftFont",
    "SkTimer",
};
static const char *lua_alias_to[] = {
    "StringList",
    "HotkeyKey",
    "WindowHandle",
    "struct",
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
    "struct",
    "struct",
    "struct",
    "struct",
    "struct",
    "Timer",
};
static const LuaStructDesc *ski_lua_struct_find(const char *name) {
  for (size_t i = 0; i < sizeof(lua_alias_from)/sizeof(lua_alias_from[0]); i++)
    if (strcmp(name, lua_alias_from[i]) == 0)
      name = lua_alias_to[i];
  for (size_t i = 0; i < sizeof(lua_generated_structs)/sizeof(lua_generated_structs[0]); i++)
    if (strcmp(name, lua_generated_structs[i]->name) == 0)
      return lua_generated_structs[i];
  return NULL;
}

#endif
