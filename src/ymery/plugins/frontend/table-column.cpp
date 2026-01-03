// table-column widget plugin - table column for use inside table-row
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class TableColumn : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TableColumn>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TableColumn::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        ImGui::TableNextColumn();
        _container_open = true;  // Columns always "open"
        return Ok();
    }

    Result<void> _end_container() override {
        // No end call needed for table columns
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "table-column"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::TableColumn::create(wf, d, ns, db)));
}
