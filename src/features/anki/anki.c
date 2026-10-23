#include "core/flapi.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

typedef enum { CARD_TYPE_BASIC, CARD_TYPE_CLOZE, CARD_TYPE_UNKNOWN } CardType;
typedef struct { char* front; char* back; } BasicCard;
typedef struct { char* text; } ClozeCard;
typedef struct { CardType type; long long due; int queue; long long due_ts; union { BasicCard basic; ClozeCard cloze; } data; } TYPE_NAME;
typedef struct { TYPE_NAME* items; size_t count; size_t capacity; } CardList;

typedef enum {
    ANKI_STATE_IDLE = 0,
    ANKI_STATE_QUESTION,
    ANKI_STATE_ANSWER
} AnkiState;

static TYPE_NAME g_current_card = {0};
static AnkiState g_anki_state = ANKI_STATE_IDLE;

static FlLibrary g_sqlite_lib = NULL;
static int (*p_sqlite3_open_v2)(const char*, sqlite3**, int, const char*) = NULL;
static const char* (*p_sqlite3_errmsg)(sqlite3*) = NULL;
static int (*p_sqlite3_prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = NULL;
static int (*p_sqlite3_step)(sqlite3_stmt*) = NULL;
static const unsigned char* (*p_sqlite3_column_text)(sqlite3_stmt*, int) = NULL;
static long long (*p_sqlite3_column_int64)(sqlite3_stmt*, int) = NULL;
static int (*p_sqlite3_finalize)(sqlite3_stmt*) = NULL;
static int (*p_sqlite3_bind_text)(sqlite3_stmt*, int, const char*, int, void(*)(void*)) = NULL;
static int (*p_sqlite3_close)(sqlite3*) = NULL;

static FlThread anki_thread = NULL;
static FlMutex anki_lock = NULL;
static bool anki_running = false;
static int time_between_qs = 30;

static char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* new_s = malloc(len);
    if (new_s) memcpy(new_s, s, len);
    return new_s;
}

static TYPE_NAME card_clone(TYPE_NAME card) {
    TYPE_NAME clone = card;
    if (card.type == CARD_TYPE_BASIC) {
        clone.data.basic.front = safe_strdup(card.data.basic.front);
        clone.data.basic.back = safe_strdup(card.data.basic.back);
    } else if (card.type == CARD_TYPE_CLOZE) {
        clone.data.cloze.text = safe_strdup(card.data.cloze.text);
    }
    return clone;
}

static void free_card_contents(TYPE_NAME* card) {
    if (card->type == CARD_TYPE_BASIC) {
        free(card->data.basic.front);
        free(card->data.basic.back);
        card->data.basic.front = NULL;
        card->data.basic.back = NULL;
    } else if (card->type == CARD_TYPE_CLOZE) {
        free(card->data.cloze.text);
        card->data.cloze.text = NULL;
    }
    card->type = CARD_TYPE_UNKNOWN;
}

