// logs-view widget plugin - displays spdlog messages from LogBuffer
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../log_buffer.hpp"
#include <imgui.h>

namespace ymery::plugins {

class LogsView : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<LogsView>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("LogsView::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        // Get optional settings from data_bag
        if (_data_bag) {
            if (auto res = _data_bag->get("auto-scroll"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _auto_scroll = *b;
            }
            if (auto res = _data_bag->get("show-timestamp"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _show_timestamp = *b;
            }
            if (auto res = _data_bag->get("show-level"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _show_level = *b;
            }
            if (auto res = _data_bag->get("show-source"); res && res->has_value()) {
                if (auto b = get_as<bool>(*res)) _show_source = *b;
            }
        }
        return Ok();
    }

    Result<void> render() override {
        auto& buffer = get_log_buffer();

        // Toolbar
        if (ImGui::Button(("Clear###clear_" + _uid).c_str())) {
            buffer.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(("Auto-scroll###autoscroll_" + _uid).c_str(), &_auto_scroll);
        ImGui::SameLine();
        ImGui::Checkbox(("Time###time_" + _uid).c_str(), &_show_timestamp);
        ImGui::SameLine();
        ImGui::Checkbox(("Level###level_" + _uid).c_str(), &_show_level);
        ImGui::SameLine();
        ImGui::Checkbox(("Source###source_" + _uid).c_str(), &_show_source);
        ImGui::SameLine();
        ImGui::Text("Logs: %zu/%zu", buffer.size(), buffer.max_size());

        // Level filter
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        const char* level_names[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL" };
        ImGui::Combo(("###minlevel_" + _uid).c_str(), &_min_level, level_names, IM_ARRAYSIZE(level_names));

        ImGui::Separator();

        // Log entries in scrollable region
        ImGui::BeginChild(("LogEntries###logentries_" + _uid).c_str(), ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        // Get a copy of entries (thread-safe)
        auto entries = buffer.entries();

        for (const auto& entry : entries) {
            // Filter by level
            if (static_cast<int>(entry.level) < _min_level) {
                continue;
            }

            // Color based on level
            ImVec4 color = LogBuffer::level_to_color(entry.level);

            // Build log line
            std::string line;
            if (_show_timestamp) {
                line += "[" + entry.timestamp + "] ";
            }
            if (_show_level) {
                line += "[";
                line += LogBuffer::level_to_string(entry.level);
                line += "] ";
            }
            line += entry.message;

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.c_str());

            if (_show_source && !entry.source_file.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("@ %s:%d", entry.source_file.c_str(), entry.source_line);
            }

            ImGui::PopStyleColor();
        }

        // Auto-scroll to bottom
        if (_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        return Ok();
    }

private:
    bool _auto_scroll = true;
    bool _show_timestamp = true;
    bool _show_level = true;
    bool _show_source = false;
    int _min_level = 0;  // spdlog::level::trace
};

} // namespace ymery::plugins

extern "C" const char* name() { return "logs-view"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::LogsView::create(wf, d, ns, db);
}
