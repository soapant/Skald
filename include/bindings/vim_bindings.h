
#ifndef VIM_BINDINGS_H
#define VIM_BINDINGS_H

#include "bindings/vim_motions.h"
#include "config.h"

typedef void (*VimMotionHandler)(FlintContext*);

extern const MotionBinding G_VIM_BINDINGS[];
extern const SequenceBinding G_VIM_SEQUENCE_BINDINGS[];

int VimBindings_Get(const MotionBinding** out);
int VimSequenceBindings_Get(const SequenceBinding** out);

#endif