static int sqlite_init(void) {
    if (g_sqlite_lib) return 1;
    char buf[1024] = {0};
    char* dir = fl_resolve_feature_path("anki");
#if defined (_WIN32)
    snprintf(buf, sizeof(buf), "%s/sqlite3.dll", dir);
#else
    snprintf(buf, sizeof(buf), "%s/libsqlite3.so", dir);
#endif
    g_sqlite_lib = fl_library_open(buf);
    if (!g_sqlite_lib) return 0;
    p_sqlite3_open_v2 = (int(*)(const char*,sqlite3**,int,const char*))fl_library_symbol(g_sqlite_lib, "sqlite3_open_v2");
    p_sqlite3_errmsg = (const char*(*)(sqlite3*))fl_library_symbol(g_sqlite_lib, "sqlite3_errmsg");
    p_sqlite3_prepare_v2 = (int(*)(sqlite3*,const char*,int,sqlite3_stmt**,const char**))fl_library_symbol(g_sqlite_lib, "sqlite3_prepare_v2");
    p_sqlite3_step = (int(*)(sqlite3_stmt*))fl_library_symbol(g_sqlite_lib, "sqlite3_step");
    p_sqlite3_column_text = (const unsigned char*(*)(sqlite3_stmt*,int))fl_library_symbol(g_sqlite_lib, "sqlite3_column_text");
    p_sqlite3_column_int64 = (long long(*)(sqlite3_stmt*,int))fl_library_symbol(g_sqlite_lib, "sqlite3_column_int64");
    p_sqlite3_finalize = (int(*)(sqlite3_stmt*))fl_library_symbol(g_sqlite_lib, "sqlite3_finalize");
    p_sqlite3_bind_text = (int(*)(sqlite3_stmt*,int,const char*,int,void(*)(void*)))fl_library_symbol(g_sqlite_lib, "sqlite3_bind_text");
    p_sqlite3_close = (int(*)(sqlite3*))fl_library_symbol(g_sqlite_lib, "sqlite3_close");
    return p_sqlite3_open_v2 && p_sqlite3_errmsg && p_sqlite3_prepare_v2 && p_sqlite3_step;
}

static long long g_anki_crt = 0;

static long long anki_due_ts(long long now, int queue, long long due) {
    if (queue == 1 || queue == 3) {
        return due;
    }
    if (queue == 2) {
        long long days_elapsed = (now - g_anki_crt) / 86400;
        long long next_day_at = g_anki_crt + (days_elapsed + 1) * 86400;
        return next_day_at + (due - days_elapsed) * 86400;
    }
    return 0;
}

static int card_due_cmp(const void* a, const void* b) {
    const TYPE_NAME* ca = (const TYPE_NAME*)a;
    const TYPE_NAME* cb = (const TYPE_NAME*)b;
    if (ca->due_ts < cb->due_ts) return -1;
    if (ca->due_ts > cb->due_ts) return 1;
    return 0;
}

static void card_list_push(CardList* list, TYPE_NAME card) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        list->items = realloc(list->items, list->capacity * sizeof(TYPE_NAME));
    }
    list->items[list->count++] = card;
}

