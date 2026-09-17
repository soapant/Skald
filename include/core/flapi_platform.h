#ifndef FLAPI_PLATFORM_H
#define FLAPI_PLATFORM_H

#include "core/flapi.h"


bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore);
char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);
bool fl_launch_with_args_platform(const char* path, const char* args);
bool fl_launch_with_args_as_admin_platform(const char* path, const char* args);
bool fl_listbox_begin_platform(const FlListSpec *spec);
bool fl_listbox_next_platform(FlListItemCtx *out);
void fl_listbox_end_platform(void);
int fl_listbox_get_count(void);
bool fl_place_editbox_platform(const FlEditSpec *spec);
void fl_editbox_reset_placed(void);
bool fl_editbox_was_placed(void);
void fl_toggle_play_pause_platform(void);
int fl_get_current_volume_platform(void);
bool fl_set_volume_platform(int volume);
bool fl_decrement_volume_by_platform(int decrement);
bool fl_increment_volume_by_platform(int increment);
bool fl_increment_volume_by_percent_platform(int increment_percent);
bool fl_decrement_volume_by_percent_platform(int decrement_percent);
bool fl_type_text_platform(const char *s);
bool fl_is_key_down_platform(int vk);
bool fl_is_key_pressed_platform(int vk);
bool fl_is_key_released_platform(int vk);
bool fl_is_key_held_platform(int vk);
void fl_on_key_platform(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);
int fl_get_key_pressed_platform(void);
char fl_get_char_pressed_platform(void);

#endif
