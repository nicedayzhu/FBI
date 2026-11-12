TARGET := 3DS

NAME := FBI-cia-bridge
TITLE := FBI CIA Bridge

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source
ROMFS_DIR := romfs

LIBRARY_DIRS += $(DEVKITPRO)/libctru $(DEVKITPRO)/portlibs/armv6k $(DEVKITPRO)/portlibs/3ds
LIBRARIES += curl mbedtls mbedx509 mbedcrypto jansson z citro3d ctru

EXTRA_OUTPUT_FILES := servefiles

# Prefer the project-local ZIP compatibility wrapper. It delegates to the
# native zip utility when available and falls back to PowerShell on Windows.
export PATH := $(CURDIR)/scripts:$(PATH)

BUILD_FLAGS := -Wno-format-truncation

VERSION_MAJOR := 2
VERSION_MINOR := 7
VERSION_MICRO := 6

DESCRIPTION := CIA Bridge discovery and fast pipelined installation.
AUTHOR := Steveice10

PRODUCT_CODE := CTR-P-CFB2
UNIQUE_ID := 0xF8002

ICON_FLAGS := --flags visible,ratingrequired,recordusage --cero 153 --esrb 153 --usk 153 --pegigen 153 --pegiptr 153 --pegibbfc 153 --cob 153 --grb 153 --cgsrr 153

BANNER_AUDIO := meta/audio_3ds.wav
BANNER_IMAGE := meta/banner_3ds.cgfx
ICON := meta/icon_3ds.png
LOGO := meta/logo_3ds.bcma.lz

# INTERNAL #

include buildtools/make_base

# Compiler version macros affect UI text, discovery replies, and the HTTP user
# agent, but a Makefile-only version bump is not visible to generated .d files.
# A version-specific stamp makes every object rebuild once when any component
# changes, preventing a new CIA from retaining stale version constants.
VERSION_STAMP := $(BUILD_DIR)/.version-$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)

$(VERSION_STAMP):
	@mkdir -p $(@D)
	@touch $@

$(OBJECT_FILES): $(VERSION_STAMP)
