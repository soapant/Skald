#include "core/flapi.h"
#include <math.h>
#include <ctype.h>

double last_a = 0.0;
double last_b = 0.0;
char last_op = 0;
bool has_last_op = false;
char last_expression[256] = {0};
double flipped_result = 0.0;
bool is_flipped = false;

typedef struct {
    const char* canonical;
    double to_seconds;
    const char* aliases[16];
    int alias_count;
} UnitDef;

static UnitDef UNITS[] = {
    {"ms",   0.001,       {"ms","millis","millisecond","milliseconds","milli","msec","msecs"}, 7},
    {"s",    1.0,         {"s","sec","secs","second","seconds","sec.","secs.","sec"}, 6},
    {"min",  60.0,        {"m","min","mins","minute","minutes"}, 5},
    {"hour", 3600.0,      {"h","hr","hrs","hour","hours"}, 5},
    {"day",  86400.0,     {"d","day","days"}, 3},
    {"week", 604800.0,    {"w","wk","wks","week","weeks"}, 5},
    {"month",2629746.0,   {"mo","mos","month","months"}, 4}, 
    {"year", 31557600.0,  {"y","yr","yrs","year","years"}, 5},
};
#define UNIT_COUNT (sizeof(UNITS)/sizeof(UNITS[0]))

static int str_casecmp_n(const char* a, const char* b) {
    while(*a && *b){ int ca=tolower((unsigned char)*a), cb=tolower((unsigned char)*b); if(ca!=cb) return ca-cb; a++; b++; }
    return tolower((unsigned char)*a)-tolower((unsigned char)*b);
}
static bool str_equals_ci(const char* a, const char* b){
    if(!a||!b) return false;
    if(strlen(a)!=strlen(b)) return false;
    return str_casecmp_n(a,b)==0;
}
static int levenshtein_ci(const char* s, const char* t){
    int ls=strlen(s), lt=strlen(t);
    if(ls>32||lt>32) return 999;
    int d[33][33];
    for(int i=0;i<=ls;i++) d[i][0]=i;
    for(int j=0;j<=lt;j++) d[0][j]=j;
    for(int i=1;i<=ls;i++) for(int j=1;j<=lt;j++){
        int cost = tolower((unsigned char)s[i-1])==tolower((unsigned char)t[j-1])?0:1;
        int del=d[i-1][j]+1, ins=d[i][j-1]+1, sub=d[i-1][j-1]+cost;
        int m=del<ins?del:ins; m=m<sub?m:sub; d[i][j]=m;
    }
    return d[ls][lt];
}
static UnitDef* find_unit_fuzzy(const char* input){
    if(!input || !*input) return NULL;
    
    char tmp[64]; strncpy(tmp, input, sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0';
    
    char* s=tmp; while(*s && isspace((unsigned char)*s)) s++;
    char* e=s+strlen(s); while(e>s && isspace((unsigned char)*(e-1))) *--e='\0';
    if(!*s) return NULL;
    
    for(size_t i=0;i<UNIT_COUNT;i++) for(int j=0;j<UNITS[i].alias_count;j++) if(str_equals_ci(s, UNITS[i].aliases[j])) return &UNITS[i];
    for(size_t i=0;i<UNIT_COUNT;i++) if(str_equals_ci(s, UNITS[i].canonical)) return &UNITS[i];
    
    for(size_t i=0;i<UNIT_COUNT;i++) for(int j=0;j<UNITS[i].alias_count;j++){
        if(Platform_StringContainsCaseInsensitive(s, (char*)UNITS[i].aliases[j]) || Platform_StringContainsCaseInsensitive((char*)UNITS[i].aliases[j], s)) return &UNITS[i];
    }
    
    UnitDef* best=NULL; int bestd=999;
    for(size_t i=0;i<UNIT_COUNT;i++) for(int j=0;j<UNITS[i].alias_count;j++){
        int d=levenshtein_ci(s, UNITS[i].aliases[j]);
        if(d<bestd && d<=2 && strlen(s)>=3) { bestd=d; best=&UNITS[i]; }
    }
    if(best) return best;
    
    for(size_t i=0;i<UNIT_COUNT;i++){ int d=levenshtein_ci(s, UNITS[i].canonical); if(d<bestd && d<=2 && strlen(s)>=3){ bestd=d; best=&UNITS[i]; } }
    return best;
}

void format_number_plain(double num, char* out, size_t max_len){
    if(isnan(num) || isinf(num)){ snprintf(out, max_len, "%g", num); return; }
    double a=fabs(num);
    if(a>=1e12 || (a!=0 && a<1e-6)){
        snprintf(out, max_len, "%.6e", num);
    } else {
        
        snprintf(out, max_len, "%.12g", num);
    }
}

void format_number_with_suffix(double num, char* out, size_t max_len){
  format_number_plain(num,out,max_len);
}

double parse_number_with_suffix(const char* str) {
    char cleaned[64];
    strncpy(cleaned, str, sizeof(cleaned) - 1);
    cleaned[sizeof(cleaned) - 1] = '\0';
    int len = strlen(cleaned);
    while (len > 0 && isspace((unsigned char)cleaned[len - 1])) cleaned[--len] = '\0';
    char* p=cleaned; while(*p && isspace((unsigned char)*p)) p++;
    if(*p=='\0') return 0.0;
    
    len=strlen(p);
    if(len==0) return 0.0;
    char suffix = tolower((unsigned char)p[len - 1]);
    if(suffix=='k' || suffix=='m'){
        
        
        
        char num_part[64]; strncpy(num_part, p, len-1); num_part[len-1]='\0';
        char* end=NULL; double v=strtod(num_part, &end);
        if(end && *end=='\0'){
            if(suffix=='k') return v*1000.0;
            if(suffix=='m') return v*1000000.0;
        }
    }
    return atof(p);
}

void trim_trailinspaces(char* str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) str[--len] = '\0';
}




