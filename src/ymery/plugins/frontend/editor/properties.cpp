// editor-properties widget plugin - properties panel for selected widget
// Reads/writes properties of the selected node in the layout DataTree
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "../../../plugin_manager.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <map>
#include <cstring>

namespace ymery::plugins {

class EditorProperties : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorProperties>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorProperties::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Get optional title from config
        std::string title = "Properties";
        if (auto res = _data_bag->get("title"); res && res->has_value()) {
            if (auto t = get_as<std::string>(*res)) {
                title = *t;
            }
        }

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        // Get selected node path from config
        std::string selected_path_str;
        if (auto res = _data_bag->get("selected"); res && res->has_value()) {
            if (auto p = get_as<std::string>(*res)) {
                selected_path_str = *p;
            }
        }

        if (selected_path_str.empty()) {
            ImGui::TextDisabled("No widget selected");
            return Ok();
        }

        DataPath selected_path = DataPath::parse(selected_path_str);

        // Get metadata for the selected node
        auto meta_res = _data_bag->get_metadata();
        if (!meta_res || meta_res->empty()) {
            ImGui::TextDisabled("No properties available");
            return Ok();
        }

        _render_properties(*meta_res, selected_path);

        return Ok();
    }

private:
    // Buffer for input fields
    std::map<std::string, std::string> _prop_buffers;

    void _render_properties(const Dict& metadata, const DataPath& path) {
        // Get widget type
        std::string widget_type = "unknown";
        if (auto it = metadata.find("widget_type"); it != metadata.end()) {
            if (auto t = get_as<std::string>(it->second)) {
                widget_type = *t;
            }
        }

        ImGui::Text("Widget: %s", widget_type.c_str());
        ImGui::Separator();

        // Always show label first
        _render_property_field(path, "label", metadata);

        ImGui::Separator();
        ImGui::Text("Properties:");

        // Render all other properties
        for (const auto& [key, value] : metadata) {
            if (key == "widget_type" || key == "label") continue;
            _render_property_field(path, key, metadata);
        }

        // Get widget meta for additional properties
        auto widget_meta = _get_widget_meta(widget_type);
        if (!widget_meta.empty()) {
            _render_widget_specific_properties(path, widget_type, widget_meta, metadata);
        }
    }

    void _render_property_field(const DataPath& path, const std::string& key, const Dict& metadata) {
        std::string buffer_key = path.to_string() + "_" + key;

        // Initialize buffer if needed
        if (_prop_buffers.find(buffer_key) == _prop_buffers.end()) {
            auto it = metadata.find(key);
            if (it != metadata.end()) {
                if (auto s = get_as<std::string>(it->second)) {
                    _prop_buffers[buffer_key] = *s;
                } else if (auto i = get_as<int>(it->second)) {
                    _prop_buffers[buffer_key] = std::to_string(*i);
                } else if (auto d = get_as<double>(it->second)) {
                    _prop_buffers[buffer_key] = std::to_string(*d);
                } else if (auto b = get_as<bool>(it->second)) {
                    _prop_buffers[buffer_key] = *b ? "true" : "false";
                } else {
                    _prop_buffers[buffer_key] = "";
                }
            } else {
                _prop_buffers[buffer_key] = "";
            }
        }

        char buf[256];
        strncpy(buf, _prop_buffers[buffer_key].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::Text("%s:", key.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);

        std::string input_id = "##" + key;
        if (ImGui::InputText(input_id.c_str(), buf, sizeof(buf))) {
            _prop_buffers[buffer_key] = buf;
            // Update the value in the data tree
            _data_bag->set(key, Value(std::string(buf)));
        }
    }

    void _render_widget_specific_properties(const DataPath& path,
                                            const std::string& widget_type,
                                            const Dict& widget_meta,
                                            const Dict& current_metadata) {
        // Get properties list from widget meta
        auto props_it = widget_meta.find("properties");
        if (props_it != widget_meta.end()) {
            if (auto props_list = get_as<List>(props_it->second)) {
                ImGui::Separator();
                ImGui::Text("Widget Properties:");

                for (const auto& prop_val : *props_list) {
                    if (auto prop_dict = get_as<Dict>(prop_val)) {
                        std::string prop_name;
                        std::string prop_desc;

                        if (auto name_it = prop_dict->find("name"); name_it != prop_dict->end()) {
                            if (auto n = get_as<std::string>(name_it->second)) {
                                prop_name = *n;
                            }
                        }
                        if (auto desc_it = prop_dict->find("description"); desc_it != prop_dict->end()) {
                            if (auto d = get_as<std::string>(desc_it->second)) {
                                prop_desc = *d;
                            }
                        }

                        if (prop_name.empty() || prop_name == "label") continue;

                        _render_property_input(path, prop_name, prop_desc, current_metadata);
                    }
                }
            }
        }

        // Get events list from widget meta
        auto events_it = widget_meta.find("events");
        if (events_it != widget_meta.end()) {
            if (auto events_list = get_as<List>(events_it->second)) {
                if (!events_list->empty()) {
                    ImGui::Separator();
                    ImGui::Text("Event Handlers:");

                    for (const auto& event_val : *events_list) {
                        if (auto event_name = get_as<std::string>(event_val)) {
                            _render_property_input(path, *event_name, "Event handler", current_metadata);
                        }
                    }
                }
            }
        }
    }

    void _render_property_input(const DataPath& path,
                                const std::string& prop_name,
                                const std::string& description,
                                const Dict& current_metadata) {
        std::string buffer_key = path.to_string() + "_" + prop_name;

        // Initialize buffer if needed
        if (_prop_buffers.find(buffer_key) == _prop_buffers.end()) {
            auto it = current_metadata.find(prop_name);
            if (it != current_metadata.end()) {
                if (auto s = get_as<std::string>(it->second)) {
                    _prop_buffers[buffer_key] = *s;
                } else {
                    _prop_buffers[buffer_key] = "";
                }
            } else {
                _prop_buffers[buffer_key] = "";
            }
        }

        char buf[256];
        strncpy(buf, _prop_buffers[buffer_key].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::Text("  %s:", prop_name.c_str());
        if (!description.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", description.c_str());
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);

        std::string input_id = "##prop_" + prop_name;
        if (ImGui::InputText(input_id.c_str(), buf, sizeof(buf))) {
            _prop_buffers[buffer_key] = buf;
            // Update the value in the data tree
            _data_bag->set(prop_name, Value(std::string(buf)));
        }
    }

    Dict _get_widget_meta(const std::string& widget_type) {
        auto plugin_manager = _widget_factory->plugin_manager();
        if (!plugin_manager) {
            return Dict{};
        }

        // Get metadata from /widget/<type>/meta
        DataPath path = DataPath::parse("/widget/" + widget_type + "/meta");
        auto res = plugin_manager->get_metadata(path);
        if (res) {
            return *res;
        }
        return Dict{};
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-properties"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::EditorProperties::create(wf, d, ns, db)));
}
