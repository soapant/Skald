#include "core/flapi.h"

typedef struct {
    const char* from_unit;
    const char* to_unit;
    double factor;
} UnitConversion;

#define CONVERSION_COUNT ARRAY_LEN(CONVERSIONS)

double last_a = 0.0;
double last_b = 0.0;
char last_op = 0;
bool has_last_op = false;
char last_expression[256] = {0};
double flipped_result = 0.0;
bool is_flipped = false;

const UnitConversion CONVERSIONS[] = {
    
    { "minutes", "hours",   1.0 / 60.0 },
    { "minute",  "hours",   1.0 / 60.0 },
    { "minute",  "hour",    1.0 / 60.0 },
    { "mins",    "hours",   1.0 / 60.0 },
    { "mins",    "hour",    1.0 / 60.0 },
    { "min",     "hours",   1.0 / 60.0 },
    { "min",     "hour",    1.0 / 60.0 },
    { "m",       "h",       1.0 / 60.0 },

    
    { "hours",   "minutes", 60.0 },
    { "hour",    "minutes", 60.0 },
    { "hour",    "minute",  60.0 },
    { "hrs",     "mins",    60.0 },
    { "hrs",     "min",     60.0 },
    { "hr",      "mins",    60.0 },
    { "hr",      "min",     60.0 },
    { "h",       "m",       60.0 },

    
    { "seconds", "hours",   1.0 / 3600.0 },
    { "second",  "hour",    1.0 / 3600.0 },
    { "secs",    "hours",   1.0 / 3600.0 },
    { "secs",    "hr",      1.0 / 3600.0 },
    { "sec",     "hour",    1.0 / 3600.0 },
    { "sec",     "hr",      1.0 / 3600.0 },
    { "s",       "h",       1.0 / 3600.0 },

    
    { "hours",   "seconds", 3600.0 },
    { "hour",    "second",  3600.0 },
    { "hours",   "secs",    3600.0 },
    { "hour",    "sec",     3600.0 },
    { "hrs",     "secs",    3600.0 },
    { "hr",      "sec",     3600.0 },
    { "h",       "s",       3600.0 },

    
    { "seconds", "minutes", 1.0 / 60.0 },
    { "second",  "minute",  1.0 / 60.0 },
    { "secs",    "mins",    1.0 / 60.0 },
    { "sec",     "min",     1.0 / 60.0 },
    { "s",       "m",       1.0 / 60.0 },

    
    { "minutes", "seconds", 60.0 },
    { "minute",  "second",  60.0 },
    { "mins",    "secs",    60.0 },
    { "min",     "sec",     60.0 },
    { "m",       "s",       60.0 },

    
    { "days",    "weeks",   1.0 / 7.0 },
    { "day",     "weeks",   1.0 / 7.0 },
    { "day",     "week",    1.0 / 7.0 },
    { "d",       "w",       1.0 / 7.0 },

    
    { "weeks",   "days",    7.0 },
    { "week",    "days",    7.0 },
    { "week",    "day",     7.0 },
    { "wks",     "days",    7.0 },
    { "wk",      "day",     7.0 },
    { "w",       "d",       7.0 },

    
    { "days",    "hours",   24.0 },
    { "day",     "hours",   24.0 },
    { "day",     "hour",    24.0 },
    { "d",       "h",       24.0 },

    
    { "hours",   "days",    1.0 / 24.0 },
    { "hour",    "days",    1.0 / 24.0 },
    { "hour",    "day",     1.0 / 24.0 },
    { "hrs",     "days",    1.0 / 24.0 },
    { "hr",      "day",     1.0 / 24.0 },
    { "h",       "d",       1.0 / 24.0 }
};

double parse_number_with_suffix(const char* str) {
    char cleaned[64];
    strncpy(cleaned, str, sizeof(cleaned) - 1);
    cleaned[sizeof(cleaned) - 1] = '\0';
    
    int len = strlen(cleaned);
    while (len > 0 && isspace(cleaned[len - 1])) {
        cleaned[--len] = '\0';
    }
    
    if (len == 0) return 0.0;
    
    char suffix = tolower(cleaned[len - 1]);
    char num_part[64];
    strncpy(num_part, cleaned, len - 1);
    num_part[len - 1] = '\0';
    
    double val = atof(num_part);
    
    if (suffix == 'k') {
        return val * 1000.0;
    } else if (suffix == 'm') {
        return val * 1000000.0;
    } else {
        return atof(cleaned);
    }
}

