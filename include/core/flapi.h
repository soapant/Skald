#ifndef FLAPI_H
#define FLAPI_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"

#include <stdbool.h>
#include <stddef.h>
#include "core/types.h"
#include "core/types/prompt.h"
#include "core/types/flapi_types.h"
#include "core/checkbox.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "core/flapi_input.h"
#include "core/clipboard_history.h"
#include "config.h"



/**
 * @brief A list of strings.
 *
 * Fields:
 * - `items` — List of strings.
 * - `count` — Amount of strings.
 */
typedef StringList FlStringList;

/**
 * @brief Hotkey key definition.
 */
typedef HotkeyKey FlKey;

typedef struct PlatformWidget PlatformWidget;

/**
 * @brief A color value.
 *
 * Construct with `RGB(r, g, b)` or `HEX(0xRRGGBB)`.
 */
typedef PlColor FlColor;

/**
 * @brief A rectangle in screen coordinates.
 *
 * Fields:
 * - `x`, `y` — Top-left corner.
 * - `w`, `h` — Width and height in pixels.
 */
typedef PlRect FlRect;

/**
 * @brief A position in screen coordinates.
 *
 * Fields:
 * - `x` — Horizontal position.
 * - `y` — Vertical position.
 */
typedef PlPos FlPos;

/**
 * @brief Parameters for a listbox widget.
 *
 * Fields:
 * - `lb`          — Internal listbox state.
 * - `items`       — Array of `count` strings to display.
 * - `count`       — Number of items in `items`.
 * - `x`, `y`      — Top-left corner in screen coordinates.
 * - `w`, `h`      — Width and height in pixels.
 * - `item_height` — Height of each row in pixels, or 0 for the default.
 */
typedef PlListSpec FlListSpec;

/**
 * @brief A window list, as returned by window enumeration functions.
 *
 * Fields:
 * - `windows` — List of window titles or ids.
 * - `count` — Amount of windows.
 */
typedef PlWindowList FlWindowList;

typedef CheckboxSpec FlCheckboxSpec;

/**
 * @brief A single entry in the clipboard history.
 *
 * Fields:
 * - `type` — CLIPBOARD_TEXT or CLIPBOARD_IMAGE.
 * - `text` — Null-terminated string.
 * - `image_path` — Path to the image.
 * - `image_width` — Image width.
 * - `image_height` — Image height.
 * - `timestamp` — Unix epoch timestamp.
 */
typedef ClipboardEntry FlClipboardEntry;

/**
 * @brief The full clipboard history.
 *
 * Fields:
 * - `entries` — Array of entries.
 * - `count` — Number of valid entries.
 */
typedef ClipboardHistory FlClipboardHistory;

/**
 * @brief Copy a string into the system clipboard.
 * @param text  String to copy. Must not be NULL.
 * @return      True on success, false if unavailable.
 */
bool fl_copy_to_clipboard(const char* text);

/**
 * @brief Copy a file into the system clipboard.
 * @param path  File path to copy.
 * @return      True on success, false if unavailable.
 */
bool fl_copy_file_path(const char* path);

/**
 * @brief Paste the current clipboard text into the focused window.
 * @return True if a paste was performed, false otherwise.
 */
bool fl_paste_from_clipboard(void);

/**
 * @brief Paste a specific string into the focused window.
 * @param content  String to paste. Must not be NULL.
 * @return         True on success, false if could not paste.
 */
bool fl_paste(char* content);

/**
 * @brief Get the most recent text entry from the clipboard history.
 * @return Pointer to a string (caller must free), or NULL if empty/images only.
 */
char* fl_get_last_clipboard_entry(void);

/**
 * @brief Number of entries in the clipboard history.
 * @return Number of entries stored, or 0 if empty/disabled.
 */
size_t fl_clipboard_history_count(void);

/**
 * @brief Get a clipboard history entry by index.
 * @param index  Index in range [0, fl_clipboard_history_count()).
 * @return       Borrowed pointer to `FlClipboardEntry`.
 */
const FlClipboardEntry* fl_clipboard_history_get(size_t index);

/**
 * @brief Get the most recent entry in the clipboard history.
 * @return Borrowed pointer to `FlClipboardEntry`, or NULL if empty.
 */
const FlClipboardEntry* fl_clipboard_latest(void);

/**
 * @brief Clear all entries from the clipboard history.
 */
void fl_clipboard_clear_history(void);


/**
 * @brief Current network connectivity state.
 * @return The current connectivity state (`FlNetStatus`).
 */
FlNetStatus fl_net_status(void);

/**
 * @brief Cumulative bytes received on the active network interface.
 * @return Bytes received since boot, or 0 if unavailable.
 */
unsigned long long fl_net_rx_bytes(void);

/**
 * @brief Bytes received in the last `seconds` seconds.
 * @param seconds  Sampling window in seconds. Must be greater than 0.
 * @return         Bytes received, or 0 if unavailable blocking.
 */
unsigned long long fl_net_rx_bytes_over(double seconds);

/** @} */


#define FL_DIR_WALK_UNLIMITED 0
/**
 * @brief Search for a file within a directory.
 * @param directory      The directory path.
 * @param search_string  The filename string to search for.
 * @param output_path    Buffer to store the result.
 * @return               Pointer to output_path, or NULL if not found.
 */
char* fl_search_file_in_dir(const char* directory, const char* search_string, char* output_path);
/**
 * @brief Get all files from a directory.
 * @param dir_path                Directory path.
 * @param list_of_allowed_extensions List of allowed extensions.
 * @param list_of_files           Resulting file list.
 * @param list_of_names_to_ignore List of names to ignore.
 * @param strip_extension         Whether to strip extensions.
 * @param max_depth               Maximum depth.
 * @return                        True on success, false otherwise.
 */
bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore, bool strip_extension, int max_depth);

/**
 * @brief Load a file list.
 * @param filepath  Path to the file.
 * @return          `FlStringList` of items. See `FlStringList`.
 */
FlStringList fl_load_file_list(const char* filepath);

/**
 * @brief Load file content into a buffer.
 * @param filepath     Path to the file.
 * @param buffer       Destination buffer.
 * @param buffer_size  Size of the buffer.
 * @return             Bytes loaded, or -1 on failure.
 */
long fl_load_file(const char* filepath, char* buffer, long buffer_size);

/**
 * @brief Read file to a newly allocated string.
 * @param path     Path to the file.
 * @param out_len  Pointer to store the length.
 * @return         String (caller must free), or NULL on failure.
 */
char* fl_read_file_to_string(const char* path, size_t* out_len);

/**
 * @brief Read entire file into a buffer.
 * @param full_path    Full path to the file.
 * @param buffer       Destination buffer.
 * @param buffer_size  Size of the buffer.
 * @return             Bytes read.
 */
long fl_read_entire_file(const char* full_path, char* buffer, long buffer_size);

/**
 * @brief Write a string to a file.
 * @param path     Path to the file.
 * @param content  String content.
 * @return         True on success, false otherwise.
 */
bool fl_write_string_to_file(const char* path, const char* content);

/**
 * @brief Check if a file exists.
 * @param path  Path to the file.
 * @return      True if it exists, false otherwise.
 */
bool fl_file_exists(const char* path);

/**
 * @brief Append a line if it doesn't already exist.
 * @param path  Path to the file.
 * @param line  The line content.
 * @return      True on success, false otherwise.
 */
bool fl_append_unique_line(const char* path, const char* line);

/**
 * @brief Remove a line from a file.
 * @param path  Path to the file.
 * @param line  The line content to remove.
 * @return      True on success, false otherwise.
 */
bool fl_remove_line(const char* path, const char* line);

/**
 * @brief Remove any line containing a substring.
 * @param path     Path to the file.
 * @param substr   Substring to match.
 * @return         True on success, false otherwise.
 */
bool fl_remove_line_contains(const char* path, const char* substr);

/**
 * @brief Parse a desktop file.
 * @param path          Path to the desktop file.
 * @param out_name      Buffer for the output name.
 * @param n             Size of out_name.
 * @param out_nodisplay Pointer to store NoDisplay flag.
 * @return              True on success, false otherwise.
 */
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay);

/**
 * @brief Get the icon path for an application.
 * @param app_path_path  Path to the application.
 * @return               Icon path (caller must free), or NULL.
 */
