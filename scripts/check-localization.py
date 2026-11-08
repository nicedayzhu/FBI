#!/usr/bin/env python3
"""Static catalog, UTF-8, format-string, and BCFNT coverage checks."""

from __future__ import annotations

import hashlib
import ast
import re
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = ROOT / "romfs" / "zh_cn.bcfnt"
SOURCE_ROOT = ROOT / "source"
MAKEFILE_PATH = ROOT / "Makefile"
UPDATE_SOURCE_PATH = SOURCE_ROOT / "fbi" / "update.c"
I18N_HEADER_PATH = SOURCE_ROOT / "core" / "i18n_messages.h"
I18N_SOURCE_PATH = SOURCE_ROOT / "core" / "i18n.c"
EN_CATALOG_PATH = SOURCE_ROOT / "core" / "i18n" / "lang_en.c"
ZH_CN_CATALOG_PATH = SOURCE_ROOT / "core" / "i18n" / "lang_zh_cn.c"
EXPECTED_UPDATE_API = "https://api.github.com/repos/nicedayzhu/FBI/releases/latest"
EXPECTED_PRODUCT_TITLE = "FBI CIA Bridge"

STRING_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')
CATALOG_ENTRY = re.compile(r'\[(I18N_[A-Z0-9_]+)\]\s*=\s*("(?:\\.|[^"\\])*")\s*,')
PRINTF_TOKEN = re.compile(
    r"%(?:\d+\$)?[-+ #0']*(?:\*|\d+)?(?:\.(?:\*|\d+))?"
    r"(?:hh|h|ll|l|j|z|t|L)?[diuoxXfFeEgGaAcspn%]"
)
DIRECT_UI_WORDS = re.compile(
    r"\b(?:Confirmation|Success|Failure|Unknown|Loading|Installing|Deleting|"
    r"Fetching|Waiting|Press|Select|Cancel|Return|Refresh|Options|Enabled|"
    r"Disabled|Overwrite|Continue|Retry|Description|Version|Language)\b"
)


class CheckError(RuntimeError):
    pass


