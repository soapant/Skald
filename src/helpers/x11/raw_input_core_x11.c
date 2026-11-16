#define _GNU_SOURCE

#include "helpers/x11/raw_input_core_x11.h"
#include "helpers/helpers_text.h"
#include "core/window_manager.h"

struct WindowManager;

static X11Keyboard x11_keyboards[X11_MAX_KEYBOARDS];
static int x11_keyboard_count = 0;
static X11Keyboard x11_mice[X11_MAX_MICE];
static int x11_mouse_count = 0;
static bool x11_mouse_grabbed[X11_MAX_MICE] = {false};
static int x11_uinput_mouse_fd = -1;
static int x11_uinput_fd = -1;
static bool x11_suppress_release[KEY_MAX + 1] = {0};
static long x11_suppress_ms[KEY_MAX + 1] = {0};
static bool x11_device_grabbed[X11_MAX_KEYBOARDS] = {0};

#define X11_REJECTED_MAX 128
static int x11_rejected_fds[X11_REJECTED_MAX];
static int x11_rejected_count = 0;
static int x11_linux_button_to_mouse(int code);

static int x11_linux_button_to_mouse(int code) {
  switch (code) {
  case BTN_LEFT:
    return MOUSE_BTN_LEFT;
  case BTN_RIGHT:
    return MOUSE_BTN_RIGHT;
  case BTN_MIDDLE:
    return MOUSE_BTN_MIDDLE;
  case BTN_SIDE:
    return MOUSE_BTN_SIDE1;
  case BTN_EXTRA:
    return MOUSE_BTN_SIDE2;
  case BTN_BACK:
    return MOUSE_BTN_BACK;
  case BTN_FORWARD:
    return MOUSE_BTN_FORWARD;
  case BTN_TASK:
    return MOUSE_BTN_TASK;
  default:
    break;
  }
  if (code >= BTN_0 && code <= BTN_9) {
    int n = MOUSE_BTN_EXTRA1 + (code - BTN_0);
    return n <= MOUSE_BTN_MAX ? n : 0;
  }
  if (code >= 0x14e && code <= 0x159) {
    int n = 0x13 + (code - 0x14e);
    return (n > 0 && n <= MOUSE_BTN_MAX) ? n : 0;
  }
  return 0;
}

static int x11_convert_key_to_windows_vk(int key_code) {
  if (key_code >= KEY_1 && key_code <= KEY_0) {
    return '1' + (key_code - KEY_1);
  }

  switch (key_code) {
  case KEY_A:
    return 'A';
  case KEY_B:
    return 'B';
  case KEY_C:
    return 'C';
  case KEY_D:
    return 'D';
  case KEY_E:
    return 'E';
  case KEY_F:
    return 'F';
  case KEY_G:
    return 'G';
  case KEY_H:
    return 'H';
  case KEY_I:
    return 'I';
  case KEY_J:
    return 'J';
  case KEY_K:
    return 'K';
  case KEY_L:
    return 'L';
  case KEY_M:
    return 'M';
  case KEY_N:
    return 'N';
  case KEY_O:
    return 'O';
  case KEY_P:
    return 'P';
  case KEY_Q:
    return 'Q';
  case KEY_R:
    return 'R';
  case KEY_S:
    return 'S';
  case KEY_T:
    return 'T';
  case KEY_U:
    return 'U';
  case KEY_V:
    return 'V';
  case KEY_W:
    return 'W';
  case KEY_X:
    return 'X';
  case KEY_Y:
    return 'Y';
  case KEY_Z:
    return 'Z';
  case KEY_SPACE:
    return VK_SPACE;
  case KEY_ENTER:
    return VK_RETURN;
  case KEY_BACKSPACE:
    return VK_BACK;
  case KEY_TAB:
    return VK_TAB;
  case KEY_ESC:
    return VK_ESCAPE;
  case KEY_LEFT:
    return VK_LEFT;
  case KEY_UP:
    return VK_UP;
  case KEY_RIGHT:
    return VK_RIGHT;
  case KEY_DOWN:
    return VK_DOWN;
  case KEY_LEFTCTRL:
    return VK_LCONTROL;
  case KEY_RIGHTCTRL:
    return VK_RCONTROL;
  case KEY_LEFTALT:
    return VK_LMENU;
  case KEY_RIGHTALT:
    return VK_RMENU;
  case KEY_LEFTSHIFT:
    return VK_LSHIFT;
  case KEY_RIGHTSHIFT:
    return VK_RSHIFT;
  case KEY_LEFTMETA:
    return VK_LWIN;
  case KEY_RIGHTMETA:
    return VK_RWIN;
  case KEY_SEMICOLON:
    return VK_OEM_1;
  case KEY_SLASH:
    return VK_OEM_2;
  case KEY_LEFTBRACE:
    return VK_OEM_4;
  case KEY_RIGHTBRACE:
    return VK_OEM_6;
  case KEY_COMMA:
    return VK_OEM_COMMA;
  case KEY_DOT:
    return VK_OEM_PERIOD;
  case KEY_APOSTROPHE:
    return VK_OEM_7;
  case KEY_MINUS:
    return VK_OEM_MINUS;
  case KEY_EQUAL:
    return VK_OEM_PLUS;
  case KEY_GRAVE:
    return VK_OEM_3;
  case KEY_BACKSLASH:
    return VK_OEM_5;
  default:
    return 0;
  }
}

