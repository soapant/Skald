#ifndef MISC_X11_H
#define MISC_X11_H

#include "core/types.h"

bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size);
int Platform_FileAppend(MappedFile* mf, const char* text);
void Platform_FileClose(MappedFile* mf);
int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text);
Line* Platform_FileGetLines(MappedFile* mf, size_t* outLineCount);
char** Platform_FileGetStrings(MappedFile* mf, size_t* outLineCount, NewlineInfo** outNewlines, size_t* outNewlineCount);
void Platform_CleanUpFile(MappedFile* mf, NewlineInfo* newlines, size_t newlineCount);
void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight);
FontHandle Platform_CreateFont(int size, const char* face_name);
void Platform_DestroyFont(FontHandle font);
FontHandle Platform_CreateFontOnDisplay(Display* dpy, int size, const char* face_name);
const char* Platform_GetRealUser(void);
const char* Platform_GetRealHome(void);
bool Platform_IsRunningAsRoot(void);

#endif
