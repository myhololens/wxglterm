#pragma once

#include "plugin_manager.h"
#include "plugin.h"

template<class PluginManagerBase = PluginManager>
class PyPluginManager : public PluginManagerBase {
public:
    using PluginManagerBase::PluginManagerBase;

public:
    void RegisterPlugin(PluginPtr plugin) override {
        PYBIND11_OVERLOAD_PURE_NAME(void,
                                    PluginManagerBase,
                                    "register_plugin",
                                    RegisterPlugin,
                                    plugin);
    }

    void RegisterPlugin(const char * plugin_file_path) override {
        PYBIND11_OVERLOAD_PURE_NAME(void,
                                    PluginManagerBase,
                                    "register_plugin",
                                    RegisterPlugin,
                                    plugin_file_path);
    }

    PluginPtr GetPlugin(const char * plugin_name, uint64_t plugin_version_code) override {
        PYBIND11_OVERLOAD_PURE_NAME(PluginPtr,
                                    PluginManagerBase,
                                    "get_plugin",
                                    GetPlugin,
                                    plugin_name,
                                    plugin_version_code);
    }
};