static long long x11_mouse_last_fire_ms[MOUSE_BTN_MAX + 1] = {0};
static void x11_queue_mouse_event(struct input_event *ev) {

  if (!ev || ev->type != EV_KEY) {
    return;
  }
  int btn = x11_linux_button_to_mouse(ev->code);
  if (btn == 0) {
    return;
  }
  bool down = (ev->value != 0);
  bool repeat = (ev->value == 2);
  bool was_down = input_mouse_state_down(btn);
  if (down && was_down && !repeat) {
    input_update_mouse_button(btn, false);
    int rmx = 0, rmy = 0;
    Platform_GetMousePos(&rmx, &rmy);
    PlatformMouseEvent rev;
    rev.button = btn;
    rev.x = rmx;
    rev.y = rmy;
    rev.mods = remap_current_mods();
    rev.down = false;
    rev.repeat = false;
    rev.t_us = 0;
    global_mouse_push(&rev);
  }
  input_update_mouse_button(btn, down);
  if (!down && btn > 0 && btn <= MOUSE_BTN_MAX) {
    hotkey_key_up(MOUSE_VK(btn));
  }
  int mx = 0, my = 0;
  Platform_GetMousePos(&mx, &my);
  PlatformMouseEvent mev;
  mev.button = btn;
  mev.x = mx;
  mev.y = my;
  mev.mods = remap_current_mods();
  mev.down = down;
  mev.repeat = repeat;
  mev.t_us = 0;
  global_mouse_push(&mev);
  if (down && !repeat && btn > 0 && btn <= MOUSE_BTN_MAX) {
    long long now_ms =
        (long long)ev->time.tv_sec * 1000LL + ev->time.tv_usec / 1000LL;
    bool rising = !was_down;
    bool stale = (now_ms - x11_mouse_last_fire_ms[btn]) > 200;
    if (rising || stale) {
      x11_mouse_last_fire_ms[btn] = now_ms;
      hotkey_process(MOUSE_VK(btn), mev.mods);
    }
  }
}

bool x11_handle_evdev_mouse(struct input_event *ev) {
  if (!ev)
    return false;
  x11_queue_mouse_event(ev);
  return true;
}

static void x11_release_rejected_devices(void) {
  for (int i = 0; i < x11_rejected_count; i++) {
    if (x11_rejected_fds[i] >= 0) {
      close(x11_rejected_fds[i]);
      x11_rejected_fds[i] = -1;
    }
  }
  x11_rejected_count = 0;
}

