#ifndef DESKTOP_HELPERS_X11_H
#define DESKTOP_HELPERS_X11_H

#include <stdbool.h>

bool Desktop_Parse(const char* desktop_path, char exec_line[1024], char workdir[1024], bool* is_terminal);
void Desktop_CleanExec(char exec_line[1024]);

#endif
