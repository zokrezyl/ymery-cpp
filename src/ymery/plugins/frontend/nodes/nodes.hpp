#pragma once
// nodes widget plugin
#define IMGUI_DEFINE_MATH_OPERATORS

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui {
////////////////////////////////////////////////////////////////////////////////

enum ImGuiNodesConnectorType_ {
  ImGuiNodesConnectorType_None = 0,
  ImGuiNodesConnectorType_Generic,
  ImGuiNodesConnectorType_Int,
  ImGuiNodesConnectorType_Float,
  ImGuiNodesConnectorType_Vector,
  ImGuiNodesConnectorType_Image,
  ImGuiNodesConnectorType_Text
};

enum ImGuiNodesNodeType_ {
  ImGuiNodesNodeType_None = 0,
  ImGuiNodesNodeType_Generic,
  ImGuiNodesNodeType_Generator,
  ImGuiNodesNodeType_Test
};

enum ImGuiNodesConnectorStateFlag_ {
  ImGuiNodesConnectorStateFlag_Default = 0,
  ImGuiNodesConnectorStateFlag_Visible = 1 << 0,
  ImGuiNodesConnectorStateFlag_Hovered = 1 << 1,
  ImGuiNodesConnectorStateFlag_Consider = 1 << 2,
  ImGuiNodesConnectorStateFlag_Draging = 1 << 3
};

enum ImGuiNodesNodeStateFlag_ {
  ImGuiNodesNodeStateFlag_Default = 0,
  ImGuiNodesNodeStateFlag_Visible = 1 << 0,
  ImGuiNodesNodeStateFlag_Hovered = 1 << 1,
  ImGuiNodesNodeStateFlag_Marked = 1 << 2,
  ImGuiNodesNodeStateFlag_Selected = 1 << 3,
  ImGuiNodesNodeStateFlag_Collapsed = 1 << 4,
  ImGuiNodesNodeStateFlag_Disabled = 1 << 5,
  ImGuiNodesNodeStateFlag_Processing = 1 << 6
};

enum ImGuiNodesState_ {
  ImGuiNodesState_Default = 0,
  ImGuiNodesState_HoveringNode,
  ImGuiNodesState_HoveringInput,
  ImGuiNodesState_HoveringOutput,
  ImGuiNodesState_Draging,
  ImGuiNodesState_DragingInput,
  ImGuiNodesState_DragingOutput,
  ImGuiNodesState_Selecting
};

////////////////////////////////////////////////////////////////////////////////

typedef unsigned int ImGuiNodesConnectorType;
typedef unsigned int ImGuiNodesNodeType;

typedef unsigned int ImGuiNodesConnectorState;
typedef unsigned int ImGuiNodesNodeState;

typedef unsigned int ImGuiNodesState;

////////////////////////////////////////////////////////////////////////////////

// connector text name heights factors
constexpr float ImGuiNodesConnectorDotDiameter =
    0.7f; // connectors dot diameter
constexpr float ImGuiNodesConnectorDotPadding =
    0.35f; // connectors dot left/right sides padding
constexpr float ImGuiNodesConnectorDistance =
    1.5f; // vertical distance between connectors centers

// title text name heights factors
constexpr float ImGuiNodesHSeparation =
    1.7f; // extra horizontal separation inbetween IOs
constexpr float ImGuiNodesVSeparation =
    1.5f; // total IOs area separation from title and node bottom edge
constexpr float ImGuiNodesTitleHight = 2.0f;

struct ImGuiNodesNode;
struct ImGuiNodesInput;
struct ImGuiNodesOutput;

////////////////////////////////////////////////////////////////////////////////

struct ImGuiNodesInput {
  ImVec2 pos_;
  ImRect area_input_;
  ImRect area_name_;
  ImGuiNodesConnectorType type_;
  ImGuiNodesConnectorState state_;
  const char *name_;
  ImGuiNodesNode *target_;
  ImGuiNodesOutput *output_;

  inline void TranslateInput(ImVec2 delta) {
    pos_ += delta;
    area_input_.Translate(delta);
    area_name_.Translate(delta);
  }

  inline void DrawInput(ImDrawList *draw_list, ImVec2 offset, float scale,
                        ImGuiNodesState state) const {
    if (type_ == ImGuiNodesConnectorType_None)
      return;

    if (state != ImGuiNodesState_Draging &&
        state_ & ImGuiNodesConnectorStateFlag_Hovered &&
        false == (state_ & ImGuiNodesConnectorStateFlag_Consider)) {
      const ImColor color = target_ == NULL ? ImColor(0.0f, 0.0f, 1.0f, 0.5f)
                                            : ImColor(1.0f, 0.5f, 0.0f, 0.5f);
      draw_list->AddRectFilled((area_input_.Min * scale) + offset,
                               (area_input_.Max * scale) + offset, color);
    }

    if (state_ & (ImGuiNodesConnectorStateFlag_Consider |
                  ImGuiNodesConnectorStateFlag_Draging))
      draw_list->AddRectFilled((area_input_.Min * scale) + offset,
                               (area_input_.Max * scale) + offset,
                               ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    bool consider_fill = false;
    consider_fill |= bool(state_ & ImGuiNodesConnectorStateFlag_Draging);
    consider_fill |= bool(state_ & ImGuiNodesConnectorStateFlag_Hovered &&
                          state_ & ImGuiNodesConnectorStateFlag_Consider);

    ImColor color = consider_fill ? ImColor(0.0f, 1.0f, 0.0f, 1.0f)
                                  : ImColor(1.0f, 1.0f, 1.0f, 1.0f);

    consider_fill |= bool(target_);

    if (consider_fill)
      draw_list->AddCircleFilled((pos_ * scale) + offset,
                                 (ImGuiNodesConnectorDotDiameter * 0.5f) *
                                     area_name_.GetHeight() * scale,
                                 color);
    else
      draw_list->AddCircle((pos_ * scale) + offset,
                           (ImGuiNodesConnectorDotDiameter * 0.5f) *
                               area_name_.GetHeight() * scale,
                           color);

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_);
  }

  ImGuiNodesInput(const char *name, ImGuiNodesConnectorType type) {
    type_ = type;
    state_ = ImGuiNodesConnectorStateFlag_Default;
    target_ = NULL;
    output_ = NULL;
    name_ = name;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name);

    area_input_.Min = ImVec2(0.0f, 0.0f);
    area_input_.Max.x = ImGuiNodesConnectorDotPadding +
                        ImGuiNodesConnectorDotDiameter +
                        ImGuiNodesConnectorDotPadding;
    area_input_.Max.y = ImGuiNodesConnectorDistance;
    area_input_.Max *= area_name_.GetHeight();

    ImVec2 offset = ImVec2(0.0f, 0.0f) - area_input_.GetCenter();

    area_name_.Translate(
        ImVec2(area_input_.GetWidth(),
               (area_input_.GetHeight() - area_name_.GetHeight()) * 0.5f));

    area_input_.Max.x += area_name_.GetWidth();
    area_input_.Max.x += ImGuiNodesConnectorDotPadding * area_name_.GetHeight();

    area_input_.Translate(offset);
    area_name_.Translate(offset);
  }
};

