// piano widget plugin - Piano keyboard widget
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imgui_internal.h>

namespace ymery::plugins {

// Piano keyboard implementation (based on imgui_piano by frowrik)
enum ImGuiPianoKeyboardMsg {
    NoteGetStatus,
    NoteOn,
    NoteOff,
};

struct ImGuiPianoStyles {
    ImU32 Colors[5]{
        IM_COL32(255, 255, 255, 255),  // light note
        IM_COL32(0, 0, 0, 255),        // dark note
        IM_COL32(255, 255, 0, 255),    // active light note
        IM_COL32(200, 200, 0, 255),    // active dark note
        IM_COL32(75, 75, 75, 255),     // background
    };
    float NoteDarkHeight = 2.0f / 3.0f;
    float NoteDarkWidth = 2.0f / 3.0f;
};

class Piano : public Widget {
public:
    using PianoCallback = bool (*)(void* UserData, int Msg, int Key, float Vel);

    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Piano>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Piano::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) {
            return res;
        }
        // Initialize all keys as not pressed
        for (int i = 0; i < 128; i++) {
            _key_pressed[i] = false;
        }
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Piano";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Get size
        ImVec2 size(400, 100);
        if (auto res = _data_bag->get("size"); res) {
            if (auto s = get_as<List>(*res)) {
                if (s->size() >= 2) {
                    if (auto w = get_as<double>((*s)[0])) size.x = static_cast<float>(*w);
                    if (auto h = get_as<double>((*s)[1])) size.y = static_cast<float>(*h);
                }
            }
        }

        // Get note range (default: standard 88-key piano range)
        int begin_note = 21;  // A0
        int end_note = 108;   // C8
        if (auto res = _data_bag->get_static("begin_note"); res) {
            if (auto n = get_as<double>(*res)) {
                begin_note = static_cast<int>(*n);
            }
        }
        if (auto res = _data_bag->get_static("end_note"); res) {
            if (auto n = get_as<double>(*res)) {
                end_note = static_cast<int>(*n);
            }
        }

        // Render piano keyboard
        _render_keyboard(label.c_str(), size, begin_note, end_note);

        return Ok();
    }

private:
    void _render_keyboard(const char* id_name, ImVec2 size, int begin_note, int end_note) {
        static int NoteIsDark[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
        static int NoteLightNumber[12] = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 7 };
        static float NoteDarkOffset[12] = { 0.0f, -2.0f / 3.0f, 0.0f, -1.0f / 3.0f, 0.0f, 0.0f, -2.0f / 3.0f, 0.0f, -0.5f, 0.0f, -1.0f / 3.0f, 0.0f };

        // Fix range - ensure we start/end on white keys
        if (NoteIsDark[begin_note % 12] > 0) begin_note++;
        if (NoteIsDark[end_note % 12] > 0) end_note--;

        if (!id_name || begin_note < 0 || end_note < 0 || end_note <= begin_note) return;

        ImGuiPianoStyles style;

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        const ImGuiID id = window->GetID(id_name);
        ImDrawList* draw_list = window->DrawList;
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;

        // Calculate sizes
        int count_notes_align7 = (end_note / 12 - begin_note / 12) * 7 + NoteLightNumber[end_note % 12] - (NoteLightNumber[begin_note % 12] - 1);

        float note_height = size.y;
        float note_width = size.x / (float)count_notes_align7;
        float note_height2 = note_height * style.NoteDarkHeight;
        float note_width2 = note_width * style.NoteDarkWidth;

        if (note_height < 5.0f || note_width < 3.0f) return;

        bool is_mouse_input = (note_height >= 10.0f && note_width >= 5.0f);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(size, 0);
        if (!ImGui::ItemAdd(bb, id)) return;

        bool held = false;
        if (is_mouse_input) {
            ImGui::ButtonBehavior(bb, id, nullptr, &held, 0);
        }

        int note_mouse_collision = -1;
        float note_mouse_vel = 0.0f;

        // Draw white keys
        float offset_x = bb.Min.x;
        float offset_y = bb.Min.y;
        float offset_y2 = offset_y + note_height;
        for (int real_num = begin_note; real_num <= end_note; real_num++) {
            int i = real_num % 12;
            if (NoteIsDark[i] > 0) continue;

            ImRect note_rect(
                round(offset_x),
                offset_y,
                round(offset_x + note_width),
                offset_y2
            );

            if (held && note_rect.Contains(mouse_pos)) {
                note_mouse_collision = real_num;
                note_mouse_vel = (mouse_pos.y - note_rect.Min.y) / note_height;
            }

            bool is_active = _key_pressed[real_num];
            draw_list->AddRectFilled(note_rect.Min, note_rect.Max, style.Colors[is_active ? 2 : 0], 0.0f);
            draw_list->AddRect(note_rect.Min, note_rect.Max, style.Colors[4], 0.0f);

            offset_x += note_width;
        }

        // Draw black keys
        offset_x = bb.Min.x;
        offset_y = bb.Min.y;
        offset_y2 = offset_y + note_height2;
        for (int real_num = begin_note; real_num <= end_note; real_num++) {
            int i = real_num % 12;
            if (NoteIsDark[i] == 0) {
                offset_x += note_width;
                continue;
            }

            float offset_dark = NoteDarkOffset[i] * note_width2;
            ImRect note_rect(
                round(offset_x + offset_dark),
                offset_y,
                round(offset_x + note_width2 + offset_dark),
                offset_y2
            );

            if (held && note_rect.Contains(mouse_pos)) {
                note_mouse_collision = real_num;
                note_mouse_vel = (mouse_pos.y - note_rect.Min.y) / note_height2;
            }

            bool is_active = _key_pressed[real_num];
            draw_list->AddRectFilled(note_rect.Min, note_rect.Max, style.Colors[is_active ? 3 : 1], 0.0f);
            draw_list->AddRect(note_rect.Min, note_rect.Max, style.Colors[4], 0.0f);
        }

        // Handle mouse note click
        if (_prev_note_active != note_mouse_collision) {
            if (_prev_note_active >= 0 && _prev_note_active < 128) {
                _key_pressed[_prev_note_active] = false;
                // Fire note off event
                _fire_note_event("on_note_off", _prev_note_active, 0.0f);
            }
            _prev_note_active = -1;

            if (held && note_mouse_collision >= 0 && note_mouse_collision < 128) {
                _key_pressed[note_mouse_collision] = true;
                _prev_note_active = note_mouse_collision;
                // Fire note on event
                _fire_note_event("on_note_on", note_mouse_collision, note_mouse_vel);
            }
        }
    }

    void _fire_note_event(const std::string& event_name, int note, float velocity) {
        // Update data bag with current note info
        _data_bag->set("note", Value(static_cast<double>(note)));
        _data_bag->set("velocity", Value(static_cast<double>(velocity)));

        // Execute event handlers
        _execute_event_commands(event_name);
    }

    int _prev_note_active = -1;
    bool _key_pressed[128];
};

} // namespace ymery::plugins

extern "C" const char* name() { return "piano"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Piano::create(wf, d, ns, db);
}