void format_number_with_suffix(double num, char* out, size_t max_len) {
    if (num >= 1000000.0) {
        snprintf(out, max_len, "%gm", num / 1000000.0);
    } else if (num >= 1000.0) {
        snprintf(out, max_len, "%gk", num / 1000.0);
    } else {
        snprintf(out, max_len, "%g", num);
    }
}

double get_flipped_result(void) {
    return flipped_result;
}

bool is_flipped_mode(void) {
    return is_flipped;
}

void reset_flip_state(void) {
    is_flipped = false;
    flipped_result = 0.0;
}

void trim_trailinspaces(char* str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

void live_evaluate(const char* input, char* out_result, size_t max_len) {
    if (!input || strlen(input) == 0) {
        strncpy(out_result, "Waiting for expression...", max_len);
        return;
    }

    char clean[256];
    strncpy(clean, input, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';

    char* in_keyword = strchr(clean,'i');
    if (in_keyword) {
        char target_unit[64] = {0};
        strncpy(target_unit, in_keyword + 4, sizeof(target_unit) - 1);
        trim_trailinspaces(target_unit);

        *in_keyword = '\0'; 

        double val = 0.0;
        char source_unit[64] = {0};
        if (sscanf(clean, "%lf %63s", &val, source_unit) == 2) {
            for (size_t i = 0; i < CONVERSION_COUNT; i++) {
#if defined(_WIN32)
                if (StrCmpIA(source_unit, CONVERSIONS[i].from_unit) == 0 &&
                    StrCmpIA(target_unit, CONVERSIONS[i].to_unit) == 0) {
#else
                if (strcasecmp(source_unit, CONVERSIONS[i].from_unit) == 0 &&
                    strcasecmp(target_unit, CONVERSIONS[i].to_unit) == 0) {
#endif
                    snprintf(out_result, max_len, "= %.4g %s", val * CONVERSIONS[i].factor, CONVERSIONS[i].to_unit);
                    has_last_op = false;
                    return;
                }
            }
        }
        strncpy(out_result, "Unknown unit conversion format", max_len);
        has_last_op = false;
        return;
    }

    
    
    
    for (int i = 0; clean[i]; i++) {
        if (clean[i] == 'x' || clean[i] == 'X') clean[i] = '*';
    }

    char a_str[64] = {0};
    char op_str[4] = {0};
    char b_str[64] = {0};
    
    if (sscanf(clean, "%63s %3s %63s", a_str, op_str, b_str) == 3) {
        double a = parse_number_with_suffix(a_str);
        double b = parse_number_with_suffix(b_str);
        char op = op_str[0];
        
        last_a = a;
        last_b = b;
        last_op = op;
        has_last_op = true;
        strncpy(last_expression, clean, sizeof(last_expression) - 1);
        
        double result = 0.0;
        
        switch (op) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case '*': result = a * b; break;
            case '/': 
                if (b != 0) {
                    result = a / b;
                } else {
                    strncpy(out_result, "Error: Div by 0", max_len);
                    return;
                }
                break;
            default:
                strncpy(out_result, "Unknown operator", max_len);
                return;
        }
        
        char formatted[64];
        format_number_with_suffix(result, formatted, sizeof(formatted));
        
        snprintf(out_result, max_len, "= %s", formatted);
        return;
    }
    
    
    
    
    double val = parse_number_with_suffix(clean);
    if (val != 0.0 || (strlen(clean) > 0 && clean[0] == '0')) {
        char formatted[64];
        format_number_with_suffix(val, formatted, sizeof(formatted));
        snprintf(out_result, max_len, "= %s", formatted);
        has_last_op = false;
        return;
    }
    
    strncpy(out_result, "Parsing input...", max_len);
}




void flip_last_operation(char* out_result, size_t max_len) {
    if (!has_last_op) {
        strncpy(out_result, "No operation to flip", max_len);
        return;
    }
    
    if (last_b == 0 && last_op == '/') {
        strncpy(out_result, "Cannot flip: division by zero", max_len);
        return;
    }
    
    double flipped_a = last_b;
    double flipped_b = last_a;
    
    double result = 0.0;
    
    switch (last_op) {
        case '+': result = flipped_a + flipped_b; break;
        case '-': result = flipped_a - flipped_b; break;
        case '*': result = flipped_a * flipped_b; break;
        case '/': 
            if (flipped_b != 0) {
                result = flipped_a / flipped_b;
            } else {
                strncpy(out_result, "Error: Div by 0 in flipped", max_len);
                return;
            }
            break;
        default:
            strncpy(out_result, "Unknown operator", max_len);
            return;
    }
    
    flipped_result = result;
    is_flipped = true;
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(result, result_formatted, sizeof(result_formatted));
    
    snprintf(out_result, max_len, "FLIP: %s %c %s = %s", 
             a_formatted, last_op, b_formatted, result_formatted);
}




void get_flip_info(char* out, size_t max_len) {
    if (!has_last_op || !is_flipped) {
        strncpy(out, "Press Tab to flip", max_len);
        return;
    }
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(last_b, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(last_a, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(flipped_result, result_formatted, sizeof(result_formatted));
    
    snprintf(out, max_len, "↕ %s %c %s = %s", 
             a_formatted, last_op, b_formatted, result_formatted);
}

void get_flip_display(char* out, size_t max_len) {
    if (!has_last_op) {
        strncpy(out, "No operation to flip", max_len);
        return;
    }
    
    if (last_b == 0 && last_op == '/') {
        strncpy(out, "Cannot flip: division by zero", max_len);
        return;
    }
    
    double flipped_a = last_b;
    double flipped_b = last_a;
    
    double result = 0.0;
    
    switch (last_op) {
        case '+': result = flipped_a + flipped_b; break;
        case '-': result = flipped_a - flipped_b; break;
        case '*': result = flipped_a * flipped_b; break;
        case '/': 
            if (flipped_b != 0) {
                result = flipped_a / flipped_b;
            } else {
                strncpy(out, "Error: Div by 0 in flipped", max_len);
                return;
            }
            break;
        default:
            strncpy(out, "Unknown operator", max_len);
            return;
    }
    
    flipped_result = result;
    is_flipped = true;
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(result, result_formatted, sizeof(result_formatted));
    
    snprintf(out, max_len, "↕ %s %c %s = %s", 
             a_formatted, last_op, b_formatted, result_formatted);
}

void get_flip_expression(char* out, size_t max_len) {
    if (!has_last_op) {
        strncpy(out, "", max_len);
        return;
    }
    
    char a_formatted[32], b_formatted[32];
    format_number_with_suffix(last_b, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(last_a, b_formatted, sizeof(b_formatted));
    
    snprintf(out, max_len, "%s %c %s", a_formatted, last_op, b_formatted);
}



void calculator_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    char result_buffer[256] = {0};
    if (filter && strlen(filter) > 0) {
        live_evaluate(filter, result_buffer, sizeof(result_buffer));
    } else {
        strncpy(result_buffer, "Waiting for expression...", sizeof(result_buffer) - 1);
    }
    
    const char* items[1] = { result_buffer };

    BuildAliasDisplayItems(items, 1);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, NULL, items, 1,NULL);
}

bool calculator_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = lb->populated_cache[0];
    fl_copy_into_clipboard(result);
    return true;
}



void calculator_tab_callback(ListBoxInfo* lb) {
    if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) return;
    
    
    char current_input[256] = {0};
    UI_GetCurrentText(FlintContext_Get()->window.hEditControl,current_input);
    
    
    char a_str[64] = {0};
    char op_str[4] = {0};
    char b_str[64] = {0};
    
    if (sscanf(current_input, "%63s %3s %63s", a_str, op_str, b_str) == 3) {
        
        char flipped[256];
        snprintf(flipped, sizeof(flipped), "%s %s %s", b_str, op_str, a_str);
        UI_SetText(FlintContext_Get()->window.hEditControl,flipped);
        
        int len = strlen(flipped);
        UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,len);
        UI_GetParent(FlintContext_Get()->window.hEditControl);
        
        printf("[CALCULATOR] Flipped: %s\n", flipped);
    } else {
        printf("[CALCULATOR] Cannot flip: not a valid operation\n");
    }
}




bool calculator_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = lb->populated_cache[0] + 2;
    UI_SetText(FlintContext_Get()->window.hEditControl, result);
    int len = strlen(result);
    UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, len);
    return false;
}




void TriggerCalculator(void) {
    const char* default_msg[1] = { "Enter operation (e.g. 5 x 5 or 10 hours in minutes)" };

    PROMPT(.title = "🧮 Calculator", .items = default_msg, .count = 1,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = calculator_refresh_callback,
           .enter = calculator_enter_callback,
           .ctrl_enter = calculator_ctrlenter_callback,
           .tab = calculator_tab_callback);
  
                                                             
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+C", TriggerCalculator);
}
