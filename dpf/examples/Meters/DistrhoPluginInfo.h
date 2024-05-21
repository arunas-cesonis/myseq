/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2024 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "DISTRHO"
#define DISTRHO_PLUGIN_NAME    "Meters"
#define DISTRHO_PLUGIN_URI     "http://distrho.sf.net/examples/Meters"
#define DISTRHO_PLUGIN_CLAP_ID "studio.kx.distrho.examples.meters"

#define DISTRHO_PLUGIN_BRAND_ID  Dstr
#define DISTRHO_PLUGIN_UNIQUE_ID dMtr

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_NUM_INPUTS  2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_STATE  1
#define DISTRHO_UI_FILE_BROWSER    0
#define DISTRHO_UI_USER_RESIZABLE  1
#define DISTRHO_UI_USE_NANOVG      1

#define METER_COLOR_GREEN 0
#define METER_COLOR_BLUE  1

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