char* fl_get_icon_path(const char* app_path_path);

/**
 * @brief Append a line to a file.
 * @param filepath  Path to the file.
 * @param line      Line content.
 * @return          True on success, false otherwise.
 */
bool fl_append_to_file(const char* filepath, const char* line);

/**
 * @brief Write lines to a file.
 * @param filepath  Path to the file.
 * @param lines     Array of strings.
 * @param count     Number of lines.
 * @return          True on success, false otherwise.
 */
bool fl_write_to_file(const char* filepath, const char* lines[], int count);

/**
 * @brief Write a temporary file.
 * @param name      Base name.
 * @param data      Data.
 * @param len       Length of data.
 * @param out_path  Output path buffer.
 * @param out_len   Length of out_path buffer.
 * @return          True on success, false otherwise.
 */
bool fl_write_temp_file(const char* name, const void* data, size_t len, char* out_path, size_t out_len);

/**
 * @brief Get the size of an icon.
 * @param icon_path  Path to the icon.
 * @return           Size, or 0 on failure.
 */
int fl_get_icon_size(const char* icon_path);

/**
 * @brief Free a file list.
 * @param list  The list to free. See `FlStringList`.
 */
void fl_free_file_list(FlStringList* list);

/**
 * @brief Free a list of strings.
 * @param list   The string list.
 * @param count  Number of items.
 * @return       True.
 */
bool fl_free_list(const char* list[], int count);

/**
 * @brief Free a window list.
 * @param list  The window list to free. See `FlWindowList`.
 * @return      True.
 */
bool fl_free_window_list(FlWindowList *list);

/**
 * @brief Free a StringList.
 * @param list  The string list to free. See `StringList`.
 */
void fl_string_list_free(StringList* list);

/**
 * @brief Format bytes into human-readable string.
 * @param bytes    Bytes.
 * @param out_str  Buffer.
 * @param out_len  Buffer size.
 */
void fl_format_bytes(unsigned long long bytes, char* out_str, size_t out_len);


/**
 * @brief Launch a process with arguments.
 * @param path  Path to the executable.
 * @param args  Arguments string.
 * @return      True on success, false on failure.
 */
bool fl_launch_with_args(const char* path, const char* args);

/**
 * @brief Launch a process with arguments as administrator.
 * @param path  Path to the executable.
 * @param args  Arguments string.
 * @return      True on success, false on failure.
 */
bool fl_launch_with_args_as_admin(const char* path, const char* args);

/**
 * @brief Launch a terminal at a specific path.
 * @param path  Working directory for the terminal.
 * @param args  Arguments for the terminal command.
 * @return      True on success, false on failure.
 */
bool fl_launch_terminal_at_path(char* path,char* args);

/**
 * @brief Run a terminal command.
 * @param command  The command to run.
 * @return         Output of the command (caller must free), or NULL.
 */
char* fl_run_terminal_command(char* command);

/**
 * @brief Run a terminal command, blocking until it finishes.
 * @param command  The command to run.
 * @return         Output of the command (caller must free), or NULL.
 */
char* fl_run_terminal_command_blocking(char* command);

/**
 * @brief Run a terminal command and open its output.
 * @param command  The command to run.
 * @return         Output of the command (caller must free), or NULL.
 */
char* fl_run_terminal_command_and_open(char* command);

/**
 * @brief Run a command given an argument array.
 * @param argv      Argument array.
 * @param stdin_data Input data for stdin.
 * @param out_status Pointer to store exit status.
 * @return           Output of the command (caller must free), or NULL.
 */
char* fl_run_argv(const char* const argv[], const char* stdin_data, int* out_status);

/**
 * @brief Launch a process detached from the current one.
 * @param command  Command to launch.
 * @return         True on success, false on failure.
 */
bool fl_launch_detached(char* command);

/**
 * @brief Terminate a process by its executable name.
 * @param exe_name  Executable name to terminate.
 * @return          True on success, false on failure.
 */
bool fl_terminate_process_by_name(const char* exe_name);

/**
 * @brief Feature registration specification.
 *
 * Fields:
 * - `name` — Display name of the feature.
 * - `cb` — Callback function invoked when the feature is selected.
 * - `icon` — Icon path or name.
 */
typedef struct {
    const char* name;
    void (*cb)(void);
    const char* icon;
} FlFeatureSpec;

/**
 * @def Register a new feature.
 * @param ... Fields of `FlFeatureSpec`. See `FlFeatureSpec`.
 */
#define fl_register_feature(...) fl_register_feature_impl(&(FlFeatureSpec){__VA_ARGS__})

/**
 * @brief Register a feature (internal implementation).
 * @param spec  Pointer to feature specification. See `FlFeatureSpec`.
 */
void fl_register_feature_impl(const FlFeatureSpec* spec);

/**
 * @brief Get list of registered feature names.
 * @param out_names  Array to store feature names.
 * @param out_cbs    Array to store feature callbacks.
 * @param max        Maximum number of features to retrieve.
 * @return           Number of features retrieved.
 */
int fl_get_feature_list(const char** out_names, void (**out_cbs)(void), int max);

/**
 * @brief Get list of registered features with icons.
 * @param out_names  Array to store feature names.
 * @param out_cbs    Array to store feature callbacks.
 * @param out_icons  Array to store feature icon paths.
 * @param max        Maximum number of features to retrieve.
 * @return           Number of features retrieved.
 */
int fl_get_feature_list_with_icons(const char** out_names, void (**out_cbs)(void), const char*** out_icons, int max);

/**
 * @brief Initialize all configured hotkeys.
 */
void fl_init_all_hotkeys(void);

/**
 * @brief Trigger a hotkey by its string representation.
 * @param hotkey_string  The hotkey string (e.g., "Ctrl+C").
 */
void fl_send_hotkey(const char* hotkey_string);

/**
 * @brief Convert a hotkey keycode to a string.
 * @param key  The hotkey keycode.
 * @return     String representation (caller must free).
 */
char* fl_hotkey_to_string(FlKey key);

/**
 * @brief Convert a string to a hotkey keycode.
 * @param str  The hotkey string.
 * @return     The hotkey keycode.
 */
FlKey fl_hotkey_from_string(const char* str);

/**
 * @brief Launch the default browser with a URL.
 * @param url  The URL to open.
 * @return     True on success, false on failure.
 */
bool fl_launch_browser(char* url);

/**
 * @brief Perform a browser search query.
 * @param query  The search query.
 * @return       True on success, false on failure.
 */
bool fl_query_browser(const char* query);

/**
 * @brief Focus a window by its title.
 * @param title  The title of the window.
 * @return       True on success, false on failure.
 */
bool fl_focus_window_by_title(char* title);

/**
 * @brief Close the active window.
 * @return  True on success, false on failure.
 */
bool fl_close_active_window(void);

/**
 * @brief Dismiss a message popup by its name.
 * @param name  The name of the message popup.
 * @return      True on success, false on failure.
 */
bool fl_dismiss_message(const char* name);

/**
 * @brief Check if a message popup is active.
 * @param name  The name of the message popup.
 * @return      True if active, false otherwise.
 */
bool fl_message_active(const char* name);

/**
 * @brief Toggle fullscreen mode for the active window.
 * @return  True on success, false on failure.
 */
bool fl_toggle_fullscreen_active(void);

/**
 * @brief Move the active window to the next/previous monitor.
 * @param right  If true, move to the right; if false, move to the left.
 * @return       True on success, false on failure.
 */
bool fl_move_active_window_to_monitor(bool right);

/**
 * @brief Move the active window to the left.
 * @return  True on success, false on failure.
 */
bool fl_move_active_window_left(void);

/**
 * @brief Move the active window to the right.
 * @return  True on success, false on failure.
 */
bool fl_move_active_window_right(void);

/**
 * @brief Focus the window to the left.
 * @return  True on success, false on failure.
 */
bool fl_focus_window_left(void);

/**
 * @brief Focus the window to the right.
 * @return  True on success, false on failure.
 */
bool fl_focus_window_right(void);

/**
 * @brief Toggle minimized state for the active window.
 * @return  True on success, false on failure.
 */
bool fl_toggle_minimized_active(void);

/**
 * @brief Get the screen width.
 * @return  Screen width in pixels.
 */
int fl_get_screen_width(void);

/**
 * @brief Get the screen height.
 * @return  Screen height in pixels.
 */