static int precedence(char op) {
  if(op=='*'||op=='/'||op=='^') return 2;
  if(op=='+'||op=='-') return 1; 
  return 0; 
}
static bool apply_op(double* vals, int* vtop, char* ops, int* otop, bool* ok){
    if(*vtop<1 || *otop<0) { *ok=false; return false; }
    double b=vals[(*vtop)--];
    double a=vals[(*vtop)--];
    char op=ops[(*otop)--];
    double r=0;
    switch(op){
        case '+': r=a+b; break;
        case '-': r=a-b; break;
        case '*': r=a*b; break;
        case '^': r=pow(a,b); break;
        case '/': if(b==0){ *ok=false; return false; } r=a/b; break;
        default: *ok=false; return false;
    }
    vals[++(*vtop)]=r; return true;
}
static double evaluate_expression(const char* expr, bool* out_ok){
    if(!expr){ if(out_ok) *out_ok=false; return 0; }
    char clean[512]; strncpy(clean, expr, sizeof(clean)-1); clean[sizeof(clean)-1]='\0';
    
    for(int i=0; clean[i]; i++) if(clean[i]=='x'||clean[i]=='X') clean[i]='*';
    double vals[128]; int vtop=-1;
    char ops[128]; int otop=-1;
    bool ok=true;
    bool expect_number=true;
    int n=strlen(clean);
    for(int i=0;i<n;){
        if(isspace((unsigned char)clean[i])){ i++; continue; }
        if(clean[i]=='('){ ops[++otop]='('; expect_number=true; i++; continue; }
        if(clean[i]==')'){
            while(otop>=0 && ops[otop]!='('){ if(!apply_op(vals,&vtop,ops,&otop,&ok)) break; }
            if(otop>=0 && ops[otop]=='(') otop--; else ok=false;
            expect_number=false; i++; continue;
        }
        if(expect_number && (clean[i]=='-' || clean[i]=='+')){
            
            int j=i+1; while(j<n && isspace((unsigned char)clean[j])) j++;
            if(j<n && (isdigit((unsigned char)clean[j]) || clean[j]=='.')){
                
            } else if(clean[j]=='('){
                
                vals[++vtop]=0; ops[++otop]=clean[i]; expect_number=true; i++; continue;
            } else { ok=false; break; }
        }
        if(isdigit((unsigned char)clean[i]) || clean[i]=='.' || (expect_number && (clean[i]=='-'||clean[i]=='+'))){
            int j=i;
            if(clean[j]=='-'||clean[j]=='+') j++;
            while(j<n && (isdigit((unsigned char)clean[j]) || clean[j]=='.')) j++;
            
            if(j<n && (clean[j]=='k'||clean[j]=='K'||clean[j]=='m'||clean[j]=='M')){
                
                
                char nxt = (j+1<n)?clean[j+1]:'\0';
                if(nxt=='\0' || isspace((unsigned char)nxt) || strchr("+-*^/()", nxt)){
                    j++;
                }
            }
            char numtok[64]={0}; int len=j-i; if(len>= (int)sizeof(numtok)) len=sizeof(numtok)-1;
            strncpy(numtok, clean+i, len); numtok[len]='\0';
            double v=parse_number_with_suffix(numtok);
            
            if(v==0 && numtok[0]!='0'){
                
            }
            vals[++vtop]=v;
            expect_number=false;
            i=j; continue;
        }
        if(strchr("+-^*/", clean[i])){
            char cur=clean[i];
            while(otop>=0 && ops[otop]!='(' && precedence(ops[otop])>=precedence(cur)){
                if(!apply_op(vals,&vtop,ops,&otop,&ok)) break;
            }
            ops[++otop]=cur;
            expect_number=true; i++; continue;
        }
        
        ok=false; break;
    }
    while(ok && otop>=0){
        if(ops[otop]=='('||ops[otop]==')'){ ok=false; break; }
        if(!apply_op(vals,&vtop,ops,&otop,&ok)) break;
    }
    if(!ok || vtop!=0){ if(out_ok) *out_ok=false; return 0; }
    if(out_ok) *out_ok=true;
    return vals[0];
}




