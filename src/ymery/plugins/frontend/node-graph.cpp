// node-graph widget plugin - Node graph editor
// Integrated from: https://gist.github.com/spacechase0/e2ff2c4820726d62074ec0d3708d61c3

#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <any>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cmath>
#include <imgui_internal.h>

// ============================================================================
// NodeGraph Library - Header Section
// ============================================================================

namespace NodeGraph
{
    enum ConnectionType
    {
        Sequence,
        Int,
        Float,
        String,
        Vector2,
    };

    struct NodeType
    {
        std::vector< std::pair< ConnectionType, std::string > > inputs;
        std::vector< std::pair< ConnectionType, std::string > > outputs;
        bool canUserCreate = true;
    };

    struct Node
    {
        public:
            ImVec2 position;

            std::string type;

            std::vector< std::any > inputs;
            std::vector< std::any > outputs;

            bool collapsed = false;
            bool selected = false;

        private:
            friend class Graph;

            ImVec2 getInputConnectorPos( ImVec2 base, int index );

            ImVec2 getOutputConnectorPos( ImVec2 base, int index );
    };

    struct Connection
    {
        Node* other = nullptr;
        int index = 0;
        bool selected = false;
    };

    class Graph
    {
        public:
            std::vector< std::unique_ptr< Node > > nodes;
            std::map< std::string, NodeType > types;

            ImU32 gridColor = ImColor( 128, 128, 128, 32 );
            float gridSize = 64;

            void update();
            void deletePressed();

        private:
            ImVec2 scroll = ImVec2( 0, 0 );

            std::unique_ptr< Connection > connSel;
            bool connSelInput = false;

            void deselectAll();

            ImU32 getConnectorColor( ConnectionType connType );
            void doPinCircle( ImDrawList* draw, ImVec2 pos, ConnectionType connType, bool filled );
            void doPinValue( const std::string& label, ConnectionType connType, std::any& input );
    };
}

// ============================================================================
// NodeGraph Library - Implementation Section
// ============================================================================

namespace
{
    ImVec2 operator - ( ImVec2 a, ImVec2 b ) { return ImVec2( a.x - b.x, a.y - b.y ); }
    ImVec2 operator + ( ImVec2 a, ImVec2 b ) { return ImVec2( a.x + b.x, a.y + b.y ); }
    void operator += ( ImVec2& a, ImVec2 b ) { a = a + b; }
    void operator -= ( ImVec2& a, ImVec2 b ) { a = a - b; }

    template<int n>
    struct BezierWeights
    {
        constexpr BezierWeights() : x_(), y_(), z_(), w_()
        {
            for (int i = 1; i <= n; ++i)
            {
                float t = (float)i / (float)(n + 1);
                float u = 1.0f - t;

                x_[i - 1] = u * u * u;
                y_[i - 1] = 3 * u * u * t;
                z_[i - 1] = 3 * u * t * t;
                w_[i - 1] = t * t * t;
            }
        }

        float x_[n];
        float y_[n];
        float z_[n];
        float w_[n];
    };

    static constexpr auto bezier_weights_ = BezierWeights<16>();

    float ImVec2Dot(const ImVec2& S1, const ImVec2& S2)
    {
        return (S1.x * S2.x + S1.y * S2.y);
    }

    float GetSquaredDistancePointSegment(const ImVec2& P, const ImVec2& S1, const ImVec2& S2)
    {
        const float l2 = (S1.x - S2.x) * (S1.x - S2.x) + (S1.y - S2.y) * (S1.y - S2.y);

        if (l2 < 1.0f)
        {
            return (P.x - S2.x) * (P.x - S2.x) + (P.y - S2.y) * (P.y - S2.y);
        }

        ImVec2 PS1(P.x - S1.x, P.y - S1.y);
        ImVec2 T(S2.x - S1.x, S2.y - S2.y);

        const float tf = ImVec2Dot(PS1, T) / l2;
        const float minTf = 1.0f < tf ? 1.0f : tf;
        const float t = 0.0f > minTf ? 0.0f : minTf;

        T.x = S1.x + T.x * t;
        T.y = S1.y + T.y * t;

        return (P.x - T.x) * (P.x - T.x) + (P.y - T.y) * (P.y - T.y);
    }

