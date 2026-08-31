#include <stdio.h>
#include "common.h"
#include "core/platform.h"
#include "config.h"
#if defined(_WIN32)
#include <shlwapi.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif


