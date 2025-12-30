// editor-code-preview widget plugin - displays YAML output with syntax highlighting
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <TextEditor.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <set>

namespace ymery::plugins {

class EditorCodePreview : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorCodePreview>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorCodePreview::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        auto res = Widget::init();
        if (!res) return res;

        // Initialize text editor
        _editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());  // YAML-like
        _editor.SetPalette(TextEditor::GetDarkPalette());
        _editor.SetReadOnly(true);
        _editor.SetShowWhitespaces(false);

        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        // Generate complete YAML structure
        std::string yaml = _generate_full_yaml(model);

        // Update editor if content changed
        if (yaml != _last_yaml) {
            _last_yaml = yaml;
            _editor.SetText(yaml);
        }

        // Render the editor
        std::string uid = "code_preview";
        if (auto res = _data_bag->get("uid")) {
            if (auto s = get_as<std::string>(*res)) {
                uid = *s;
            }
        }
        _editor.Render(("##" + uid).c_str());

        return Ok();
    }

private:
    TextEditor _editor;
    std::string _last_yaml;
    std::set<std::string> _used_types;

    // Generate complete YAML with data, widgets, and app sections
    std::string _generate_full_yaml(SharedLayoutModel& model) {
        std::string result;
        _used_types.clear();

        // Collect all widget types used in the tree
        if (!model.empty()) {
            _collect_types(model.root());
        }

        // 1. Data section (if any data entries)
        auto& entries = model.data_entries();
        if (!entries.empty()) {
            result += "data:\n";
            for (const auto& entry : entries) {
                result += _data_entry_to_yaml(entry, 1);
            }
            result += "\n";
        }

        // 2. Widgets section
        result += "widgets:\n";

        // First output widget type definitions
        for (const auto& type : _used_types) {
            result += "  " + type + ":\n";
            result += "    type: " + type + "\n";
        }

        // Then output the main widget
        if (!model.empty()) {
            result += "\n  # Main widget\n";
            result += "  main-widget:\n";
            result += _widget_to_yaml(model.root(), 2);
        }

        // 3. App section
        result += "\napp:\n";
        result += "  root-widget: app.main-widget\n";

        // Include data-tree reference if data entries exist
        if (!entries.empty()) {
            // Use first data entry name as data-tree reference
            result += "  data-tree: " + entries[0].name + "\n";
        }

        return result;
    }

    // Collect all widget types used in the tree
    void _collect_types(const Value& widget) {
        std::string type = SharedLayoutModel::get_widget_type(widget);
        if (!type.empty()) {
            _used_types.insert(type);
        }

        // Recursively collect from children
        List body = SharedLayoutModel::get_body(widget);
        for (const auto& child : body) {
            _collect_types(child);
        }
    }

    // Convert DataEntry to YAML
    std::string _data_entry_to_yaml(const DataEntry& entry, int indent) {
        std::string prefix(indent * 2, ' ');
        std::string result;

        result += prefix + entry.name + ":\n";
        result += prefix + "  type: " + entry.type + "\n";

        if (!entry.children.empty()) {
            result += prefix + "  children:\n";
            for (const auto& child : entry.children) {
                result += _data_child_to_yaml(child, indent + 2);
            }
        }

        return result;
    }

    // Convert DataEntry child to YAML
    std::string _data_child_to_yaml(const DataEntry& entry, int indent) {
        std::string prefix(indent * 2, ' ');
        std::string result;

        result += prefix + "- name: " + entry.name + "\n";
        result += prefix + "  type: " + entry.type + "\n";

        if (!entry.children.empty()) {
            result += prefix + "  children:\n";
            for (const auto& child : entry.children) {
                result += _data_child_to_yaml(child, indent + 2);
            }
        }

        return result;
    }

    // Convert widget to YAML (indented under main-widget)
    std::string _widget_to_yaml(const Value& widget, int indent) {
        std::string prefix(indent * 2, ' ');
        std::string result;

        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return result;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);

        result += prefix + "type: " + type + "\n";

        if (props) {
            for (const auto& [key, val] : *props) {
                // Skip uid in output
                if (key == "uid") continue;

                if (key == "body") {
                    auto body = get_as<List>(val);
                    if (body && !body->empty()) {
                        result += prefix + "body:\n";
                        for (const auto& child : *body) {
                            result += _body_item_to_yaml(child, indent + 1);
                        }
                    }
                } else {
                    result += prefix + key + ": " + _value_to_string(val) + "\n";
                }
            }
        }

        return result;
    }

    // Convert body item (child widget) to YAML
    std::string _body_item_to_yaml(const Value& widget, int indent) {
        std::string prefix(indent * 2, ' ');
        std::string result;

        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return result;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);

        result += prefix + "- " + type + ":\n";

        if (props) {
            for (const auto& [key, val] : *props) {
                // Skip uid in output
                if (key == "uid") continue;

                if (key == "body") {
                    auto body = get_as<List>(val);
                    if (body && !body->empty()) {
                        result += prefix + "    body:\n";
                        for (const auto& child : *body) {
                            result += _body_item_to_yaml(child, indent + 2);
                        }
                    }
                } else {
                    result += prefix + "    " + key + ": " + _value_to_string(val) + "\n";
                }
            }
        }

        return result;
    }

    // Convert a simple value to string
    std::string _value_to_string(const Value& value) {
        if (auto s = get_as<std::string>(value)) {
            // Quote if contains special chars
            if (s->find(':') != std::string::npos ||
                s->find('#') != std::string::npos ||
                s->find('\n') != std::string::npos ||
                s->empty()) {
                return "\"" + *s + "\"";
            }
            return *s;
        }
        if (auto i = get_as<int>(value)) {
            return std::to_string(*i);
        }
        if (auto d = get_as<double>(value)) {
            std::ostringstream oss;
            oss << *d;
            return oss.str();
        }
        if (auto b = get_as<bool>(value)) {
            return *b ? "true" : "false";
        }
        return "~";
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "code-preview"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::EditorCodePreview::create(wf, d, ns, db);
}
