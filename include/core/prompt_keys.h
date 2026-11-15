#ifndef PROMPT_KEYS_H
#define PROMPT_KEYS_H

#include <stdbool.h>
#include "core/types.h"
#include "core/window_manager.h"

bool prompt_handle_key(PromptState *state, const PopupKeyEvent *key);
void prompt_trigger_enter(PromptState *state, bool ctrl, bool shift, bool alt);
bool prompt_handle_list_click(PromptState *state, bool ctrl, bool shift,
                            bool alt);
void prompt_refresh(PromptState *state);
bool prompt_delete_selection(PromptState *state);
void prompt_insert_text(PromptState *state, const char *text, int len);

#endif
