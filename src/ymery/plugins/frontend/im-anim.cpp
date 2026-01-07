// ImAnim widget plugin - Animation container for ImGui elements
// Provides tweening, easing, and animation capabilities via ImAnim library
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins {

class ImAnim : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ImAnim>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ImAnim::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Initialize ImAnim frame
        iam_update_begin_frame();
        
        // Get animation parameters
        std::string mode = "tween";
        if (auto res = _data_bag->get_static("mode"); res) {
            if (auto m = get_as<std::string>(*res)) {
                mode = *m;
            }
        }

        // Get animation target value
        double target = 1.0;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) {
                target = *t;
            }
        }

        // Get animation duration
        double duration = 0.3;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) {
                duration = *d;
            }
        }

        // Get easing type (0 = linear by default)
        int easing = 1; // ease_out_quad by default
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) {
                easing = static_cast<int>(*e);
            }
        }

        // Get policy (0 = crossfade, 1 = cut, 2 = queue)
        int policy = 0;
        if (auto res = _data_bag->get_static("policy"); res) {
            if (auto p = get_as<double>(*res)) {
                policy = static_cast<int>(*p);
            }
        }

        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 0.016f;

        if (mode == "tween") {
            iam_ease_desc ease = iam_ease_preset(static_cast<iam_ease_type>(easing));

            ImGuiID id = ImGui::GetID(_uid.c_str());
            ImGuiID channel = ImGui::GetID((_uid + "_channel").c_str());

            // Perform tween
            float animated_value = iam_tween_float(id, channel, static_cast<float>(target),
                                                    static_cast<float>(duration), ease, policy, dt);

            // Store result
            _data_bag->set("value", Value(static_cast<double>(animated_value)));
        }

        return Ok();
    }

    Result<void> _begin_container() override {
        // Animation is a container, but typically transparent to layout
        // Body widgets render normally
        return Ok();
    }

    Result<void> _end_container() override {
        // Nothing special needed at end
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* PLUGIN_EXPORT_NAME() { return "im-anim"; }
extern "C" const char* PLUGIN_EXPORT_TYPE() { return "widget"; }
extern "C" void* PLUGIN_EXPORT_CREATE(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::ImAnim::create(wf, d, ns, db)));
}