int x11_keyboard_count_get(void) { return x11_keyboard_count; }
int x11_mouse_count_get(void) { return x11_mouse_count; }
int x11_keyboard_fd(int i) {
  return (i >= 0 && i < X11_MAX_KEYBOARDS) ? x11_keyboards[i].fd : -1;
}
int x11_mouse_fd(int i) {
  return (i >= 0 && i < X11_MAX_MICE) ? x11_mice[i].fd : -1;
}
void x11_keyboard_fd_set(int i, int fd) {
  if (i >= 0 && i < X11_MAX_KEYBOARDS)
    x11_keyboards[i].fd = fd;
}
bool x11_device_grabbed_get(int i) {
  return (i >= 0 && i < X11_MAX_KEYBOARDS) ? x11_device_grabbed[i] : false;
}
void x11_device_grabbed_set(int i, bool v) {
  if (i >= 0 && i < X11_MAX_KEYBOARDS)
    x11_device_grabbed[i] = v;
}
bool x11_mouse_grabbed_get(int i) {
  return (i >= 0 && i < X11_MAX_MICE) ? x11_mouse_grabbed[i] : false;
}
int x11_uinput_fd_get(void) { return x11_uinput_fd; }
void x11_uinput_fd_set(int fd) { x11_uinput_fd = fd; }

static bool x11_is_traced(void) {
  FILE *f = fopen("/proc/self/status", "r");
  if (!f)
    return false;
  char line[256];
  bool traced = false;
  while (fgets(line, sizeof(line), f)) {
    int pid = 0;
    if (sscanf(line, "TracerPid: %d", &pid) == 1 && pid > 0) {
      traced = true;
      break;
    }
  }
  fclose(f);
  return traced;
}

bool x11_grab_blocked(void) {
  const char *hw = getenv("SKALD_NO_GRAB");
  if (hw && hw[0])
    return true;
  return x11_is_traced();
}

bool x11_should_use_grab(void) {
  if (x11_env_is_wayland())
    return false;
  if (x11_env_is_compositor())
    return false;
  if (x11_grab_blocked())
    return false;
  const char *t = getenv("XDG_SESSION_TYPE");
  if (t && strcmp(t, "wayland") == 0)
    return false;
  return true;
}

static struct WindowManager *x11_wm = NULL;

void x11_set_window_manager(struct WindowManager *wm) { x11_wm = wm; }

static void x11_force_cleanup(void);
static void x11_install_cleanup_handlers(void);
void x11_mouse_grab_end(void);
static volatile sig_atomic_t x11_cleanup_done = 0;

static void x11_sig_handler(int sig) {
  x11_force_cleanup();
  signal(sig, SIG_DFL);
  raise(sig);
}

static void x11_term_handler(int sig) {
  (void)sig;
  x11_force_cleanup();
  if (x11_wm) {
    WindowManager_Quit(x11_wm);
  }
  _exit(0);
}

static void x11_force_cleanup(void) {

  if (x11_cleanup_done)
    return;
  x11_cleanup_done = 1;
  for (int i = 0; i < x11_keyboard_count; i++) {
    if (x11_keyboards[i].fd >= 0) {
      ioctl(x11_keyboards[i].fd, EVIOCGRAB, 0);
    }
  }
  x11_mouse_grab_end();
  for (int i = 0; i < 256; i++) {
    extern bool remap_is_virt_held(int vk);
    if (remap_is_virt_held(i))
      Platform_EmitKey(i, 0);
  }
  remap_reset();
  x11_clear_suppress();
  if (x11_uinput_fd >= 0) {
    ioctl(x11_uinput_fd, UI_DEV_DESTROY);
    close(x11_uinput_fd);
    x11_uinput_fd = -1;
  }
  for (int i = 0; i < x11_keyboard_count; i++) {
    if (x11_keyboards[i].fd >= 0) {
      close(x11_keyboards[i].fd);
      x11_keyboards[i].fd = -1;
    }
  }
  for (int i = 0; i < x11_mouse_count; i++) {
    if (x11_mice[i].fd >= 0) {
      close(x11_mice[i].fd);
      x11_mice[i].fd = -1;
    }
  }
  x11_mouse_count = 0;
  x11_release_rejected_devices();
}

static void x11_atexit_handler(void) { x11_force_cleanup(); }

static void x11_install_cleanup_handlers(void) {

  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  atexit(x11_atexit_handler);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = x11_term_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sa.sa_handler = x11_sig_handler;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
}

