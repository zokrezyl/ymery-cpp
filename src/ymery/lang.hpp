#pragma once

#include "result.hpp"
#include "types.hpp"
#include <yaml-cpp/yaml.h>
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <filesystem>

namespace ymery {

// Lang - YAML loader and module resolver
class Lang {
public:
    // Factory method
    static Result<std::shared_ptr<Lang>> create(
        const std::vector<std::filesystem::path>& layout_paths,
        const std::string& main_module = "app"
    );

    // Access loaded definitions
    const std::map<std::string, Dict>& widget_definitions() const { return _widget_definitions; }
    const std::map<std::string, Dict>& data_definitions() const { return _data_definitions; }
    const Dict& app_config() const { return _app_config; }

    // Lifecycle
    Result<void> init();

private:
    Lang() = default;

    // Module loading
    Result<void> _load_module(
        const std::string& module_name,
        const std::string& namespace_,
        std::queue<std::pair<std::string, std::string>>& to_load
    );
    Result<void> _load_module_from_string(
        const std::string& yaml_content,
        const std::string& namespace_,
        std::queue<std::pair<std::string, std::string>>& to_load
    );
    Result<std::filesystem::path> _resolve_module_path(const std::string& module_name);

    // Builtin layouts
    static const char* _get_builtin_yaml();

    // YAML to Dict conversion
    static Dict _yaml_to_dict(const YAML::Node& node);
    static Value _yaml_to_value(const YAML::Node& node);

    std::vector<std::filesystem::path> _layout_paths;
    std::string _main_module;

    std::map<std::string, Dict> _widget_definitions;
    std::map<std::string, Dict> _data_definitions;
    Dict _app_config;

    std::set<std::string> _loaded_modules;
};

using LangPtr = std::shared_ptr<Lang>;

} // namespace ymery