    float GetSquaredDistanceToBezierCurve(const ImVec2& point, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4)
    {
        float minSquaredDistance = FLT_MAX;
        float tmp;

        ImVec2 L = p1;
        ImVec2 temp;

        for (int i = 1; i < 16 - 1; ++i)
        {
            const ImVec4& W = ImVec4(bezier_weights_.x_[i], bezier_weights_.y_[i], bezier_weights_.z_[i], bezier_weights_.w_[i]);

            temp.x = W.x * p1.x + W.y * p2.x + W.z * p3.x + W.w * p4.x;
            temp.y = W.x * p1.y + W.y * p2.y + W.z * p3.y + W.w * p4.y;

            tmp = GetSquaredDistancePointSegment(point, L, temp);

            if (minSquaredDistance > tmp)
            {
                minSquaredDistance = tmp;
            }

            L = temp;
        }

        tmp = GetSquaredDistancePointSegment(point, L, p4);

        if (minSquaredDistance > tmp)
        {
            minSquaredDistance = tmp;
        }

        return minSquaredDistance;
    }
}

ImVec2 NodeGraph::Node::getInputConnectorPos( ImVec2 base, int index )
{
    if ( inputs.empty() )
    {
        return base;
    }

    ImVec2 pinSize( 4, 4 );
    ImVec2 ret = base + ImVec2( -8, 20 + index * 20 );

    return ret;
}

ImVec2 NodeGraph::Node::getOutputConnectorPos( ImVec2 base, int index )
{
    if ( outputs.empty() )
    {
        return base;
    }

    ImVec2 pinSize( 4, 4 );
    ImVec2 ret = base + ImVec2( 120, 20 + index * 20 );

    return ret;
}

void NodeGraph::Graph::deselectAll()
{
    for ( auto& node : nodes )
    {
        node->selected = false;
    }
}

ImU32 NodeGraph::Graph::getConnectorColor( ConnectionType connType )
{
    switch ( connType )
    {
        case Sequence:
            return ImColor( 255, 255, 255 );
        case Int:
            return ImColor( 100, 200, 255 );
        case Float:
            return ImColor( 200, 100, 255 );
        case String:
            return ImColor( 100, 255, 100 );
        case Vector2:
            return ImColor( 200, 200, 100 );
    }

    return ImColor( 255, 100, 100 );
}

void NodeGraph::Graph::doPinCircle( ImDrawList* draw, ImVec2 pos, ConnectionType connType, bool filled )
{
    ImU32 col = getConnectorColor( connType );

    if ( filled )
    {
        draw->AddCircleFilled( pos, 4, col );
    }
    else
    {
        draw->AddCircle( pos, 4, col );
    }
}

void NodeGraph::Graph::doPinValue( const std::string& label, ConnectionType connType, std::any& input )
{
    ImGui::Text( "%s", label.c_str() );

    ImU32 col = getConnectorColor( connType );
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    ImVec2 pinPos = curPos + ImVec2( -12, 5 );
    doPinCircle( draw, pinPos, connType, false );
}

void NodeGraph::Graph::deletePressed()
{
    for ( int i = 0; i < nodes.size(); i++ )
    {
        if ( nodes[ i ]->selected )
        {
            nodes.erase( nodes.begin() + i );
        }
    }

    connSel = nullptr;
}

