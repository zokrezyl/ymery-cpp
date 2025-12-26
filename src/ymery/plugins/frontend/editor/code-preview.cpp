// editor-code-preview widget plugin - displays YAML output with syntax highlighting
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <TextEditor.h>
#include <spdlog/spdlog.h>
#include <sstream>

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

        // Convert model to YAML string
        std::string yaml = _to_yaml(model.root(), 0);

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

    // Convert Value to YAML string
    std::string _to_yaml(const Value& value, int indent) {
        std::string prefix(indent * 2, ' ');

        // Empty value
        if (!value.has_value()) {
            return prefix + "~\n";
        }

        // String
        if (auto s = get_as<std::string>(value)) {
            // Check if needs quoting
            if (s->find(':') != std::string::npos ||
                s->find('#') != std::string::npos ||
                s->find('\n') != std::string::npos ||
                s->empty()) {
                return "\"" + *s + "\"";
            }
            return *s;
        }

        // Int
        if (auto i = get_as<int>(value)) {
            return std::to_string(*i);
        }

        // Double
        if (auto d = get_as<double>(value)) {
            std::ostringstream oss;
            oss << *d;
            return oss.str();
        }

        // Bool
        if (auto b = get_as<bool>(value)) {
            return *b ? "true" : "false";
        }

        // List
        if (auto list = get_as<List>(value)) {
            if (list->empty()) {
                return "[]\n";
            }
            std::string result;
            for (const auto& item : *list) {
                result += prefix + "- ";
                // Check if item is a dict (widget)
                if (auto dict = get_as<Dict>(item)) {
                    if (!dict->empty()) {
                        auto& [key, val] = *dict->begin();
                        result += key + ":\n";
                        result += _dict_to_yaml(val, indent + 2);
                    }
                } else {
                    result += _to_yaml(item, 0) + "\n";
                }
            }
            return result;
        }

        // Dict (widget format: {type: {props}})
        if (auto dict = get_as<Dict>(value)) {
            if (dict->empty()) {
                return "{}\n";
            }
            std::string result;
            for (const auto& [key, val] : *dict) {
                result += prefix + key + ":";
                if (auto nested_dict = get_as<Dict>(val)) {
                    result += "\n" + _dict_to_yaml(val, indent + 1);
                } else if (auto nested_list = get_as<List>(val)) {
                    result += "\n" + _to_yaml(val, indent + 1);
                } else {
                    result += " " + _to_yaml(val, 0) + "\n";
                }
            }
            return result;
        }

        return "# unknown type\n";
    }

    std::string _dict_to_yaml(const Value& value, int indent) {
        auto dict = get_as<Dict>(value);
        if (!dict) return "";

        std::string prefix(indent * 2, ' ');
        std::string result;

        for (const auto& [key, val] : *dict) {
            // Skip uid in output for cleaner display
            if (key == "uid") continue;

            result += prefix + key + ":";
            if (auto nested_dict = get_as<Dict>(val)) {
                result += "\n" + _dict_to_yaml(val, indent + 1);
            } else if (auto nested_list = get_as<List>(val)) {
                result += "\n" + _to_yaml(val, indent + 1);
            } else {
                result += " " + _to_yaml(val, 0) + "\n";
            }
        }

        return result;
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-code-preview"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::EditorCodePreview::create(wf, d, ns, db);
}
