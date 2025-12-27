#include "widget.hpp"
#include "widget_factory.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery {

Result<std::shared_ptr<Widget>> Widget::create(
    std::shared_ptr<WidgetFactory> widget_factory,
    std::shared_ptr<Dispatcher> dispatcher,
    const std::string& ns,
    std::shared_ptr<DataBag> data_bag
) {
    auto widget = std::make_shared<Widget>();
    widget->_widget_factory = widget_factory;
    widget->_dispatcher = dispatcher;
    widget->_namespace = ns;
    widget->_data_bag = data_bag;

    if (auto res = widget->init(); !res) {
        return Err<std::shared_ptr<Widget>>("Widget::create: init failed", res);
    }
    return widget;
}

Result<void> Widget::init() {
    // Parse event handlers from statics
    if (auto res = _data_bag->get_static("event-handlers"); res) {
        if (auto handlers = get_as<Dict>(*res)) {
            for (const auto& [event_name, commands_val] : *handlers) {
                if (auto commands = get_as<List>(commands_val)) {
                    for (const auto& cmd : *commands) {
                        if (auto cmd_dict = get_as<Dict>(cmd)) {
                            _event_handlers[event_name].push_back(*cmd_dict);
                        }
                    }
                } else if (auto cmd_dict = get_as<Dict>(commands_val)) {
                    _event_handlers[event_name].push_back(*cmd_dict);
                } else if (auto cmd_str = get_as<std::string>(commands_val)) {
                    Dict simple_cmd;
                    simple_cmd[*cmd_str] = Value{};
                    _event_handlers[event_name].push_back(simple_cmd);
                }
            }
        }
    }
    return Ok();
}

Result<void> Widget::dispose() {
    if (_body) {
        _body->dispose();
        _body.reset();
    }
    return Ok();
}

Result<void> Widget::render() {
    // Clear errors from previous render cycle
    _error_messages.clear();

    // Push styles - continue even on error
    if (auto res = _push_styles(); !res) {
        _handle_error(Err<void>("Widget::render: _push_styles failed", res));
    }

    // Pre-render head - continue even on error
    if (_error_messages.empty()) {
        if (auto res = _pre_render_head(); !res) {
            _handle_error(Err<void>("Widget::render: _pre_render_head failed", res));
        }
    }

    // Detect and execute events - continue even on error
    if (_error_messages.empty()) {
        if (auto res = _detect_and_execute_events(); !res) {
            _handle_error(Err<void>("Widget::render: _detect_and_execute_events failed", res));
        }
    }

    // Show tooltip if widget has tooltip property
    if (ImGui::IsItemHovered()) {
        if (auto tooltip_res = _data_bag->get_static("tooltip"); tooltip_res) {
            if (auto tooltip_text = get_as<std::string>(*tooltip_res)) {
                ImGui::SetTooltip("%s", tooltip_text->c_str());
            }
        }
    }

    // Body handling - continue even on error
    if (_error_messages.empty() && _is_body_activated) {
        if (auto res = _ensure_body(); !res) {
            _handle_error(Err<void>("Widget::render: _ensure_body failed", res));
        }
        if (_body) {
            if (auto res = _body->render(); !res) {
                _handle_error(Err<void>("Widget::render: body render failed", res));
            }
        }
    }

    // Post-render head - continue even on error
    if (auto res = _post_render_head(); !res) {
        _handle_error(Err<void>("Widget::render: _post_render_head failed", res));
    }

    // Pop styles - always try to cleanup
    if (auto res = _pop_styles(); !res) {
        _handle_error(Err<void>("Widget::render: _pop_styles failed", res));
    }

    // Render accumulated errors in-widget
    return _render_errors();
}

void Widget::_handle_error(const Result<void>& result) {
    if (!result) {
        _error_messages.push_back(result.error().to_string());
    }
}

Result<void> Widget::_render_errors() {
    if (_error_messages.empty()) {
        return Ok();
    }

    // Render errors inline using ImGui
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::TextUnformatted("Errors:");
    ImGui::Separator();

    for (const auto& msg : _error_messages) {
        ImGui::TextWrapped("%s", msg.c_str());
    }

    ImGui::PopStyleColor();
    return Ok();
}

Result<void> Widget::_pre_render_head() {
    return Ok();
}

Result<void> Widget::_post_render_head() {
    return Ok();
}

Result<void> Widget::_ensure_body() {
    if (_body) return Ok();

    auto body_res = _data_bag->get_static("body");
    if (!body_res) return Ok();

    auto body_spec = *body_res;
    if (!body_spec.has_value()) return Ok();

    auto widget_res = _widget_factory->create_widget(_data_bag, body_spec, _namespace);
    if (!widget_res) {
        return Err<void>("Widget::_ensure_body: failed", widget_res);
    }
    _body = *widget_res;
    return Ok();
}

