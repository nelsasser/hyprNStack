#include "globals.hpp"
#include "nstackLayout.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:orientation", Hyprlang::STRING{"left"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor", Hyprlang::FLOAT{0.8f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:stacks", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:mfact", Hyprlang::FLOAT{0.5f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:nstack:layout:single_mfact", Hyprlang::FLOAT{0.5f});

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
