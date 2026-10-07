// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dsmod_module_extensions.h"
#define DUO_FONT_FIRST 32u
#define DUO_FONT_COUNT 95u
#define DUO_FONT_HEADER 32u
#define DUO_FONT_RECORD 14u
#define DUO_FONT_SIZE (DUO_FONT_HEADER + DUO_FONT_COUNT * DUO_FONT_RECORD)
EdenDsmodBool duo_font_decode(void *instance, const uint8_t *bytes, size_t size, void *receiver, EdenDsmodFontSink sink);
