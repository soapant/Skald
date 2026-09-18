#include "platform/input_state.h"
#include "core/types/keys.h"
#include <stddef.h>

static bool g_down[256] = {0};
static bool g_pressed[256] = {0};
static bool g_released[256] = {0};
static void (*g_cb)(int vk, bool down, int mods, void *ud) = NULL;
static void *g_ud = NULL;
static char g_char_queue[32] = {0};
static int g_char_head = 0, g_char_tail = 0;

static int norm_vk(int vk) {
  if(vk>='a'&&vk<='z') return vk-'a'+'A'; return vk; 
}
void Platform_InputState_Update(int vk, bool down, int mods){
 int orig = vk;
 int nv=norm_vk(vk);
 if(nv<0||nv>=256) return;
 bool was=g_down[nv];
 if(down){
  g_down[nv]=true; g_pressed[nv]=true; g_released[nv]=false;
  char ch = 0;
  if(nv>='A' && nv<='Z'){
   ch = (mods & HOTKEY_MOD_SHIFT) ? (char)nv : (char)(nv - 'A' + 'a');
  } else if(nv>='0' && nv<='9'){
   if(mods & HOTKEY_MOD_SHIFT){
    const char *shiftDigits = ")!@#$%^&*(";
    ch = shiftDigits[nv - '0'];
   } else ch = (char)nv;
  } else if(nv>=32 && nv<=126){
   ch = (char)orig;
   if((mods & HOTKEY_MOD_SHIFT) && ch>='a' && ch<='z') ch = ch - 'a' + 'A';
   else if(!(mods & HOTKEY_MOD_SHIFT) && ch>='A' && ch<='Z') ch = ch - 'A' + 'a';
  }
  if(ch){
   int nxt = (g_char_tail+1)%32;
   if(nxt != g_char_head){ g_char_queue[g_char_tail]=ch; g_char_tail=nxt; }
  }
 } else { if(was){ g_down[nv]=false; g_released[nv]=true; } g_pressed[nv]=false; if(!was) g_released[nv]=false; }
 if(g_cb) g_cb(nv,down,mods,g_ud);
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
 g_char_head=g_char_tail=0;
}
int Platform_GetPressedKey(void){
 for(int i=0;i<256;i++){
  if(g_pressed[i]){
   if(i>=32 && i<=126){
    g_pressed[i]=false;
    continue;
   }
   g_pressed[i]=false;
   return i;
  }
 }
 return 0;
}
char Platform_GetPressedChar(void){
 if(g_char_head != g_char_tail){
  char c = g_char_queue[g_char_head];
  g_char_head = (g_char_head+1)%32;
  return c;
 }
 return 0;
}