struct ImGuiNodesOutput {
  ImVec2 pos_;
  ImRect area_output_;
  ImRect area_name_;
  ImGuiNodesConnectorType type_;
  ImGuiNodesConnectorState state_;
  const char *name_;
  unsigned int connections_;

  inline void TranslateOutput(ImVec2 delta) {
    pos_ += delta;
    area_output_.Translate(delta);
    area_name_.Translate(delta);
  }

  inline void DrawOutput(ImDrawList *draw_list, ImVec2 offset, float scale,
                         ImGuiNodesState state) const {
    if (type_ == ImGuiNodesConnectorType_None)
      return;

    if (state != ImGuiNodesState_Draging &&
        state_ & ImGuiNodesConnectorStateFlag_Hovered &&
        false == (state_ & ImGuiNodesConnectorStateFlag_Consider))
      draw_list->AddRectFilled((area_output_.Min * scale) + offset,
                               (area_output_.Max * scale) + offset,
                               ImColor(0.0f, 0.0f, 1.0f, 0.5f));

    if (state_ & (ImGuiNodesConnectorStateFlag_Consider |
                  ImGuiNodesConnectorStateFlag_Draging))
      draw_list->AddRectFilled((area_output_.Min * scale) + offset,
                               (area_output_.Max * scale) + offset,
                               ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    bool consider_fill = false;
    consider_fill |= bool(state_ & ImGuiNodesConnectorStateFlag_Draging);
    consider_fill |= bool(state_ & ImGuiNodesConnectorStateFlag_Hovered &&
                          state_ & ImGuiNodesConnectorStateFlag_Consider);

    ImColor color = consider_fill ? ImColor(0.0f, 1.0f, 0.0f, 1.0f)
                                  : ImColor(1.0f, 1.0f, 1.0f, 1.0f);

    consider_fill |= bool(connections_ > 0);

    if (consider_fill)
      draw_list->AddCircleFilled((pos_ * scale) + offset,
                                 (ImGuiNodesConnectorDotDiameter * 0.5f) *
                                     area_name_.GetHeight() * scale,
                                 color);
    else
      draw_list->AddCircle((pos_ * scale) + offset,
                           (ImGuiNodesConnectorDotDiameter * 0.5f) *
                               area_name_.GetHeight() * scale,
                           color);

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_);
  }

  ImGuiNodesOutput(const char *name, ImGuiNodesConnectorType type) {
    type_ = type;
    state_ = ImGuiNodesConnectorStateFlag_Default;
    connections_ = 0;
    name_ = name;

    area_name_.Min = ImVec2(0.0f, 0.0f) - ImGui::CalcTextSize(name);
    area_name_.Max = ImVec2(0.0f, 0.0f);

    area_output_.Min.x = ImGuiNodesConnectorDotPadding +
                         ImGuiNodesConnectorDotDiameter +
                         ImGuiNodesConnectorDotPadding;
    area_output_.Min.y = ImGuiNodesConnectorDistance;
    area_output_.Min *= -area_name_.GetHeight();
    area_output_.Max = ImVec2(0.0f, 0.0f);

    ImVec2 offset = ImVec2(0.0f, 0.0f) - area_output_.GetCenter();

    area_name_.Translate(
        ImVec2(area_output_.Min.x,
               (area_output_.GetHeight() - area_name_.GetHeight()) * -0.5f));

    area_output_.Min.x -= area_name_.GetWidth();
    area_output_.Min.x -=
        ImGuiNodesConnectorDotPadding * area_name_.GetHeight();

    area_output_.Translate(offset);
    area_name_.Translate(offset);
  }
};

struct ImGuiNodesNode {
  ImRect area_node_;
  ImRect area_name_;
  float title_height_;
  float body_height_;
  ImGuiNodesNodeState state_;
  ImGuiNodesNodeType type_;
  const char *name_;
  ImColor color_;
  ImVector<ImGuiNodesInput> inputs_;
  ImVector<ImGuiNodesOutput> outputs_;

  inline void TranslateNode(ImVec2 delta, bool selected_only = false) {
    if (selected_only && !(state_ & ImGuiNodesNodeStateFlag_Selected))
      return;

    area_node_.Translate(delta);
    area_name_.Translate(delta);

    for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
      inputs_[input_idx].TranslateInput(delta);

    for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
      outputs_[output_idx].TranslateOutput(delta);
  }