int fl_get_screen_height(void);

/**
 * @brief Get the monitor geometry.
 * @return  A `FlRect` containing the monitor coordinates and size. See `FlRect`.
 */
FlRect fl_get_monitor_geometry(void);

/** @} */

/**
 * @brief Cut a string at the first occurrence of a delimiter.
 * @param str    The string to cut.
 * @param delim  The delimiter character.
 * @return       The part of the string before the delimiter (caller must free), or NULL.
 */
char* fl_string_cut_at(const char* str, char delim);

/**
 * @brief Cut a string before the last occurrence of a delimiter.
 * @param str    The string to cut.
 * @param delim  The delimiter character.
 * @return       The part of the string before the delimiter (caller must free), or NULL.
 */
char* fl_string_cut_before(const char* str, char delim);

/**
 * @brief Cut a string after the last occurrence of a delimiter.
 * @param str    The string to cut.
 * @param delim  The delimiter character.
 * @return       The part of the string after the delimiter (caller must free), or NULL.
 */
char* fl_string_cut_after(const char* str, char delim);

/**
 * @brief Get the string between two delimiter characters.
 * @param str    The string to parse.
 * @param open   The opening delimiter.
 * @param close  The closing delimiter.
 * @return       The string between the delimiters (caller must free), or NULL.
 */
char* fl_string_between(const char* str, char open, char close);

/**
 * @brief Get the string inside a specific delimiter.
 * @param str    The string to parse.
 * @param delim  The delimiter character.
 * @return       The string inside the delimiter (caller must free), or NULL.
 */
char* fl_string_inside(const char* str, char delim);

/**
 * @brief Trim whitespace from a string.
 * @param str  The string to trim.
 * @return     A new trimmed string (caller must free), or NULL.
 */
char* fl_string_trim(const char* str);

/**
 * @brief Convert a string to lowercase.
 * @param str  The string to convert.
 * @return     A new lowercase string (caller must free), or NULL.
 */
char* fl_string_to_lower(const char* str);

/**
 * @brief Convert a string to uppercase.
 * @param str  The string to convert.
 * @return     A new uppercase string (caller must free), or NULL.
 */
char* fl_string_to_upper(const char* str);

/**
 * @brief Check if a string starts with a prefix.
 * @param str     The string to check.
 * @param prefix  The prefix to look for.
 * @return        True if it starts with the prefix, false otherwise.
 */
bool fl_string_starts_with(const char* str, const char* prefix);

/**
 * @brief Check if a string ends with a suffix.
 * @param str     The string to check.
 * @param suffix  The suffix to look for.
 * @return        True if it ends with the suffix, false otherwise.
 */
bool fl_string_ends_with(const char* str, const char* suffix);

/**
 * @brief Replace all occurrences of a substring.
 * @param str   The original string.
 * @param from  The substring to replace.
 * @param to    The replacement substring.
 * @return      A new string with replacements (caller must free), or NULL.
 */
char* fl_string_replace(const char* str, const char* from, const char* to);

/**
 * @brief Slice a string.
 * @param str    The original string.
 * @param start  Start index.
 * @param end    End index.
 * @return       A new sliced string (caller must free), or NULL.
 */
char* fl_string_slice(const char* str, int start, int end);

/**
 * @brief Split a string by a delimiter.
 * @param str        The string to split.
 * @param delimiter  The delimiter string.
 * @return           A `FlStringList` containing the parts. See `FlStringList`.
 */
FlStringList fl_string_split(const char* str, const char* delimiter);

/**
 * @brief Populate a listbox widget.
 * @param lb      Pointer to the listbox information. See `ListBoxInfo`.
 * @param filter  Filter string.
 * @param items   Array of items.
 * @param count   Number of items.
 */
void fl_populate_listbox(ListBoxInfo* lb, const char* filter, const char** items, int count);

/**
 * @brief Populate a listbox widget with a custom filter.
 * @param lb            Pointer to the listbox information. See `ListBoxInfo`.
 * @param filter        Filter string.
 * @param items         Array of items.
 * @param count         Number of items.
 * @param filter_match  Custom filter matching function.
 */
void fl_populate_listbox_ex(ListBoxInfo* lb, const char* filter, const char** items, int count, bool (*filter_match)(const char* item, const char* filter));

/**
 * @brief Check if an item is selected in a listbox.
 * @param lb  Pointer to the listbox information. See `ListBoxInfo`.
 * @return    True if selected, false otherwise.
 */
bool fl_is_selected(ListBoxInfo* lb);

/**
 * @brief Set the text in the edit box.
 * @param text  The text to set.
 */
void fl_set_edit_text(const char* text);

/**
 * @brief Set the cursor position in the edit box.
 * @param position  The cursor position.
 */
void fl_set_edit_cursor(int position);

/**
 * @brief Get the cursor position in the edit box.
 * @return  The current cursor position.
 */
int  fl_get_edit_cursor(void);

/**
 * @brief Get the text from the edit box.
 * @param buffer  Destination buffer.
 * @param cap     Size of the buffer.
 */
void fl_get_edit_text(char* buffer, size_t cap);

/**
 * @brief Edit popup specification.
 *
 * Fields:
 * - `initial_text` — Starting text.
 * - `x`, `y` — Top-left corner.
 * - `w`, `h` — Width and height.
 * - `align` — Text alignment.
 * - `lb` — Associated ListboxInfo. See `ListBoxInfo`.
 * - `out_index` — Index of selected item.
 */
typedef struct {
    const char* initial_text;
    int x;
    int y;
    int w;
    int h;
    const char* align;
    ListBoxInfo* lb;
    int* out_index;
} FlEditPopupSpec;

/**
 * @def Create and show an edit text popup.
 * @param ... Fields of `FlEditPopupSpec`. See `FlEditPopupSpec`.
 */
#define fl_edit_text_popup(...) fl_edit_text_popup_impl(&(FlEditPopupSpec){__VA_ARGS__})

/**
 * @brief Implementation for creating an edit text popup.
 * @param spec  The specification. See `FlEditPopupSpec`.
 * @return      Resulting text (caller must free), or NULL.
 */
char* fl_edit_text_popup_impl(const FlEditPopupSpec* spec);

/**
 * @brief Replace a line in a file.
 * @param filepath    Path to the file.
 * @param line_index  Index of the line to replace.
 * @param new_text    New text content.
 * @return            True on success, false on failure.
 */
bool fl_replace_line_in_file(const char* filepath, int line_index, const char* new_text);

/**
 * @def Create and show a generic popup.
 * @param ... Fields of `PopupSpec`. See `PopupSpec`.
 */
#define fl_popup(...) fl_popup_impl(&(PopupSpec){__VA_ARGS__})

/**
 * @brief Implementation for creating a generic popup.
 * @param spec  The specification. See `PopupSpec`.
 * @return      Resulting text or NULL.
 */
char* fl_popup_impl(const PopupSpec *spec);

/**
 * @brief Per-frame state passed to a widget's render callback.
 *
 * Fields:
 * - `width`   — Widget width in pixels.
 * - `height`  — Widget height in pixels.
 * - `frame`   — Frame counter. Increments every render. Useful for
 *               animation (e.g., `sin(frame * 0.1)`).
 * - `elapsed` — Seconds since the widget was created. Useful for
 *               time-based animation independent of frame rate.
 *
 * Passed by pointer to `FlWidgetRenderFn` once per frame.
 */

typedef struct {
    int width;
    int height;
    unsigned long frame;
    double elapsed;
    bool hovered;
} FlWidgetState;

/**
 * @brief Render callback for a widget.
 *
 * Called once per frame to draw the widget's contents. The callback
 * receives the widget's current state See `FlWidgetState`.
 * Drawing is clipped to the widget's bounds; content outside the
 * widget's width and height will not appear.
 *
 * @param state  Current state of the widget. See `FlWidgetState`.
 */
typedef void (*FlWidgetRenderFn)(FlWidgetState *state);

