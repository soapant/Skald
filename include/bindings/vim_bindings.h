
#ifndef VIM_BINDINGS_H
#define VIM_BINDINGS_H

#include "bindings/vim_motions.h"
#include "config.h"

extern const MotionBinding G_VIM_BINDINGS[32];

#define VIM_BINDING_COUNT ARRAY_LEN(G_VIM_BINDINGS)

extern const SequenceBinding G_VIM_SEQUENCE_BINDINGS[18];

#define VIM_SEQUENCE_BINDING_COUNT ARRAY_LEN(G_VIM_SEQUENCE_BINDINGS)

#endif