int x11_init_uinput(void) {
  x11_install_cleanup_handlers();
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    log_error("open failed: %s (swallow disabled)", strerror(errno));
    return -1;
  }
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
    perror("[UINPUT] UI_SET_EVBIT EV_KEY");
  if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
    perror("[UINPUT] UI_SET_EVBIT EV_SYN");
  ioctl(fd, UI_SET_EVBIT, EV_MSC);
  ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);
  for (int i = 0; i <= KEY_MAX; i++)
    ioctl(fd, UI_SET_KEYBIT, i);
  struct uinput_setup setup;
  memset(&setup, 0, sizeof(setup));
  setup.id.bustype = BUS_USB;
  setup.id.vendor = 0x1234;
  setup.id.product = 0x5678;
  snprintf(setup.name, sizeof(setup.name), "skald-virtual-keyboard");
  if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
      perror("[UINPUT] UI_DEV_CREATE");
      close(fd);
      return -1;
    }
  } else {
    struct uinput_user_dev uud;
    memset(&uud, 0, sizeof(uud));
    snprintf(uud.name, sizeof(uud.name), "skald-virtual-keyboard");
    uud.id.bustype = BUS_USB;
    uud.id.vendor = 0x1234;
    uud.id.product = 0x5678;
    if (write(fd, &uud, sizeof(uud)) != sizeof(uud))
      perror("[UINPUT] write uud");
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
      perror("[UINPUT] UI_DEV_CREATE legacy");
      close(fd);
      return -1;
    }
  }
  log_info("virtual keyboard created (fd=%d)", fd);
  return fd;
}

void x11_mouse_grab_end(void) {
  for (int i = 0; i < x11_mouse_count; i++) {
    if (x11_mice[i].fd >= 0 && x11_mouse_grabbed[i]) {
      ioctl(x11_mice[i].fd, EVIOCGRAB, 0);
    }
    x11_mouse_grabbed[i] = false;
  }
  if (x11_uinput_mouse_fd >= 0) {
    ioctl(x11_uinput_mouse_fd, UI_DEV_DESTROY);
    close(x11_uinput_mouse_fd);
    x11_uinput_mouse_fd = -1;
  }
}

void x11_destroy_uinput(void) {
  if (x11_uinput_fd >= 0) {
    ioctl(x11_uinput_fd, UI_DEV_DESTROY);
    close(x11_uinput_fd);
    x11_uinput_fd = -1;
  }
}

static int x11_init_uinput_mouse(void) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    log_error("uinput mouse open failed: %s", strerror(errno));
    return -1;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  ioctl(fd, UI_SET_EVBIT, EV_MSC);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);

  for (int c = BTN_MISC; c <= BTN_TASK; c++)
    ioctl(fd, UI_SET_KEYBIT, c);
  for (int c = BTN_0; c <= BTN_9; c++)
    ioctl(fd, UI_SET_KEYBIT, c);
  for (int c = 0x14e; c <= 0x159; c++)
    ioctl(fd, UI_SET_KEYBIT, c);

  for (int r = REL_X; r <= REL_MAX; r++)
    ioctl(fd, UI_SET_RELBIT, r);
  ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);

  struct uinput_setup setup;
  memset(&setup, 0, sizeof(setup));
  setup.id.bustype = BUS_USB;
  setup.id.vendor = 0x1234;
  setup.id.product = 0x5679;
  snprintf(setup.name, sizeof(setup.name), "skald-virtual-mouse");
  if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
      perror("[UINPUT-M] UI_DEV_CREATE");
      close(fd);
      return -1;
    }
  } else {
    struct uinput_user_dev uud;
    memset(&uud, 0, sizeof(uud));
    snprintf(uud.name, sizeof(uud.name), "skald-virtual-mouse");
    uud.id.bustype = BUS_USB;
    uud.id.vendor = 0x1234;
    uud.id.product = 0x5679;
    if (write(fd, &uud, sizeof(uud)) != sizeof(uud))
      perror("[UINPUT-M] write uud");
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
      perror("[UINPUT-M] UI_DEV_CREATE legacy");
      close(fd);
      return -1;
    }
  }
  log_info("virtual mouse created (fd=%d)", fd);
  return fd;
}

