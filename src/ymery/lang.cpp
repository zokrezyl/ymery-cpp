#include "lang.hpp"
#include <fstream>
#include <queue>
#include <algorithm>
#include <sstream>

namespace ymery {

// Builtin filesystem browser layout - shown when no layout is specified
static const char* BUILTIN_YAML = R"(
widgets:
  # Recursive filesystem tree display
  fs-recursive:
    type: composite
    body:
      - foreach-child:
          - tree-node:
              body: builtin.fs-recursive

  # Main filesystem browser window
  filesystem-browser:
    type: imgui-main-window
    label: "Ymery - Filesystem Browser"
    window-size: [900, 600]
    body:
      - text:
          content: "Filesystem Browser"
      - separator
      - text:
          content: "Navigate to a layout file (.yaml) to open it"
      - separator
      - child:
          size: [0, 0]
          border: true
          body:
            - collapsing-header:
                label: "Available Locations"
                data-path: /available
                default-open: true
                body:
                  - builtin.fs-recursive:
            - collapsing-header:
                label: "Recent Files"
                data-path: /opened
                body:
                  - builtin.fs-recursive:

app:
  root-widget: builtin.filesystem-browser
  data-tree: filesystem
)";

Result<std::shared_ptr<Lang>> Lang::create(
    const std::vector<std::filesystem::path>& layout_paths,
    const std::string& main_module
) {
    auto lang = std::shared_ptr<Lang>(new Lang());
    lang->_layout_paths = layout_paths;
    lang->_main_module = main_module;

    if (auto res = lang->init(); !res) {
        return Err<std::shared_ptr<Lang>>("Lang::create: init failed", res);
    }

    return lang;
}

const char* Lang::_get_builtin_yaml() {
    return BUILTIN_YAML;
}

Result<void> Lang::init() {
    // Breadth-first loading starting from main module
    std::queue<std::pair<std::string, std::string>> to_load; // (module_name, namespace)

    // Always load builtin first (from embedded string)
    if (auto res = _load_module_from_string(BUILTIN_YAML, "builtin", to_load); !res) {
        // Builtin should always work, but don't fail if it doesn't
    } else {
        _loaded_modules.insert("builtin");
    }

    // Then load main module
    to_load.push({_main_module, _main_module});

    while (!to_load.empty()) {
        auto [module_name, ns] = to_load.front();
        to_load.pop();

        if (_loaded_modules.count(module_name)) {
            continue;
        }

        auto res = _load_module(module_name, ns, to_load);
        if (!res) {
            return Err<void>("Lang::init: failed to load module '" + module_name + "'", res);
        }

        _loaded_modules.insert(module_name);
    }

    return Ok();
}

Result<void> Lang::_load_module(
    const std::string& module_name,
    const std::string& namespace_,
    std::queue<std::pair<std::string, std::string>>& to_load
) {
    auto path_res = _resolve_module_path(module_name);
    if (!path_res) {
        return Err<void>("Lang::_load_module: could not resolve path", path_res);
    }

    auto path = *path_res;

    // Load YAML file
    YAML::Node root;
    try {
        root = YAML::LoadFile(path.string());
    } catch (const YAML::Exception& e) {
        return Err<void>("Lang::_load_module: YAML parse error: " + std::string(e.what()));
    }

    // Process 'import' section - queue imported modules for loading
    if (root["import"]) {
        for (const auto& import_node : root["import"]) {
            std::string import_name = import_node.as<std::string>();
            // Queue with import name as both module name and namespace
            to_load.push({import_name, import_name});
        }
    }

    // Process 'widgets' section
    if (root["widgets"]) {
        for (const auto& kv : root["widgets"]) {
            std::string widget_name = kv.first.as<std::string>();
            std::string full_name = namespace_ + "." + widget_name;
            _widget_definitions[full_name] = _yaml_to_dict(kv.second);
        }
    }

    // Process 'data' section
    if (root["data"]) {
        for (const auto& kv : root["data"]) {
            std::string data_name = kv.first.as<std::string>();
            _data_definitions[data_name] = _yaml_to_dict(kv.second);
        }
    }

    // Process 'app' section
    if (root["app"]) {
        _app_config = _yaml_to_dict(root["app"]);
    }

    return Ok();
}

Result<void> Lang::_load_module_from_string(
    const std::string& yaml_content,
    const std::string& namespace_,
    std::queue<std::pair<std::string, std::string>>& to_load
) {
    // Parse YAML from string
    YAML::Node root;
    try {
        root = YAML::Load(yaml_content);
    } catch (const YAML::Exception& e) {
        return Err<void>("Lang::_load_module_from_string: YAML parse error: " + std::string(e.what()));
    }

    // Process 'import' section - queue imported modules for loading
    if (root["import"]) {
        for (const auto& import_node : root["import"]) {
            std::string import_name = import_node.as<std::string>();
            to_load.push({import_name, import_name});
        }
    }

    // Process 'widgets' section
    if (root["widgets"]) {
        for (const auto& kv : root["widgets"]) {
            std::string widget_name = kv.first.as<std::string>();
            std::string full_name = namespace_ + "." + widget_name;
            _widget_definitions[full_name] = _yaml_to_dict(kv.second);
        }
    }

    // Process 'data' section
    if (root["data"]) {
        for (const auto& kv : root["data"]) {
            std::string data_name = kv.first.as<std::string>();
            _data_definitions[data_name] = _yaml_to_dict(kv.second);
        }
    }

    // Process 'app' section - only if not already set by main module
    if (root["app"] && _app_config.empty()) {
        _app_config = _yaml_to_dict(root["app"]);
    }

    return Ok();
}

Result<std::filesystem::path> Lang::_resolve_module_path(const std::string& module_name) {
    // Convert dots to path separators
    std::string path_str = module_name;
    std::replace(path_str.begin(), path_str.end(), '.', '/');
    path_str += ".yaml";

    for (const auto& base_path : _layout_paths) {
        auto full_path = base_path / path_str;
        std::error_code ec;
        if (std::filesystem::exists(full_path, ec) && !ec) {
            return Ok(full_path);
        }
    }

    return Err<std::filesystem::path>(
        "Lang::_resolve_module_path: module '" + module_name + "' not found");
}

Dict Lang::_yaml_to_dict(const YAML::Node& node) {
    Dict result;
    if (!node.IsMap()) {
        return result;
    }

    for (const auto& kv : node) {
        std::string key = kv.first.as<std::string>();
        result[key] = _yaml_to_value(kv.second);
    }

    return result;
}

Value Lang::_yaml_to_value(const YAML::Node& node) {
    if (node.IsNull()) {
        return Value{};
    }

    if (node.IsScalar()) {
        // Try to determine type
        std::string str = node.as<std::string>();

        // Try bool
        if (str == "true" || str == "True" || str == "TRUE") {
            return Value(true);
        }
        if (str == "false" || str == "False" || str == "FALSE") {
            return Value(false);
        }

        // Try int
        try {
            size_t pos;
            int i = std::stoi(str, &pos);
            if (pos == str.size()) {
                return Value(i);
            }
        } catch (...) {}

        // Try double
        try {
            size_t pos;
            double d = std::stod(str, &pos);
            if (pos == str.size()) {
                return Value(d);
            }
        } catch (...) {}

        // Default to string
        return Value(str);
    }

    if (node.IsSequence()) {
        List list;
        for (const auto& item : node) {
            list.push_back(_yaml_to_value(item));
        }
        return Value(list);
    }

    if (node.IsMap()) {
        return Value(_yaml_to_dict(node));
    }

    return Value{};
}

} // namespace ymery
