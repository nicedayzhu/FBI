#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <3ds.h>

#include "i18n.h"
#include "i18n/lang_en.h"
#include "i18n/lang_zh_cn.h"

#define I18N_CONFIG_DIRECTORY "sdmc:/3ds/FBI-cia-bridge"
#define I18N_CONFIG_PATH I18N_CONFIG_DIRECTORY "/language.cfg"
#define I18N_CONFIG_TEMP_PATH I18N_CONFIG_DIRECTORY "/language.tmp"

static i18n_mode current_mode = I18N_MODE_AUTO;
static i18n_language current_language = I18N_LANGUAGE_EN;

static i18n_language i18n_detect_system_language(void) {
    u8 language = CFG_LANGUAGE_EN;
    if(R_SUCCEEDED(CFGU_GetSystemLanguage(&language)) && language == CFG_LANGUAGE_ZH) {
        return I18N_LANGUAGE_ZH_CN;
    }

    return I18N_LANGUAGE_EN;
}

static void i18n_apply_mode(void) {
    if(current_mode == I18N_MODE_ZH_CN) {
        current_language = I18N_LANGUAGE_ZH_CN;
    } else if(current_mode == I18N_MODE_ENGLISH) {
        current_language = I18N_LANGUAGE_EN;
    } else {
        current_language = i18n_detect_system_language();
    }
}

static i18n_mode i18n_read_mode(void) {
    FILE* file = fopen(I18N_CONFIG_PATH, "rb");
    if(file == NULL) {
        return I18N_MODE_AUTO;
    }

    char value[16] = {0};
    fgets(value, sizeof(value), file);
    fclose(file);

    if(strncmp(value, "en", 2) == 0) {
        return I18N_MODE_ENGLISH;
    }
    if(strncmp(value, "zh-CN", 5) == 0) {
        return I18N_MODE_ZH_CN;
    }

    return I18N_MODE_AUTO;
}

static bool i18n_write_mode(i18n_mode mode) {
    mkdir("sdmc:/3ds", 0777);
    if(mkdir(I18N_CONFIG_DIRECTORY, 0777) != 0 && errno != EEXIST) {
        return false;
    }

    const char* value = mode == I18N_MODE_ENGLISH ? "en\n" : mode == I18N_MODE_ZH_CN ? "zh-CN\n" : "auto\n";
    FILE* file = fopen(I18N_CONFIG_TEMP_PATH, "wb");
    if(file == NULL) {
        return false;
    }

    bool success = fputs(value, file) >= 0 && fflush(file) == 0;
    success = fclose(file) == 0 && success;
    if(!success) {
        remove(I18N_CONFIG_TEMP_PATH);
        return false;
    }

    remove(I18N_CONFIG_PATH);
    if(rename(I18N_CONFIG_TEMP_PATH, I18N_CONFIG_PATH) != 0) {
        remove(I18N_CONFIG_TEMP_PATH);
        return false;
    }

    return true;
}

void i18n_init(void) {
    current_mode = i18n_read_mode();
    i18n_apply_mode();
}

const char* i18n_get(i18n_message message) {
    if(message <= I18N_NONE || message >= I18N_MESSAGE_COUNT) {
        return "";
    }

    return current_language == I18N_LANGUAGE_ZH_CN ? i18n_zh_cn_catalog[message] : i18n_en_catalog[message];
}

const char* i18n_resolve(const char* text) {
    if(text == NULL) {
        return NULL;
    }

    for(i18n_message message = I18N_NONE + 1; message < I18N_MESSAGE_COUNT; message++) {
        if(text == i18n_en_catalog[message] || text == i18n_zh_cn_catalog[message]) {
            return i18n_get(message);
        }
    }

    return text;
}

i18n_mode i18n_get_mode(void) {
    return current_mode;
}

i18n_language i18n_get_language(void) {
    return current_language;
}

bool i18n_set_mode(i18n_mode mode) {
    if(mode < I18N_MODE_AUTO || mode > I18N_MODE_ZH_CN) {
        return false;
    }

    current_mode = mode;
    i18n_apply_mode();
    return i18n_write_mode(mode);
}
