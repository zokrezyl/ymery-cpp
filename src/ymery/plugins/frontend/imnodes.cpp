// imnodes widget plugin - Node graph editor
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imnodes.h>
#include <imgui.h>

namespace ymery::plugins {

class ImnodesEditor : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ImnodesEditor>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ImnodesEditor::create failed", res);
        }
        return widget;
    }

protected:
    int _next_node_id = 100;
    int _next_link_id = 200;

    Result<void> _begin_container() override {
        // Get editor context (created once on first use)
        ImNodesContext* context = ImNodes::GetCurrentContext();
        if (!context) {
            ImNodes::CreateContext();
        }

        std::string label = "Node Editor";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Begin the node editor
        ImNodes::BeginNodeEditor();
        _container_open = true;

        // Load and render nodes from data-bag
        if (auto nodes_res = _data_bag->get("nodes"); nodes_res) {
            if (auto nodes_dict = get_as<Dict>(*nodes_res)) {
                for (const auto& [node_name, node_data] : *nodes_dict) {
                    if (auto node_dict = get_as<Dict>(node_data)) {
                        int node_id = 100; // Default
                        int x = 0, y = 0;
                        std::string title = node_name;
                        
                        auto find_id = node_dict->find("id");
                        if (find_id != node_dict->end()) {
                            if (auto id = get_as<int>(find_id->second)) node_id = *id;
                        }
                        auto find_x = node_dict->find("x");
                        if (find_x != node_dict->end()) {
                            if (auto x_val = get_as<int>(find_x->second)) x = *x_val;
                        }
                        auto find_y = node_dict->find("y");
                        if (find_y != node_dict->end()) {
                            if (auto y_val = get_as<int>(find_y->second)) y = *y_val;
                        }
                        auto find_title = node_dict->find("title");
                        if (find_title != node_dict->end()) {
                            if (auto t = get_as<std::string>(find_title->second)) title = *t;
                        }

                        ImNodes::BeginNode(node_id);
                        ImNodes::SetNodeScreenSpacePos(node_id, ImVec2(static_cast<float>(x), static_cast<float>(y)));

                        ImGui::Text("%s", title.c_str());
                        ImGui::Spacing();

                        // Add input pin
                        ImNodes::BeginInputAttribute(node_id * 10 + 1);
                        ImGui::Text("In");
                        ImNodes::EndInputAttribute();

                        ImGui::Spacing();

                        // Add output pin
                        ImNodes::BeginOutputAttribute(node_id * 10 + 2);
                        ImGui::Text("Out");
                        ImNodes::EndOutputAttribute();

                        ImNodes::EndNode();
                    }
                }
            }
        }

        // Render links from data-bag
        if (auto links_res = _data_bag->get("links"); links_res) {
            if (auto links_dict = get_as<Dict>(*links_res)) {
                int link_counter = 200;
                
                auto find_children = links_dict->find("children");
                if (find_children != links_dict->end()) {
                    if (auto child_dict = get_as<Dict>(find_children->second)) {
                        for (const auto& [link_name, link_data] : *child_dict) {
                            if (auto link = get_as<Dict>(link_data)) {
                                auto find_meta = link->find("metadata");
                                if (find_meta != link->end()) {
                                    if (auto meta_dict = get_as<Dict>(find_meta->second)) {
                                        int start_attr = 0, end_attr = 0;
                                        auto find_start = meta_dict->find("start-attr");
                                        if (find_start != meta_dict->end()) {
                                            if (auto sv = get_as<int>(find_start->second)) start_attr = *sv;
                                        }
                                        auto find_end = meta_dict->find("end-attr");
                                        if (find_end != meta_dict->end()) {
                                            if (auto ev = get_as<int>(find_end->second)) end_attr = *ev;
                                        }
                                        ImNodes::Link(link_counter++, start_attr, end_attr);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImNodes::EndNodeEditor();
        }

        // Check for link creation
        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
            _execute_event_commands("on-link-created");
        }

        // Check for link deletion
        if (ImNodes::IsLinkDestroyed(&start_attr)) {
            _execute_event_commands("on-link-destroyed");
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "imnodes"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::ImnodesEditor::create(wf, d, ns, db)));
}
