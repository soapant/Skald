#define XML_H_IMPLEMENTATION
#include "xml.h"
#include "core/flapi.h"
#include <stdio.h>

typedef enum {
    DOC_TYPE_FUNC,
    DOC_TYPE_STRUCT,
    DOC_TYPE_UNKNOWN,
} DocType;

typedef struct {
    DocType type;
    char* name;
    char* refid;    
    char* file_refid;
} DocItem;

static DocItem* g_doc_items = NULL;
static int g_doc_item_count = 0;

static XMLNode* xml_root(XMLNode* parsed) {
    if (!parsed) return NULL;
    if (parsed->tag) return parsed;
    if (parsed->children && parsed->children->len > 0)
        return xml_node_child_at(parsed, 0);
    return parsed;
}

void fill_doc_items(void) {
    if (g_doc_items) return;

    size_t len = 0;
    char* content = fl_read_file_to_string("docs/xml/index.xml", &len);
    if (!content) return;

    XMLNode* parsed = xml_parse_string(content);
    XMLNode* root = xml_root(parsed);
    if (!root || !root->children) {
        xml_node_free(parsed);
        free(content);
        return;
    }

    int cap = 0;
    for (size_t i = 0; i < root->children->len; i++) {
        XMLNode* compound = xml_node_child_at(root, i);
        if (!compound->tag || strcmp(compound->tag, "compound") != 0) continue;

        const char* kind  = xml_node_attr(compound, "kind");
        const char* refid = xml_node_attr(compound, "refid");
        if (!kind || !refid) continue;

        if (strcmp(kind, "file") == 0) {
            char path[512];
            snprintf(path, sizeof(path), "docs/xml/%s.xml", refid);

            size_t flen = 0;
            char* fcontent = fl_read_file_to_string(path, &flen);
            if (!fcontent) continue;

            XMLNode* fparsed = xml_parse_string(fcontent);
            XMLNode* froot = xml_root(fparsed);
            XMLNode* compounddef = froot ? xml_node_find_tag(froot, "compounddef", true) : NULL;

            if (compounddef && compounddef->children) {
                for (size_t s = 0; s < compounddef->children->len; s++) {
                    XMLNode* section = xml_node_child_at(compounddef, s);
                    if (!section->tag || strcmp(section->tag, "sectiondef") != 0) continue;

                    const char* skind = xml_node_attr(section, "kind");
                    if (!skind || strcmp(skind, "func") != 0) continue;

                    for (size_t m = 0; m < section->children->len; m++) {
                        XMLNode* member = xml_node_child_at(section, m);
                        if (!member->tag || strcmp(member->tag, "memberdef") != 0) continue;
                        const char* member_id = xml_node_attr(member, "id");    
                        XMLNode* name_node = xml_node_find_tag(member, "name", true);
                        if (!name_node || !name_node->text) continue;

                        if (g_doc_item_count >= cap) {
                            cap = cap ? cap * 2 : 64;
                            g_doc_items = realloc(g_doc_items, cap * sizeof(DocItem));
                        }
                        g_doc_items[g_doc_item_count].type = DOC_TYPE_FUNC;
                        g_doc_items[g_doc_item_count].name = strdup(name_node->text);
                        g_doc_items[g_doc_item_count].refid = strdup(member_id);          
                        g_doc_items[g_doc_item_count].file_refid = strdup(refid);  
                        g_doc_item_count++;
                    }
                }
            }

            xml_node_free(fparsed);
            free(fcontent);
        }
        else if (strcmp(kind, "struct") == 0) {
            XMLNode* name_node = xml_node_find_tag(compound, "name", true);
            if (!name_node || !name_node->text) continue;

            if (g_doc_item_count >= cap) {
                cap = cap ? cap * 2 : 64;
                g_doc_items = realloc(g_doc_items, cap * sizeof(DocItem));
            }
            g_doc_items[g_doc_item_count].type = DOC_TYPE_STRUCT;
            g_doc_items[g_doc_item_count].name = strdup(name_node->text);
            g_doc_item_count++;
        }
    }

    xml_node_free(parsed);
    free(content);
}

DocItem doc_item(const char* name) {
    for (int i = 0; i < g_doc_item_count; i++) {
        if (strcmp(g_doc_items[i].name, name) == 0) return g_doc_items[i];
    }
    return (DocItem){0};
}

