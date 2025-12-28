// Basic widget tests using imgui_test_engine
// These are simplified tests that verify basic widget behavior with pure ImGui

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

// Shared test state
struct BasicWidgetVars {
    bool button_clicked = false;
    bool checkbox_value = false;
    int slider_value = 50;
    char input_text[256] = "initial";
    int combo_index = 0;
};

void RegisterBasicWidgetTests(ImGuiTestEngine* engine) {
    ImGuiTest* t = nullptr;

    //-------------------------------------------------------------------------
    // Test: Button can be clicked
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "button_click");
    t->SetVarsDataType<BasicWidgetVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::Button("Test Button")) {
            vars.button_clicked = true;
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ctx->SetRef("Test Window");

        IM_CHECK(!vars.button_clicked);
        ctx->ItemClick("Test Button");
        IM_CHECK(vars.button_clicked);
    };

    //-------------------------------------------------------------------------
    // Test: Checkbox can be toggled
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "checkbox_toggle");
    t->SetVarsDataType<BasicWidgetVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::Checkbox("Test Checkbox", &vars.checkbox_value);
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ctx->SetRef("Test Window");

        IM_CHECK(!vars.checkbox_value);

        ctx->ItemCheck("Test Checkbox");
        IM_CHECK(vars.checkbox_value);
        IM_CHECK(ctx->ItemIsChecked("Test Checkbox"));

        ctx->ItemUncheck("Test Checkbox");
        IM_CHECK(!vars.checkbox_value);
        IM_CHECK(!ctx->ItemIsChecked("Test Checkbox"));
    };

    //-------------------------------------------------------------------------
    // Test: Slider can be manipulated
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "slider_input");
    t->SetVarsDataType<BasicWidgetVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SliderInt("Test Slider", &vars.slider_value, 0, 100);
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ctx->SetRef("Test Window");

        IM_CHECK_EQ(vars.slider_value, 50);
        ctx->ItemInputValue("Test Slider", 75);
        IM_CHECK_EQ(vars.slider_value, 75);
    };

    //-------------------------------------------------------------------------
    // Test: Input text can be edited
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "input_text");
    t->SetVarsDataType<BasicWidgetVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::InputText("Test Input", vars.input_text, sizeof(vars.input_text));
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ctx->SetRef("Test Window");

        ctx->ItemInput("Test Input");
        ctx->KeyCharsReplace("new text");

        IM_CHECK_STR_EQ(vars.input_text, "new text");
    };

    //-------------------------------------------------------------------------
    // Test: Combo box selection
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "combo_select");
    t->SetVarsDataType<BasicWidgetVars>();
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        const char* items[] = { "Option A", "Option B", "Option C" };
        ImGui::Combo("Test Combo", &vars.combo_index, items, 3);
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        BasicWidgetVars& vars = ctx->GetVars<BasicWidgetVars>();
        ctx->SetRef("Test Window");

        IM_CHECK_EQ(vars.combo_index, 0);

        // Open combo and select option B
        ctx->ComboClick("Test Combo/Option B");
        IM_CHECK_EQ(vars.combo_index, 1);
    };

    //-------------------------------------------------------------------------
    // Test: Window renders correctly
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "basic_widgets", "window_renders");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("My Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Hello World");
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        auto* window = ctx->GetWindowByRef("My Window");
        IM_CHECK(window != nullptr);
        IM_CHECK(window->Active);
    };
}