static int find_in_keyword(const char* s){ 
    if(!s) return -1;
    int n=strlen(s);
    for(int i=0;i<n-2;i++){
        if(tolower((unsigned char)s[i])==' ' && tolower((unsigned char)s[i+1])=='i' && tolower((unsigned char)s[i+2])=='n'){
            
            
            if(i+3>=n || isspace((unsigned char)s[i+3]) || s[i+3]=='\0'){
                
                return i+1; 
            }
        }
    }
    return -1;
}
static void trim_copy(const char* src, char* dst, size_t cap){
    if(!src||!dst||cap==0) return;
    while(*src && isspace((unsigned char)*src)) src++;
    size_t len=strlen(src); while(len>0 && isspace((unsigned char)src[len-1])) len--;
    if(len>=cap) len=cap-1;
    strncpy(dst, src, len); dst[len]='\0';
}

void live_evaluate(const char* input, char* out_result, size_t max_len) {
    if (!input || strlen(input) == 0) { strncpy(out_result, "Waiting for expression...", max_len); out_result[max_len-1]='\0'; return; }
    char clean[256]; strncpy(clean, input, sizeof(clean)-1); clean[sizeof(clean)-1]='\0';
    int in_pos = find_in_keyword(clean);
    if(in_pos>=0){
        char target_unit[64]={0};
        strncpy(target_unit, clean + in_pos + 2, sizeof(target_unit)-1);
        trim_trailinspaces(target_unit);
        char* tp=target_unit; while(*tp && isspace((unsigned char)*tp)) tp++; memmove(target_unit, tp, strlen(tp)+1);
        clean[in_pos-1]='\0'; 
        trim_trailinspaces(clean);
        char left_trim[256]; trim_copy(clean, left_trim, sizeof(left_trim));
        char* last_sp = strrchr(left_trim, ' ');
        char source_unit[64]={0};
        char expr_part[256]={0};
        UnitDef* fromU=NULL;
        double val=0; bool ok=false;
        if(last_sp){
            char cand[64]; trim_copy(last_sp+1, cand, sizeof(cand));
            UnitDef* u=find_unit_fuzzy(cand);
            if(u){
                strncpy(source_unit, cand, sizeof(source_unit)-1);
                *last_sp='\0'; trim_copy(left_trim, expr_part, sizeof(expr_part));
                if(expr_part[0]=='\0') strcpy(expr_part,"1");
                fromU=u;
                val=evaluate_expression(expr_part,&ok);
                if(!ok) val=parse_number_with_suffix(expr_part); 
            } else {
                int L=strlen(left_trim); int p=L-1; while(p>=0 && isalpha((unsigned char)left_trim[p])) p--;
                if(p<L-1){
                    char unit_cand[64]={0}; strncpy(unit_cand, left_trim+p+1, sizeof(unit_cand)-1);
                    UnitDef* u2=find_unit_fuzzy(unit_cand);
                    if(u2){
                        fromU=u2; strncpy(source_unit, unit_cand, sizeof(source_unit)-1);
                        char numpart[256]={0}; strncpy(numpart, left_trim, p+1); numpart[p+1]='\0';
                        trim_copy(numpart, expr_part, sizeof(expr_part));
                        if(expr_part[0]=='\0') strcpy(expr_part,"1");
                        val=evaluate_expression(expr_part,&ok); if(!ok) val=parse_number_with_suffix(expr_part);
                    } else {
                        strncpy(out_result, "Unknown unit conversion format", max_len); has_last_op=false; return;
                    }
                } else {
                    strncpy(out_result, "Unknown unit conversion format", max_len); has_last_op=false; return;
                }
            }
        } else {
            
            int L=strlen(left_trim); int p=L-1; while(p>=0 && isalpha((unsigned char)left_trim[p])) p--;
            if(p<L-1){
                char unit_cand[64]={0}; strncpy(unit_cand, left_trim+p+1, sizeof(unit_cand)-1);
                UnitDef* u2=find_unit_fuzzy(unit_cand);
                if(u2){ fromU=u2; strncpy(source_unit, unit_cand, sizeof(source_unit)-1);
                    char numpart[256]={0}; strncpy(numpart, left_trim, p+1); numpart[p+1]='\0';
                    trim_copy(numpart, expr_part, sizeof(expr_part));
                    if(expr_part[0]=='\0') strcpy(expr_part,"1");
                    val=evaluate_expression(expr_part,&ok); if(!ok) val=parse_number_with_suffix(expr_part);
                } else { strncpy(out_result, "Unknown unit conversion format", max_len); has_last_op=false; return; }
            } else { strncpy(out_result, "Unknown unit conversion format", max_len); has_last_op=false; return; }
        }
        UnitDef* toU=find_unit_fuzzy(target_unit);
        if(!fromU || !toU){ strncpy(out_result, "Unknown unit conversion format", max_len); has_last_op=false; return; }
        double result = val * (fromU->to_seconds / toU->to_seconds);
        char formatted[64]; format_number_plain(result, formatted, sizeof(formatted));
        snprintf(out_result, max_len, "= %s %s", formatted, toU->canonical);
        has_last_op=false;
        
        snprintf(last_expression, sizeof(last_expression), "%s", input);
        return;
    }
    
    
    bool ok=false;
    double result=evaluate_expression(clean,&ok);
    if(ok){
        
        
        snprintf(last_expression, sizeof(last_expression), "%s", clean);
        
        
        has_last_op=false; 
        char formatted[64]; format_number_plain(result, formatted, sizeof(formatted));
        snprintf(out_result, max_len, "= %s", formatted);
        return;
    }
    
    double val = parse_number_with_suffix(clean);
    if (val != 0.0 || (strlen(clean) > 0 && clean[0] == '0')) {
        char formatted[64]; format_number_plain(val, formatted, sizeof(formatted));
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
            if (flipped_b != 0) result = flipped_a / flipped_b;
            else { strncpy(out_result, "Error: Div by 0 in flipped", max_len); return; }
            break;
        default: strncpy(out_result, "Unknown operator", max_len); return;
    }
    flipped_result = result;
    is_flipped = true;
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_plain(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_plain(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_plain(result, result_formatted, sizeof(result_formatted));
    snprintf(out_result, max_len, "FLIP: %s %c %s = %s", a_formatted, last_op, b_formatted, result_formatted);
}

void get_flip_info(char* out, size_t max_len) {
    if (!has_last_op || !is_flipped) { strncpy(out, "Press Tab to flip", max_len); return; }
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_plain(last_b, a_formatted, sizeof(a_formatted));
    format_number_plain(last_a, b_formatted, sizeof(b_formatted));
    format_number_plain(flipped_result, result_formatted, sizeof(result_formatted));
    snprintf(out, max_len, "↕ %s %c %s = %s", a_formatted, last_op, b_formatted, result_formatted);
}

void get_flip_display(char* out, size_t max_len) {
    if (!has_last_op) { strncpy(out, "No operation to flip", max_len); return; }
    if (last_b == 0 && last_op == '/') { strncpy(out, "Cannot flip: division by zero", max_len); return; }
    double flipped_a = last_b; double flipped_b = last_a; double result = 0.0;
    switch (last_op) {
        case '+': result = flipped_a + flipped_b; break;
        case '-': result = flipped_a - flipped_b; break;
        case '*': result = flipped_a * flipped_b; break;
        case '^': result = pow(flipped_a,flipped_b); break;
        case '/': if (flipped_b != 0) result = flipped_a / flipped_b; else { strncpy(out, "Error: Div by 0 in flipped", max_len); return; } break;
        default: strncpy(out, "Unknown operator", max_len); return;
    }
    flipped_result = result; is_flipped = true;
    char a_formatted[32], b_formatted[32], result_formatted[32];
    format_number_plain(flipped_a, a_formatted, sizeof(a_formatted));
    format_number_plain(flipped_b, b_formatted, sizeof(b_formatted));
    format_number_plain(result, result_formatted, sizeof(result_formatted));
    snprintf(out, max_len, "↕ %s %c %s = %s", a_formatted, last_op, b_formatted, result_formatted);
}

void get_flip_expression(char* out, size_t max_len) {
    if (!has_last_op) { strncpy(out, "", max_len); return; }
    char a_formatted[32], b_formatted[32];
    format_number_plain(last_b, a_formatted, sizeof(a_formatted));
    format_number_plain(last_a, b_formatted, sizeof(b_formatted));
    snprintf(out, max_len, "%s %c %s", a_formatted, last_op, b_formatted);
}



static char g_calc_last_result[256] = {0};

void calculator_render_callback(PromptState *state) {
    char result[256] = {0};
    if (strlen(state->filter) > 0) {
        live_evaluate(state->filter, result, sizeof(result));
    } else {
        strncpy(result, "Waiting for expression...", sizeof(result) - 1);
    }
    fl_place_editbox();
    strncpy(g_calc_last_result, result, sizeof(g_calc_last_result)-1);
    int cx = 0;
    int cy = state->height / 2 - 30;
    fl_place_text(.x=cx, .y=cy, .text=g_calc_last_result, .align="CENTER", .pad=cx,.fontsize=30);
    fl_place_text(.x=cx, .y=cy+36, .text="Enter: copy  Ctrl+Enter: use  Tab: flip", .align="CENTER", .pad=cx,.fontsize=30);
}

bool calculator_enter_callback(const char* selection, ListBoxInfo* lb) {
    fl_copy_to_clipboard(g_calc_last_result);
    return true;
}

void calculator_tab_callback(ListBoxInfo* lb) {
    char current_input[256] = {0};
    UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl,current_input,sizeof(current_input));
    char a_str[64] = {0}; char op_str[4] = {0}; char b_str[64] = {0};
    if (sscanf(current_input, "%63s %3s %63s", a_str, op_str, b_str) == 3) {
        char flipped[256]; snprintf(flipped, sizeof(flipped), "%s %s %s", b_str, op_str, a_str);
        UI_SetText(FlintContext_Get()->window.hEditControl,flipped);
        int len = strlen(flipped); UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,len);
        UI_GetParent(FlintContext_Get()->window.hEditControl);
        printf("[CALCULATOR] Flipped: %s\n", flipped);
    }
}

bool calculator_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    (void)selection; (void)lb;
    const char *src = g_calc_last_result;
    if (src[0]=='=' && src[1]==' ') src+=2;
    
    UI_SetText(FlintContext_Get()->window.hEditControl, src);
    int len = strlen(src); UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, len);
    return false;
}

void TriggerCalculator(void) {
    PROMPT(.title = "🧮 Calculator",
           .render = calculator_render_callback,
           .enter = calculator_enter_callback,
           .ctrl_enter = calculator_ctrlenter_callback,
           .tab = calculator_tab_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+C", TriggerCalculator,"Fortnite");
    fl_register_feature("Calculator", TriggerCalculator);
}
