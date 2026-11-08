#ifndef DEFERRED_INIT_H
#define DEFERRED_INIT_H

void DeferredInit_Register(void (*cb)(void));
void DeferredInit_RunAll(void);

#endif