  inline void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size) {
    body_height_ = ImMax(inputs_size.y, outputs_size.y) +
                   (ImGuiNodesVSeparation * area_name_.GetHeight());

    area_node_.Min = ImVec2(0.0f, 0.0f);
    area_node_.Max = ImVec2(0.0f, 0.0f);
    area_node_.Max.x += inputs_size.x + outputs_size.x;
    area_node_.Max.x += ImGuiNodesHSeparation * area_name_.GetHeight();
    area_node_.Max.y += title_height_ + body_height_;

    area_name_.Translate(
        ImVec2((area_node_.GetWidth() - area_name_.GetWidth()) * 0.5f,
               ((title_height_ - area_name_.GetHeight()) * 0.5f)));

    ImVec2 inputs = area_node_.GetTL();
    inputs.y +=
        title_height_ + (ImGuiNodesVSeparation * area_name_.GetHeight() * 0.5f);
    for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx) {
      inputs_[input_idx].TranslateInput(inputs -
                                        inputs_[input_idx].area_input_.GetTL());
      inputs.y += inputs_[input_idx].area_input_.GetHeight();
    }

    ImVec2 outputs = area_node_.GetTR();
    outputs.y +=
        title_height_ + (ImGuiNodesVSeparation * area_name_.GetHeight() * 0.5f);
    for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx) {
      outputs_[output_idx].TranslateOutput(
          outputs - outputs_[output_idx].area_output_.GetTR());
      outputs.y += outputs_[output_idx].area_output_.GetHeight();
    }
  }

  inline void DrawNode(ImDrawList *draw_list, ImVec2 offset, float scale,
                       ImGuiNodesState state) const {
    if (false == (state_ & ImGuiNodesNodeStateFlag_Visible))
      return;

    ImRect node_rect = area_node_;
    node_rect.Min *= scale;
    node_rect.Max *= scale;
    node_rect.Translate(offset);

    float rounding = title_height_ * scale * 0.3f;

    ImColor head_color = color_, body_color = color_;
    head_color.Value.x *= 0.5;
    head_color.Value.y *= 0.5;
    head_color.Value.z *= 0.5;

    head_color.Value.w = 1.00f;
    body_color.Value.w = 0.75f;

    const ImVec2 outline(4.0f * scale, 4.0f * scale);

    const ImDrawFlags rounding_corners_flags = ImDrawFlags_RoundCornersAll;

    if (state_ & ImGuiNodesNodeStateFlag_Disabled) {
      body_color.Value.w = 0.25f;

      if (state_ & ImGuiNodesNodeStateFlag_Collapsed)
        head_color.Value.w = 0.25f;
    }

    if (state_ & ImGuiNodesNodeStateFlag_Processing)
      draw_list->AddRectFilled(node_rect.Min - outline, node_rect.Max + outline,
                               body_color, rounding, rounding_corners_flags);
    else
      draw_list->AddRectFilled(node_rect.Min, node_rect.Max, body_color,
                               rounding, rounding_corners_flags);

    const ImVec2 head = node_rect.GetTR() + ImVec2(0.0f, title_height_ * scale);

    if (false == (state_ & ImGuiNodesNodeStateFlag_Collapsed))
      draw_list->AddLine(ImVec2(node_rect.Min.x, head.y),
                         ImVec2(head.x - 1.0f, head.y),
                         ImColor(0.0f, 0.0f, 0.0f, 0.5f), 2.0f);

    const ImDrawFlags head_corners_flags =
        state_ & ImGuiNodesNodeStateFlag_Collapsed ? rounding_corners_flags
                                                   : ImDrawFlags_RoundCornersTop;
    draw_list->AddRectFilled(node_rect.Min, head, head_color, rounding,
                             head_corners_flags);

    ////////////////////////////////////////////////////////////////////////////////

    if (state_ & ImGuiNodesNodeStateFlag_Disabled) {
      IM_ASSERT(false == node_rect.IsInverted());

      const float separation = 15.0f * scale;

      for (float line = separation; true; line += separation) {
        ImVec2 start = node_rect.Min + ImVec2(0.0f, line);
        ImVec2 stop = node_rect.Min + ImVec2(line, 0.0f);

        if (start.y > node_rect.Max.y)
          start =
              ImVec2(start.x + (start.y - node_rect.Max.y), node_rect.Max.y);

        if (stop.x > node_rect.Max.x)
          stop = ImVec2(node_rect.Max.x, stop.y + (stop.x - node_rect.Max.x));

        if (start.x > node_rect.Max.x)
          break;

        if (stop.y > node_rect.Max.y)
          break;

        draw_list->AddLine(start, stop, body_color, 3.0f * scale);
      }
    }

    ////////////////////////////////////////////////////////////////////////////////

    if (false == (state_ & ImGuiNodesNodeStateFlag_Collapsed)) {
      for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
        inputs_[input_idx].DrawInput(draw_list, offset, scale, state);

      for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
        outputs_[output_idx].DrawOutput(draw_list, offset, scale, state);
    }

    ////////////////////////////////////////////////////////////////////////////////

    ImGui::SetCursorScreenPos(((area_name_.Min + ImVec2(2, 2)) * scale) +
                              offset);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::Text("%s", name_);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_);

    if (state_ &
        (ImGuiNodesNodeStateFlag_Marked | ImGuiNodesNodeStateFlag_Selected))
      draw_list->AddRectFilled(node_rect.Min, node_rect.Max,
                               ImColor(1.0f, 1.0f, 1.0f, 0.25f), rounding,
                               rounding_corners_flags);

    if (state_ & ImGuiNodesNodeStateFlag_Processing) {
      ImColor processing_color = color_;
      processing_color.Value.x *= 1.5;
      processing_color.Value.y *= 1.5;
      processing_color.Value.z *= 1.5;
      processing_color.Value.w = 1.0f;

      draw_list->AddRect(node_rect.Min - outline, node_rect.Max + outline,
                         processing_color, rounding, rounding_corners_flags,
                         2.0f * scale);
    } else {
      draw_list->AddRect(node_rect.Min - outline * 0.5f,
                         node_rect.Max + outline * 0.5f,
                         ImColor(0.0f, 0.0f, 0.0f, 0.5f), rounding,
                         rounding_corners_flags, 3.0f * scale);
    }
  }

  ImGuiNodesNode(const char *name, ImGuiNodesNodeType type, ImColor color) {
    name_ = name;
    type_ = type;
    state_ = ImGuiNodesNodeStateFlag_Default;
    color_ = color;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name);
    title_height_ = ImGuiNodesTitleHight * area_name_.GetHeight();
  }
};

////////////////////////////////////////////////////////////////////////////////

// ImGuiNodesConnectionDesc size round up to 32 bytes to be cache boundaries
// friendly
constexpr int ImGuiNodesNamesMaxLen = 32 - sizeof(ImGuiNodesConnectorType);

struct ImGuiNodesConnectionDesc {
  char name_[ImGuiNodesNamesMaxLen];
  ImGuiNodesConnectorType type_;
};

// TODO: ImVector me
struct ImGuiNodesNodeDesc {
  char name_[ImGuiNodesNamesMaxLen];
  ImGuiNodesNodeType type_;
  ImColor color_;
  ImVector<ImGuiNodesConnectionDesc> inputs_;
  ImVector<ImGuiNodesConnectionDesc> outputs_;
};

////////////////////////////////////////////////////////////////////////////////

struct ImGuiNodes {
private:
  ImVec2 mouse_;
  ImVec2 pos_;
  ImVec2 size_;
  ImVec2 scroll_;
  ImVec4 connection_;
  float scale_;

  ////////////////////////////////////////////////////////////////////////////////

  ImGuiNodesState state_;

  ImRect area_;
  ImGuiNodesNode *element_node_ = NULL;
  ImGuiNodesInput *element_input_ = NULL;
  ImGuiNodesOutput *element_output_ = NULL;
  ImGuiNodesNode *processing_node_ = NULL;

  ////////////////////////////////////////////////////////////////////////////////

public:
  ImVector<ImGuiNodesNode *> nodes_;
  ImVector<ImGuiNodesNodeDesc> nodes_desc_;

  ////////////////////////////////////////////////////////////////////////////////

  ImGuiNodesNode *CreateNodeFromDesc(ImGuiNodesNodeDesc *desc, ImVec2 pos);

private:
  void UpdateCanvasGeometry(ImDrawList *draw_list);
  ImGuiNodesNode *UpdateNodesFromCanvas();

  inline void DrawConnection(ImVec2 p1, ImVec2 p4, ImColor color) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    float line = 25.0f;

    ImVec2 p2 = p1;
    ImVec2 p3 = p4;

    p2 += (ImVec2(-line, 0.0f) * scale_);
    p3 += (ImVec2(+line, 0.0f) * scale_);

    draw_list->AddBezierCubic(p1, p2, p3, p4, color, 1.5f * scale_);
  }

  inline bool ConnectionMatrix(ImGuiNodesNode *input_node,
                               ImGuiNodesNode *output_node,
                               ImGuiNodesInput *input,
                               ImGuiNodesOutput *output) {
    if (input->target_ && input->target_ == output_node)
      return false;

    if (input->type_ == output->type_)
      return true;

    if (input->type_ == ImGuiNodesConnectorType_Generic)
      return true;

    if (output->type_ == ImGuiNodesConnectorType_Generic)
      return true;

    return false;
  }

  inline bool SortSelectedNodesOrder();

