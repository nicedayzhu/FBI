#include "../core/i18n.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "resources.h"
#include "section.h"
#include "action/action.h"
#include "task/uitask.h"
#include "../core/core.h"

#define UPDATE_RELEASE_API "https://api.github.com/repos/nicedayzhu/FBI/releases/latest"
#define UPDATE_ASSET_NAME_MAX 64

static bool update_parse_version(const char* value, unsigned int* major, unsigned int* minor, unsigned int* micro) {
    if(value == NULL || major == NULL || minor == NULL || micro == NULL) {
        return false;
    }

    if(value[0] == 'v' || value[0] == 'V') {
        value++;
    }

    char trailing = '\0';
    return sscanf(value, "%u.%u.%u%c", major, minor, micro, &trailing) == 3;
}

static bool update_is_newer(unsigned int major, unsigned int minor, unsigned int micro) {
    return major > VERSION_MAJOR
        || (major == VERSION_MAJOR && minor > VERSION_MINOR)
        || (major == VERSION_MAJOR && minor == VERSION_MINOR && micro > VERSION_MICRO);
}

static void update_check_update(ui_view* view, void* data, float* progress, char* text) {
    bool hasUpdate = false;
    char updateURL[DOWNLOAD_URL_MAX];

    Result res = 0;

    json_t* json = NULL;
    if(R_SUCCEEDED(res = http_download_json(UPDATE_RELEASE_API, &json, 16 * 1024))) {
        if(json_is_object(json)) {
            json_t* tagName = json_object_get(json, "tag_name");
            json_t* assets = json_object_get(json, "assets");

            if(json_is_string(tagName) && json_is_array(assets)) {
                const char* releaseVersion = json_string_value(tagName);
                unsigned int releaseMajor = 0;
                unsigned int releaseMinor = 0;
                unsigned int releaseMicro = 0;

                if(update_parse_version(releaseVersion, &releaseMajor, &releaseMinor, &releaseMicro)) {
                    if(releaseVersion[0] == 'v' || releaseVersion[0] == 'V') {
                        releaseVersion++;
                    }

                    if(update_is_newer(releaseMajor, releaseMinor, releaseMicro)) {
                        char expectedAssetName[UPDATE_ASSET_NAME_MAX];
                        int expectedAssetNameLength = snprintf(
                            expectedAssetName,
                            sizeof(expectedAssetName),
                            "FBI-cia-bridge-v%s.%s",
                            releaseVersion,
                            fs_get_3dsx_path() != NULL ? "3dsx" : "cia");
                        const char* url = NULL;

                        if(expectedAssetNameLength > 0 && expectedAssetNameLength < (int) sizeof(expectedAssetName)) {
                            for(u32 i = 0; i < json_array_size(assets); i++) {
                                json_t* val = json_array_get(assets, i);
                                if(json_is_object(val)) {
                                    json_t* assetName = json_object_get(val, "name");
                                    json_t* assetUrl = json_object_get(val, "browser_download_url");

                                    if(json_is_string(assetName) && json_is_string(assetUrl)) {
                                        if(strcmp(json_string_value(assetName), expectedAssetName) == 0) {
                                            url = json_string_value(assetUrl);
                                            break;
                                        }
                                    }
                                }
                            }
                        } else {
                            res = R_APP_BAD_DATA;
                        }

                        if(url != NULL) {
                            string_copy(updateURL, url, DOWNLOAD_URL_MAX);
                            hasUpdate = true;
                        } else {
                            res = R_APP_BAD_DATA;
                        }
                    }
                } else {
                    res = R_APP_BAD_DATA;
                }
            } else {
                res = R_APP_BAD_DATA;
            }
        } else {
            res = R_APP_BAD_DATA;
        }

        json_decref(json);
    }

    ui_pop();
    info_destroy(view);

    if(hasUpdate) {
        action_install_url(i18n_get(I18N_UPDATE_FBI_TO_THE_LATEST_VERSION), updateURL, fs_get_3dsx_path(), NULL, NULL, NULL, NULL);
    } else {
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, i18n_get(I18N_FAILED_TO_CHECK_FOR_UPDATE));
        } else {
            prompt_display_notify(i18n_get(I18N_SUCCESS), i18n_get(I18N_NO_UPDATES_AVAILABLE), COLOR_TEXT, NULL, NULL, NULL);
        }
    }
}

void update_open() {
    info_display(i18n_get(I18N_CHECKING_FOR_UPDATES), "", false, NULL, update_check_update, NULL);
}
