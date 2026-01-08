#pragma once
#include "../../../frontend/widget.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <TextEditor.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <set>

namespace ymery::plugins::editor {

class CodePreview : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<CodePreview>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("CodePreview::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        auto res = Widget::init();
        if (!res) return res;
        _editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
        _editor.SetPalette(TextEditor::GetDarkPalette());
        _editor.SetReadOnly(true);
        _editor.SetShowWhitespaces(false);
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();
        std::string yaml = _generate_full_yaml(model);

        if (yaml != _last_yaml) {
            _last_yaml = yaml;
            _editor.SetText(yaml);
        }

        std::string uid = "code_preview";
        if (auto res = _data_bag->get("uid")) {
            if (auto s = get_as<std::string>(*res)) uid = *s;
        }
        _editor.Render(("##" + uid).c_str());
        return Ok();
    }

private:
    TextEditor _editor;
    std::string _last_yaml;
    std::set<std::string> _used_types;

    std::string _generate_full_yaml(SharedLayoutModel& model) {
        std::string result;
        _used_types.clear();

        if (!model.empty()) _collect_types(model.root());

        auto& entries = model.data_entries();
        if (!entries.empty()) {
            result += "data:\n";
            for (const auto& entry : entries) {
                result += _data_entry_to_yaml(entry, 1);
            }
            result += "\n";
        }

        result += "widgets:\n";
        for (const auto& type : _used_types) {
            result += "  " + type + ":\n";
            result += "    type: " + type + "\n";
        }

        if (!model.empty()) {
            result += "\n  # Main widget\n";
            result += "  main-widget:\n";
            result += _widget_to_yaml(model.root(), 2);
        }

        result += "\napp:\n";
        result += "  root-widget: app.main-widget\n";
        if (!entries.empty()) {
            result += "  data-tree: " + entries[0].name + "\n";
        }
        return result;
    }

    void _collect_types(const Value& widget) {
        std::string type = SharedLayoutModel::get_widget_type(widget);
        if (!type.empty()) _used_types.insert(type);
        List body = SharedLayoutModel::get_body(widget);
        for (const auto& child : body) _collect_types(child);
    }

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

    std::string _value_to_string(const Value& value) {
        if (auto s = get_as<std::string>(value)) {
            if (s->find(':') != std::string::npos || s->find('#') != std::string::npos ||
                s->find('\n') != std::string::npos || s->empty()) {
                return "\"" + *s + "\"";
            }
            return *s;
        }
        if (auto i = get_as<int>(value)) return std::to_string(*i);
        if (auto d = get_as<double>(value)) {
            std::ostringstream oss;
            oss << *d;
            return oss.str();
        }
        if (auto b = get_as<bool>(value)) return *b ? "true" : "false";
        return "~";
    }
};

} // namespace ymery::plugins::editor