public:
  void Update();
  void ProcessNodes();
  void ProcessContextMenu();

  ImGuiNodes() {
    scale_ = 1.0f;
    state_ = ImGuiNodesState_Default;
    element_node_ = NULL;
    element_input_ = NULL;
    element_output_ = NULL;

    ////////////////////////////////////////////////////////////////////////////////

    {
      ImGuiNodesNodeDesc desc("Test", ImGuiNodesNodeType_Generic,
                              ImColor(0.2, 0.3, 0.6, 0.0f));
      nodes_desc_.push_back(desc);

      desc.inputs_.push_back({"Float", ImGuiNodesConnectorType_Float});
      desc.inputs_.push_back({"Int", ImGuiNodesConnectorType_Int});
      desc.inputs_.push_back({"TextStream", ImGuiNodesConnectorType_Text});

      desc.outputs_.push_back({"Float", ImGuiNodesConnectorType_Float});

      auto &back = nodes_desc_.back();
      back.inputs_ = desc.inputs_;
      back.outputs_ = desc.outputs_;
    }

    {
      ImGuiNodesNodeDesc desc("InputBox", ImGuiNodesNodeType_Generic,
                              ImColor(0.3, 0.5, 0.5, 0.0f));
      nodes_desc_.push_back(desc);

      desc.inputs_.push_back({"Float1", ImGuiNodesConnectorType_Float});
      desc.inputs_.push_back({"Float2", ImGuiNodesConnectorType_Float});
      desc.inputs_.push_back({"Int1", ImGuiNodesConnectorType_Int});
      desc.inputs_.push_back({"Int2", ImGuiNodesConnectorType_Int});
      desc.inputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.inputs_.push_back({"GenericSink", ImGuiNodesConnectorType_Generic});
      desc.inputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.inputs_.push_back({"Vector", ImGuiNodesConnectorType_Vector});
      desc.inputs_.push_back({"Image", ImGuiNodesConnectorType_Image});
      desc.inputs_.push_back({"Text", ImGuiNodesConnectorType_Text});

      desc.outputs_.push_back({"TextStream", ImGuiNodesConnectorType_Text});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"Float", ImGuiNodesConnectorType_Float});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"Int", ImGuiNodesConnectorType_Int});

      auto &back = nodes_desc_.back();
      back.inputs_.swap(desc.inputs_);
      back.outputs_.swap(desc.outputs_);
    }

    {
      ImGuiNodesNodeDesc desc("OutputBox", ImGuiNodesNodeType_Generic,
                              ImColor(0.4, 0.3, 0.5, 0.0f));
      nodes_desc_.push_back(desc);

      desc.inputs_.push_back({"GenericSink1", ImGuiNodesConnectorType_Generic});
      desc.inputs_.push_back({"GenericSink2", ImGuiNodesConnectorType_Generic});
      desc.inputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.inputs_.push_back({"Float", ImGuiNodesConnectorType_Float});
      desc.inputs_.push_back({"Int", ImGuiNodesConnectorType_Int});
      desc.inputs_.push_back({"Text", ImGuiNodesConnectorType_Text});

      desc.outputs_.push_back({"Vector", ImGuiNodesConnectorType_Vector});
      desc.outputs_.push_back({"Image", ImGuiNodesConnectorType_Image});
      desc.outputs_.push_back({"Text", ImGuiNodesConnectorType_Text});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"Float", ImGuiNodesConnectorType_Float});
      desc.outputs_.push_back({"Int", ImGuiNodesConnectorType_Int});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"", ImGuiNodesConnectorType_None});
      desc.outputs_.push_back({"Generic", ImGuiNodesConnectorType_Generic});

      auto &back = nodes_desc_.back();
      back.inputs_.swap(desc.inputs_);
      back.outputs_.swap(desc.outputs_);
    }

    ////////////////////////////////////////////////////////////////////////////////

    return;
  }

  ~ImGuiNodes() {
    for (int desc_idx = 0; desc_idx < nodes_desc_.size(); ++desc_idx) {
      ImGuiNodesNodeDesc &node = nodes_desc_[desc_idx];

      node.inputs_.~ImVector();
      node.outputs_.~ImVector();
    }

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
      delete nodes_[node_idx];
  }
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ImGui

namespace ImGui {
void ImGuiNodes::UpdateCanvasGeometry(ImDrawList *draw_list) {
  const ImGuiIO &io = ImGui::GetIO();

  mouse_ = ImGui::GetMousePos();

  {
    ImVec2 min = ImGui::GetWindowContentRegionMin();
    ImVec2 max = ImGui::GetWindowContentRegionMax();

    pos_ = ImGui::GetWindowPos() + min;
    size_ = max - min;
  }

  ImRect canvas(pos_, pos_ + size_);

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    scroll_ = {};
    scale_ = 1.0f;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (false == ImGui::IsMouseDown(0) && canvas.Contains(mouse_)) {
    if (ImGui::IsMouseDragging(1))
      scroll_ += io.MouseDelta;

    if (io.KeyShift && !io.KeyCtrl)
      scroll_.x += io.MouseWheel * 16.0f;

    if (!io.KeyShift && !io.KeyCtrl)
      scroll_.y += io.MouseWheel * 16.0f;

    if (!io.KeyShift && io.KeyCtrl) {
      ImVec2 focus = (mouse_ - scroll_ - pos_) / scale_;

      if (io.MouseWheel < 0.0f)
        for (float zoom = io.MouseWheel; zoom < 0.0f; zoom += 1.0f)
          scale_ = ImMax(0.3f, scale_ / 1.05f);

      if (io.MouseWheel > 0.0f)
        for (float zoom = io.MouseWheel; zoom > 0.0f; zoom -= 1.0f)
          scale_ = ImMin(3.0f, scale_ * 1.05f);

      ImVec2 shift = scroll_ + (focus * scale_);
      scroll_ += mouse_ - shift - pos_;
    }

    if (ImGui::IsMouseReleased(1) && element_node_ == NULL)
      if (io.MouseDragMaxDistanceSqr[1] <
          (io.MouseDragThreshold * io.MouseDragThreshold)) {
        bool selected = false;

        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx) {
          if (nodes_[node_idx]->state_ & ImGuiNodesNodeStateFlag_Selected) {
            selected = true;
            break;
          }
        }

        if (false == selected)
          ImGui::OpenPopup("NodesContextMenu");
      }
  }

  ////////////////////////////////////////////////////////////////////////////////

  const float grid = 64.0f * scale_;

  int mark_x = (int)(scroll_.x / grid);
  for (float x = fmodf(scroll_.x, grid); x < size_.x;
       x += grid, --mark_x) {
    ImColor color = mark_x % 5 ? ImColor(0.5f, 0.5f, 0.5f, 0.1f)
                               : ImColor(1.0f, 1.0f, 1.0f, 0.1f);
    draw_list->AddLine(ImVec2(x, 0.0f) + pos_, ImVec2(x, size_.y) + pos_, color,
                       0.1f);
  }

  int mark_y = (int)(scroll_.y / grid);
  for (float y = fmodf(scroll_.y, grid); y < size_.y;
       y += grid, --mark_y) {
    ImColor color = mark_y % 5 ? ImColor(0.5f, 0.5f, 0.5f, 0.1f)
                               : ImColor(1.0f, 1.0f, 1.0f, 0.1f);
    draw_list->AddLine(ImVec2(0.0f, y) + pos_, ImVec2(size_.x, y) + pos_, color,
                       0.1f);
  }
}