void x11_mouse_grab_begin(void) {
  if (x11_grab_blocked())
    return;

  if (x11_uinput_mouse_fd < 0) {
    x11_uinput_mouse_fd = x11_init_uinput_mouse();
    if (x11_uinput_mouse_fd < 0) {
      log_warn("mouse grab disabled, uinput mouse failed");
      return;
    }
  }
  for (int i = 0; i < x11_mouse_count; i++) {
    if (x11_mice[i].fd < 0 || x11_mouse_grabbed[i])
      continue;
    struct input_event drain;
    while (read(x11_mice[i].fd, &drain, sizeof(drain)) > 0) {
    }
    if (ioctl(x11_mice[i].fd, EVIOCGRAB, 1) == 0) {
      x11_mouse_grabbed[i] = true;
    }
  }
}

void x11_mouse_forward(const struct input_event *ev, int grabbed) {
  if (!grabbed || x11_uinput_mouse_fd < 0)
    return;
  if (ev->type == EV_KEY) {
    int btn = x11_linux_button_to_mouse(ev->code);
    if (btn > 0 && hotkey_mouse_button_bound(btn))
      return;
  }
  if (write(x11_uinput_mouse_fd, ev, sizeof(*ev)) < 0) {
    if (errno != EAGAIN)
      perror("[UINPUT-M] write");
    return;
  }
  if (ev->type != EV_SYN) {
    struct input_event syn;
    memset(&syn, 0, sizeof(syn));
    gettimeofday(&syn.time, NULL);
    syn.type = EV_SYN;
    syn.code = SYN_REPORT;
    syn.value = 0;
    if (write(x11_uinput_mouse_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN)
      perror("[UINPUT-M] write syn");
  }
}

void x11_uinput_emit(const struct input_event *ev) {
  if (x11_uinput_fd < 0) {
    return;
  }
  if (write(x11_uinput_fd, ev, sizeof(*ev)) < 0) {
    if (errno != EAGAIN)
      perror("[UINPUT] write");
    return;
  }
  struct input_event syn;
  memset(&syn, 0, sizeof(syn));
  gettimeofday(&syn.time, NULL);
  syn.type = EV_SYN;
  syn.code = SYN_REPORT;
  syn.value = 0;
  if (write(x11_uinput_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN)
    perror("[UINPUT] write syn");
}

void x11_scan_keyboards(void) {
  x11_install_cleanup_handlers();
  x11_release_rejected_devices();
  DIR *dir = opendir("/dev/input");
  if (!dir) {
    return;
  }
  const struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "event", 5) == 0) {
      char full_path[sizeof("/dev/input/") + 255];
      snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);
      int fd = open(full_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (fd < 0)
        continue;
      unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
      unsigned long keybits[(KEY_MAX / 8 + sizeof(unsigned long) - 1) /
                                sizeof(unsigned long) +
                            1] = {0};
      struct input_id id;
      bool is_kb = false;
      bool is_mouse = false;
      if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0 &&
          (ev_bits[0] & (1UL << EV_KEY)) &&
          ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
          ioctl(fd, EVIOCGID, &id) >= 0) {
        bool self = (id.vendor == 0x1234 && id.product == 0x5678);
        bool has_rel = (ev_bits[0] & (1UL << EV_REL)) != 0;
        bool has_abs = (ev_bits[0] & (1UL << EV_ABS)) != 0;
        bool has_btn = false;
        for (int b = BTN_MISC; b <= BTN_TASK; b++) {
          int w = b / (8 * (int)sizeof(unsigned long));
          int k = b % (8 * (int)sizeof(unsigned long));
          if (w < (int)(sizeof(keybits) / sizeof(keybits[0])) &&
              (keybits[w] & (1UL << k))) {
            has_btn = true;
            break;
          }
        }
        if (!self && !has_rel && !has_abs) {
          keybits[0] &= ~1UL;
          int nwords = (0xFF + 1 + (int)(sizeof(unsigned long) * 8) - 1) /
                       (int)(sizeof(unsigned long) * 8);
          for (int i = 0; i < nwords; i++)
            if (keybits[i]) {
              is_kb = true;
              break;
            }
        } else if (!self && has_btn && (has_rel || has_abs)) {
          is_mouse = true;
        }
      }
      if (is_kb && x11_keyboard_count < X11_MAX_KEYBOARDS) {
        x11_keyboards[x11_keyboard_count].fd = fd;
        safe_copy(x11_keyboards[x11_keyboard_count].path,
                  sizeof(x11_keyboards[x11_keyboard_count].path), full_path);
        x11_keyboard_count++;
      } else if (is_mouse && x11_mouse_count < X11_MAX_MICE) {
        x11_mice[x11_mouse_count].fd = fd;
        safe_copy(x11_mice[x11_mouse_count].path,
                  sizeof(x11_mice[x11_mouse_count].path), full_path);
        x11_mouse_count++;
      } else if (x11_rejected_count < X11_REJECTED_MAX) {
        x11_rejected_fds[x11_rejected_count++] = fd;
      } else {
        close(fd);
      }
    }
  }
  closedir(dir);
  log_warn("x11_scan_keyboards found %d keyboards %d mice held=%d",
           x11_keyboard_count, x11_mouse_count, x11_rejected_count);
}

