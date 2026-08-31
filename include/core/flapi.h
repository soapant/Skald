#ifndef FLAPI_H
#define FLAPI_H

#include "common.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#if defined(_WIN32)
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h> 
#include <pthread.h>
#endif

#endif