ImGuiNodesNode *ImGuiNodes::UpdateNodesFromCanvas() {
  if (nodes_.empty())
    return NULL;

  const ImGuiIO &io = ImGui::GetIO();

  ImVec2 offset = pos_ + scroll_;
  ImRect canvas(pos_, pos_ + size_);
  ImGuiNodesNode *hovered_node = NULL;

  for (int node_idx = nodes_.size(); node_idx != 0;) {
    ImGuiNodesNode *node = nodes_[--node_idx];
    IM_ASSERT(node);

    ImRect node_rect = node->area_node_;
    node_rect.Min *= scale_;
    node_rect.Max *= scale_;
    node_rect.Translate(offset);

    node_rect.ClipWith(canvas);

    if (canvas.Overlaps(node_rect)) {
      node->state_ |= ImGuiNodesNodeStateFlag_Visible;
      node->state_ &= ~ImGuiNodesNodeStateFlag_Hovered;
    } else {
      node->state_ &=
          ~(ImGuiNodesNodeStateFlag_Visible | ImGuiNodesNodeStateFlag_Hovered |
            ImGuiNodesNodeStateFlag_Marked);
      continue;
    }

    if (NULL == hovered_node && node_rect.Contains(mouse_))
      hovered_node = node;

    ////////////////////////////////////////////////////////////////////////////////

    if (state_ == ImGuiNodesState_Selecting) {
      if (io.KeyCtrl && area_.Overlaps(node_rect)) {
        node->state_ |= ImGuiNodesNodeStateFlag_Marked;
        continue;
      }

      if (false == io.KeyCtrl && area_.Contains(node_rect)) {
        node->state_ |= ImGuiNodesNodeStateFlag_Marked;
        continue;
      }

      node->state_ &= ~ImGuiNodesNodeStateFlag_Marked;
    }

    ////////////////////////////////////////////////////////////////////////////////

    for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx) {
      ImGuiNodesInput &input = node->inputs_[input_idx];

      if (input.type_ == ImGuiNodesConnectorType_None)
        continue;

      input.state_ &= ~(ImGuiNodesConnectorStateFlag_Hovered |
                        ImGuiNodesConnectorStateFlag_Consider |
                        ImGuiNodesConnectorStateFlag_Draging);

      if (state_ == ImGuiNodesState_DragingInput) {
        if (&input == element_input_)
          input.state_ |= ImGuiNodesConnectorStateFlag_Draging;

        continue;
      }

      if (state_ == ImGuiNodesState_DragingOutput) {
        if (element_node_ == node)
          continue;

        if (ConnectionMatrix(node, element_node_, &input, element_output_))
          input.state_ |= ImGuiNodesConnectorStateFlag_Consider;
      }

      if (!hovered_node || hovered_node != node)
        continue;

      if (state_ == ImGuiNodesState_Selecting)
        continue;

      if (state_ != ImGuiNodesState_DragingOutput &&
          node->state_ & ImGuiNodesNodeStateFlag_Selected)
        continue;

      ImRect input_rect = input.area_input_;
      input_rect.Min *= scale_;
      input_rect.Max *= scale_;
      input_rect.Translate(offset);

      if (input_rect.Contains(mouse_)) {
        if (state_ != ImGuiNodesState_DragingOutput) {
          input.state_ |= ImGuiNodesConnectorStateFlag_Hovered;
          continue;
        }

        if (input.state_ & ImGuiNodesConnectorStateFlag_Consider)
          input.state_ |= ImGuiNodesConnectorStateFlag_Hovered;
      }
    }

    ////////////////////////////////////////////////////////////////////////////////

    for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx) {
      ImGuiNodesOutput &output = node->outputs_[output_idx];

      if (output.type_ == ImGuiNodesConnectorType_None)
        continue;

      output.state_ &= ~(ImGuiNodesConnectorStateFlag_Hovered |
                         ImGuiNodesConnectorStateFlag_Consider |
                         ImGuiNodesConnectorStateFlag_Draging);

      if (state_ == ImGuiNodesState_DragingOutput) {
        if (&output == element_output_)
          output.state_ |= ImGuiNodesConnectorStateFlag_Draging;

        continue;
      }

      if (state_ == ImGuiNodesState_DragingInput) {
        if (element_node_ == node)
          continue;

        if (ConnectionMatrix(element_node_, node, element_input_, &output))
          output.state_ |= ImGuiNodesConnectorStateFlag_Consider;
      }

      if (!hovered_node || hovered_node != node)
        continue;

      if (state_ == ImGuiNodesState_Selecting)
        continue;

      if (state_ != ImGuiNodesState_DragingInput &&
          node->state_ & ImGuiNodesNodeStateFlag_Selected)
        continue;

      ImRect output_rect = output.area_output_;
      output_rect.Min *= scale_;
      output_rect.Max *= scale_;
      output_rect.Translate(offset);

      if (output_rect.Contains(mouse_)) {
        if (state_ != ImGuiNodesState_DragingInput) {
          output.state_ |= ImGuiNodesConnectorStateFlag_Hovered;
          continue;
        }

        if (output.state_ & ImGuiNodesConnectorStateFlag_Consider)
          output.state_ |= ImGuiNodesConnectorStateFlag_Hovered;
      }
    }
  }

  if (hovered_node)
    hovered_node->state_ |= ImGuiNodesNodeStateFlag_Hovered;

  return hovered_node;
}

ImGuiNodesNode *ImGuiNodes::CreateNodeFromDesc(ImGuiNodesNodeDesc *desc,
                                               ImVec2 pos) {
  IM_ASSERT(desc);
  ImGuiNodesNode *node =
      new ImGuiNodesNode(desc->name_, desc->type_, desc->color_);

  ImVec2 inputs;
  ImVec2 outputs;

  ////////////////////////////////////////////////////////////////////////////////

  for (int input_idx = 0; input_idx < desc->inputs_.size(); ++input_idx) {
    ImGuiNodesInput input(desc->inputs_[input_idx].name_,
                          desc->inputs_[input_idx].type_);

    inputs.x = ImMax(inputs.x, input.area_input_.GetWidth());
    inputs.y += input.area_input_.GetHeight();
    node->inputs_.push_back(input);
  }

  for (int output_idx = 0; output_idx < desc->outputs_.size(); ++output_idx) {
    ImGuiNodesOutput output(desc->outputs_[output_idx].name_,
                            desc->outputs_[output_idx].type_);

    outputs.x = ImMax(outputs.x, output.area_output_.GetWidth());
    outputs.y += output.area_output_.GetHeight();
    node->outputs_.push_back(output);
  }

  ////////////////////////////////////////////////////////////////////////////////

  node->BuildNodeGeometry(inputs, outputs);
  node->TranslateNode(pos - node->area_node_.GetCenter());
  node->state_ |= ImGuiNodesNodeStateFlag_Visible |
                  ImGuiNodesNodeStateFlag_Hovered |
                  ImGuiNodesNodeStateFlag_Processing;

  ////////////////////////////////////////////////////////////////////////////////

  if (processing_node_)
    processing_node_->state_ &= ~(ImGuiNodesNodeStateFlag_Processing);

  return processing_node_ = node;
}

