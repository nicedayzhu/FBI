# coding: utf-8 -*-

import locale
import os

try:
    from .lang_en import MESSAGES as EN_MESSAGES
    from .lang_zh_cn import MESSAGES as ZH_CN_MESSAGES
except (ImportError, ValueError):
    from lang_en import MESSAGES as EN_MESSAGES
    from lang_zh_cn import MESSAGES as ZH_CN_MESSAGES


def _language():
    override = os.environ.get('FBI_LANGUAGE', '').lower()
    if override in ('zh', 'zh-cn', 'zh_cn', 'chinese'):
        return 'zh_cn'
    if override in ('en', 'en-us', 'en_us', 'english'):
        return 'en'

    candidates = [
        os.environ.get('LC_ALL', ''),
        os.environ.get('LC_MESSAGES', ''),
        os.environ.get('LANG', ''),
    ]
    try:
        candidates.append(locale.getdefaultlocale()[0] or '')
    except (ValueError, TypeError):
        pass
    return 'zh_cn' if any(value.lower().startswith('zh') for value in candidates) else 'en'


_MESSAGES = ZH_CN_MESSAGES if _language() == 'zh_cn' else EN_MESSAGES


def tr(key, *args):
    value = _MESSAGES.get(key, EN_MESSAGES[key])
    return value % args if args else value
