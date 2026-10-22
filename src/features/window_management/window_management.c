#include "core/flapi.h"

void move_active_window_right(void) {
    fl_move_active_window_right();
}
void move_active_window_left(void) {
    fl_move_active_window_left();
}
void focus_window_right(void) {
    fl_focus_window_right();
}
void focus_window_left(void) {
    fl_focus_window_left();
}
void close_active_window(void) {
    fl_close_active_window();
}
void toggle_fullscreen(void) {
    fl_toggle_fullscreen_active();
}
void toggle_minimized(void) {
    fl_toggle_minimized_active();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Shift+L", .cb=move_active_window_right);
    fl_register_hotkey(.hotkey="Alt+Shift+H", .cb=move_active_window_left);
    fl_register_hotkey(.hotkey="Alt+Shift+O", .cb=focus_window_right);
    fl_register_hotkey(.hotkey="Alt+Shift+I", .cb=focus_window_left);
    fl_register_hotkey(.hotkey="Ctrl+Q", .cb=close_active_window);
    fl_register_hotkey(.hotkey="Alt+F", .cb=toggle_fullscreen);
    fl_register_hotkey(.hotkey="Alt+M", .cb=toggle_minimized);

    fl_add_to_cc(.display_name = "MoveActiveWindowRight", .on_action = move_active_window_right,.type="WindowManagement",.icon="/arrow-right.png");
    fl_add_to_cc(.display_name = "MoveActiveWindowLeft",  .on_action = move_active_window_left,.type="WindowManagement",.icon="/arrow-left.png");
    fl_add_to_cc(.display_name = "FocusWindowRight",      .on_action = focus_window_right,.type="WindowManagement",.icon="/arrow-right.png");
    fl_add_to_cc(.display_name = "FocusWindowLeft",       .on_action = focus_window_left,.type="WindowManagement",.icon="/arrow-left.png");
    fl_add_to_cc(.display_name = "CloseActiveWindow",     .on_action = close_active_window,.type="WindowManagement",.icon="/x.png");
    fl_add_to_cc(.display_name = "ToggleFullscreen",      .on_action = toggle_fullscreen,.type="WindowManagement",.icon="/maximize-2.png");
    fl_add_to_cc(.display_name = "ToggleMinimized",       .on_action = toggle_minimized,.type="WindowManagement",.icon="/minimize-2.png");
}
    