bool ImGuiNodes::SortSelectedNodesOrder() {
  bool selected = false;

  ImVector<ImGuiNodesNode *> nodes_unselected;
  nodes_unselected.reserve(nodes_.size());

  ImVector<ImGuiNodesNode *> nodes_selected;
  nodes_selected.reserve(nodes_.size());

  for (ImGuiNodesNode **iterator = nodes_.begin(); iterator != nodes_.end();
       ++iterator) {
    ImGuiNodesNode *node = ((ImGuiNodesNode *)*iterator);

    if (node->state_ & ImGuiNodesNodeStateFlag_Marked ||
        node->state_ & ImGuiNodesNodeStateFlag_Selected) {
      selected = true;
      node->state_ &= ~ImGuiNodesNodeStateFlag_Marked;
      node->state_ |= ImGuiNodesNodeStateFlag_Selected;
      nodes_selected.push_back(node);
    } else
      nodes_unselected.push_back(node);
  }

  int node_idx = 0;

  for (int unselected_idx = 0; unselected_idx < nodes_unselected.size();
       ++unselected_idx)
    nodes_[node_idx++] = nodes_unselected[unselected_idx];

  for (int selected_idx = 0; selected_idx < nodes_selected.size();
       ++selected_idx)
    nodes_[node_idx++] = nodes_selected[selected_idx];

  return selected;
}