// Color name to ImGuiCol mapping
static int get_imgui_color_idx(const std::string& name) {
    static const std::map<std::string, int> color_map = {
        {"text", ImGuiCol_Text},
        {"text-disabled", ImGuiCol_TextDisabled},
        {"window-bg", ImGuiCol_WindowBg},
        {"child-bg", ImGuiCol_ChildBg},
        {"popup-bg", ImGuiCol_PopupBg},
        {"border", ImGuiCol_Border},
        {"frame-bg", ImGuiCol_FrameBg},
        {"frame-bg-hovered", ImGuiCol_FrameBgHovered},
        {"frame-bg-active", ImGuiCol_FrameBgActive},
        {"button", ImGuiCol_Button},
        {"button-hovered", ImGuiCol_ButtonHovered},
        {"button-active", ImGuiCol_ButtonActive},
        {"header", ImGuiCol_Header},
        {"header-hovered", ImGuiCol_HeaderHovered},
        {"header-active", ImGuiCol_HeaderActive},
        {"slider-grab", ImGuiCol_SliderGrab},
        {"slider-grab-active", ImGuiCol_SliderGrabActive},
    };
    auto it = color_map.find(name);
    return it != color_map.end() ? it->second : -1;
}

Result<void> Widget::_push_styles() {
    auto style_res = _data_bag->get_static("style");
    if (!style_res) return Ok();

    auto style_dict = get_as<Dict>(*style_res);
    if (!style_dict) return Ok();

    for (const auto& [name, val] : *style_dict) {
        int idx = get_imgui_color_idx(name);
        if (idx >= 0) {
            if (auto color_list = get_as<List>(val)) {
                if (color_list->size() >= 3) {
                    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
                    if (auto rv = get_as<double>((*color_list)[0])) r = static_cast<float>(*rv);
                    if (auto gv = get_as<double>((*color_list)[1])) g = static_cast<float>(*gv);
                    if (auto bv = get_as<double>((*color_list)[2])) b = static_cast<float>(*bv);
                    if (color_list->size() >= 4) {
                        if (auto av = get_as<double>((*color_list)[3])) a = static_cast<float>(*av);
                    }
                    ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
                    _pushed_colors.push_back({idx, 0.0f});
                }
            }
        }
    }
    return Ok();
}

Result<void> Widget::_pop_styles() {
    if (!_pushed_colors.empty()) {
        ImGui::PopStyleColor(static_cast<int>(_pushed_colors.size()));
        _pushed_colors.clear();
    }
    if (!_pushed_vars.empty()) {
        ImGui::PopStyleVar(static_cast<int>(_pushed_vars.size()));
        _pushed_vars.clear();
    }
    return Ok();
}

Result<void> Widget::_detect_and_execute_events() {
    if (ImGui::IsItemClicked()) _execute_event_commands("on-click");
    if (ImGui::IsItemHovered()) {
        _execute_event_commands("on-hover");
    }
    return Ok();
}

Result<void> Widget::_execute_event_commands(const std::string& event_name) {
    auto it = _event_handlers.find(event_name);
    if (it == _event_handlers.end()) return Ok();

    for (const auto& command : it->second) {
        _execute_event_command(command);
    }
    return Ok();
}

Result<void> Widget::_execute_event_command(const Dict& command) {
    for (const auto& [cmd_type, cmd_data] : command) {
        if (cmd_type == "show") {
            // Create and render widget from spec
            auto widget_res = _widget_factory->create_widget(_data_bag, cmd_data, _namespace);
            if (widget_res) {
                (*widget_res)->render();
            }
        } else if (cmd_type == "dispatch-event") {
            Dict event;
            if (auto name = get_as<std::string>(cmd_data)) {
                event["name"] = cmd_data;
            } else if (auto event_dict = get_as<Dict>(cmd_data)) {
                event = *event_dict;
            }
            _dispatcher->dispatch_event(event);
        } else if (cmd_type == "close") {
            ImGui::CloseCurrentPopup();
        } else if (cmd_type == "open-popup") {
            if (auto popup_id = get_as<std::string>(cmd_data)) {
                ImGui::OpenPopup(popup_id->c_str());
            }
        }
        break;
    }
    return Ok();
}

Result<void> Widget::handle_event(const Dict& event) {
    return Ok();
}

// Force all widget registrations by referencing them
void register_all_widgets() {
    // Include headers to trigger static registration
    // This function is called from main to ensure linkage
}

} // namespace ymery
