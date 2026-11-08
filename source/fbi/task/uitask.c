#include "../../core/i18n.h"
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "uitask.h"
#include "../resources.h"
#include "../../core/core.h"

void task_draw_meta_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    meta_info* info = (meta_info*) data;

    u32 metaInfoBoxShadowWidth;
    u32 metaInfoBoxShadowHeight;
    screen_get_texture_size(&metaInfoBoxShadowWidth, &metaInfoBoxShadowHeight, TEXTURE_META_INFO_BOX_SHADOW);

    float metaInfoBoxShadowX = x1 + (x2 - x1 - metaInfoBoxShadowWidth) / 2;
    float metaInfoBoxShadowY = y1 + (y2 - y1) / 4 - metaInfoBoxShadowHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX_SHADOW, metaInfoBoxShadowX, metaInfoBoxShadowY, metaInfoBoxShadowWidth, metaInfoBoxShadowHeight);

    u32 metaInfoBoxWidth;
    u32 metaInfoBoxHeight;
    screen_get_texture_size(&metaInfoBoxWidth, &metaInfoBoxHeight, TEXTURE_META_INFO_BOX);

    float metaInfoBoxX = x1 + (x2 - x1 - metaInfoBoxWidth) / 2;
    float metaInfoBoxY = y1 + (y2 - y1) / 4 - metaInfoBoxHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX, metaInfoBoxX, metaInfoBoxY, metaInfoBoxWidth, metaInfoBoxHeight);

    if(info->texture != 0) {
        u32 iconWidth;
        u32 iconHeight;
        screen_get_texture_size(&iconWidth, &iconHeight, info->texture);

        float iconX = metaInfoBoxX + (64 - iconWidth) / 2;
        float iconY = metaInfoBoxY + (metaInfoBoxHeight - iconHeight) / 2;
        screen_draw_texture(info->texture, iconX, iconY, iconWidth, iconHeight);
    }

    float metaTextX = metaInfoBoxX + 64;

    float shortDescriptionHeight;
    screen_get_string_size_wrap(NULL, &shortDescriptionHeight, info->shortDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float longDescriptionHeight;
    screen_get_string_size_wrap(NULL, &longDescriptionHeight, info->longDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float publisherHeight;
    screen_get_string_size_wrap(NULL, &publisherHeight, info->publisher, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float shortDescriptionY = metaInfoBoxY + (64 - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
    screen_draw_string_wrap(info->shortDescription, metaTextX, shortDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float longDescriptionY = shortDescriptionY + shortDescriptionHeight + 2;
    screen_draw_string_wrap(info->longDescription, metaTextX, longDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float publisherY = longDescriptionY + longDescriptionHeight + 2;
    screen_draw_string_wrap(info->publisher, metaTextX, publisherY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);
}

void task_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ext_save_data_info* info = (ext_save_data_info*) data;

    if(info->hasMeta) {
        task_draw_meta_info(view, &info->meta, x1, y1, x2, y2);
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             i18n_get(I18N_EXT_SAVE_DATA_ID_016LLX_SHARED_S),
             info->extSaveDataId,
             info->shared ? i18n_get(I18N_YES) : i18n_get(I18N_NO));

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    file_info* info = (file_info*) data;

    char infoText[512];
    size_t infoTextPos = 0;

    if(strlen(info->name) > 48) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_NAME_45S), info->name);
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_NAME_48S), info->name);
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_ATTRIBUTES));

    if(info->attributes & (FS_ATTRIBUTE_DIRECTORY | FS_ATTRIBUTE_HIDDEN | FS_ATTRIBUTE_ARCHIVE | FS_ATTRIBUTE_READ_ONLY)) {
        bool needsSeparator = false;

        if(info->attributes & FS_ATTRIBUTE_DIRECTORY) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_DIRECTORY));
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_HIDDEN) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_HIDDEN));
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_ARCHIVE) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_ARCHIVE));
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_READ_ONLY) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_READ_ONLY));
            needsSeparator = true;
        }
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_NONE_2));
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "\n");

    if(!(info->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_SIZE_2F_S),
                                ui_get_display_size(info->size), ui_get_display_size_units(info->size));

        if(info->isCia && info->ciaInfo.loaded) {
            char regionString[64];

            if(info->ciaInfo.hasMeta) {
                task_draw_meta_info(view, &info->ciaInfo.meta, x1, y1, x2, y2);

                smdh_region_to_string(regionString, info->ciaInfo.meta.region, sizeof(regionString));
            } else {
                snprintf(regionString, sizeof(regionString), i18n_get(I18N_UNKNOWN_2));
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos,
                                    i18n_get(I18N_TITLE_ID_VERSION_REGION_INSTALLED_SIZE),
                                    info->ciaInfo.titleId,
                                    info->ciaInfo.version, (info->ciaInfo.version >> 10) & 0x3F, (info->ciaInfo.version >> 4) & 0x3F, info->ciaInfo.version & 0xF,
                                    regionString,
                                    ui_get_display_size(info->ciaInfo.installedSize),
                                    ui_get_display_size_units(info->ciaInfo.installedSize));
        } else if(info->isTicket && info->ticketInfo.loaded) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, i18n_get(I18N_TICKET_ID_016LLX), info->ticketInfo.titleId);
        }
    }

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    pending_title_info* info = (pending_title_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             i18n_get(I18N_PENDING_TITLE_ID_016LLX_MEDIA_TYPE_S_VERSION_HU_D),
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? i18n_get(I18N_SD_2) : i18n_get(I18N_GAME_CARD),
             info->version, (info->version >> 10) & 0x3F, (info->version >> 4) & 0x3F, info->version & 0xF);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    system_save_data_info* info = (system_save_data_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText), i18n_get(I18N_SYSTEM_SAVE_DATA_ID_08LX), info->systemSaveDataId);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ticket_info* info = (ticket_info*) data;

    if(info->loaded) {
        char infoText[512];

        snprintf(infoText, sizeof(infoText), i18n_get(I18N_TITLE_ID_016LLX), info->titleId);

        float infoWidth;
        screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

        float infoX = x1 + (x2 - x1 - infoWidth) / 2;
        float infoY = y1 + (y2 - y1) / 2 - 8;
        screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
    }
}

void task_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    title_info* info = (title_info*) data;

    char regionString[64];

    if(info->hasMeta) {
        task_draw_meta_info(view, &info->meta, x1, y1, x2, y2);

        smdh_region_to_string(regionString, info->meta.region, sizeof(regionString));
    } else {
        snprintf(regionString, sizeof(regionString), i18n_get(I18N_UNKNOWN_2));
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             i18n_get(I18N_TITLE_ID_016LLX_MEDIA_TYPE_S_VERSION_HU_D_D),
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? i18n_get(I18N_SD_2) : i18n_get(I18N_GAME_CARD),
             info->version, (info->version >> 10) & 0x3F, (info->version >> 4) & 0x3F, info->version & 0xF,
             info->productCode,
             regionString,
             ui_get_display_size(info->installedSize), ui_get_display_size_units(info->installedSize));

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}