void ImGuiNodes::Update() {
  const ImGuiIO &io = ImGui::GetIO();

  UpdateCanvasGeometry(ImGui::GetWindowDrawList());

  ////////////////////////////////////////////////////////////////////////////////

  ImGuiNodesNode *hovered_node = UpdateNodesFromCanvas();

  bool consider_hover = state_ == ImGuiNodesState_Default;
  consider_hover |= state_ == ImGuiNodesState_HoveringNode;
  consider_hover |= state_ == ImGuiNodesState_HoveringInput;
  consider_hover |= state_ == ImGuiNodesState_HoveringOutput;

  ////////////////////////////////////////////////////////////////////////////////

  if (hovered_node && consider_hover) {
    element_input_ = NULL;
    element_output_ = NULL;

    for (int input_idx = 0; input_idx < hovered_node->inputs_.size();
         ++input_idx) {
      if (hovered_node->inputs_[input_idx].state_ &
          ImGuiNodesConnectorStateFlag_Hovered) {
        element_input_ = &hovered_node->inputs_[input_idx];
        state_ = ImGuiNodesState_HoveringInput;
        break;
      }
    }

    for (int output_idx = 0; output_idx < hovered_node->outputs_.size();
         ++output_idx) {
      if (hovered_node->outputs_[output_idx].state_ &
          ImGuiNodesConnectorStateFlag_Hovered) {
        element_output_ = &hovered_node->outputs_[output_idx];
        state_ = ImGuiNodesState_HoveringOutput;
        break;
      }
    }

    if (!element_input_ && !element_output_)
      state_ = ImGuiNodesState_HoveringNode;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (state_ == ImGuiNodesState_DragingInput) {
    element_output_ = NULL;

    if (hovered_node)
      for (int output_idx = 0; output_idx < hovered_node->outputs_.size();
           ++output_idx) {
        ImGuiNodesConnectorState state =
            hovered_node->outputs_[output_idx].state_;

        if (state & ImGuiNodesConnectorStateFlag_Hovered &&
            state & ImGuiNodesConnectorStateFlag_Consider)
          element_output_ = &hovered_node->outputs_[output_idx];
      }
  }

  if (state_ == ImGuiNodesState_DragingOutput) {
    element_input_ = NULL;

    if (hovered_node)
      for (int input_idx = 0; input_idx < hovered_node->inputs_.size();
           ++input_idx) {
        ImGuiNodesConnectorState state =
            hovered_node->inputs_[input_idx].state_;

        if (state & ImGuiNodesConnectorStateFlag_Hovered &&
            state & ImGuiNodesConnectorStateFlag_Consider)
          element_input_ = &hovered_node->inputs_[input_idx];
      }
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (consider_hover) {
    element_node_ = hovered_node;

    if (!hovered_node)
      state_ = ImGuiNodesState_Default;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsMouseDoubleClicked(0)) {
    switch (state_) {
    case ImGuiNodesState_Default: {
      bool selected = false;

      for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx) {
        ImGuiNodesState &state = nodes_[node_idx]->state_;

        if (state & ImGuiNodesNodeStateFlag_Selected)
          selected = true;

        state &=
            ~(ImGuiNodesNodeStateFlag_Selected |
              ImGuiNodesNodeStateFlag_Marked | ImGuiNodesNodeStateFlag_Hovered);
      }

      if (processing_node_ && false == selected) {
        processing_node_->state_ &= ~(ImGuiNodesNodeStateFlag_Processing);
        processing_node_ = NULL;
      }

      return;
    };

    case ImGuiNodesState_HoveringInput: {
      if (element_input_->target_) {
        element_input_->output_->connections_--;
        element_input_->output_ = NULL;
        element_input_->target_ = NULL;

        state_ = ImGuiNodesState_DragingInput;
      }

      return;
    }

    case ImGuiNodesState_HoveringNode: {
      IM_ASSERT(element_node_);

      if (element_node_->state_ & ImGuiNodesNodeStateFlag_Collapsed) {
        element_node_->state_ &= ~ImGuiNodesNodeStateFlag_Collapsed;
        element_node_->area_node_.Max.y += element_node_->body_height_;
        element_node_->TranslateNode(
            ImVec2(0.0f, element_node_->body_height_ * -0.5f));
      } else {
        element_node_->state_ |= ImGuiNodesNodeStateFlag_Collapsed;
        element_node_->area_node_.Max.y -= element_node_->body_height_;

        // const ImVec2 click = (mouse_ - scroll_ - pos_) / scale_;
        // const ImVec2 position = click -
        // element_node_->area_node_.GetCenter();

        element_node_->TranslateNode(
            ImVec2(0.0f, element_node_->body_height_ * 0.5f));
      }

      state_ = ImGuiNodesState_Draging;
      return;
    }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsMouseDoubleClicked(1)) {
    switch (state_) {
    case ImGuiNodesState_HoveringNode: {
      IM_ASSERT(hovered_node);

      if (hovered_node->state_ & ImGuiNodesNodeStateFlag_Disabled)
        hovered_node->state_ &= ~(ImGuiNodesNodeStateFlag_Disabled);
      else
        hovered_node->state_ |= (ImGuiNodesNodeStateFlag_Disabled);

      return;
    }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsMouseClicked(0)) {
    switch (state_) {
    case ImGuiNodesState_HoveringNode: {
      if (io.KeyCtrl)
        element_node_->state_ ^= ImGuiNodesNodeStateFlag_Selected;

      if (io.KeyShift)
        element_node_->state_ |= ImGuiNodesNodeStateFlag_Selected;

      bool selected = element_node_->state_ & ImGuiNodesNodeStateFlag_Selected;

      if (false == selected) {
        if (processing_node_)
          processing_node_->state_ &= ~(ImGuiNodesNodeStateFlag_Processing);

        element_node_->state_ |= ImGuiNodesNodeStateFlag_Processing;
        processing_node_ = element_node_;

        IM_ASSERT(false == nodes_.empty());

        if (nodes_.back() != element_node_) {
          ImGuiNodesNode **iterator = nodes_.find(element_node_);
          nodes_.erase(iterator);
          nodes_.push_back(element_node_);
        }
      } else
        SortSelectedNodesOrder();

      state_ = ImGuiNodesState_Draging;
      return;
    }

    case ImGuiNodesState_HoveringInput: {
      if (!element_input_->target_)
        state_ = ImGuiNodesState_DragingInput;
      else
        state_ = ImGuiNodesState_Draging;

      return;
    }

    case ImGuiNodesState_HoveringOutput: {
      state_ = ImGuiNodesState_DragingOutput;
      return;
    }
    }

    return;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsMouseDragging(0)) {
    switch (state_) {
    case ImGuiNodesState_Default: {
      ImRect canvas(pos_, pos_ + size_);
      if (false == canvas.Contains(mouse_))
        return;

      if (false == io.KeyShift)
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
          nodes_[node_idx]->state_ &= ~(ImGuiNodesNodeStateFlag_Selected |
                                        ImGuiNodesNodeStateFlag_Marked);

      state_ = ImGuiNodesState_Selecting;
      return;
    }

    case ImGuiNodesState_Selecting: {
      const ImVec2 pos = mouse_ - ImGui::GetMouseDragDelta(0);

      area_.Min = ImMin(pos, mouse_);
      area_.Max = ImMax(pos, mouse_);

      return;
    }

    case ImGuiNodesState_Draging: {
      if (element_input_ && element_input_->output_ &&
          element_input_->output_->connections_ > 0)
        return;

      // Only drag nodes when mouse is inside canvas
      ImRect canvas(pos_, pos_ + size_);
      if (false == canvas.Contains(mouse_))
        return;

      if (false == (element_node_->state_ & ImGuiNodesNodeStateFlag_Selected))
        element_node_->TranslateNode(io.MouseDelta / scale_, false);
      else
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
          nodes_[node_idx]->TranslateNode(io.MouseDelta / scale_, true);

      return;
    }

    case ImGuiNodesState_DragingInput: {
      ImVec2 offset = pos_ + scroll_;
      ImVec2 p1 = offset + (element_input_->pos_ * scale_);
      ImVec2 p4 = element_output_ ? (offset + (element_output_->pos_ * scale_))
                                  : mouse_;

      connection_ = ImVec4(p1.x, p1.y, p4.x, p4.y);
      return;
    }

    case ImGuiNodesState_DragingOutput: {
      ImVec2 offset = pos_ + scroll_;
      ImVec2 p1 = offset + (element_output_->pos_ * scale_);
      ImVec2 p4 =
          element_input_ ? (offset + (element_input_->pos_ * scale_)) : mouse_;

      connection_ = ImVec4(p4.x, p4.y, p1.x, p1.y);
      return;
    }
    }

    return;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsMouseReleased(0)) {
    switch (state_) {
    case ImGuiNodesState_Selecting: {
      element_node_ = NULL;
      element_input_ = NULL;
      element_output_ = NULL;

      area_ = {};

      ////////////////////////////////////////////////////////////////////////////////

      SortSelectedNodesOrder();
      state_ = ImGuiNodesState_Default;
      return;
    }

    case ImGuiNodesState_Draging: {
      state_ = ImGuiNodesState_HoveringNode;
      return;
    }

    case ImGuiNodesState_DragingInput:
    case ImGuiNodesState_DragingOutput: {
      if (element_input_ && element_output_) {
        IM_ASSERT(hovered_node);
        IM_ASSERT(element_node_);
        element_input_->target_ = state_ == ImGuiNodesState_DragingInput
                                      ? hovered_node
                                      : element_node_;

        if (element_input_->output_)
          element_input_->output_->connections_--;

        element_input_->output_ = element_output_;
        element_output_->connections_++;
      }

      connection_ = ImVec4();
      state_ = ImGuiNodesState_Default;
      return;
    }
    }

    return;
  }

  ////////////////////////////////////////////////////////////////////////////////

  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    ImVector<ImGuiNodesNode *> nodes;
    nodes.reserve(nodes_.size());

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx) {
      ImGuiNodesNode *node = nodes_[node_idx];
      IM_ASSERT(node);

      if (node->state_ & ImGuiNodesNodeStateFlag_Selected) {
        element_node_ = NULL;
        element_input_ = NULL;
        element_output_ = NULL;

        state_ = ImGuiNodesState_Default;

        for (int sweep_idx = 0; sweep_idx < nodes_.size(); ++sweep_idx) {
          ImGuiNodesNode *sweep = nodes_[sweep_idx];
          IM_ASSERT(sweep);

          for (int input_idx = 0; input_idx < sweep->inputs_.size();
               ++input_idx) {
            ImGuiNodesInput &input = sweep->inputs_[input_idx];

            if (node == input.target_) {
              if (input.output_)
                input.output_->connections_--;

              input.target_ = NULL;
              input.output_ = NULL;
            }
          }
        }

        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx) {
          ImGuiNodesInput &input = node->inputs_[input_idx];

          if (input.output_)
            input.output_->connections_--;

          input.type_ = ImGuiNodesNodeType_None;
          input.name_ = NULL;
          input.target_ = NULL;
          input.output_ = NULL;
        }

        for (int output_idx = 0; output_idx < node->outputs_.size();
             ++output_idx) {
          ImGuiNodesOutput &output = node->outputs_[output_idx];
          IM_ASSERT(output.connections_ == 0);
        }

        if (node == processing_node_)
          processing_node_ = NULL;

        delete node;
      } else {
        nodes.push_back(node);
      }
    }

    nodes_ = nodes;

    return;
  }
}

