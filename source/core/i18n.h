#pragma once

#include <stdbool.h>

#include "i18n_messages.h"

typedef enum {
    I18N_MODE_AUTO = 0,
    I18N_MODE_ENGLISH,
    I18N_MODE_ZH_CN,
} i18n_mode;

typedef enum {
    I18N_LANGUAGE_EN = 0,
    I18N_LANGUAGE_ZH_CN,
} i18n_language;

void i18n_init(void);

const char* i18n_get(i18n_message message);
const char* i18n_resolve(const char* text);

i18n_mode i18n_get_mode(void);
i18n_language i18n_get_language(void);
bool i18n_set_mode(i18n_mode mode);
