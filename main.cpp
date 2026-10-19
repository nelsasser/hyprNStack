
#include "globals.hpp"
#include "nstackLayout.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    using namespace Config::Values;

    // Every option changes tiling, so a runtime change (hyprctl keyword, hl.config) re-lays out windows.
    const auto addString = [](const char* name, const char* desc, const char* def) {
        auto value = makeShared<CStringValue>(name, desc, def, SStringValueOptions{.refresh = Config::Supplementary::REFRESH_LAYOUTS});
        HyprlandAPI::addConfigValueV2(PHANDLE, value);
        return value;
    };
    const auto addInt = [](const char* name, const char* desc, Config::INTEGER def) {
        auto value = makeShared<CIntValue>(name, desc, def, SIntValueOptions{.refresh = Config::Supplementary::REFRESH_LAYOUTS});
        HyprlandAPI::addConfigValueV2(PHANDLE, value);
        return value;
    };
    const auto addFloat = [](const char* name, const char* desc, Config::FLOAT def) {
        auto value = makeShared<CFloatValue>(name, desc, def, SFloatValueOptions{.refresh = Config::Supplementary::REFRESH_LAYOUTS});
        HyprlandAPI::addConfigValueV2(PHANDLE, value);
        return value;
    };

    g_nstackConfig.orientation          = addString("plugin:nstack:layout:orientation", "side of the screen the master area is on", "left");
    g_nstackConfig.new_on_top           = addInt("plugin:nstack:layout:new_on_top", "add new windows to the top of their stack", 0);
    g_nstackConfig.new_is_master        = addInt("plugin:nstack:layout:new_is_master", "new windows replace the master", 1);
    g_nstackConfig.no_gaps_when_only    = addInt("plugin:nstack:layout:no_gaps_when_only", "drop gaps when a workspace has one window", 0);
    g_nstackConfig.special_scale_factor = addFloat("plugin:nstack:layout:special_scale_factor", "scale of windows on special workspaces", 0.8f);
    g_nstackConfig.inherit_fullscreen   = addInt("plugin:nstack:layout:inherit_fullscreen", "a new window inherits the fullscreen state", 1);
    g_nstackConfig.stacks               = addInt("plugin:nstack:layout:stacks", "total number of stacks, including the master", 2);
    g_nstackConfig.center_single_master = addInt("plugin:nstack:layout:center_single_master", "center a lone master window", 0);
    g_nstackConfig.mfact                = addFloat("plugin:nstack:layout:mfact", "master size as a fraction of the screen, 0 for equal stacks", 0.5f);
    g_nstackConfig.single_mfact         = addFloat("plugin:nstack:layout:single_mfact", "size of a centered single master", 0.5f);

    HyprlandAPI::addTiledAlgo(PHANDLE, "nstack",
        &typeid(CNstackAlgorithm),
        []() -> UP<Layout::ITiledAlgorithm> {
            return makeUnique<CNstackAlgorithm>();
        });

    HyprlandAPI::reloadConfig();

    return {"hyprNStack", "N-Stack tiled layout plugin", "nelsasser", "2.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::removeAlgo(PHANDLE, "nstack");
}