static void free_card_list(CardList* list) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].type == CARD_TYPE_BASIC) {
            free(list->items[i].data.basic.front);
            free(list->items[i].data.basic.back);
        } else if (list->items[i].type == CARD_TYPE_CLOZE) {
            free(list->items[i].data.cloze.text);
        }
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static CardList get_cards_in_deck(const char* db_path, const char* target_deck_name) {
    CardList list = {0};
    if (!sqlite_init()) { fprintf(stderr, "sqlite lib not found\n"); return list; }
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    if (p_sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to open DB: %s\n", p_sqlite3_errmsg(db));
        return list;
    }
    {
        sqlite3_stmt* cs = NULL;
        if (p_sqlite3_prepare_v2(db, "SELECT crt FROM col;", -1, &cs, NULL) == SQLITE_OK) {
            if (p_sqlite3_step(cs) == SQLITE_ROW) g_anki_crt = p_sqlite3_column_int64(cs, 0);
            p_sqlite3_finalize(cs);
        }
    }
    char* deck_id = NULL;
    if (p_sqlite3_prepare_v2(db, "SELECT id, name FROM decks;", -1, &stmt, NULL) == SQLITE_OK) {
        while (p_sqlite3_step(stmt) == SQLITE_ROW) {
            long long id = p_sqlite3_column_int64(stmt, 0);
            const char* name = (const char*)p_sqlite3_column_text(stmt, 1);
            if (name && strcmp(name, target_deck_name) == 0) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%lld", id);
                deck_id = safe_strdup(tmp);
                break;
            }
        }
        p_sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (!deck_id) {
        fprintf(stderr, "Deck '%s' not found in collection.\n", target_deck_name);
        p_sqlite3_close(db);
        return list;
    }
    const char* query = "SELECT n.flds, n.mid, c.due, c.queue FROM notes n JOIN cards c ON n.id = c.nid WHERE c.did = ?;";
    if (p_sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        p_sqlite3_bind_text(stmt, 1, deck_id, -1, SQLITE_TRANSIENT);
        while (p_sqlite3_step(stmt) == SQLITE_ROW) {
            const char* flds = (const char*)p_sqlite3_column_text(stmt, 0);
            if (!flds) continue;
            TYPE_NAME card = {0};
            card.due = p_sqlite3_column_int64(stmt, 2);
            card.queue = (int)p_sqlite3_column_int64(stmt, 3);
            card.due_ts = anki_due_ts((long long)time(NULL), card.queue, card.due);
            if (strstr(flds, "{{c") != NULL) {
                card.type = CARD_TYPE_CLOZE;
                char* csep = strchr(flds, '\x1f');
                size_t clen = csep ? (size_t)(csep - flds) : strlen(flds);
                card.data.cloze.text = malloc(clen + 1);
                strncpy(card.data.cloze.text, flds, clen);
                card.data.cloze.text[clen] = '\0';
            } else {
                card.type = CARD_TYPE_BASIC;
                char* sep = strchr(flds, '\x1f');
                if (sep) {
                    size_t front_len = sep - flds;
                    char* front = malloc(front_len + 1);
                    strncpy(front, flds, front_len);
                    front[front_len] = '\0';
                    card.data.basic.front = front;
                    card.data.basic.back = safe_strdup(sep + 1);
                } else {
                    card.data.basic.front = safe_strdup(flds);
                    card.data.basic.back = safe_strdup("");
                }
            }
            card_list_push(&list, card);
        }
        p_sqlite3_finalize(stmt);
        stmt = NULL;
    } else {
        fprintf(stderr, "Failed to prepare query: %s\n", p_sqlite3_errmsg(db));
    }
    qsort(list.items, list.count, sizeof(TYPE_NAME), card_due_cmp);
    free(deck_id);
    p_sqlite3_close(db);
    return list;
}

void display_card_question(TYPE_NAME card) {
    char question[1024];
    question[0] = '\0';
    if (card.type == CARD_TYPE_BASIC) {
        snprintf(question, sizeof(question), "%s", card.data.basic.front);
    } else if (card.type == CARD_TYPE_CLOZE) {
        const char* src = card.data.cloze.text;
        size_t src_len = strlen(src);
        size_t o = 0;

        size_t i = 0;
        while (i < src_len && o + 1 < sizeof(question)) {
            if (src[i] == '{') {
                size_t j = i + 1;
                while (j < src_len && src[j] != '}') j++;

                if (j < src_len) {
                    const char* replacement = "[...]";
                    for (size_t k = 0; replacement[k] && o + 1 < sizeof(question); k++) {
                        question[o++] = replacement[k];
                    }
                    i = j + 2;
                    continue;
                }
            }
            question[o++] = src[i++];
        }
        question[o] = '\0';
    }
    fl_message(.name="question",.message = question,.slot=TOP_CENTER,.timeout=10000.0);
}