/**
 * @brief Message popup specification. Automatically auto scales the window when w and h are 0 / NULL.
 *
 * Fields:
 * - `message` — Message text.
 * - `slot` — Message position slot. See `MessageSlot`.
 * - `x`, `y` — Top-left corner.
 * - `w`, `h` — Size.
 * - `mw`, `mh` — Max size for auto scaling
 * - `fps` — How many times the message window renders every second.
 * - `render` — Render callback (void void) callback.
 * - `timeout` — Auto-dismissal time.
 * - `close_on_keypress` — Close the message as soon as any key is pressed (overrides auto-dismissal).
 * - `color_text` — Text color See `FlColor`.
 * - `color_bg` — Background color See `FlColor`.
 * - `name` — Unique identifier.
 */
typedef struct {
    const char* message;
    MessageSlot slot;
    int x;
    int y;
    int w;
    int h;
    int mw;
    int mh;
    double timeout;
    int prompt_gap;
    ColorRef color_text;
    const char* hover_key;
    void (*render)(void);
    int fps;
    bool no_bg;
    ColorRef color_bg;
    const char* name;
    bool close_on_keypress;
} FlMessageSpec;

typedef struct {
    void (*render)(void);
    int w;
    int h;
    unsigned long frame;
    FlTimer created_at;
    int pad;
} FlHoverPopupCtx;

/**
 * @def Create and show a message popup.
 * @param ... Fields of FlMessageSpec. See `FlMessageSpec`.
 */
#define fl_message(...) fl_message_impl(&(FlMessageSpec){__VA_ARGS__})

/**
 * @brief Implementation for creating a message popup.
 * @param spec  The specification. See `FlMessageSpec`.
 */
void fl_message_impl(const FlMessageSpec* spec);

#define fl_overlay(...) fl_message(__VA_ARGS__, .timeout=10000000.0)
#define fl_dismiss_overlay(...) fl_dismiss_message(__VA_ARGS__)
#define fl_subwindow(...) fl_window(__VA_ARGS__,.subwindow=true)

typedef PromptSpec FlPromptSpec;

/**
 * @def Create and show a window.
 * @param ... Fields of FlPromptSpec. See `FlPromptSpec`.
 */
#define fl_window(...) fl_window_impl(&(FlPromptSpec){__VA_ARGS__})

/**
 * @brief Implementation for creating a window.
 * @param spec  The specification. See `FlPromptSpec`.
 */
void fl_window_impl(const FlPromptSpec* spec);

typedef enum {
    FLANCHOR_TOP_LEFT,
    FLANCHOR_TOP_CENTER,
    FLANCHOR_TOP_RIGHT,
    FLANCHOR_CENTER_LEFT,
    FLANCHOR_CENTER,
    FLANCHOR_CENTER_RIGHT,
    FLANCHOR_BOTTOM_LEFT,
    FLANCHOR_BOTTOM_CENTER,
    FLANCHOR_BOTTOM_RIGHT,
    FLANCHOR_FILL_WIDTH,
    FLANCHOR_FILL_HEIGHT
} FlAnchor;

/**
 * @brief Text drawing specification.
 *
 * Fields:
 * - `text` — Text string.
 * - `x`, `y` — Position.
 * - `color` — Text color.
 * - `fontsize` — Font size.
 */
typedef struct {
    const char *text;
    int x;
    int y;
    unsigned long color;
    FontHandle font;
    int fontsize;
    const char *align;
    int pad;
} FlTextSpec;

typedef struct {
    int x;
    int y;
} FlCharPos;

/**
 * @brief Draw wrapped text.
 * @param x, y            Position.
 * @param max_width       Max width.
 * @param line_height     Line height.
 * @param text            Text string.
 * @param fontsize        Font size.
 * @param color           Color.
 * @param out_positions   Output character positions.
 * @param max_positions   Max positions to store.
 * @return                Height of drawn text.
 */
int fl_draw_text_wrapped_ex(int x, int y, int max_width, int line_height, const char *text, int fontsize, unsigned long color, FlCharPos *out_positions, int max_positions);

/**
 * @brief Text measurement specification.
 *
 * Fields:
 * - `x`, `y` — Position.
 * - `max_width` — Maximum width before wrap.
 * - `text` — Text to measure.
 * - `fontsize` — Font size.
 */
typedef struct {
    int x;
    int y;
    int max_width;
    int line_height;
    const char *text;
    int fontsize;
    FlCharPos *out_positions;
} FlMeasureSpec;

/**
 * @def Measure wrapped text.
 * @param ... Fields of `FlMeasureSpec`. See `FlMeasureSpec`.
 */
#define fl_measure_text_wrapped(...) fl_measure_text_wrapped_impl(&(FlMeasureSpec){__VA_ARGS__})

/**
 * @brief Implementation for measuring wrapped text.
 * @param spec  The measurement specification. See `FlMeasureSpec`.
 * @return      Measured height.
 */
int fl_measure_text_wrapped_impl(const FlMeasureSpec *spec);

/** @} */

typedef struct {
    const char *path;
    int x; int y; int w; int h;
} FlImageSpec;

bool fl_draw_text_impl(const FlTextSpec *spec);
bool fl_draw_image_impl(const FlImageSpec *spec);
bool fl_place_editbox_impl(const FlEditSpec *spec);
bool fl_place_listbox_impl(const FlListSpec *spec);
bool fl_place_checkbox_impl(const FlCheckboxSpec *spec);

bool fl_checkbox_is_checked(const char *name);
bool fl_checkbox_is_hovered(const char *name);

#define  fl_checkbox_hovered fl_checkbox_is_hovered
#define  fl_checkbox_clicked fl_checkbox_is_clicked

#define fl_place_button(...) fl_place_checkbox_impl(&(FlCheckboxSpec){.border=true, __VA_ARGS__,.button=true})
#define fl_place_checkbox(...) fl_place_checkbox_impl(&(FlCheckboxSpec){.border=true, __VA_ARGS__,.checkbox=true})

#define fl_button_is_clicked fl_checkbox_is_checked
#define fl_button_is_hovered fl_checkbox_is_hovered
#define fl_button_clicked fl_checkbox_checked
#define fl_button_hovered fl_checkbox_hovered

#define fl_draw_text(...) fl_draw_text_impl(&(FlTextSpec){__VA_ARGS__})
#define fl_draw_image(...) fl_draw_image_impl(&(FlImageSpec){__VA_ARGS__})
#define fl_place_editbox(...) fl_place_editbox_impl(&(FlEditSpec){__VA_ARGS__})
#define fl_place_listbox(...) fl_place_listbox_impl(&(FlListSpec){__VA_ARGS__})

bool fl_listbox_begin(const FlListSpec *spec);
bool fl_listbox_next(FlListItemCtx *out);
void fl_listbox_end(void);


bool fl_draw_text_anchored(FlAnchor anchor, int offset_x, int offset_y, const char *text);
bool fl_place_editbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *buffer);
bool fl_place_listbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char **items, int count);
bool fl_draw_image_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *path);