def read_u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise CheckError(f"16-bit read outside font at 0x{offset:X}")
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise CheckError(f"32-bit read outside font at 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0]


def require_span(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise CheckError(f"{label} is outside the font: offset=0x{offset:X}, size={size}")


def require_section(data: bytes, payload_offset: int, signature: bytes) -> int:
    require_span(data, payload_offset - 8, 8, f"{signature.decode()} header")
    if data[payload_offset - 8 : payload_offset - 4] != signature:
        raise CheckError(f"missing {signature.decode()} signature before 0x{payload_offset:X}")
    section_size = read_u32(data, payload_offset - 4)
    if section_size < 8:
        raise CheckError(f"invalid {signature.decode()} section size {section_size}")
    require_span(data, payload_offset - 8, section_size, signature.decode())
    return section_size


def parse_font(data: bytes) -> dict[str, object]:
    require_span(data, 0, 56, "CFNT/FINF header")
    if data[:4] != b"CFNT":
        raise CheckError("font signature is not CFNT")
    if read_u16(data, 4) != 0xFEFF:
        raise CheckError("font is not little-endian BCFNT")
    if read_u16(data, 6) != 20:
        raise CheckError("unexpected CFNT header size")
    if read_u32(data, 12) != len(data):
        raise CheckError("CFNT file-size field does not match the actual file")
    block_count = read_u32(data, 16)
    if block_count < 3:
        raise CheckError("CFNT block count is too small")
    if data[20:24] != b"FINF":
        raise CheckError("missing FINF block")

    line_feed = data[29]
    default_advance = data[34]
    tglp_offset = read_u32(data, 36)
    cwdh_offset = read_u32(data, 40)
    cmap_offset = read_u32(data, 44)

    require_section(data, tglp_offset, b"TGLP")
    require_span(data, tglp_offset, 24, "TGLP payload")
    cell_width = data[tglp_offset]
    cell_height = data[tglp_offset + 1]
    baseline = data[tglp_offset + 2]
    sheet_size = read_u32(data, tglp_offset + 4)
    sheet_count = read_u16(data, tglp_offset + 8)
    rows = read_u16(data, tglp_offset + 12)
    lines = read_u16(data, tglp_offset + 14)
    sheet_width = read_u16(data, tglp_offset + 16)
    sheet_height = read_u16(data, tglp_offset + 18)
    sheet_data = read_u32(data, tglp_offset + 20)

    if not all((cell_width, cell_height, line_feed, sheet_size, sheet_count, rows, lines, sheet_width, sheet_height)):
        raise CheckError("BCFNT has a zero metric or sheet dimension")
    require_span(data, sheet_data, sheet_size * sheet_count, "glyph sheet data")
    if (cell_width + 1) * rows + 1 > sheet_width:
        raise CheckError("glyph cells overflow the sheet width")
    if (cell_height + 1) * lines + 1 > sheet_height:
        raise CheckError("glyph cells overflow the sheet height")
    glyph_capacity = sheet_count * rows * lines

    mapped: dict[int, int] = {}
    visited: set[int] = set()
    offset = cmap_offset
    while offset:
        if offset in visited:
            raise CheckError(f"CMAP chain contains a cycle at 0x{offset:X}")
        visited.add(offset)
        if len(visited) > block_count:
            raise CheckError("CMAP chain exceeds the declared block count")

        require_section(data, offset, b"CMAP")
        require_span(data, offset, 14, "CMAP payload")
        code_begin, code_end, method, _reserved, next_offset = struct.unpack_from("<HHHHI", data, offset)
        if code_end < code_begin:
            raise CheckError(f"CMAP range is reversed at 0x{offset:X}")
        count = code_end - code_begin + 1

        if method == 0:
            first_index = read_u16(data, offset + 12)
            for delta in range(count):
                mapped[code_begin + delta] = first_index + delta
        elif method == 1:
            require_span(data, offset + 12, count * 2, "CMAP table")
            for delta in range(count):
                glyph_index = read_u16(data, offset + 12 + delta * 2)
                if glyph_index != 0xFFFF:
                    mapped[code_begin + delta] = glyph_index
        elif method == 2:
            scan_count = read_u16(data, offset + 12)
            require_span(data, offset + 14, scan_count * 4, "CMAP scan table")
            for index in range(scan_count):
                codepoint, glyph_index = struct.unpack_from("<HH", data, offset + 14 + index * 4)
                mapped[codepoint] = glyph_index
        else:
            raise CheckError(f"unsupported CMAP method {method} at 0x{offset:X}")
        offset = next_offset

    if not mapped:
        raise CheckError("font contains no mapped characters")
    invalid_indices = [(codepoint, index) for codepoint, index in mapped.items() if index >= glyph_capacity]
    if invalid_indices:
        codepoint, index = invalid_indices[0]
        raise CheckError(f"U+{codepoint:04X} maps to out-of-range glyph {index}")

    visited.clear()
    offset = cwdh_offset
    while offset:
        if offset in visited:
            raise CheckError(f"CWDH chain contains a cycle at 0x{offset:X}")
        visited.add(offset)
        if len(visited) > block_count:
            raise CheckError("CWDH chain exceeds the declared block count")
        require_section(data, offset, b"CWDH")
        require_span(data, offset, 8, "CWDH payload")
        start_index, end_index, next_offset = struct.unpack_from("<HHI", data, offset)
        if end_index < start_index:
            raise CheckError(f"CWDH range is reversed at 0x{offset:X}")
        require_span(data, offset + 8, (end_index - start_index + 1) * 3, "CWDH width table")
        offset = next_offset

    return {
        "mapped": mapped,
        "glyph_capacity": glyph_capacity,
        "glyph_count": max(mapped.values()) + 1,
        "cell_width": cell_width,
        "cell_height": cell_height,
        "baseline": baseline,
        "line_feed": line_feed,
        "default_advance": default_advance,
        "sheet_count": sheet_count,
        "sheet_width": sheet_width,
        "sheet_height": sheet_height,
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def source_files() -> list[Path]:
    return sorted(path for path in SOURCE_ROOT.rglob("*") if path.suffix in {".c", ".h"})


def read_sources() -> dict[Path, str]:
    sources: dict[Path, str] = {}
    for path in source_files():
        try:
            sources[path] = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            raise CheckError(f"source is not valid UTF-8: {path.relative_to(ROOT)}") from exc
    return sources


def parse_message_ids() -> list[str]:
    text = I18N_HEADER_PATH.read_text(encoding="utf-8")
    match = re.search(r"typedef\s+enum\s*\{(.*?)\}\s*i18n_message\s*;", text, re.S)
    if match is None:
        raise CheckError("cannot parse i18n_message enum")
    ids = re.findall(r"\bI18N_[A-Z0-9_]+\b", match.group(1))
    return [message_id for message_id in ids if message_id not in {"I18N_NONE", "I18N_MESSAGE_COUNT"}]


def parse_catalog(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    matches = CATALOG_ENTRY.findall(text)
    ids = [message_id for message_id, _ in matches]
    if len(ids) != len(set(ids)):
        duplicate = next(message_id for message_id in ids if ids.count(message_id) > 1)
        raise CheckError(f"duplicate catalog entry in {path.name}: {duplicate}")

    result: dict[str, str] = {}
    for message_id, literal in matches:
        try:
            result[message_id] = ast.literal_eval(literal)
        except (SyntaxError, ValueError) as exc:
            raise CheckError(f"invalid C string for {message_id} in {path.name}") from exc
    return result


def printf_tokens(value: str) -> list[str]:
    return [token for token in PRINTF_TOKEN.findall(value) if token != "%%"]


def check_catalogs(ids: list[str], en: dict[str, str], zh_cn: dict[str, str]) -> None:
    expected = {"I18N_NONE", *ids}
    for name, catalog in (("English", en), ("Simplified Chinese", zh_cn)):
        actual = set(catalog)
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        if missing or extra:
            raise CheckError(f"{name} catalog mismatch: missing={missing[:5]}, extra={extra[:5]}")

    for message_id in ids:
        en_tokens = printf_tokens(en[message_id])
        zh_tokens = printf_tokens(zh_cn[message_id])
        if en_tokens != zh_tokens:
            raise CheckError(
                f"format placeholders differ for {message_id}: English={en_tokens}, zh-CN={zh_tokens}"
            )

    allowed_native_labels = {"I18N_LANGUAGE_SIMPLIFIED_CHINESE"}
    unexpected = [
        message_id
        for message_id, value in en.items()
        if message_id not in allowed_native_labels and any(ord(char) >= 0x80 for char in value)
    ]
    if unexpected:
        raise CheckError(f"English catalog contains unexpected non-ASCII text: {unexpected[:5]}")


def hardcoded_non_ascii_literals(sources: dict[Path, str]) -> list[str]:
    hits: list[str] = []
    catalog_paths = {EN_CATALOG_PATH, ZH_CN_CATALOG_PATH}
    for path, text in sources.items():
        if path in catalog_paths or (SOURCE_ROOT / "libs") in path.parents:
            continue
        for line_number, line in enumerate(text.splitlines(), 1):
            for literal in STRING_LITERAL.findall(line):
                try:
                    value = ast.literal_eval(literal)
                except (SyntaxError, ValueError):
                    continue
                if any(ord(char) >= 0x80 for char in value):
                    hits.append(f"{path.relative_to(ROOT).as_posix()}:{line_number}: {literal}")
    return hits


def hardcoded_english_ui_literals(sources: dict[Path, str]) -> list[str]:
    hits: list[str] = []
    excluded = {EN_CATALOG_PATH, ZH_CN_CATALOG_PATH, SOURCE_ROOT / "core" / "error.c"}
    for path, text in sources.items():
        if path in excluded or (SOURCE_ROOT / "libs") in path.parents:
            continue
        for line_number, line in enumerate(text.splitlines(), 1):
            if "error_panic(" in line:
                continue
            for literal in STRING_LITERAL.findall(line):
                match = DIRECT_UI_WORDS.search(literal)
                if match:
                    hits.append(f"{path.relative_to(ROOT).as_posix()}:{line_number}: {literal}")
    return hits


def required_codepoints(*catalogs: dict[str, str]) -> set[int]:
    return {
        ord(char)
        for catalog in catalogs
        for value in catalog.values()
        for char in value
        if ord(char) >= 0x80
    }


def check_runtime_wiring(sources: dict[Path, str]) -> None:
    i18n_source = sources.get(I18N_SOURCE_PATH, "")
    main_source = sources.get(SOURCE_ROOT / "fbi" / "main.c", "")
    required = (
        '"sdmc:/3ds/FBI-cia-bridge"',
        "CFGU_GetSystemLanguage",
        "CFG_LANGUAGE_ZH",
        "i18n_en_catalog",
        "i18n_zh_cn_catalog",
    )
    missing = [fragment for fragment in required if fragment not in i18n_source]
    if missing:
        raise CheckError(f"runtime language selector is missing: {missing}")
    if main_source.find("i18n_init();") > main_source.find("screen_init();") or "i18n_init();" not in main_source:
        raise CheckError("i18n_init must run before screen_init")


def check_release_updater(sources: dict[Path, str]) -> None:
    update_source = sources.get(UPDATE_SOURCE_PATH)
    if update_source is None:
        raise CheckError(f"missing update source: {UPDATE_SOURCE_PATH.relative_to(ROOT)}")
    if "api.github.com/repos/Steveice10/FBI/releases" in update_source:
        raise CheckError("release updater still references the original FBI repository")

    required_fragments = (
        EXPECTED_UPDATE_API,
        'json_object_get(json, "tag_name")',
        '"FBI-cia-bridge-v%s.%s"',
    )
    missing = [fragment for fragment in required_fragments if fragment not in update_source]
    if missing:
        raise CheckError("release updater is missing the fork API or versioned asset matching")


def check_product_title() -> None:
    makefile = MAKEFILE_PATH.read_text(encoding="utf-8")
    if f"TITLE := {EXPECTED_PRODUCT_TITLE}" not in makefile:
        raise CheckError(f"3DS product title must remain in English: {EXPECTED_PRODUCT_TITLE}")
    if "桥接版" in makefile:
        raise CheckError("3DS product title contains the translated brand name")


def main() -> int:
    try:
        sources = read_sources()
        message_ids = parse_message_ids()
        en_catalog = parse_catalog(EN_CATALOG_PATH)
        zh_cn_catalog = parse_catalog(ZH_CN_CATALOG_PATH)
        check_catalogs(message_ids, en_catalog, zh_cn_catalog)

        hardcoded = hardcoded_non_ascii_literals(sources)
        if hardcoded:
            raise CheckError("non-ASCII UI literals exist outside language catalogs:\n  " + "\n  ".join(hardcoded))
        hardcoded_english = hardcoded_english_ui_literals(sources)
        if hardcoded_english:
            raise CheckError("common English UI literals exist outside language catalogs:\n  " + "\n  ".join(hardcoded_english))

        font_data = FONT_PATH.read_bytes()
        font = parse_font(font_data)
        mapped = font["mapped"]
        assert isinstance(mapped, dict)

        required = required_codepoints(en_catalog, zh_cn_catalog)
        missing = sorted(codepoint for codepoint in required if codepoint not in mapped)
        if missing:
            preview = ", ".join(f"U+{codepoint:04X} ({chr(codepoint)})" for codepoint in missing[:20])
            raise CheckError(f"font is missing {len(missing)} source codepoints: {preview}")

        check_runtime_wiring(sources)
        check_release_updater(sources)
        check_product_title()

        print(f"[OK] UTF-8 source files: {len(sources)}")
        print(f"[OK] Runtime catalogs: {len(message_ids)} messages in English and zh-CN")
        print("[OK] Format placeholders: English and zh-CN catalogs match")
        print("[OK] UI literal isolation: no common English or non-ASCII strings outside catalogs")
        print(f"[OK] BCFNT SHA-256: {font['sha256']}")
        print(
            "[OK] BCFNT glyphs: "
            f"mapped={len(mapped)}, used={font['glyph_count']}, capacity={font['glyph_capacity']}, "
            f"sheets={font['sheet_count']} x {font['sheet_width']}x{font['sheet_height']}"
        )
        print(
            "[OK] BCFNT metrics: "
            f"cell={font['cell_width']}x{font['cell_height']}, baseline={font['baseline']}, "
            f"line_feed={font['line_feed']}, default_advance={font['default_advance']}"
        )
        print(f"[OK] Catalog non-ASCII coverage: {len(required)}/{len(required)} codepoints")
        print("[OK] Runtime selection: Auto/system, English, zh-CN, persistent override")
        print(f"[OK] Release updater: {EXPECTED_UPDATE_API}")
        print(f"[OK] Product title: {EXPECTED_PRODUCT_TITLE}")
        return 0
    except (CheckError, OSError) as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