void display_card_answer(void) {
    char answer[1024];
    answer[0] = '\0';
    
    fl_mutex_lock(anki_lock);
    TYPE_NAME card = g_current_card;
    fl_mutex_unlock(anki_lock);

    if (card.type == CARD_TYPE_BASIC) {
        snprintf(answer, sizeof(answer), "%s\n---\n%s",
                 card.data.basic.front,
                 card.data.basic.back);
    } else if (card.type == CARD_TYPE_CLOZE) {
        const char* src = card.data.cloze.text;
        size_t src_len = strlen(src);
        size_t o = 0;

        size_t i = 0;
        while (i < src_len && o + 1 < sizeof(answer)) {
            if (src[i] == '{') {
                size_t j = i + 1;
                while (j < src_len && src[j] != '}') j++;

                if (j < src_len) {
                    const char* content_start = src + i + 1;
                    size_t content_len = j - i - 1;

                    const char* sep = NULL;
                    for (size_t k = 0; k + 1 < content_len; k++) {
                        if (content_start[k] == ':' && content_start[k + 1] == ':') {
                            sep = content_start + k;
                            break;
                        }
                    }

                    const char* ans = sep ? sep + 2 : content_start;
                    size_t ans_len = sep
                        ? (size_t)((content_start + content_len) - ans)
                        : content_len;

                    for (size_t k = 0; k < ans_len && o + 1 < sizeof(answer); k++) {
                        answer[o++] = ans[k];
                    }
                    i = j + 2;
                    continue;
                }
            }
            answer[o++] = src[i++];
        }
        answer[o] = '\0';
    }
    
    fl_mutex_lock(anki_lock);
    g_anki_state = ANKI_STATE_ANSWER;
    fl_mutex_unlock(anki_lock);

    fl_dismiss_message("question");
    fl_message(.name="answer",.message = answer,.slot=TOP_CENTER,.timeout=1000.0);
}

void print_due_card(void) {
    fl_dismiss_message("question");
    fl_dismiss_message("answer");
    FlTimer p = fl_profile_start();
    char buf[300];
#if defined (_WIN32)
    snprintf(buf,sizeof(buf),"%s/AppData/Roaming/Anki2/User 1/collection.anki2",fl_resolve_user_path());
#else
    snprintf(buf,sizeof(buf),"%s/.local/share/Anki2/User 1/collection.anki2",fl_resolve_user_path());
#endif
    CardList list = get_cards_in_deck(buf, "Chemistry");
    long long now = (long long)time(NULL);
    size_t* due = malloc(list.count * sizeof(size_t));
    size_t due_count = 0;
    for (size_t i = 0; i < list.count; i++) {
        long long left = list.items[i].due_ts - now;
        if (left <= 0) due[due_count++] = i;
    }
    if (due_count == 0) {
        printf("No due cards.\n");
        free(due);
        free_card_list(&list);
        fl_profile_end(p);
        return;
    }
    int random = fl_rand_num_between(0, (int)(due_count - 1));
    printf("%i \n",random);
    size_t pick = due[random];
    TYPE_NAME card = list.items[pick];
    
    fl_mutex_lock(anki_lock);
    free_card_contents(&g_current_card);
    g_current_card = card_clone(list.items[pick]);
    g_anki_state = ANKI_STATE_QUESTION;
    fl_mutex_unlock(anki_lock);

    display_card_question(card);
    free(due);
    free_card_list(&list);
    fl_profile_end(p);
}

static void* anki_timer_thread(void* arg) {
    srand((unsigned int)(time(NULL) ^ (uintptr_t)&anki_running));
    while (anki_running) {
        fl_sleep(time_between_qs); 
        if (anki_running) {
            print_due_card();
        }
    }
    return NULL;
}

void handle_anki(void) {
    fl_mutex_lock(anki_lock);
    AnkiState current_state = g_anki_state;
    fl_mutex_unlock(anki_lock);

    if (current_state == ANKI_STATE_QUESTION) {
        fl_mutex_lock(anki_lock);
        fl_mutex_unlock(anki_lock);

        display_card_answer();
    }
    else if (current_state == ANKI_STATE_ANSWER) {
        fl_mutex_lock(anki_lock);
        g_anki_state = ANKI_STATE_IDLE;
        fl_mutex_unlock(anki_lock);
        fl_dismiss_message("answer");
    }
}

bool should_swallow_anki(void) {
    fl_mutex_lock(anki_lock);
    bool active = (g_anki_state != ANKI_STATE_IDLE);
    fl_mutex_unlock(anki_lock);
    return active;
}

FLINT_FEATURE_INIT {
    anki_lock = fl_mutex_create();
    anki_running = true;
    anki_thread = fl_thread_create(anki_timer_thread, NULL);
    fl_register_hotkey(.hotkey="Shift+Space", .cb=handle_anki, .should_swallow=should_swallow_anki);
}
