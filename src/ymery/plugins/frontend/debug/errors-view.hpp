#pragma once
#include "../../../frontend/widget.hpp"
#include "../../../error_buffer.hpp"
#include <imgui.h>

namespace ymery::plugins::debug {

class ErrorsView : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ErrorsView>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ErrorsView::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (_data_bag) {
            if (auto res = _data_bag->get("auto-scroll"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _auto_scroll = *b;
            }
            if (auto res = _data_bag->get("show-timestamp"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _show_timestamp = *b;
            }
            if (auto res = _data_bag->get("show-location"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _show_location = *b;
            }
        }
        return Ok();
    }

    Result<void> render() override {
        auto& buffer = get_thread_error_buffer();

        if (ImGui::Button(("Clear###clear_" + _uid).c_str())) {
            buffer.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(("Auto-scroll###autoscroll_" + _uid).c_str(), &_auto_scroll);
        ImGui::SameLine();
        ImGui::Checkbox(("Timestamps###timestamps_" + _uid).c_str(), &_show_timestamp);
        ImGui::SameLine();
        ImGui::Checkbox(("Locations###locations_" + _uid).c_str(), &_show_location);
        ImGui::SameLine();
        ImGui::Text("Errors: %zu/%zu", buffer.size(), buffer.max_size());

        ImGui::Separator();

        ImGui::BeginChild(("ErrorEntries###errorentries_" + _uid).c_str(), ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        int entry_idx = 0;
        for (const auto& entry : buffer.entries()) {
            ImGui::PushID(entry_idx++);

            ImVec4 color = ErrorBuffer::level_to_color(entry.level);

            std::string label;
            if (_show_timestamp) {
                label = "[" + entry.timestamp + "] ";
            }
            label += entry.error.message();

            ImGui::PushStyleColor(ImGuiCol_Text, color);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
            if (!entry.error.prev_error()) {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }

            if (ImGui::TreeNodeEx(label.c_str(), flags)) {
                if (_show_location) {
                    ImGui::TextDisabled("  @ %s:%u",
                        entry.error.location().file_name(),
                        entry.error.location().line());
                }

                if (entry.error.prev_error()) {
                    _render_error_chain(entry.error.prev_error());
                }

                ImGui::TreePop();
            }

            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        if (_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        return Ok();
    }

private:
    bool _auto_scroll = true;
    bool _show_timestamp = true;
    bool _show_location = true;

    void _render_error_chain(const Error* error) {
        if (!error) return;

        std::string label = "<- " + error->message();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (!error->prev_error()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        if (ImGui::TreeNodeEx(label.c_str(), flags)) {
            if (_show_location) {
                ImGui::TextDisabled("  @ %s:%u",
                    error->location().file_name(),
                    error->location().line());
            }

            if (error->prev_error()) {
                _render_error_chain(error->prev_error());
            }

            ImGui::TreePop();
        }
    }
};

} // namespace ymery::plugins::debug
