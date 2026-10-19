#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

inline HANDLE PHANDLE = nullptr;

// Registered through addConfigValueV2, which resolves under both hyprlang and Lua
// configs. The deprecated getConfigValue returns nullptr once Hyprland runs a Lua config.
struct SNstackConfig {
    SP<Config::Values::CStringValue> orientation;
    SP<Config::Values::CIntValue>    new_on_top;
    SP<Config::Values::CIntValue>    new_is_master;
    SP<Config::Values::CIntValue>    no_gaps_when_only;
    SP<Config::Values::CFloatValue>  special_scale_factor;
    SP<Config::Values::CIntValue>    inherit_fullscreen;
    SP<Config::Values::CIntValue>    stacks;
    SP<Config::Values::CIntValue>    center_single_master;
    SP<Config::Values::CFloatValue>  mfact;
    SP<Config::Values::CFloatValue>  single_mfact;
};

inline SNstackConfig g_nstackConfig;
