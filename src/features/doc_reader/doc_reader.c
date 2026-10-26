#define XML_IMPLEMENTATION
#include "xml.h"
#include "core/flapi.h"

void doc_refresh(const char* filter, ListBoxInfo* lb) {
    // const char* strings[] = {"value1"};
    // fl_populate_listbox(lb, filter,strings, 1);
}

bool doc_enter(const char* selection, ListBoxInfo* lb) {
    return true;
}

void doc_render(PromptState* state) {
  ListBoxInfo *lb = &state->listbox;
  FlListSpec spec = {
      .lb = lb,
  };
  if (!fl_listbox_begin(&spec)) return;
  FlListItemCtx ctx;
  while (fl_listbox_next(&ctx)) {
      fl_draw_text(.x = 14, .y = ctx.y + 4,
                      .text = ctx.text, .align = "LEFT");
  }
  fl_listbox_end();
  fl_place_editbox();
}

void TriggerDocReader(void) {
    fl_window(.title = "📝 FlintDocReader",
              .enter = doc_enter,
              .refresh = doc_refresh,
              .render=doc_render);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+C",.cb=TriggerDocReader);
    fl_register_feature(.name="FlintDocs",.cb=TriggerDocReader,.icon="/list-check.png");
}
