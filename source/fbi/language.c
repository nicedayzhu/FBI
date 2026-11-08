#include <3ds.h>

#include "resources.h"
#include "section.h"
#include "../core/core.h"

static i18n_mode auto_mode = I18N_MODE_AUTO;
static i18n_mode english_mode = I18N_MODE_ENGLISH;
static i18n_mode zh_cn_mode = I18N_MODE_ZH_CN;

static list_item auto_item = LIST_ITEM_I18N(I18N_LANGUAGE_AUTO, COLOR_TEXT, &auto_mode);
static list_item english_item = LIST_ITEM_I18N(I18N_LANGUAGE_ENGLISH, COLOR_TEXT, &english_mode);
static list_item zh_cn_item = LIST_ITEM_I18N(I18N_LANGUAGE_SIMPLIFIED_CHINESE, COLOR_TEXT, &zh_cn_mode);

static void language_refresh_colors(void) {
    i18n_mode mode = i18n_get_mode();
    auto_item.color = mode == I18N_MODE_AUTO ? COLOR_ENABLED : COLOR_TEXT;
    english_item.color = mode == I18N_MODE_ENGLISH ? COLOR_ENABLED : COLOR_TEXT;
    zh_cn_item.color = mode == I18N_MODE_ZH_CN ? COLOR_ENABLED : COLOR_TEXT;
}

static void language_update(ui_view* view, void* data, linked_list* items, list_item* selected, bool selectedTouched) {
    if(hidKeysDown() & KEY_B) {
        ui_pop();
        list_destroy(view);
        return;
    }

    if(selected != NULL && selected->data != NULL && (selectedTouched || (hidKeysDown() & KEY_A))) {
        if(!i18n_set_mode(*(i18n_mode*) selected->data)) {
            error_display(NULL, NULL, i18n_get(I18N_FAILED_TO_SAVE_LANGUAGE_SETTING));
        }
        language_refresh_colors();
    }

    if(linked_list_size(items) == 0) {
        language_refresh_colors();
        linked_list_add(items, &auto_item);
        linked_list_add(items, &english_item);
        linked_list_add(items, &zh_cn_item);
    }
}

void language_open() {
    list_display(i18n_get(I18N_LANGUAGE), i18n_get(I18N_A_SELECT_B_RETURN), NULL, language_update, NULL);
}