void ImGuiNodes::ProcessNodes() {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  const ImVec2 offset = pos_ + scroll_;

  ////////////////////////////////////////////////////////////////////////////////

  ImGui::SetWindowFontScale(scale_);

  for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx) {
    const ImGuiNodesNode *node = nodes_[node_idx];
    IM_ASSERT(node);

    for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx) {
      const ImGuiNodesInput &input = node->inputs_[input_idx];

      if (const ImGuiNodesNode *target = input.target_) {
        IM_ASSERT(target);

        ImVec2 p1 = offset;
        ImVec2 p4 = offset;

        if (node->state_ & ImGuiNodesNodeStateFlag_Collapsed) {
          ImVec2 collapsed_input = {
              0, (node->area_node_.Max.y - node->area_node_.Min.y) * 0.5f};

          p1 += ((node->area_node_.Min + collapsed_input) * scale_);
        } else {
          p1 += (input.pos_ * scale_);
        }

        if (target->state_ & ImGuiNodesNodeStateFlag_Collapsed) {
          ImVec2 collapsed_output = {
              0, (target->area_node_.Max.y - target->area_node_.Min.y) * 0.5f};

          p4 += ((target->area_node_.Max - collapsed_output) * scale_);
        } else {
          p4 += (input.output_->pos_ * scale_);
        }

        DrawConnection(p1, p4, ImColor(1.0f, 1.0f, 1.0f, 1.0f));
      }
    }
  }

  for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx) {
    const ImGuiNodesNode *node = nodes_[node_idx];
    IM_ASSERT(node);
    node->DrawNode(draw_list, offset, scale_, state_);
  }

  if (connection_.x != connection_.z && connection_.y != connection_.w)
    DrawConnection(ImVec2(connection_.x, connection_.y),
                   ImVec2(connection_.z, connection_.w),
                   ImColor(0.0f, 1.0f, 0.0f, 1.0f));

  ImGui::SetWindowFontScale(1.0f);

  ////////////////////////////////////////////////////////////////////////////////

  if (state_ == ImGuiNodesState_Selecting) {
    draw_list->AddRectFilled(area_.Min, area_.Max,
                             ImColor(1.0f, 1.0f, 0.0f, 0.1f));
    draw_list->AddRect(area_.Min, area_.Max, ImColor(1.0f, 1.0f, 0.0f, 0.5f));
  }

  ////////////////////////////////////////////////////////////////////////////////

  ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
  ImGui::NewLine();

  switch (state_) {
  case ImGuiNodesState_Default:
    ImGui::Text("ImGuiNodesState_Default");
    break;
  case ImGuiNodesState_HoveringNode:
    ImGui::Text("ImGuiNodesState_HoveringNode");
    break;
  case ImGuiNodesState_HoveringInput:
    ImGui::Text("ImGuiNodesState_HoveringInput");
    break;
  case ImGuiNodesState_HoveringOutput:
    ImGui::Text("ImGuiNodesState_HoveringOutput");
    break;
  case ImGuiNodesState_Draging:
    ImGui::Text("ImGuiNodesState_Draging");
    break;
  case ImGuiNodesState_DragingInput:
    ImGui::Text("ImGuiNodesState_DragingInput");
    break;
  case ImGuiNodesState_DragingOutput:
    ImGui::Text("ImGuiNodesState_DragingOutput");
    break;
  case ImGuiNodesState_Selecting:
    ImGui::Text("ImGuiNodesState_Selecting");
    break;
  default:
    ImGui::Text("UNKNOWN");
    break;
  }

  ImGui::NewLine();

  ImGui::Text("Position: %.2f, %.2f", pos_.x, pos_.y);
  ImGui::Text("Size: %.2f, %.2f", size_.x, size_.y);
  ImGui::Text("Mouse: %.2f, %.2f", mouse_.x, mouse_.y);
  ImGui::Text("Scroll: %.2f, %.2f", scroll_.x, scroll_.y);
  ImGui::Text("Scale: %.2f", scale_);

  ImGui::NewLine();

  if (element_node_)
    ImGui::Text("Element_node: %s", element_node_->name_);
  if (element_input_)
    ImGui::Text("Element_input: %s", element_input_->name_);
  if (element_output_)
    ImGui::Text("Element_output: %s", element_output_->name_);

  ////////////////////////////////////////////////////////////////////////////////
}

void ImGuiNodes::ProcessContextMenu() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

  if (ImGui::BeginPopup("NodesContextMenu")) {
    for (int node_idx = 0; node_idx < nodes_desc_.size(); ++node_idx)
      if (ImGui::MenuItem(nodes_desc_[node_idx].name_)) {
        ImVec2 position = (mouse_ - scroll_ - pos_) / scale_;
        ImGuiNodesNode *node =
            CreateNodeFromDesc(&nodes_desc_[node_idx], position);
        nodes_.push_back(node);
      }

    ImGui::EndPopup();
  }

  ImGui::PopStyleVar();
}

} // namespace ImGui

// ============================================================================
// NodesWidget - Widget Plugin Wrapper for ImGuiNodes
// ============================================================================

namespace ymery::plugins {

class NodesWidget : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<NodesWidget>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("NodesWidget::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        _initialized = false;
        return Ok();
    }

protected:
    Result<void> _begin_container() override {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##NodesCanvas", avail, true, 
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::EndChild();
            _container_open = false;
        }
        return Ok();
    }

    Result<void> _post_render_head() override {
        // Create nodes on first frame when ImGui context is fully active
        if (!_initialized) {
            _initialized = true;
            
            // Create 4 nodes
            auto* node1 = editor_.CreateNodeFromDesc(&editor_.nodes_desc_[0], ImVec2(100, 100));
            if (node1) editor_.nodes_.push_back(node1);
            
            auto* node2 = editor_.CreateNodeFromDesc(&editor_.nodes_desc_[1], ImVec2(100, 300));
            if (node2) editor_.nodes_.push_back(node2);
            
            auto* node3 = editor_.CreateNodeFromDesc(&editor_.nodes_desc_[2], ImVec2(400, 200));
            if (node3) editor_.nodes_.push_back(node3);
            
            auto* node4 = editor_.CreateNodeFromDesc(&editor_.nodes_desc_[0], ImVec2(400, 400));
            if (node4) editor_.nodes_.push_back(node4);

            // Create connections
            if (node1 && node3 && node1->outputs_.size() > 0 && node3->inputs_.size() > 0) {
                node3->inputs_[0].target_ = node1;
                node3->inputs_[0].output_ = &node1->outputs_[0];
                node1->outputs_[0].connections_++;
            }

            if (node2 && node3 && node2->outputs_.size() > 0 && node3->inputs_.size() > 1) {
                node3->inputs_[1].target_ = node2;
                node3->inputs_[1].output_ = &node2->outputs_[0];
                node2->outputs_[0].connections_++;
            }

            if (node3 && node4 && node3->outputs_.size() > 0 && node4->inputs_.size() > 0) {
                node4->inputs_[0].target_ = node3;
                node4->inputs_[0].output_ = &node3->outputs_[0];
                node3->outputs_[0].connections_++;
            }
        }

        editor_.Update();
        editor_.ProcessNodes();
        editor_.ProcessContextMenu();
        
        // Capture mouse events to prevent parent window from moving
        ImVec2 canvasMin = ImGui::GetWindowContentRegionMin() + ImGui::GetWindowPos();
        ImVec2 canvasMax = ImGui::GetWindowContentRegionMax() + ImGui::GetWindowPos();
        if (ImGui::IsMouseHoveringRect(canvasMin, canvasMax)) {
            if (ImGui::IsMouseDragging(0) || ImGui::IsMouseDragging(1) || ImGui::IsMouseDragging(2)) {
                ImGui::SetWindowFocus();
            }
        }
        
        return Ok();
    }

public:
    ImGui::ImGuiNodes editor_;
    bool _container_open = false;
    bool _initialized = false;
};

} // namespace ymery::plugins
