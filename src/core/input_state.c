#include "platform/input_state.h"
#include "core/types/keys.h"
#include <stddef.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <time.h>

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
 } else { if(was){ g_down[nv]=false; g_released[nv]=true; } if(!was) g_released[nv]=false; }
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

#define GLOBAL_KEY_RING 512

static PlatformKeyEvent g_gkeys[GLOBAL_KEY_RING];
static int g_gkey_head = 0, g_gkey_tail = 0;
static unsigned long g_gkey_dropped = 0;
#if !defined(_WIN32)
static pthread_mutex_t g_gkey_mtx = PTHREAD_MUTEX_INITIALIZER;
#define GKEY_LOCK() pthread_mutex_lock(&g_gkey_mtx)
#define GKEY_UNLOCK() pthread_mutex_unlock(&g_gkey_mtx)
#else
static CRITICAL_SECTION g_gkey_cs;
static LONG g_gkey_cs_ready = 0;
static void gkey_lock_init(void) {
    if (InterlockedCompareExchange(&g_gkey_cs_ready, 1, 0) == 0) InitializeCriticalSection(&g_gkey_cs);
}
#define GKEY_LOCK() do { gkey_lock_init(); EnterCriticalSection(&g_gkey_cs); } while (0)
#define GKEY_UNLOCK() do { gkey_lock_init(); LeaveCriticalSection(&g_gkey_cs); } while (0)
#endif

void Platform_GlobalKey_Clear(void){
  GKEY_LOCK();
  g_gkey_head = g_gkey_tail = 0;
  g_gkey_dropped = 0;
  GKEY_UNLOCK();
}

static unsigned long long win32_qpc_us(void) {
  static LONG freq = 0;
  if (!freq) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    freq = (LONG)f.QuadPart;
    if (freq <= 0) freq = 1;
  }
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  return (unsigned long long)((q.QuadPart * 1000000ULL) / (unsigned long long)freq);
}

void Platform_GlobalKey_Push(const PlatformKeyEvent* ev){
  if(!ev) return;
  PlatformKeyEvent stamped = *ev;
#if defined(_WIN32)
  stamped.t_us = win32_qpc_us();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  stamped.t_us = (unsigned long long)ts.tv_sec * 1000000ULL + (unsigned long long)(ts.tv_nsec / 1000);
#endif
  GKEY_LOCK();
  int nxt = (g_gkey_tail + 1) % GLOBAL_KEY_RING;
  if(nxt == g_gkey_head){ g_gkey_dropped++; GKEY_UNLOCK(); return; }
  g_gkeys[g_gkey_tail] = stamped;
  g_gkey_tail = nxt;
  GKEY_UNLOCK();
}

bool Platform_GlobalKey_Pop(PlatformKeyEvent* out){
  if(!out) return false;
  bool got = false;
  GKEY_LOCK();
  if(g_gkey_head != g_gkey_tail){
    *out = g_gkeys[g_gkey_head];
    g_gkey_head = (g_gkey_head + 1) % GLOBAL_KEY_RING;
    got = true;
  }
  GKEY_UNLOCK();
  return got;
}

int Platform_GlobalKey_Pending(void){
  GKEY_LOCK();
  int n = g_gkey_tail - g_gkey_head;
  if(n < 0) n += GLOBAL_KEY_RING;
  GKEY_UNLOCK();
  return n;
}

unsigned long Platform_GlobalKey_Dropped(void){
  GKEY_LOCK();
  unsigned long d = g_gkey_dropped;
  GKEY_UNLOCK();
  return d;
}

#define GLOBAL_MOUSE_RING 256

static PlatformMouseEvent g_gmice[GLOBAL_MOUSE_RING];
static int g_gmouse_head = 0, g_gmouse_tail = 0;
static unsigned long g_gmouse_dropped = 0;
static bool g_mbtn_down[FL_MOUSE_BTN_MAX] = {0};
#if !defined(_WIN32)
static pthread_mutex_t g_gmouse_mtx = PTHREAD_MUTEX_INITIALIZER;
#define GMOUSE_LOCK() pthread_mutex_lock(&g_gmouse_mtx)
#define GMOUSE_UNLOCK() pthread_mutex_unlock(&g_gmouse_mtx)
#else
static CRITICAL_SECTION g_gmouse_cs;
static LONG g_gmouse_cs_ready = 0;
static void gmouse_lock_init(void) {
    if (InterlockedCompareExchange(&g_gmouse_cs_ready, 1, 0) == 0) InitializeCriticalSection(&g_gmouse_cs);
}
#define GMOUSE_LOCK() do { gmouse_lock_init(); EnterCriticalSection(&g_gmouse_cs); } while (0)
#define GMOUSE_UNLOCK() do { gmouse_lock_init(); LeaveCriticalSection(&g_gmouse_cs); } while (0)
#endif

int Platform_GlobalMouse_Full(void){
  int next = (g_gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  return next == g_gmouse_head;
}

void Platform_GlobalMouse_Push(const PlatformMouseEvent* ev){
  if(!ev) return;
  if (Platform_GlobalMouse_Full()) return;
  PlatformMouseEvent stamped = *ev;
#if defined(_WIN32)
  stamped.t_us = win32_qpc_us();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  stamped.t_us = (unsigned long long)ts.tv_sec * 1000000ULL + (unsigned long long)(ts.tv_nsec / 1000);
#endif
  GMOUSE_LOCK();
  int nxt = (g_gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  if(nxt == g_gmouse_head){ g_gmouse_dropped++; GMOUSE_UNLOCK(); return; }
  g_gmice[g_gmouse_tail] = stamped;
  g_gmouse_tail = nxt;
  GMOUSE_UNLOCK();
}

bool Platform_GlobalMouse_Pop(PlatformMouseEvent* out){
  if(!out) return false;
  bool got = false;
  GMOUSE_LOCK();
  if(g_gmouse_head != g_gmouse_tail){
    *out = g_gmice[g_gmouse_head];
    g_gmouse_head = (g_gmouse_head + 1) % GLOBAL_MOUSE_RING;
    got = true;
  }
  GMOUSE_UNLOCK();
  return got;
}

void Platform_GlobalMouse_Clear(void){
  GMOUSE_LOCK();
  g_gmouse_head = g_gmouse_tail = 0;
  g_gmouse_dropped = 0;
  GMOUSE_UNLOCK();
}

int Platform_GlobalMouse_Pending(void){
  GMOUSE_LOCK();
  int n = g_gmouse_tail - g_gmouse_head;
  if(n < 0) n += GLOBAL_MOUSE_RING;
  GMOUSE_UNLOCK();
  return n;
}

unsigned long Platform_GlobalMouse_Dropped(void){
  GMOUSE_LOCK();
  unsigned long d = g_gmouse_dropped;
  GMOUSE_UNLOCK();
  return d;
}

void Platform_UpdateMouseButton(int button, bool down){
  if(button < 0 || button >= (int)(sizeof(g_mbtn_down)/sizeof(g_mbtn_down[0]))) return;
  GMOUSE_LOCK();
  g_mbtn_down[button] = down;
  GMOUSE_UNLOCK();
}

bool Platform_MouseStateDown(int button){
  if(button < 0 || button >= (int)(sizeof(g_mbtn_down)/sizeof(g_mbtn_down[0]))) return false;
  GMOUSE_LOCK();
  bool d = g_mbtn_down[button];
  GMOUSE_UNLOCK();
  return d;
}
