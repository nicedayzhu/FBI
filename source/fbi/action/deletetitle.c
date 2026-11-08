#include "../../core/i18n.h"
#include <malloc.h>

#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

typedef struct {
    linked_list* items;
    list_item* selected;
    bool ticket;
} delete_title_data;

static void action_delete_title_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    task_draw_title_info(view, ((delete_title_data*) data)->selected->data, x1, y1, x2, y2);
}

static void action_delete_title_update(ui_view* view, void* data, float* progress, char* text) {
    delete_title_data* deleteData = (delete_title_data*) data;

    title_info* info = (title_info*) deleteData->selected->data;

    Result res = 0;

    if(R_SUCCEEDED(res = AM_DeleteTitle(info->mediaType, info->titleId)) && deleteData->ticket) {
        res = AM_DeleteTicket(info->titleId);
    }

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(info, task_draw_title_info, res, i18n_get(I18N_FAILED_TO_DELETE_TITLE));
    } else {
        linked_list_remove(deleteData->items, deleteData->selected);
        task_free_title(deleteData->selected);

        prompt_display_notify(i18n_get(I18N_SUCCESS), i18n_get(I18N_TITLE_DELETED), COLOR_TEXT, NULL, NULL, NULL);
    }

    free(data);
}

static void action_delete_title_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display(i18n_get(I18N_DELETING_TITLE), "", false, data, action_delete_title_update, action_delete_title_draw_top);
    } else {
        free(data);
    }
}

static void action_delete_title_internal(linked_list* items, list_item* selected, const char* message, bool ticket) {
    delete_title_data* data = (delete_title_data*) calloc(1, sizeof(delete_title_data));
    if(data == NULL) {
        error_display(NULL, NULL, i18n_get(I18N_FAILED_TO_ALLOCATE_DELETE_TITLE_DATA));

        return;
    }

    data->items = items;
    data->selected = selected;
    data->ticket = ticket;

    prompt_display_yes_no(i18n_get(I18N_CONFIRMATION), message, COLOR_TEXT, data, action_delete_title_draw_top, action_delete_title_onresponse);
}

void action_delete_title(linked_list* items, list_item* selected) {
    action_delete_title_internal(items, selected, i18n_get(I18N_DELETE_THE_SELECTED_TITLE), false);
}

void action_delete_title_ticket(linked_list* items, list_item* selected) {
    action_delete_title_internal(items, selected, i18n_get(I18N_DELETE_THE_SELECTED_TITLE_AND_TICKET), true);
}