int x11_keysym_to_vk(KeySym ks) {
  if (ks >= XK_a && ks <= XK_z)
    return 'A' + (ks - XK_a);
  if (ks >= XK_A && ks <= XK_Z)
    return 'A' + (ks - XK_A);
  if (ks >= XK_0 && ks <= XK_9)
    return '0' + (ks - XK_0);
  if (ks >= XK_F1 && ks <= XK_F12)
    return VK_F1 + (ks - XK_F1);
  switch (ks) {
  case XK_Escape:
    return VK_ESCAPE;
  case XK_Return:
  case XK_KP_Enter:
    return VK_RETURN;
  case XK_Tab:
    return VK_TAB;
  case XK_BackSpace:
    return VK_BACK;
  case XK_Up:
    return VK_UP;
  case XK_Down:
    return VK_DOWN;
  case XK_Left:
    return VK_LEFT;
  case XK_Right:
    return VK_RIGHT;
  case XK_Delete:
    return VK_DELETE;
  case XK_Home:
    return VK_HOME;
  case XK_End:
    return VK_END;
  case XK_Page_Up:
    return VK_PRIOR;
  case XK_Page_Down:
    return VK_NEXT;
  case XK_KP_0:
  case XK_KP_1:
  case XK_KP_2:
  case XK_KP_3:
  case XK_KP_4:
  case XK_KP_5:
  case XK_KP_6:
  case XK_KP_7:
  case XK_KP_8:
  case XK_KP_9:
    return '0' + (ks - XK_KP_0);
  case XK_space:
    return VK_SPACE;
  case XK_Control_L:
    return VK_LCONTROL;
  case XK_Control_R:
    return VK_RCONTROL;
  case XK_Shift_L:
    return VK_LSHIFT;
  case XK_Shift_R:
    return VK_RSHIFT;
  case XK_Alt_L:
    return VK_LMENU;
  case XK_Alt_R:
    return VK_RMENU;
  case XK_Super_L:
    return VK_LWIN;
  case XK_Super_R:
    return VK_RWIN;
  case XK_semicolon:
  case XK_colon:
    return VK_OEM_1;
  case XK_slash:
  case XK_question:
    return VK_OEM_2;
  case XK_comma:
  case XK_less:
    return VK_OEM_COMMA;
  case XK_period:
  case XK_greater:
    return VK_OEM_PERIOD;
  case XK_apostrophe:
  case XK_quotedbl:
    return VK_OEM_7;
  case XK_bracketleft:
  case XK_braceleft:
    return VK_OEM_4;
  case XK_bracketright:
  case XK_braceright:
    return VK_OEM_6;
  case XK_grave:
  case XK_asciitilde:
    return VK_OEM_3;
  case XK_minus:
  case XK_underscore:
    return VK_OEM_MINUS;
  case XK_equal:
  case XK_plus:
    return VK_OEM_PLUS;
  case XK_backslash:
  case XK_bar:
    return VK_OEM_5;
  case XK_exclam:
    return '1';
  case XK_at:
    return '2';
  case XK_numbersign:
    return '3';
  case XK_dollar:
    return '4';
  case XK_percent:
    return '5';
  case XK_asciicircum:
    return '6';
  case XK_ampersand:
    return '7';
  case XK_asterisk:
    return '8';
  case XK_parenleft:
    return '9';
  case XK_parenright:
    return '0';
  default:
    return 0;
  }
}

