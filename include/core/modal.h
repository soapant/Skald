#ifndef MODAL_H
#define MODAL_H

#include <windows.h>
#include <stdbool.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL,
} EditorMode;

extern EditorMode g_current_mode;
extern bool g_vim_mode_enabled;
extern const char* g_vim_mode;
extern int last_index;
extern char* selection_feature;

void SetMode(EditorMode mode);
const char* GetModeName(void);
EditorMode GetMode(void);
bool IsNormalMode(void);
bool IsInsertMode(void);
bool IsVisualMode(void);

#endif
