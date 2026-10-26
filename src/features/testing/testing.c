#define XML_H_IMPLEMENTATION
#include "xml.h"
#include "core/flapi.h"
#include <stdio.h>

void Testing() {
    FlTimer p = fl_profile_start();
    size_t len = 0;
    char* xml_content = fl_read_file_to_string("docs/xml/index.xml", &len);
    if (!xml_content) {
        printf("ERROR: couldn't read docs/xml/index.xml\n");
        fl_profile_end(p);
        return;
    }
    printf("read %zu bytes\n", len);

    XMLNode* root = xml_parse_string(xml_content);
    if (!root) {
        printf("ERROR: xml_parse_string returned NULL\n");
        free(xml_content);
        fl_profile_end(p);
        return;
    }
    XMLNode* real_root = root;
    if (!real_root->tag && real_root->children && real_root->children->len > 0) {
        real_root = xml_node_child_at(real_root, 0);
    }

    for (size_t i = 0; i < real_root->children->len; i++) {
        XMLNode* node = xml_node_child_at(real_root, i);
        if (!node->tag || strcmp(node->tag, "compound") != 0) continue;

        const char* kind = xml_node_attr(node, "kind");
        const char* refid = xml_node_attr(node, "refid");
        XMLNode* name_node = xml_node_find_tag(node, "name", true);
        if (name_node && name_node->text) {
            printf("%s %s %s\n", name_node->text, kind, refid);
        }
    }

    xml_node_free(root);
    free(xml_content);
    fl_profile_end(p);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+Shift+V",.cb=Testing);
}
