#include "core/flapi.h"
#include <stdio.h>
#include <string.h>

#define ADB_BUF 1024

static char *adb_run(const char *fmt, const char *a, const char *b) {
  char buffer[ADB_BUF];
  if (b) snprintf(buffer, sizeof(buffer), fmt, a ? a : "", b ? b : "");
  else if (a) snprintf(buffer, sizeof(buffer), fmt, a);
  else snprintf(buffer, sizeof(buffer), "%s", fmt);
  return fl_run_terminal_command(buffer);
}

static char *adb_tap(int x, int y) {
  char buffer[ADB_BUF];
  snprintf(buffer, sizeof(buffer), "adb shell input tap %i %i", x, y);
  return fl_run_terminal_command(buffer);
}

static char *adb_type_text(char *text) {
  char buffer[ADB_BUF];
  snprintf(buffer, sizeof(buffer), "adb shell input text '%s'", text ? text : "");
  return fl_run_terminal_command(buffer);
}

static char *adb_get_devices(void) {
  return fl_run_terminal_command("adb devices");
}

static char *adb_install(char *app_path) {
  return adb_run("adb install '%s'", app_path, NULL);
}

static char *adb_uninstall(char *app_name) {
  return adb_run("adb uninstall '%s'", app_name, NULL);
}

static char *adb_force_close(char *app_name) {
  return adb_run("adb shell am force-stop '%s'", app_name, NULL);
}

static char *adb_clear_app_data(char *app_name) {
  return adb_run("adb shell pm clear '%s'", app_name, NULL);
}

static char *adb_get_file(char *filepath, char *output_file_path) {
  return adb_run("adb pull '%s' '%s'", filepath, output_file_path);
}

static char *adb_put_file(char *filepath, char *output_file_path) {
  return adb_run("adb push '%s' '%s'", filepath, output_file_path);
}

static char *adb_reboot(void) {
  return fl_run_terminal_command("adb reboot");
}

static void adb_publish_caps(void) {
  fl_cap_register("adb.tap", adb_tap);
  fl_cap_register("adb.type_text", adb_type_text);
  fl_cap_register("adb.get_devices", adb_get_devices);
  fl_cap_register("adb.install", adb_install);
  fl_cap_register("adb.uninstall", adb_uninstall);
  fl_cap_register("adb.force_close", adb_force_close);
  fl_cap_register("adb.clear_app_data", adb_clear_app_data);
  fl_cap_register("adb.get_file", adb_get_file);
  fl_cap_register("adb.put_file", adb_put_file);
  fl_cap_register("adb.reboot", adb_reboot);
}

FLINT_FEATURE_INIT {
  adb_publish_caps();
}