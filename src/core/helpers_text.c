#include "common.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

int ParseTimeToSeconds(const char* input) {
    if (!input || strlen(input) == 0) return 0;
    
    int total = 0;
    char buffer[64];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* token = strtok(buffer, " ");
    while (token != NULL) {
        int value = 0;
        char unit = 's';
        int len = strlen(token);
        
        if (len > 1) {
            char last = tolower(token[len - 1]);
            if (last == 's' || last == 'm' || last == 'h' || last == 'd') {
                token[len - 1] = '\0';
                value = atoi(token);
                unit = last;
            } else {
                value = atoi(token);
            }
        } else {
            value = atoi(token);
        }
        
        switch (unit) {
            case 's': total += value; break;
            case 'm': total += value * 60; break;
            case 'h': total += value * 3600; break;
            case 'd': total += value * 86400; break;
            default: total += value; break;
        }
        
        token = strtok(NULL, " ");
    }
    
    return total;
}

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    
    int match_count = 0;
    char* text_copy = _strdup(text);
    if (!text_copy) return 0;
    
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    for (int i = 0; i < flag_count && match_count < max_matches; i++) {
        const char* flag_str = *(const char**)((const char*)flag_array + i * flag_size);
        
        if (!flag_str) continue;
        
        char* found = strstr(text_copy, flag_str);
        if (found) {
            int before_ok = (found == text_copy || *(found - 1) == ' ');
            
            int after_ok = 1;
            char* after = found + strlen(flag_str);
            if (*after != '\0' && *after != ' ' && *after != '\t') {
                after_ok = 0;
            }
            
            if (before_ok && after_ok) {
                matches[match_count].flag = (char*)flag_str;
                matches[match_count].index = found - text_copy;
                match_count++;
            }
        }
    }
    
    free(text_copy);
    return match_count;
}

char* GetFlagParameter(const char* text, int flag_index) {
    if (!text || flag_index < 0) return NULL;
    
    static char param[256];
    param[0] = '\0';
    
    int pos = flag_index;
    while (text[pos] && text[pos] != ' ') pos++;
    while (text[pos] && text[pos] == ' ') pos++;
    
    int start = pos;
    while (text[pos]) {
        if (text[pos] == '-' && (pos == 0 || text[pos-1] == ' ')) {
            break;
        }
        pos++;
    }
    
    int len = pos - start;
    if (len > 0) {
        strncpy(param, text + start, len);
        param[len] = '\0';
        
        char* end = param + strlen(param) - 1;
        while (end > param && *end == ' ') end--;
        *(end + 1) = '\0';
        
        return param;
    }
    
    return NULL;
}

HotkeyBinding convert_into_hotkey_binding(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows){ 
  HotkeyBinding output = {0};
  int out_modifiers = 0;
  int out_key = 0;
  
  char buffer[256];
  strncpy(buffer, hotkey_string, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  
  char* token = strtok(buffer, "+");
  
  while (token != NULL) {
      if (strcasecmp(token, "Ctrl") == 0 || strcasecmp(token, "Control") == 0) {
          out_modifiers |= HOTKEY_MOD_CTRL;
      } else if (strcasecmp(token, "Shift") == 0) {
          out_modifiers |= HOTKEY_MOD_SHIFT;
      } else if (strcasecmp(token, "Alt") == 0) {
          out_modifiers |= HOTKEY_MOD_ALT;
      } else if (strcasecmp(token, "Win") == 0 || strcasecmp(token, "Super") == 0 || strcasecmp(token, "Meta") == 0 ) {
          out_modifiers |= HOTKEY_MOD_SUPER;
       } else {
           if (strcasecmp(token, "Space") == 0) {
               out_key = ' ';
           } else if (strcasecmp(token, "Tab") == 0) {
               out_key = '\t';
           } else if (strcasecmp(token, "Enter") == 0 || strcasecmp(token, "Return") == 0) {
               out_key = '\n';
           } else if (strcasecmp(token, "Escape") == 0 || strcasecmp(token, "Esc") == 0) {
               out_key = 27;
           } else if (strcasecmp(token, "{") == 0 || strcasecmp(token, "[") == 0 || strcasecmp(token, "OEM_4") == 0 || strcasecmp(token, "Oem4") == 0) {
               out_key = VK_OEM_4;
           } else if (strcasecmp(token, "}") == 0 || strcasecmp(token, "]") == 0 || strcasecmp(token, "OEM_6") == 0 || strcasecmp(token, "Oem6") == 0) {
               out_key = VK_OEM_6;
           } else if (strcasecmp(token, ",") == 0 || strcasecmp(token, "OEM_COMMA") == 0 || strcasecmp(token, "OemComma") == 0) {
               out_key = VK_OEM_COMMA;
           } else if (strcasecmp(token, ".") == 0 || strcasecmp(token, "OEM_PERIOD") == 0 || strcasecmp(token, "OemPeriod") == 0) {
               out_key = VK_OEM_PERIOD;
           } else if (strcasecmp(token, ";") == 0 || strcasecmp(token, "OEM_1") == 0 || strcasecmp(token, "Oem1") == 0) {
               out_key = VK_OEM_1;
           } else if (strcasecmp(token, "/") == 0 || strcasecmp(token, "OEM_2") == 0 || strcasecmp(token, "Oem2") == 0) {
               out_key = VK_OEM_2;
           } else if (strcasecmp(token, "'") == 0 || strcasecmp(token, "\"") == 0 || strcasecmp(token, "OEM_7") == 0 || strcasecmp(token, "Oem7") == 0) {
               out_key = VK_OEM_7;
           } else if (strcasecmp(token, "`") == 0 || strcasecmp(token, "OEM_3") == 0 || strcasecmp(token, "Oem3") == 0) {
               out_key = VK_OEM_3;
           } else if (strcasecmp(token, "-") == 0 || strcasecmp(token, "OEM_MINUS") == 0 || strcasecmp(token, "OemMinus") == 0) {
               out_key = VK_OEM_MINUS;
           } else if (strcasecmp(token, "=") == 0 || strcasecmp(token, "OEM_PLUS") == 0 || strcasecmp(token, "OemPlus") == 0) {
               out_key = VK_OEM_PLUS;
           } else if (strlen(token) == 1) {
               out_key = toupper((unsigned char)token[0]);
           } else {
               out_key = toupper((unsigned char)token[0]);
           }
       }
      
      token = strtok(NULL, "+");
  }
  output.vk_code = out_key;
  output.modifiers = out_modifiers;
  output.hotkey_callback = on_hotkey_trigger;
  output.hotkey_condition = optional_condition_for_hotkey;
  output.blacklist = optional_blacklistedwindows;
  if (optional_blacklistedwindows) {
      int cnt = 0;
      while (optional_blacklistedwindows[cnt] != NULL) cnt++;
      output.blacklist_count = cnt;
  } else {
      output.blacklist_count = 0;
  }
  return output;
}


