#ifndef CALC_H
#define CALC_H

#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char* from_unit;
    const char* to_unit;
    double factor;
} UnitConversion;

double parse_number_with_suffix(const char* str);
void format_number_with_suffix(double num, char* out, size_t max_len);
double get_flipped_result(void);
bool is_flipped_mode(void);
void reset_flip_state(void);
void trim_trailing_spaces(char* str);
void live_evaluate(const char* input, char* out_result, size_t max_len);
void flip_last_operation(char* out_result, size_t max_len);
void get_flip_info(char* out, size_t max_len);
void get_flip_display(char* out, size_t max_len);
void get_flip_expression(char* out, size_t max_len);
void TriggerCalculator(void);

#define CONVERSION_COUNT (sizeof(G_CONVERSIONS) / sizeof(G_CONVERSIONS[0]))

#endif