void doc_refresh(const char* filter, ListBoxInfo* lb) {
    const char** names = malloc(g_doc_item_count * sizeof(const char*));
    for (int i = 0; i < g_doc_item_count; i++) {
        if (fl_string_contains("_impl",g_doc_items[i].name)) {
            g_doc_items[i].name[strlen(g_doc_items[i].name)-5]='\0';
            names[i] = g_doc_items[i].name;
        } else {
          names[i] = g_doc_items[i].name;
        }
    }
    fl_populate_listbox(lb, filter, names, g_doc_item_count);
}

static void collect_text(XMLNode* node, char* out, size_t cap, size_t* used) {
    if (!node) return;
    if (node->text) {
        size_t n = strlen(node->text);
        if (*used + n + 1 < cap) {
            memcpy(out + *used, node->text, n);
            *used += n;
            out[*used] = '\0';
        }
    }
    if (node->children) {
        for (size_t i = 0; i < node->children->len; i++) {
            collect_text(xml_node_child_at(node, i), out, cap, used);
        }
    }
}

static void get_doc_details(const char* name, char* out, size_t cap) {
    DocItem item = doc_item(name);

    char path[512];
    snprintf(path, sizeof(path), "docs/xml/%s.xml", item.file_refid);

    size_t len = 0;
    char* content = fl_read_file_to_string(path, &len);
    if (!content) return;

    XMLNode* parsed = xml_parse_string(content);
    XMLNode* root = xml_root(parsed);

    XMLNode* compounddef = root ? xml_node_find_tag(root, "compounddef", true) : NULL;
    if (compounddef && compounddef->children) {
        for (size_t s = 0; s < compounddef->children->len; s++) {
            XMLNode* section = xml_node_child_at(compounddef, s);
            if (!section->tag || strcmp(section->tag, "sectiondef") != 0) continue;

            for (size_t m = 0; m < section->children->len; m++) {
                XMLNode* member = xml_node_child_at(section, m);
                if (!member->tag || strcmp(member->tag, "memberdef") != 0) continue;

                const char* id = xml_node_attr(member, "id");
                if (!id || strcmp(id, item.refid) != 0) continue;

                XMLNode* type_node = xml_node_find_tag(member, "type", true);
                XMLNode* brief = xml_node_find_tag(member, "briefdescription", true);
                XMLNode* detail = xml_node_find_tag(member, "detaileddescription", true);

                size_t used = 0;
                out[0] = '\0';

                if (type_node && type_node->text) {
                    snprintf(out + used, cap - used, "%s ", type_node->text);
                    used = strlen(out);
                }
                snprintf(out + used, cap - used, "%s\n\n", item.name);
                used = strlen(out);

                if (brief) collect_text(brief, out, cap, &used);
                if (detail) {
                    snprintf(out + used, cap - used, "\n\n");
                    used = strlen(out);
                    collect_text(detail, out, cap, &used);
                }

                xml_node_free(parsed);
                free(content);
                return;
            }
        }
    }

    xml_node_free(parsed);
    free(content);
}

bool doc_enter(const char* selection, ListBoxInfo* lb) {
    char details[1024];
    get_doc_details(selection, details, sizeof(details));
    printf("%s\n", details);
    return true;
}

void doc_render(PromptState* state) {
  ListBoxInfo *lb = &state->listbox;
  FlListSpec spec = {
      .lb = lb,
  };
  if (!fl_listbox_begin(&spec)) return;
  fl_place_editbox();
  FlListItemCtx ctx;
  while (fl_listbox_next(&ctx)) {
      DocItem item = doc_item(ctx.text);
      char* current_id;
      switch(item.type) {
          case DOC_TYPE_FUNC: current_id = "Function"; break;
          case DOC_TYPE_STRUCT: current_id = "Struct"; break;
          default: current_id = "Unknown"; break;
      }
      fl_draw_text(.x = 14, .y = ctx.y,
                      .text = ctx.text, .align = "LEFT");
      if (current_id) {
          fl_draw_text(.y = ctx.y, .text = current_id, .align = "RIGHT",.pad=14,.color=FL_GRAY);
      }
  }
  fl_listbox_end();
}

void TriggerDocReader(void) {
    fl_window(.title = "📝 FlintDocReader",
              .enter = doc_enter,
              .refresh = doc_refresh,
              .render=doc_render);
}

FLINT_FEATURE_INIT {
    fill_doc_items();
    fl_register_hotkey(.hotkey="Alt+C",.cb=TriggerDocReader);
    fl_register_feature(.name="FlintDocs",.cb=TriggerDocReader,.icon="/list-check.png");
}