int x11_vk_to_linux_key(int vk) {
  switch (vk) {
  case 'A':
    return KEY_A;
  case 'B':
    return KEY_B;
  case 'C':
    return KEY_C;
  case 'D':
    return KEY_D;
  case 'E':
    return KEY_E;
  case 'F':
    return KEY_F;
  case 'G':
    return KEY_G;
  case 'H':
    return KEY_H;
  case 'I':
    return KEY_I;
  case 'J':
    return KEY_J;
  case 'K':
    return KEY_K;
  case 'L':
    return KEY_L;
  case 'M':
    return KEY_M;
  case 'N':
    return KEY_N;
  case 'O':
    return KEY_O;
  case 'P':
    return KEY_P;
  case 'Q':
    return KEY_Q;
  case 'R':
    return KEY_R;
  case 'S':
    return KEY_S;
  case 'T':
    return KEY_T;
  case 'U':
    return KEY_U;
  case 'V':
    return KEY_V;
  case 'W':
    return KEY_W;
  case 'X':
    return KEY_X;
  case 'Y':
    return KEY_Y;
  case 'Z':
    return KEY_Z;
  }
  if (vk == '0')
    return KEY_0;
  if (vk >= '0' && vk <= '9')
    return KEY_1 + (vk - '1');
  if (vk == VK_SPACE)
    return KEY_SPACE;
  if (vk == VK_RETURN)
    return KEY_ENTER;
  if (vk == VK_BACK)
    return KEY_BACKSPACE;
  if (vk == VK_TAB)
    return KEY_TAB;
  if (vk == VK_ESCAPE)
    return KEY_ESC;
  if (vk == VK_OEM_1)
    return KEY_SEMICOLON;
  if (vk == VK_OEM_2)
    return KEY_SLASH;
  if (vk == VK_OEM_4)
    return KEY_LEFTBRACE;
  if (vk == VK_OEM_6)
    return KEY_RIGHTBRACE;
  if (vk == VK_OEM_COMMA)
    return KEY_COMMA;
  if (vk == VK_OEM_PERIOD)
    return KEY_DOT;
  if (vk == VK_OEM_7)
    return KEY_APOSTROPHE;
  if (vk == VK_OEM_3)
    return KEY_GRAVE;
  if (vk == VK_OEM_PLUS)
    return KEY_EQUAL;
  if (vk == VK_OEM_MINUS)
    return KEY_MINUS;
  if (vk == VK_UP)
    return KEY_UP;
  if (vk == VK_DOWN)
    return KEY_DOWN;
  if (vk == VK_LEFT)
    return KEY_LEFT;
  if (vk == VK_RIGHT)
    return KEY_RIGHT;
  return 0;
}

void x11_clear_suppress(void) {
  for (int i = 0; i <= KEY_MAX; i++) {
    x11_suppress_release[i] = false;
    x11_suppress_ms[i] = 0;
  }
}

static long x11_now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

bool x11_is_suppressed(int linux_code) {
  if (linux_code < 0 || linux_code > KEY_MAX)
    return false;
  if (!x11_suppress_release[linux_code])
    return false;
  long now = x11_now_ms();
  if (now - x11_suppress_ms[linux_code] > 5000) {
    x11_suppress_release[linux_code] = false;
    x11_suppress_ms[linux_code] = 0;
    return false;
  }
  return true;
}

void x11_set_suppress(int linux_code, bool v) {
  if (linux_code < 0 || linux_code > KEY_MAX)
    return;
  x11_suppress_release[linux_code] = v;
  x11_suppress_ms[linux_code] = v ? x11_now_ms() : 0;
}

bool x11_handle_global_key(int key_code, int value) {
  remap_update_phys(x11_convert_key_to_windows_vk(key_code), value);
  if (value == 0) {
    hotkey_key_up(x11_convert_key_to_windows_vk(key_code));
    return false;
  }
  if (value != 1 && value != 2)
    return false;
  int pressed_vk = x11_convert_key_to_windows_vk(key_code);
  int modifiers = remap_current_mods();
  bool processed = hotkey_process(pressed_vk, modifiers);
  if (pressed_vk && processed) {
    pending_hotkey_vk = pressed_vk;
    pending_hotkey_mod = modifiers;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    pending_hotkey_timeMs = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return true;
  }
  return false;
}