void NodeGraph::Graph::update()
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImVec2 canvasEnd = canvasPos + canvasSize;

    draw->AddRectFilled( canvasPos, canvasEnd, ImColor( 50, 50, 50 ) );

    // Draw grid
    for ( float x = fmod( scroll.x, gridSize ); x < canvasSize.x; x += gridSize )
    {
        draw->AddLine( ImVec2( canvasPos.x + x, canvasPos.y ),
                      ImVec2( canvasPos.x + x, canvasEnd.y ), gridColor );
    }

    for ( float y = fmod( scroll.y, gridSize ); y < canvasSize.y; y += gridSize )
    {
        draw->AddLine( ImVec2( canvasPos.x, canvasPos.y + y ),
                      ImVec2( canvasEnd.x, canvasPos.y + y ), gridColor );
    }

    // Handle mouse wheel for pan
    if ( ImGui::IsMouseHoveringRect( canvasPos, canvasEnd ) )
    {
        if ( ImGui::GetIO().MouseWheel > 0 )
        {
            scroll += ImVec2( 0, 20 );
        }
        else if ( ImGui::GetIO().MouseWheel < 0 )
        {
            scroll += ImVec2( 0, -20 );
        }
    }

    // Draw and update nodes
    for ( auto& node : nodes )
    {
        ImVec2 nodePos = canvasPos + node->position + scroll;
        ImVec2 nodeSize( 120, 20 + std::max( node->inputs.size(), node->outputs.size() ) * 20 );

        draw->AddRectFilled( nodePos, nodePos + nodeSize,
                            node->selected ? ImColor( 100, 150, 255 ) : ImColor( 60, 60, 90 ) );

        draw->AddRect( nodePos, nodePos + nodeSize, ImColor( 200, 200, 200 ), 2 );

        draw->AddText( nodePos + ImVec2( 5, 3 ), ImColor( 255, 255, 255 ), node->type.c_str() );

        // Draw input connectors
        for ( int i = 0; i < node->inputs.size(); i++ )
        {
            ImVec2 pinPos = node->getInputConnectorPos( nodePos, i );
            doPinCircle( draw, pinPos, Int, false );
        }

        // Draw output connectors
        for ( int i = 0; i < node->outputs.size(); i++ )
        {
            ImVec2 pinPos = node->getOutputConnectorPos( nodePos, i );
            doPinCircle( draw, pinPos, Int, true );
        }

        // Handle node selection
        if ( ImGui::IsMouseHoveringRect( nodePos, nodePos + nodeSize ) &&
             ImGui::IsMouseClicked( 0 ) )
        {
            deselectAll();
            node->selected = true;
        }

        // Handle node dragging - only when mouse is inside canvas
        if ( node->selected && ImGui::IsMouseDragging( 0 ) &&
             ImGui::IsMouseHoveringRect( canvasPos, canvasEnd ) )
        {
            node->position += ImGui::GetIO().MouseDelta;
        }
    }

    // Handle panning
    if ( ImGui::IsMouseHoveringRect( canvasPos, canvasEnd ) &&
         ImGui::IsMouseDragging( 2 ) )
    {
        scroll -= ImGui::GetIO().MouseDelta;
    }
}

// ============================================================================
// Ymery Plugin Wrapper
// ============================================================================

namespace ymery::plugins {

class NodeGraphWidget : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<NodeGraphWidget>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("NodeGraphWidget::create failed", res);
        }
        return widget;
    }

private:
    std::unique_ptr<NodeGraph::Graph> graph;

protected:
    Result<void> _post_render_head() override {
        if (!graph) {
            graph = std::make_unique<NodeGraph::Graph>();
            
            // Add sample nodes
            auto inputNode = std::make_unique<NodeGraph::Node>();
            inputNode->type = "Input";
            inputNode->position = ImVec2(0, 0);
            inputNode->outputs.push_back(0);
            graph->nodes.push_back(std::move(inputNode));

            auto processNode = std::make_unique<NodeGraph::Node>();
            processNode->type = "Process";
            processNode->position = ImVec2(200, 0);
            processNode->inputs.push_back(0);
            processNode->outputs.push_back(0);
            graph->nodes.push_back(std::move(processNode));

            auto outputNode = std::make_unique<NodeGraph::Node>();
            outputNode->type = "Output";
            outputNode->position = ImVec2(400, 0);
            outputNode->inputs.push_back(0);
            graph->nodes.push_back(std::move(outputNode));
        }

        ImGui::BeginChild(("NodeGraph_" + _uid).c_str(), ImVec2(-1, -1), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Capture mouse to prevent parent window movement
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##NodeGraphCanvas", canvasSize);
        ImGui::SetCursorPos(ImVec2(0, 0));
        
        graph->update();
        ImGui::EndChild();

        _execute_event_commands("on-change");
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "node-graph"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::NodeGraphWidget::create(wf, d, ns, db)));
}
