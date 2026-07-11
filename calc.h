#ifndef CALC_H
#define CALC_H
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <shlwapi.h>
#include "types.h"

// ============================================================
// GLOBAL STATE FOR TAB FLIPPING
// ============================================================
static double g_last_a = 0.0;
static double g_last_b = 0.0;
static char g_last_op = 0;
static bool g_has_last_op = false;
static char g_last_expression[256] = {0};
static double g_flipped_result = 0.0;
static bool g_is_flipped = false;


extern char* PromptUser(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);
extern HWND hEditControl;

typedef struct {
    const char* from_unit;
    const char* to_unit;
    double factor;
} UnitConversion;

int CopyToClipboard(const char* text) {
    if (text == NULL) return 0;

    size_t len = strlen(text);
    if (len == 0) return 0;

    char* clean_text = (char*)malloc(len + 1);
    if (clean_text == NULL) return 0;

    size_t write_idx = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '\r') {
            clean_text[write_idx++] = text[i];
        }
    }
    clean_text[write_idx] = '\0';

    len = write_idx;
    if (len == 0) {
        free(clean_text);
        return 0;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (hMem == NULL) {
        free(clean_text);
        return 0;
    }

    char* pMem = (char*)GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        free(clean_text);
        return 0;
    }
    
    memcpy(pMem, clean_text, len + 1); 
    GlobalUnlock(hMem);
    free(clean_text);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hMem);
        return 0;
    }

    if (!EmptyClipboard()) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    if (SetClipboardData(CF_TEXT, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    CloseClipboard();
    return 1;
}

// ============================================================
// PARSE NUMBER WITH K/M SUFFIX
// ============================================================
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

// ============================================================
// FORMAT NUMBER WITH K/M SUFFIX (for display)
// ============================================================
void format_number_with_suffix(double num, char* out, size_t max_len) {
    if (num >= 1000000.0) {
        snprintf(out, max_len, "%.4gm", num / 1000000.0);
    } else if (num >= 1000.0) {
        snprintf(out, max_len, "%.4gk", num / 1000.0);
    } else {
        snprintf(out, max_len, "%.10g", num);
    }
}

// ============================================================
// GET FLIPPED RESULT (for Tab key)
// ============================================================
double get_flipped_result() {
    return g_flipped_result;
}

bool is_flipped_mode() {
    return g_is_flipped;
}

void reset_flip_state() {
    g_is_flipped = false;
    g_flipped_result = 0.0;
}

// ============================================================
// EXTENSIBLE CONVERSION TABLE
// ============================================================
static const UnitConversion G_CONVERSIONS[] = {
    // === MINUTES TO HOURS ===
    { "minutes", "hours",   1.0 / 60.0 },
    { "minute",  "hours",   1.0 / 60.0 },
    { "minute",  "hour",    1.0 / 60.0 },
    { "mins",    "hours",   1.0 / 60.0 },
    { "mins",    "hour",    1.0 / 60.0 },
    { "min",     "hours",   1.0 / 60.0 },
    { "min",     "hour",    1.0 / 60.0 },
    { "m",       "h",       1.0 / 60.0 },

    // === HOURS TO MINUTES ===
    { "hours",   "minutes", 60.0 },
    { "hour",    "minutes", 60.0 },
    { "hour",    "minute",  60.0 },
    { "hrs",     "mins",    60.0 },
    { "hrs",     "min",     60.0 },
    { "hr",      "mins",    60.0 },
    { "hr",      "min",     60.0 },
    { "h",       "m",       60.0 },

    // === SECONDS TO HOURS ===
    { "seconds", "hours",   1.0 / 3600.0 },
    { "second",  "hour",    1.0 / 3600.0 },
    { "secs",    "hours",   1.0 / 3600.0 },
    { "secs",    "hr",      1.0 / 3600.0 },
    { "sec",     "hour",    1.0 / 3600.0 },
    { "sec",     "hr",      1.0 / 3600.0 },
    { "s",       "h",       1.0 / 3600.0 },

    // === HOURS TO SECONDS ===
    { "hours",   "seconds", 3600.0 },
    { "hour",    "second",  3600.0 },
    { "hours",   "secs",    3600.0 },
    { "hour",    "sec",     3600.0 },
    { "hrs",     "secs",    3600.0 },
    { "hr",      "sec",     3600.0 },
    { "h",       "s",       3600.0 },

    // === SECONDS TO MINUTES ===
    { "seconds", "minutes", 1.0 / 60.0 },
    { "second",  "minute",  1.0 / 60.0 },
    { "secs",    "mins",    1.0 / 60.0 },
    { "sec",     "min",     1.0 / 60.0 },
    { "s",       "m",       1.0 / 60.0 },

    // === MINUTES TO SECONDS ===
    { "minutes", "seconds", 60.0 },
    { "minute",  "second",  60.0 },
    { "mins",    "secs",    60.0 },
    { "min",     "sec",     60.0 },
    { "m",       "s",       60.0 },

    // === DAYS TO WEEKS ===
    { "days",    "weeks",   1.0 / 7.0 },
    { "day",     "weeks",   1.0 / 7.0 },
    { "day",     "week",    1.0 / 7.0 },
    { "d",       "w",       1.0 / 7.0 },

    // === WEEKS TO DAYS ===
    { "weeks",   "days",    7.0 },
    { "week",    "days",    7.0 },
    { "week",    "day",     7.0 },
    { "wks",     "days",    7.0 },
    { "wk",      "day",     7.0 },
    { "w",       "d",       7.0 },

    // === DAYS TO HOURS ===
    { "days",    "hours",   24.0 },
    { "day",     "hours",   24.0 },
    { "day",     "hour",    24.0 },
    { "d",       "h",       24.0 },

    // === HOURS TO DAYS ===
    { "hours",   "days",    1.0 / 24.0 },
    { "hour",    "days",    1.0 / 24.0 },
    { "hour",    "day",     1.0 / 24.0 },
    { "hrs",     "days",    1.0 / 24.0 },
    { "hr",      "day",     1.0 / 24.0 },
    { "h",       "d",       1.0 / 24.0 }
};
#define CONVERSION_COUNT (sizeof(G_CONVERSIONS) / sizeof(G_CONVERSIONS[0]))

void trim_trailing_spaces(char* str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

// ============================================================
// MAIN EVALUATE FUNCTION
// ============================================================
void live_evaluate(const char* input, char* out_result, size_t max_len) {
    if (!input || strlen(input) == 0) {
        strncpy(out_result, "Waiting for expression...", max_len);
        return;
    }

    char clean[256];
    strncpy(clean, input, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';

    // ============================================================
    // 1. Process Conversions via " in " keyword
    // ============================================================
    char* in_keyword = StrStrIA(clean, " in ");
    if (in_keyword) {
        char target_unit[64] = {0};
        strncpy(target_unit, in_keyword + 4, sizeof(target_unit) - 1);
        trim_trailing_spaces(target_unit);

        *in_keyword = '\0'; 

        double val = 0.0;
        char source_unit[64] = {0};
        if (sscanf(clean, "%lf %63s", &val, source_unit) == 2) {
            for (size_t i = 0; i < CONVERSION_COUNT; i++) {
                if (StrCmpIA(source_unit, G_CONVERSIONS[i].from_unit) == 0 &&
                    StrCmpIA(target_unit, G_CONVERSIONS[i].to_unit) == 0) {
                    snprintf(out_result, max_len, "= %.4g %s", val * G_CONVERSIONS[i].factor, G_CONVERSIONS[i].to_unit);
                    g_has_last_op = false;
                    return;
                }
            }
        }
        strncpy(out_result, "Unknown unit conversion format", max_len);
        g_has_last_op = false;
        return;
    }

    // ============================================================
    // 2. Process Standard Mathematical Equations
    // ============================================================
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
        
        g_last_a = a;
        g_last_b = b;
        g_last_op = op;
        g_has_last_op = true;
        strncpy(g_last_expression, clean, sizeof(g_last_expression) - 1);
        
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
    
    // ============================================================
    // 3. Try simple number parsing
    // ============================================================
    double val = parse_number_with_suffix(clean);
    if (val != 0.0 || (strlen(clean) > 0 && clean[0] == '0')) {
        char formatted[64];
        format_number_with_suffix(val, formatted, sizeof(formatted));
        snprintf(out_result, max_len, "= %s", formatted);
        g_has_last_op = false;
        return;
    }
    
    strncpy(out_result, "Parsing input...", max_len);
}

// ============================================================
// FLIP THE LAST OPERATION (for Tab key)
// ============================================================
void flip_last_operation(char* out_result, size_t max_len) {
    if (!g_has_last_op) {
        strncpy(out_result, "No operation to flip", max_len);
        return;
    }
    
    if (g_last_b == 0 && g_last_op == '/') {
        strncpy(out_result, "Cannot flip: division by zero", max_len);
        return;
    }
    
    double flipped_a = g_last_b;
    double flipped_b = g_last_a;
    
    double result = 0.0;
    
    switch (g_last_op) {
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
    
    g_flipped_result = result;
    g_is_flipped = true;
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(result, result_formatted, sizeof(result_formatted));
    
    snprintf(out_result, max_len, "FLIP: %s %c %s = %s", 
             a_formatted, g_last_op, b_formatted, result_formatted);
}

// ============================================================
// GET FLIP INFO FOR DISPLAY
// ============================================================
void get_flip_info(char* out, size_t max_len) {
    if (!g_has_last_op || !g_is_flipped) {
        strncpy(out, "Press Tab to flip", max_len);
        return;
    }
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(g_last_b, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(g_last_a, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(g_flipped_result, result_formatted, sizeof(result_formatted));
    
    snprintf(out, max_len, "↕ %s %c %s = %s", 
             a_formatted, g_last_op, b_formatted, result_formatted);
}

void get_flip_display(char* out, size_t max_len) {
    if (!g_has_last_op) {
        strncpy(out, "No operation to flip", max_len);
        return;
    }
    
    if (g_last_b == 0 && g_last_op == '/') {
        strncpy(out, "Cannot flip: division by zero", max_len);
        return;
    }
    
    double flipped_a = g_last_b;
    double flipped_b = g_last_a;
    
    double result = 0.0;
    
    switch (g_last_op) {
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
    
    g_flipped_result = result;
    g_is_flipped = true;
    
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_with_suffix(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_with_suffix(result, result_formatted, sizeof(result_formatted));
    
    snprintf(out, max_len, "↕ %s %c %s = %s", 
             a_formatted, g_last_op, b_formatted, result_formatted);
}

void get_flip_expression(char* out, size_t max_len) {
    if (!g_has_last_op) {
        strncpy(out, "", max_len);
        return;
    }
    
    char a_formatted[32], b_formatted[32];
    format_number_with_suffix(g_last_b, a_formatted, sizeof(a_formatted));
    format_number_with_suffix(g_last_a, b_formatted, sizeof(b_formatted));
    
    snprintf(out, max_len, "%s %c %s", a_formatted, g_last_op, b_formatted);
}

// ============================================================
// CALCULATOR CALLBACKS
// ============================================================
static void calculator_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    char result_buffer[256] = {0};
    if (filter && strlen(filter) > 0) {
        live_evaluate(filter, result_buffer, sizeof(result_buffer));
    } else {
        strncpy(result_buffer, "Waiting for expression...", sizeof(result_buffer) - 1);
    }
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)result_buffer);
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}

static void calculator_enter_callback(const char* selection) {
    if (selection && strlen(selection) > 0) {
        char* result = (char*)selection;
        if (strncmp(selection, "= ", 2) == 0) {
            result = (char*)selection + 2;
        }
        CopyToClipboard(result);
        printf("[CALCULATOR] Copied: %s\n", result);
    }
}

// ============================================================
// TAB CALLBACK - Flips the numbers
// ============================================================
static void calculator_tab_callback(void) {
    if (!hEditControl) return;
    
    // Get current input
    char current_input[256] = {0};
    GetWindowTextA(hEditControl, current_input, sizeof(current_input));
    
    // Try to parse it to see if it's an operation we can flip
    char a_str[64] = {0};
    char op_str[4] = {0};
    char b_str[64] = {0};
    
    if (sscanf(current_input, "%63s %3s %63s", a_str, op_str, b_str) == 3) {
        // We have a valid operation, flip it
        char flipped[256];
        snprintf(flipped, sizeof(flipped), "%s %s %s", b_str, op_str, a_str);
        SetWindowTextA(hEditControl, flipped);
        
        // Trigger refresh to update the result
        int len = strlen(flipped);
        SendMessageA(hEditControl, EM_SETSEL, len, len);
        SendMessageA(GetParent(hEditControl), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)hEditControl);
        
        printf("[CALCULATOR] Flipped: %s\n", flipped);
    } else {
        printf("[CALCULATOR] Cannot flip: not a valid operation\n");
    }
}

// ============================================================
// CTRL+ENTER CALLBACK - Puts result back into input
// ============================================================
static void calculator_ctrlenter_callback(const char* selection) {
    if (!selection || strlen(selection) == 0 || !hEditControl) return;
    
    // Extract the result (skip "= " prefix)
    char* result = (char*)selection;
    if (strncmp(selection, "= ", 2) == 0) {
        result = (char*)selection + 2;
    }
    
    // Put it in the edit box
    SetWindowTextA(hEditControl, result);
    
    // Move cursor to the end
    int len = strlen(result);
    SendMessageA(hEditControl, EM_SETSEL, len, len);
    
    // Trigger refresh to evaluate again (if user types more)
    SendMessageA(GetParent(hEditControl), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)hEditControl);
    
    printf("[CALCULATOR] Result put back: %s\n", result);
}

// ============================================================
// TRIGGER CALCULATOR
// ============================================================
void TriggerCalculator(HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return;

    const char* default_msg[1] = { "Enter operation (e.g. 5 x 5 or 10 hours in minutes)" };
    PromptUser(default_msg, 1, 220, 550, 
                        calculator_refresh_callback,   // refresh -> evaluates on typing
                        calculator_enter_callback,     // enter -> copies result to clipboard
                        calculator_ctrlenter_callback, // ctrl enter -> puts result back into input
                        calculator_tab_callback);      // tab -> flips the numbers
}

#endif
