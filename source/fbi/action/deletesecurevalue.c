#include "../../core/i18n.h"
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

static void action_delete_secure_value_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    u64 param = ((u64) SECUREVALUE_SLOT_SD << 32) | (info->titleId & 0xFFFFFFF);
    u8 out = 0;
    Result res = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &param, sizeof(param), &out, sizeof(out));

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(info, task_draw_title_info, res, i18n_get(I18N_FAILED_TO_DELETE_SECURE_VALUE));
    } else {
        prompt_display_notify(i18n_get(I18N_SUCCESS), i18n_get(I18N_SECURE_VALUE_DELETED), COLOR_TEXT, info, task_draw_title_info, NULL);
    }
}

static void action_delete_secure_value_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display(i18n_get(I18N_DELETING_SECURE_VALUE), "", false, data, action_delete_secure_value_update, task_draw_title_info);
    }
}

void action_delete_secure_value(linked_list* items, list_item* selected) {
    prompt_display_yes_no(i18n_get(I18N_CONFIRMATION), i18n_get(I18N_DELETE_THE_SECURE_VALUE_OF_THE_SELECTED_TITLE), COLOR_TEXT, selected->data, task_draw_title_info, action_delete_secure_value_onresponse);
}
