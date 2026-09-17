#include "platform/input_state.h"
#include <stddef.h>

static bool g_down[256] = {0};
static bool g_pressed[256] = {0};
static bool g_released[256] = {0};
static void (*g_cb)(int vk, bool down, int mods, void *ud) = NULL;
static void *g_ud = NULL;

static int norm_vk(int vk) {
  if(vk>='a'&&vk<='z') return vk-'a'+'A'; return vk; 
}
void Platform_InputState_Update(int vk, bool down, int mods){
 vk=norm_vk(vk);
 if(vk<0||vk>=256) return;
 bool was=g_down[vk];
 if(down){ g_down[vk]=true; g_pressed[vk]=true; g_released[vk]=false; }
 else { if(was){ g_down[vk]=false; g_released[vk]=true; } g_pressed[vk]=false; if(!was) g_released[vk]=false; }
 if(g_cb) g_cb(vk,down,mods,g_ud);
}

bool Platform_IsKeyDown(int vk){
 vk=norm_vk(vk);
 if(vk<0||vk>=256) return false;
 return g_down[vk];
}

bool Platform_WasKeyPressed(int vk){
 vk=norm_vk(vk);
 if(vk<0||vk>=256) return false;
 bool v=g_pressed[vk];
 g_pressed[vk]=false;
 return v;
}

bool Platform_WasKeyReleased(int vk){
 vk=norm_vk(vk);
 if(vk<0||vk>=256) return false;
 bool v=g_released[vk];
 g_released[vk]=false;
 return v;
}

void Platform_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud), void *ud){
 g_cb=cb; g_ud=ud;
}

void Platform_InputState_Clear(void){
 for(int i=0;i<256;i++){ g_down[i]=false; g_pressed[i]=false; g_released[i]=false; }
}