bool remap_should_swallow_linux_key(int linux_code, int value) {
  int vk = x11_convert_key_to_windows_vk(linux_code);
  if (!vk) {
    return false;
  }
  return remap_handle_vk(vk, value);
}

bool x11_handle_evdev(struct input_event *ev, bool has_popup, bool grabbed) {
  if (!ev)
    return false;
  if (ev->type != EV_KEY) {
    if (grabbed && x11_uinput_fd >= 0)
      x11_uinput_emit(ev);
    return false;
  }
  int code = ev->code;
  int val = ev->value;
  if (ev->type == EV_KEY && val != 0)
    Platform_DismissIndefiniteMessage();
  if (code < 0 || code > KEY_MAX) {
    if (grabbed && x11_uinput_fd >= 0)
      x11_uinput_emit(ev);
    return false;
  }
  if (remap_should_swallow_linux_key(code, val)) {
    if (val == 1)
      x11_set_suppress(code, true);
    else if (val == 0)
      x11_set_suppress(code, false);
    log_info("x11_handle_evdev remap swallowed code=%d val=%d popup=%d", code,
             val, has_popup);
    return true;
  }
  if (val == 0) {
    int vk0 = x11_convert_key_to_windows_vk(code);
    hotkey_key_up(vk0);
    if (vk0)
      input_state_update(vk0, false, remap_current_mods(), 0);
  }
  if (val != 2) {
    remap_update_phys(x11_convert_key_to_windows_vk(code), val);
  }
  {
    PlatformKeyEvent gev;
    gev.vk = x11_convert_key_to_windows_vk(code);
    gev.scancode = code;
    gev.mods = remap_current_mods();
    gev.down = val != 0;
    gev.repeat = (val == 2);
    global_key_push(&gev);
  }
  if (val == 0 && x11_is_suppressed(code)) {
    x11_set_suppress(code, false);
    return true;
  }
  if (val == 2 && x11_is_suppressed(code))
    return true;
  if (has_popup) {
    if (val != 0 && x11_is_suppressed(code))
      return true;
    bool hk_swallowed = (val == 1) && x11_handle_global_key(code, val);
    if (hk_swallowed) {
      x11_set_suppress(code, true);
      return true;
    }
    if (grabbed && x11_uinput_fd >= 0)
      x11_uinput_emit(ev);
    return false;
  }
  struct timeval now;
  gettimeofday(&now, NULL);
  long age_ms = (now.tv_sec - ev->time.tv_sec) * 1000L +
                (now.tv_usec - ev->time.tv_usec) / 1000L;
  const long HOTKEY_STALE_EVENT_MS = 200;
  if (age_ms > HOTKEY_STALE_EVENT_MS) {
    bool is_mod = (code == KEY_LEFTALT || code == KEY_RIGHTALT ||
                   code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL ||
                   code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT ||
                   code == KEY_LEFTMETA || code == KEY_RIGHTMETA);
    if (!is_mod) {
      if (val == 1 && !grabbed) {
        if (x11_handle_global_key(code, val)) {
          x11_set_suppress(code, true);
          return true;
        }
      }
      if (grabbed && x11_uinput_fd >= 0) {
        if (snippet_feed_vk(x11_convert_key_to_windows_vk(code), val, true))
          return false;
        x11_uinput_emit(ev);
      }
      return false;
    }
    if (val == 1) {
      if (grabbed && x11_uinput_fd >= 0)
        x11_uinput_emit(ev);
      return false;
    }
  }
  if (x11_handle_global_key(code, val)) {
    if (val == 1)
      x11_set_suppress(code, true);
    return true;
  }
  if (grabbed) {
    if (x11_uinput_fd >= 0) {
      if ((val == 1 || val == 2) &&
          hotkey_process(x11_convert_key_to_windows_vk(code),
                         remap_current_mods()))
        return true;
      if (snippet_feed_vk(x11_convert_key_to_windows_vk(code), val, true))
        return false;
      x11_uinput_emit(ev);
    } else {
      static bool warned = false;
      if (!warned) {
        log_warn("grabbed but no uinput - key %d lost", code);
        warned = true;
      }
    }
  } else {
    snippet_feed_vk(x11_convert_key_to_windows_vk(code), val, false);
  }
  return false;
}
