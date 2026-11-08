#include "../../core/i18n.h"
#include <stdio.h>

#include <3ds.h>

#include "action.h"
#include "../resources.h"
#include "../task/uitask.h"
#include "../../core/core.h"

static void action_export_secure_value_update(ui_view* view, void* data, float* progress, char* text) {
    title_info* info = (title_info*) data;

    Result res = 0;

    bool exists = false;
    u64 value = 0;
    if(R_SUCCEEDED(res = FSUSER_GetSaveDataSecureValue(&exists, &value, SECUREVALUE_SLOT_SD, (u32) ((info->titleId >> 8) & 0xFFFFF), (u8) (info->titleId & 0xFF)))) {
        if(!exists) {
            ui_pop();
            info_destroy(view);

            prompt_display_notify(i18n_get(I18N_FAILURE), i18n_get(I18N_SECURE_VALUE_NOT_SET), COLOR_TEXT, info, task_draw_title_info, NULL);

            return;
        }

        FS_Archive sdmcArchive = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))) {
            if(R_SUCCEEDED(res = fs_ensure_dir(sdmcArchive, "/fbi/")) && R_SUCCEEDED(res = fs_ensure_dir(sdmcArchive, "/fbi/securevalue/"))) {
                char pathBuf[64];
                snprintf(pathBuf, 64, "/fbi/securevalue/%016llX.dat", info->titleId);

                FS_Path* fsPath = fs_make_path_utf8(pathBuf);
                if(fsPath != NULL) {
                    Handle fileHandle = 0;
                    if(R_SUCCEEDED(res = FSUSER_OpenFile(&fileHandle, sdmcArchive, *fsPath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {
                        u32 bytesWritten = 0;
                        res = FSFILE_Write(fileHandle, &bytesWritten, 0, &value, sizeof(u64), FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
                        FSFILE_Close(fileHandle);
                    }

                    fs_free_path_utf8(fsPath);
                } else {
                    res = R_APP_OUT_OF_MEMORY;
                }
            }

            FSUSER_CloseArchive(sdmcArchive);
        }
    }

    ui_pop();
    info_destroy(view);

    if(R_SUCCEEDED(res)) {
        prompt_display_notify(i18n_get(I18N_SUCCESS), i18n_get(I18N_SECURE_VALUE_EXPORTED), COLOR_TEXT, info, task_draw_title_info, NULL);
    } else {
        error_display_res(info, task_draw_title_info, res, i18n_get(I18N_FAILED_TO_EXPORT_SECURE_VALUE));
    }
}

static void action_export_secure_value_onresponse(ui_view* view, void* data, u32 response) {
    if(response == PROMPT_YES) {
        info_display(i18n_get(I18N_EXPORTING_SECURE_VALUE), "", false, data, action_export_secure_value_update, task_draw_title_info);
    }
}

void action_export_secure_value(linked_list* items, list_item* selected) {
    prompt_display_yes_no(i18n_get(I18N_CONFIRMATION), i18n_get(I18N_EXPORT_THE_SECURE_VALUE_OF_THE_SELECTED_TITLE), COLOR_TEXT, selected->data, task_draw_title_info, action_export_secure_value_onresponse);
}
