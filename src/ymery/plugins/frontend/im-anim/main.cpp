// im-anim plugin - Animation widgets using ImAnim library
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "float-tween.hpp"
#include "vec2-tween.hpp"
#include "color-tween.hpp"
#include "oscillate.hpp"
#include "shake.hpp"
#include "spring.hpp"
#include "easing-compare.hpp"
#include "wiggle.hpp"
#include "int-tween.hpp"
#include "transform.hpp"
#include "bezier.hpp"
#include "noise.hpp"
#include "text-stagger.hpp"

namespace ymery::plugins {

class ImAnimPlugin : public Plugin {
public:
    const char* name() const override { return "im-anim"; }

    std::vector<std::string> widgets() const override {
        return {
            "float-tween",
            "vec2-tween",
            "color-tween",
            "oscillate",
            "shake",
            "spring",
            "easing-compare",
            "wiggle",
            "int-tween",
            "transform",
            "bezier",
            "noise",
            "text-stagger"
        };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "float-tween") {
            return imanim::FloatTween::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "vec2-tween") {
            return imanim::Vec2Tween::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "color-tween") {
            return imanim::ColorTween::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "oscillate") {
            return imanim::Oscillate::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "shake") {
            return imanim::Shake::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "spring") {
            return imanim::Spring::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "easing-compare") {
            return imanim::EasingCompare::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "wiggle") {
            return imanim::Wiggle::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "int-tween") {
            return imanim::IntTween::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "transform") {
            return imanim::Transform::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "bezier") {
            return imanim::Bezier::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "noise") {
            return imanim::Noise::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "text-stagger") {
            return imanim::TextStagger::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImAnimPlugin());
}
