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
} delete_system_save_data_data;

static void action_delete_system_save_data_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    task_draw_system_save_data_info(view, ((delete_system_save_data_data*) data)->selected->data, x1, y1, x2, y2);
}

static void action_delete_system_save_data_update(ui_view* view, void* data, float* progress, char* text) {
    delete_system_save_data_data* deleteData = (delete_system_save_data_data*) data;

    system_save_data_info* info = (system_save_data_info*) deleteData->selected->data;

    FS_SystemSaveDataInfo sysInfo = {.mediaType = MEDIATYPE_NAND, .saveId = info->systemSaveDataId};
    Result res = FSUSER_DeleteSystemSaveData(sysInfo);

    ui_pop();
    info_destroy(view);

    if(R_FAILED(res)) {
        error_display_res(info, task_draw_system_save_data_info, res, i18n_get(I18N_FAILED_TO_DELETE_SYSTEM_SAVE_DATA));
    } else {
        linked_list_remove(deleteData->items, deleteData->selected);
        task_free_system_save_data(deleteData->selected);

        prompt_display_notify(i18n_get(I18N_SUCCESS), i18n_get(I18N_SYSTEM_SAVE_DATA_DELETED), COLOR_TEXT, NULL, NULL, NULL);
    }

    free(data);
}

static void action_delete_system_save_data_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display(i18n_get(I18N_DELETING_SYSTEM_SAVE_DATA), "", false, data, action_delete_system_save_data_update, action_delete_system_save_data_draw_top);
    } else {
        free(data);
    }
}

void action_delete_system_save_data(linked_list* items, list_item* selected) {
    delete_system_save_data_data* data = (delete_system_save_data_data*) calloc(1, sizeof(delete_system_save_data_data));
    if(data == NULL) {
        error_display(NULL, NULL, i18n_get(I18N_FAILED_TO_ALLOCATE_DELETE_SYSTEM_SAVE_DATA_DATA));

        return;
    }

    data->items = items;
    data->selected = selected;

    prompt_display_yes_no(i18n_get(I18N_CONFIRMATION), i18n_get(I18N_DELETE_THE_SELECTED_SYSTEM_SAVE_DATA), COLOR_TEXT, data, action_delete_system_save_data_draw_top, action_delete_system_save_data_onresponse);
}