void fl_layout_begin_column_anchored(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_column(int start_x, int start_y, int gap);
void fl_layout_end_column(void);
void fl_layout_begin_row_anchored(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_row(int start_x, int start_y, int gap);
void fl_layout_end_row(void);
bool fl_draw_text_auto(const char *text);
bool fl_place_editbox_auto(int w, int h, const char *buffer);
bool fl_place_listbox_auto(int w, int h, const char **items, int count);
bool fl_draw_image_auto(int w, int h, const char *path);

typedef struct FlWidget FlWidget;

typedef enum {
    FL_WIDGET_SLOT_TOP_LEFT,
    FL_WIDGET_SLOT_TOP_RIGHT,
    FL_WIDGET_SLOT_BOTTOM_LEFT,
    FL_WIDGET_SLOT_BOTTOM_RIGHT,
    FL_WIDGET_FREE
} FlWidgetSlot;

typedef struct {
    const char *name;
    int w;
    int h;
    int slot;
    int order;
    int gap;
    int pad;                       
    int fps;
    ColorRef color;
    bool no_bg;
    void (*on_click)(void);
    void (*on_hover)(void);
    FlWidgetRenderFn render;
    int x;
    int y;
} FlWidgetSpec;

#define FL_WIDGET_MAX 32
#define FL_WIDGET_NAME_MAX 64
#define FL_WIDGET_DEFAULT_PAD 12
#define FL_WIDGET_PROVISIONAL_W 160
#define FL_WIDGET_PROVISIONAL_H 48

/**
 * @brief Widget structure.
 *
 * Fields 
 * - `name` — Widget identifier.
 * - `w`, `h` — Widget size.
 * - `slot` — Positioning slot See `FlWidgetSlot`.
 * - `fps` — How many times the widget runs every second.
 * - `bg_color` — Color of background See 'FlColor'.
 * - `no_bg` — Determines if background is drawn at all.
 * - `x`, `y` — Widget position.
 * - `on_click` — Click callback function (void void).
 * - `on_hover` — Hover callback function (void void).
 * - `render` — Render callback function See `FlWidgetRenderFn`.
 */
struct FlWidget {
    char name[FL_WIDGET_NAME_MAX];
    int w;
    int h;
    int gap_override;
    int fps;
    int slot;
    int order;
    int seq;
    bool order_set;
    bool free_pos;
    int x;
    int y;
    bool auto_w;
    bool auto_h;
    int origin_x;
    int origin_y;
    ColorRef bg_color;
    bool has_bg;
    bool no_bg;
    int pad;
    bool mapped;
    volatile bool dead;
    bool hover_running;
    void (*hover_thread)(void);
    PlatformWidget *win;
    FlWidgetRenderFn render;
    unsigned long frame;
    FlTimer created_at;
    char *owner;
    unsigned gen;
    void (*on_click)(void);
    void (*on_hover)(void);
    bool clickable;
    bool hovered;
};

/**
 * @def Create a new widget.
 * @param ... Fields of FlWidgetSpec. See `FlWidgetSpec`.
 */
#define fl_widget(...) fl_widget_create(&(FlWidgetSpec){__VA_ARGS__})

/**
 * @brief Create a new widget.
 * @param spec  The widget specification. See `FlWidgetSpec`.
 * @return      Pointer to the created FlWidget. See `FlWidget`.
 */
FlWidget *fl_widget_create(const FlWidgetSpec *spec);

/**
 * @brief Check if a widget is valid.
 * @param widget  Pointer to the widget. See `FlWidget`.
 * @return        True if valid, false otherwise.
 */
bool fl_widget_valid(FlWidget *widget);

/**
 * @brief Get the position of a widget by name.
 * @param name  Name of the widget.
 * @return      `FlPos` of the widget. See `FlPos`.
 */
FlPos fl_get_widget_pos(const char *name);

/**
 * @brief Show a widget.
 * @param widget  Pointer to the widget. See `FlWidget`.
 */
void fl_widget_show(FlWidget *widget);

/**
 * @brief Hide a widget.
 * @param widget  Pointer to the widget. See `FlWidget`.
 */
void fl_widget_hide(FlWidget *widget);

/**
 * @brief Set the render function for a widget.
 * @param widget  Pointer to the widget. See `FlWidget`.
 * @param render  Render function. See `FlWidgetRenderFn`.
 */
void fl_widget_set_render(FlWidget *widget, FlWidgetRenderFn render);

/**
 * @brief Load an image from a path.
 * @param path  Path to the image.
 * @return      Image handle (pointer), or NULL on failure.
 */
void* fl_load_image(const char *path);

/**
 * @brief Free a loaded image.
 * @param image  Image handle (pointer) to free.
 */
void fl_free_image(void *image);

#define FL_DRAW_ALPHA_OPAQUE 255
#define FL_DRAW_ALPHA_TRANSPARENT 128

/**
 * @brief Rectangle drawing specification.
 *
 * Fields:
 * - `x`, `y` — Top-left corner.
 * - `w`, `h` — Width and height.
 * - `color` — Draw color.
 * - `thickness` — Line thickness.
 * - `transparent` — Whether the rectangle is transparent.
 * - `filled` — Whether the rectangle is filled.
 * - `radius` — Corner radius if `rounded` is true.
 * - `rounded` — Whether the rectangle has rounded corners.
 */
typedef struct {
    int x; int y; int w; int h;
    unsigned long color;
    float thickness;
    bool transparent;
    bool filled;
    int radius;
    bool rounded;
} FlRectSpec;

/**
 * @brief Line drawing specification.
 *
 * Fields:
 * - `x1`, `y1` — Start point.
 * - `x2`, `y2` — End point.
 * - `color` — Draw color.
 * - `thickness` — Line thickness.
 */
typedef struct {
    int x1; int y1; int x2; int y2;
    unsigned long color;
    float thickness;
} FlLineSpec;

/**
 * @brief Circle drawing specification.
 *
 * Fields:
 * - `x`, `y` — Center position.
 * - `radius` — Radius.
 * - `color` — Draw color.
 * - `thickness` — Line thickness.
 */
typedef struct {
    int x; int y; int radius;
    unsigned long color;
    float thickness;
} FlCircleSpec;

/**
 * @brief Implementation for drawing a rectangle.
 * @param spec  Rectangle specification. See `FlRectSpec`.
 * @return      True on success, false on failure.
 */
bool fl_draw_rect_impl(const FlRectSpec *spec);

/**
 * @brief Implementation for drawing a line.
 * @param spec  Line specification. See `FlLineSpec`.
 * @return      True on success, false on failure.
 */
bool fl_draw_line_impl(const FlLineSpec *spec);

/**
 * @brief Implementation for drawing a circle.
 * @param spec  Circle specification. See `FlCircleSpec`.
 * @return      True on success, false on failure.
 */
bool fl_draw_circle_impl(const FlCircleSpec *spec);

/**
 * @def Draw a rectangle.
 * @param ... Fields of `FlRectSpec`. See `FlRectSpec`.
 */
#define fl_draw_rect(...) fl_draw_rect_impl(&(FlRectSpec){__VA_ARGS__})

/**
 * @def Draw a line.
 * @param ... Fields of `FlLineSpec`. See `FlLineSpec`.
 */
#define fl_draw_line(...) fl_draw_line_impl(&(FlLineSpec){__VA_ARGS__})

/**
 * @def Draw a circle.
 * @param ... Fields of `FlCircleSpec`. See `FlCircleSpec`.
 */
#define fl_draw_circle(...) fl_draw_circle_impl(&(FlCircleSpec){__VA_ARGS__})

/**
 * @brief Get a random number between min and max inclusive.
 * @param min  Minimum value.
 * @param max  Maximum value.
 * @return     The random number.
 */
int fl_rand_num_between(int min, int max);

/**
 * @brief Get the width of a text string.
 * @param text      The text.
 * @param fontsize  The font size.
 * @return          Width in pixels.
 */
int fl_get_text_width(const char *text, int fontsize);

/**
 * @brief Get the height of a text string.
 * @param text  The text.
 * @return      Height in pixels.
 */
int fl_get_text_height(const char *text);

/**
 * @brief Get the ink height of a glyph string.
 * @param text      The text.
 * @param fontsize  The font size.
 * @return          Ink height in pixels.
 */
int fl_get_glyph_ink_height(const char *text, int fontsize);

/**
 * @brief Perform an HTTP GET request.
 * @param url  The URL.
 * @return     Response body (caller must free), or NULL.
 */
char *fl_http_get(const char *url);

/**
 * @brief Perform an HTTP GET request with size limit.
 * @param url        The URL.
 * @param max_bytes  Maximum bytes to read.
 * @param out_len    Pointer to store content length.
 * @return           Response body (caller must free), or NULL.
 */
char *fl_http_get_limited(const char *url, size_t max_bytes, size_t *out_len);

/**
 * @brief Download a file from a URL.
 * @param url        The URL.
 * @param save_path  Path to save.
 * @return           True on success, false on failure.
 */
bool fl_http_download(const char *url, const char *save_path);

/**
 * @brief Perform an HTTP POST request.
 * @param url           The URL.
 * @param content_type  Content type string.
 * @param body          Request body.
 * @return              Response body (caller must free), or NULL.
 */
char *fl_http_post(const char *url, const char *content_type, const char *body);

/**
 * @brief Callback for async HTTP GET request.
 */
typedef void (*FlHttpCallback)(const char *body, bool ok, void *userdata);

/**
 * @brief Perform an asynchronous HTTP GET request.
 * @param url       The URL.
 * @param cb        Callback function.
 * @param userdata  Pointer to user data.
 */
void fl_http_get_async(const char *url, FlHttpCallback cb, void *userdata);

/**
 * @brief A single command-line flag.
 *
 * Fields:
 * - `name` — Flag name.
 * - `value` — Flag value.
 * - `has_value` — True if flag has a value.
 */
typedef struct {
    char name[64];
    char value[256];
    bool has_value;
} FlFlag;

/**
 * @brief A list of command-line flags.
 *
 * Fields:
 * - `flags` — Array of `FlFlag`. See `FlFlag`.
 * - `count` — Number of flags.
 */
typedef struct {
    FlFlag* flags;
    size_t count;
} FlFlagList;

/**
 * @brief Parse command-line flags from a string.
 * @param input  Input string.
 * @return       The parsed flag list. See `FlFlagList`.
 */
FlFlagList fl_parse_flags(const char* input);

/**
 * @brief Free a flag list.
 * @param list  The flag list to free.
 */
void fl_free_flags(FlFlagList* list);

/**
 * @brief Start a timer.
 * @return  Timer handle (See `FlTimer`).
 */
FlTimer fl_start_timer(void);

/**
 * @brief End a timer and get elapsed time.
 * @param t  Timer handle.
 * @return   Elapsed time in seconds.
 */
double fl_end_timer(FlTimer t);

/**
 * @brief Start profiling.
 * @return  Profiling timer handle (See `FlTimer`).
 */
FlTimer fl_profile_start(void);

/**
 * @brief End profiling.
 * @param timer  Profiling timer handle.
 */
void fl_profile_end(FlTimer timer);

/**
 * @brief Execute a callback after a delay.
 * @param seconds   Delay in seconds.
 * @param callback  Callback function.
 */
void fl_run_after_time(double seconds, void (*callback)(void));

/**
 * @brief Register a callback for directory changes.
 * @param dir_path  Directory path.
 * @param callback  Callback function (See `DirChangeCallback`).
 * @return          Handle string (caller must free), or NULL on error.
 */
char* fl_on_dir_changed(const char* dir_path, DirChangeCallback callback);

/**
 * @brief Check if a string contains another string.
 * @param needle    Substring to find.
 * @param haystack  String to search in.
 * @return          True if found, false otherwise.
 */
bool fl_string_contains(const char* needle, const char* haystack);

/**
 * @brief Get the working directory.
 * @return  Path string (caller must free), or NULL on failure.
 */
char* fl_get_working_dir(void);

/**
 * @brief Resolve a data path.
 * @param name  Name of the data.
 * @return      Resolved path (caller must free), or NULL on failure.
 */
char* fl_resolve_data_path(const char* name);

/**
 * @brief Resolve a feature path.
 * @param name  Name of the feature.
 * @return      Resolved path (caller must free), or NULL on failure.
 */
char* fl_resolve_feature_path(const char* name);

/**
 * @brief Resolve the user path.
 * @return  User path string (caller must free), or NULL on failure.
 */
char* fl_resolve_user_path(void);

/**
 * @brief Get the current username.
 * @return  Username string (caller must free), or NULL on failure.
 */
char* fl_get_user_name(void);

/**
 * @brief Check if the process is running as root/admin.
 * @return  True if root/admin, false otherwise.
 */
bool fl_is_root(void);

typedef void* FlLibrary;

/**
 * @brief Open a dynamic library.
 * @param path  Path to the library.
 * @return      Library handle, or NULL on failure.
 */
FlLibrary fl_library_open(const char *path);

/**
 * @brief Get a symbol from a library.
 * @param lib   Library handle.
 * @param name  Symbol name.
 * @return      Symbol pointer, or NULL on failure.
 */
void* fl_library_symbol(FlLibrary lib, const char *name);

/**
 * @brief Close a library handle.
 * @param lib  Library handle.
 */
void fl_library_close(FlLibrary lib);

/**
 * @brief Get the last library loading error.
 * @return  Error message string.
 */
const char *fl_library_error(void);

typedef void* FlThread;
typedef void* FlMutex;
typedef void* (*FlThreadFunc)(void* arg);

/**
 * @brief Create a new thread.
 * @param fn   Thread function.
 * @param arg  Argument to pass.
 * @return     Thread handle.
 */
FlThread fl_thread_create(FlThreadFunc fn, void* arg);

/**
 * @brief Join a thread.
 * @param thread  Thread handle.
 */
void fl_thread_join(FlThread thread);

/**
 * @brief Create a new mutex.
 * @return  Mutex handle.
 */
FlMutex fl_mutex_create(void);

/**
 * @brief Destroy a mutex.
 * @param mutex  Mutex handle.
 */
void fl_mutex_destroy(FlMutex mutex);

/**
 * @brief Lock a mutex.
 * @param mutex  Mutex handle.
 */
void fl_mutex_lock(FlMutex mutex);

/**
 * @brief Unlock a mutex.
 * @param mutex  Mutex handle.
 */
void fl_mutex_unlock(FlMutex mutex);

/**
 * @brief Hashmap structure.
 *
 * Fields:
 * - `key` — Key string.
 * - `value` — Value.
 */
typedef struct {
    char* key;
    long value;
} FlHashMap;

/**
 * @brief Initialize a hashmap.
 * @param map  Pointer to the hashmap pointer.
 */
void fl_hashmap_init(FlHashMap** map);

/**
 * @brief Free a hashmap.
 * @param map  Pointer to the hashmap pointer.
 */
void fl_hashmap_free(FlHashMap** map);

/**
 * @brief Put a value into the hashmap.
 * @param map    Pointer to the hashmap pointer.
 * @param key    Key string.
 * @param value  Value.
 */
void fl_hashmap_put(FlHashMap** map, const char* key, long value);

/**
 * @brief Add a value to an existing key (delta).
 * @param map    Pointer to the hashmap pointer.
 * @param key    Key string.
 * @param delta  Value to add.
 */
void fl_hashmap_add(FlHashMap** map, const char* key, long delta);

/**
 * @brief Get a value from the hashmap.
 * @param map  The hashmap.
 * @param key  The key.
 * @param def  Default value.
 * @return     Value.
 */
long fl_hashmap_get(FlHashMap* map, const char* key, long def);

/**
 * @brief Check if key exists.
 * @param map  The hashmap.
 * @param key  The key.
 * @return     True if exists, false otherwise.
 */
bool fl_hashmap_has(FlHashMap* map, const char* key);

/**
 * @brief Delete a key.
 * @param map  Pointer to the hashmap pointer.
 * @param key  Key to delete.
 * @return     True on success, false on failure.
 */
bool fl_hashmap_del(FlHashMap** map, const char* key);

/**
 * @brief Get hashmap size.
 * @param map  The hashmap.
 * @return     Number of elements.
 */
size_t fl_hashmap_len(FlHashMap* map);

/**
 * @brief Get hashmap element by index.
 * @param map    The hashmap.
 * @param index  The index.
 * @return       Borrowed pointer to `FlHashMap`. See `FlHashMap`.
 */
const FlHashMap* fl_hashmap_at(FlHashMap* map, size_t index);

/**
 * @brief Get all window titles.
 * @return  A `FlWindowList`. See `FlWindowList`.
 */
FlWindowList fl_get_all_window_titles(void);

/**
 * @brief Get all window names.
 * @return  A `FlWindowList`. See `FlWindowList`.
 */
FlWindowList fl_get_all_window_names(void);

/**
 * @brief Get the title of the active window.
 * @return  The title string (caller must free), or NULL.
 */
char* fl_get_active_window_title(void);

/**
 * @brief Get the application ID of the active window.
 * @return  The app ID string (caller must free), or NULL.
 */
char* fl_get_active_window_app_id(void);

/**
 * @brief Check if a window is active.
 * @param title  The window title.
 * @return       True if active, false otherwise.
 */
bool fl_is_window_active(char* title);

/**
 * @brief Get the color of a pixel at a position.
 * @param pos  The position. See `FlPos`.
 * @return     The color at that position.
 */
FlColor fl_get_pixel_color(FlPos pos);

/**
 * @brief Move the mouse cursor to a position.
 * @param pos  The position. See `FlPos`.
 * @return     True on success, false otherwise.
 */
bool fl_move_to_pos(FlPos pos);

/**
 * @brief Click at a position.
 * @param pos  The position. See `FlPos`.
 * @return     True on success, false otherwise.
 */
bool fl_click_at_pos(FlPos pos);

/**
 * @brief Perform a left mouse click.
 * @return  True on success, false otherwise.
 */
bool fl_left_click(void);

/**
 * @brief Perform a right mouse click.
 * @return  True on success, false otherwise.
 */
bool fl_right_click(void);

/**
 * @brief Emit a mouse button event.
 * @param button  The button index.
 * @param down    True for press, false for release.
 * @return        True on success, false otherwise.
 */
bool fl_emit_mouse_button(int button, bool down);

/**
 * @brief Emit a mouse button event at a position.
 * @param pos     The position. See `FlPos`.
 * @param button  The button index.
 * @param down    True for press, false for release.
 * @return        True on success, false otherwise.
 */
bool fl_emit_mouse_button_at(FlPos pos, int button, bool down);

/**
 * @brief Click a specific button at a position.
 * @param pos     The position. See `FlPos`.
 * @param button  The button index.
 * @return        True on success, false otherwise.
 */
bool fl_click_button_at(FlPos pos, int button);

/**
 * @brief Perform mouse scroll.
 * @param notches  Number of notches.
 * @return         True on success, false otherwise.
 */
bool fl_mouse_scroll(int notches);

/**
 * @brief Emit a key event.
 * @param vk    Virtual key code.
 * @param down  True for press, false for release.
 */
void fl_emit_key(int vk, bool down);

#define fl_move_mouse fl_move_to_pos
#define fl_click() fl_left_click()

/**
 * @brief Move mouse relative to current position.
 * @param rx  Relative X offset.
 * @param ry  Relative Y offset.
 * @return    True on success, false otherwise.
 */
bool fl_move_to_pos_rel(int rx, int ry);

/**
 * @brief Click relative to current position.
 * @param rx  Relative X offset.
 * @param ry  Relative Y offset.
 * @return    True on success, false otherwise.
 */
bool fl_click_at_pos_rel(int rx, int ry);

/**
 * @brief Emit mouse button event relative to position.
 * @param rx      Relative X offset.
 * @param ry      Relative Y offset.
 * @param button  Button index.
 * @param down    True for press, false for release.
 * @return        True on success, false otherwise.
 */
bool fl_emit_mouse_button_at_rel(int rx, int ry, int button, bool down);

/**
 * @brief Click button relative to position.
 * @param rx      Relative X offset.
 * @param ry      Relative Y offset.
 * @param button  Button index.
 * @return        True on success, false otherwise.
 */
bool fl_click_button_at_rel(int rx, int ry, int button);

/**
 * @brief Type a string of text.
 * @param s  The text string.
 * @return   True on success, false otherwise.
 */
bool fl_type_text(const char *s);

/**
 * @brief Sleep for a duration.
 * @param seconds  Time in seconds.
 */
void fl_sleep(double seconds);

/**
 * @brief Check if a key is down.
 * @param vk  Virtual key code.
 * @return    True if down, false otherwise.
 */
bool fl_is_key_down(int vk);

/**
 * @brief Check if a key is pressed.
 * @param vk  Virtual key code.
 * @return    True if pressed, false otherwise.
 */
bool fl_is_key_pressed(int vk);

/**
 * @brief Check if a key is released.
 * @param vk  Virtual key code.
 * @return    True if released, false otherwise.
 */
bool fl_is_key_released(int vk);

/**
 * @brief Check if Ctrl is pressed.
 * @return  True if pressed, false otherwise.
 */
bool fl_ctrl_pressed(void);

/**
 * @brief Check if Shift is pressed.
 * @return  True if pressed, false otherwise.
 */
bool fl_shift_pressed(void);

/**
 * @brief Check if Alt is pressed.
 * @return  True if pressed, false otherwise.
 */
bool fl_alt_pressed(void);

/**
 * @brief Check if Super/Win key is pressed.
 * @return  True if pressed, false otherwise.
 */
bool fl_super_pressed(void);

/**
 * @brief Set key callback.
 * @param cb  Callback function.
 * @param ud  User data.
 */
void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);

/**
 * @brief Get the last key pressed.
 * @return  Virtual key code.
 */
int fl_get_key_pressed(void);

/**
 * @brief Get the last character pressed.
 * @return  Character code.
 */
char fl_get_char_pressed(void);

#define fl_key_pressed        fl_is_key_pressed
#define fl_key_released       fl_is_key_released
#define fl_ctrl_down          fl_ctrl_pressed
#define fl_shift_down         fl_shift_pressed
#define fl_alt_down           fl_alt_pressed
#define fl_super_down         fl_super_pressed
#define fl_is_ctrl_pressed    fl_ctrl_pressed
#define fl_is_shift_pressed   fl_shift_pressed
#define fl_is_alt_pressed     fl_alt_pressed
#define fl_is_super_pressed   fl_super_pressed

/**
 * @brief Key event structure.
 */
typedef struct {
    int vk;
    int scancode;
    int mods;
    bool down;
    bool repeat;
    unsigned long long t_us;
} FlKeyEvent;

#define FL_MOUSE_LEFT    1
#define FL_MOUSE_RIGHT   2
#define FL_MOUSE_MIDDLE  3
#define FL_MOUSE_SIDE1   4
#define FL_MOUSE_SIDE2   5
#define FL_MOUSE_BACK    6
#define FL_MOUSE_FORWARD 7
#define FL_MOUSE_WHEEL   8

/**
 * @brief Mouse event structure.
 */
typedef struct {
    int button;
    int x;
    int y;
    int mods;
    bool down;
    bool repeat;
    unsigned long long t_us;
} FlMouseEvent;

/**
 * @brief Poll for mouse events.
 * @param out  Pointer to store the event. See `FlMouseEvent`.
 * @return     True if event polled, false otherwise.
 */
bool fl_poll_mouse(FlMouseEvent *out);

/**
 * @brief Get mouse event queue length.
 * @return  Queue length.
 */
int fl_mouse_queue_len(void);

/**
 * @brief Get number of dropped mouse events.
 * @return  Count of dropped events.
 */
unsigned long fl_mouse_queue_dropped(void);

/**
 * @brief Clear mouse event queue.
 */
void fl_clear_mouse_queue(void);

/**
 * @brief Get cursor position.
 * @return  `FlPos` of cursor. See `FlPos`.
 */
FlPos fl_get_cursor_pos(void);

#define fl_get_mouse_pos() fl_get_cursor_pos()

/**
 * @brief Check if mouse button is pressed.
 * @param button  Button index.
 * @return        True if pressed, false otherwise.
 */
bool fl_mouse_pressed(int button);

/**
 * @brief Poll for key events.
 * @param out  Pointer to store the event. See `FlKeyEvent`.
 * @return     True if event polled, false otherwise.
 */
bool fl_poll_key(FlKeyEvent *out);

/**
 * @brief Get key event queue length.
 * @return  Queue length.
 */
int fl_key_queue_len(void);

/**
 * @brief Get number of dropped key events.
 * @return  Count of dropped events.
 */
unsigned long fl_key_queue_dropped(void);

/**
 * @brief Clear key event queue.
 */
void fl_clear_key_queue(void);

/**
 * @brief Get character for a key event.
 * @param vk        Virtual key code.
 * @param scancode  Scan code.
 * @param mods      Modifier state.
 * @return          Character code.
 */
int fl_char_for_key(int vk, int scancode, int mods);

/**
 * @brief Control center entry specification.
 *
 * Fields:
 * - `display_name` — Display name.
 * - `internal_name` — Internal unique name.
 * - `icon` — Icon path.
 */
typedef struct {
    const char *display_name;
    const char *internal_name;
    bool (*on_enter)(void);
    bool (*on_ctrl_enter)(void);
    bool (*on_shift_enter)(void);
    bool (*on_ctrl_shift_enter)(void);
    bool (*on_ctrl_alt_enter)(void);
    bool (*on_shift_alt_enter)(void);
    bool (*on_tab)(void);
    void (*on_action)(void);
    const char *icon;
    const char *type;
} FlCcEntrySpec;

/**
 * @def Add entry to control center.
 * @param ... Fields of `FlCcEntrySpec`. See `FlCcEntrySpec`.
 */
#define fl_add_to_cc(...) fl_add_to_cc_impl(&(FlCcEntrySpec){__VA_ARGS__})

/**
 * @brief Add entry to control center (internal).
 * @param spec  The entry specification. See `FlCcEntrySpec`.
 * @return      True on success, false otherwise.
 */
bool fl_add_to_cc_impl(const FlCcEntrySpec *spec);

typedef struct {
    const char *alias;
    const char *target;
} FlAliasSpec;

/**
 * @def Add alias.
 * @param ... Fields of `FlAliasSpec`. See `FlAliasSpec`.
 */
#define fl_add_alias(...) fl_add_alias_impl(&(FlAliasSpec){__VA_ARGS__})

/**
 * @brief Add alias (internal).
 * @param spec  The alias specification. See `FlAliasSpec`.
 * @return      True on success, false otherwise.
 */
bool fl_add_alias_impl(const FlAliasSpec *spec);

/**
 * @brief Get the number of registered aliases.
 * @return  The count.
 */
int fl_alias_count(void);

/**
 * @brief Get the alias for a target.
 * @param target  The target name.
 * @return        The alias string, or NULL if not found.
 */
const char *fl_alias_for(const char *target);

/**
 * @brief Get the number of control center entries.
 * @return  The count.
 */
int fl_cc_entry_count(void);

/**
 * @brief Get a control center entry by index.
 * @param index  The index.
 * @return       Borrowed pointer to `FlCcEntrySpec`. See `FlCcEntrySpec`.
 */
const FlCcEntrySpec *fl_cc_entry_at(int index);

/**
 * @brief Purge all control center entries owned by specified paths.
 * @param path_a  Path A.
 * @param path_b  Path B.
 */
void fl_cc_registry_purge_owner(const char *path_a, const char *path_b);

/**
 * @brief Add dependency (internal).
 * @param dep_name    Dependency name.
 * @param owner_file  Owner file path.
 * @return            True on success, false otherwise.
 */
bool fl_add_dep_impl(const char *dep_name, const char *owner_file);

/**
 * @def Add dependency.
 * @param ... Parameters for `fl_add_dep_impl`.
 */
#define fl_add_dep(...) fl_add_dep_impl(__VA_ARGS__, __FILE__)

/**
 * @brief Hotkey registration specification.
 *
 * Fields:
 * - `hotkey` — Hotkey string.
 * - `cb` — Callback function.
 * - `swallow` — Whether to swallow the hotkey.
 */
typedef struct {
    const char *hotkey;
    void (*cb)(void);
    bool (*should_swallow)(void);
    const char *whitelist[16];
    const char *blacklist[16];
    bool swallow;
} FlHotkeySpec;

/**
 * @def Register a hotkey.
 * @param ... Fields of `FlHotkeySpec`. See `FlHotkeySpec`.
 */
#define fl_register_hotkey(...) fl_register_hotkey_impl(&(FlHotkeySpec){__VA_ARGS__})

/**
 * @brief Register a hotkey (internal implementation).
 * @param spec  The hotkey specification. See `FlHotkeySpec`.
 * @return      True on success, false otherwise.
 */
bool fl_register_hotkey_impl(const FlHotkeySpec *spec);

/**
 * @brief Hotkey remapping specification.
 *
 * Fields:
 * - `src` — Source hotkey.
 * - `dst` — Destination hotkey.
 */
typedef struct {
    const char *src;
    const char *dst;
    const char *whitelist[8];
    const char *blacklist[8];
} FlRemappingSpec;

/**
 * @def Register a hotkey remapping.
 * @param ... Fields of `FlRemappingSpec`. See `FlRemappingSpec`.
 */
#define fl_register_remapping(...) fl_register_remapping_impl(&(FlRemappingSpec){__VA_ARGS__})

/**
 * @brief Register a hotkey remapping (internal implementation).
 * @param spec  The remapping specification. See `FlRemappingSpec`.
 * @return      True on success, false otherwise.
 */
bool fl_register_remapping_impl(const FlRemappingSpec *spec);

/**
 * @brief Replay a hotkey.
 * @param vk    Virtual key code.
 * @param mods  Modifier state.
 * @return      True on success, false otherwise.
 */
bool fl_replay_hotkey(int vk, int mods);

typedef void (*FlSnippetFunc)(void);

/**
 * @brief Snippet registration specification.
 *
 * Fields:
 * - `trigger` — Trigger string.
 * - `text` — Text content.
 * - `fn` — Callback function.
 */
typedef struct {
    const char *trigger;
    const char *text;
    FlSnippetFunc fn;
    const char *whitelist[16];
    const char *blacklist[16];
} FlSnippetSpec;

/**
 * @def Register a snippet.
 * @param ... Fields of `FlSnippetSpec`. See `FlSnippetSpec`.
 */
#define fl_register_snippet(...) fl_register_snippet_impl(&(FlSnippetSpec){__VA_ARGS__})

/**
 * @brief Register a snippet (internal implementation).
 * @param spec  The snippet specification. See `FlSnippetSpec`.
 * @return      True on success, false otherwise.
 */
bool fl_register_snippet_impl(const FlSnippetSpec *spec);

void fl_reload_feature(char* name);
void fl_reload_all_features(void);
extern volatile int g_render_depth;
extern void (*g_pending_post_render)(void);
int fl_get_pid();

#if defined(_WIN32)
#define FLINT_FEATURE_EXPORT __declspec(dllexport)
#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @def Initialize a Flint feature.
 */
#define FLINT_FEATURE_INIT FLINT_FEATURE_EXPORT __attribute__((constructor)) void FlintFeatureInit(void)

/**
 * @def Shutdown a Flint feature.
 */
#define FLINT_FEATURE_SHUTDOWN FLINT_FEATURE_EXPORT void FlintFeatureShutdown(void)

#include "core/cap_registry.h"

/**
 * @def Register a capability.
 * @param key  Capability key.
 * @param fn   Capability function.
 */
#define fl_cap_register(key, fn)  fl_cap_register_abi((key), FL_CAP_ABI_V1, (fn))

/**
 * @def Resolve a capability.
 * @param key  Capability key.
 */
#define fl_cap_resolve(key)       fl_cap_resolve_abi((key), FL_CAP_ABI_V1)

/**
 * @def Bind a capability function pointer.
 * @param var   Variable name.
 * @param key   Capability key.
 * @param type  Capability function type.
 */
#define FL_CAP_BIND(var, key, type)                                   \
    static type var##_fn;                                             \
    static unsigned var##_gen;                                        \
    type var = NULL;                                                  \
    if (var##_gen != fl_cap_generation()) {                           \
        fl_cap_bind_declare(key, (void*)&var##_fn);                   \
        var##_fn = (type)fl_cap_resolve(key);                         \
        var##_gen = fl_cap_generation();                              \
    }                                                                 \
    var = var##_fn

#define FL_WHITE        RGB(255, 255, 255)
#define FL_BLACK        RGB(0, 0, 0)
#define FL_RED          RGB(255, 0, 0)
#define FL_GREEN        RGB(0, 255, 0)
#define FL_BLUE         RGB(0, 0, 255)
#define FL_YELLOW       RGB(255, 255, 0)
#define FL_CYAN         RGB(0, 255, 255)
#define FL_MAGENTA      RGB(255, 0, 255)
#define FL_ORANGE       RGB(255, 165, 0)
#define FL_PURPLE       RGB(128, 0, 128)
#define FL_PINK         RGB(255, 192, 203)
#define FL_BROWN        RGB(139, 69, 19)
#define FL_LIME         RGB(50, 205, 50)
#define FL_TEAL         RGB(0, 128, 128)
#define FL_NAVY         RGB(0, 0, 128)
#define FL_MAROON       RGB(128, 0, 0)
#define FL_OLIVE        RGB(128, 128, 0)
#define FL_SILVER       RGB(192, 192, 192)
#define FL_GOLD         RGB(255, 215, 0)
#define FL_INDIGO       RGB(75, 0, 130)
#define FL_VIOLET       RGB(238, 130, 238)
#define FL_TURQUOISE    RGB(64, 224, 208)
#define FL_SALMON       RGB(250, 128, 114)
#define FL_CORAL        RGB(255, 127, 80)
#define FL_CRIMSON      RGB(220, 20, 60)
#define FL_CHOCOLATE    RGB(210, 105, 30)
#define FL_GRAY         RGB(128, 128, 128)
#define FL_GREY         RGB(128, 128, 128)
#define FL_LIGHT_GRAY   RGB(211, 211, 211)
#define FL_DARK_GRAY    RGB(14, 14, 14)
#define FL_LIGHT_GREY   RGB(211, 211, 211)
#define FL_DARK_GREY    RGB(14, 14, 14)

/** @} */

#endif
