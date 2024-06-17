/*
 * Text Editor example
 * Copyright (C) 2023 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */


#include <cassert>
#include <map>
#include "DistrhoPlugin.hpp"
#include "Patterns.hpp"
#include "Player.hpp"
#include "TimePositionCalc.hpp"
#include "PluginDSP.hpp"

#ifndef DEBUG
// #error "why not debug"
#endif

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------


// --------------------------------------------------------------------------------------------------------------------

    Plugin *createPlugin() {
        return new MySeqPlugin();
    }

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
