#ifndef CORE_PROMPT_KEYS_H
#define CORE_PROMPT_KEYS_H

#include <stdbool.h>
#include "core/types.h"
#include "core/window_manager.h"

bool Prompt_HandleKey(PromptState *state, const PopupKeyEvent *key);
void Prompt_TriggerEnter(PromptState *state, bool ctrl, bool shift, bool alt);
bool Prompt_HandleListClick(PromptState *state, bool ctrl, bool shift,
                            bool alt);
void Prompt_Refresh(PromptState *state);
bool Prompt_DeleteSelection(PromptState *state);
void Prompt_InsertText(PromptState *state, const char *text, int len);
void Prompt_RedrawNow(void);

#endif
