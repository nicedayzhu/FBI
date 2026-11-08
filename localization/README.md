# Runtime localization

The runtime language selector is implemented by `source/core/i18n.c` and
`source/core/i18n.h`. Translations are intentionally split by language:

- `source/core/i18n/lang_en.c` and `lang_en.h`
- `source/core/i18n/lang_zh_cn.c` and `lang_zh_cn.h`

Message identifiers are shared in `source/core/i18n_messages.h`. When adding a
message, add one identifier and provide the corresponding designated entry in
both language catalogs. `scripts/check-localization.py` checks catalog parity,
format-placeholder compatibility, UTF-8 validity, and Chinese font coverage.
