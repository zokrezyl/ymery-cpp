// Tree Node widget tests using imgui_test_engine
// These are simplified tests that verify tree node behavior with pure ImGui

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

void RegisterTreeNodeTests(ImGuiTestEngine* engine) {
    ImGuiTest* t = nullptr;

    //-------------------------------------------------------------------------
    // Test: Basic ImGui TreeNode works
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "tree_node", "imgui_basic");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::TreeNode("Node A")) {
            ImGui::Text("Child A1");
            ImGui::Text("Child A2");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Node B")) {
            ImGui::Text("Child B1");
            ImGui::TreePop();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("Test Window");

        // Verify nodes exist
        IM_CHECK(ctx->ItemExists("Node A"));
        IM_CHECK(ctx->ItemExists("Node B"));

        // Initially closed
        IM_CHECK(!ctx->ItemIsOpened("Node A"));

        // Open Node A
        ctx->ItemOpen("Node A");
        IM_CHECK(ctx->ItemIsOpened("Node A"));

        // Close it
        ctx->ItemClose("Node A");
        IM_CHECK(!ctx->ItemIsOpened("Node A"));
    };

    //-------------------------------------------------------------------------
    // Test: TreeNode expansion persists across frames
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "tree_node", "expansion_persists");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::TreeNode("Persistent Node")) {
            ImGui::Text("Content");
            ImGui::TreePop();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("Test Window");

        // Open node
        ctx->ItemOpen("Persistent Node");
        IM_CHECK(ctx->ItemIsOpened("Persistent Node"));

        // Wait several frames
        ctx->Yield(10);

        // Node should still be open
        IM_CHECK(ctx->ItemIsOpened("Persistent Node"));
    };

    //-------------------------------------------------------------------------
    // Test: TreeNode can be closed
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "tree_node", "can_close");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::TreeNode("Closeable Node")) {
            ImGui::Text("Content");
            ImGui::TreePop();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("Test Window");

        // Open then close
        ctx->ItemOpen("Closeable Node");
        IM_CHECK(ctx->ItemIsOpened("Closeable Node"));

        ctx->ItemClose("Closeable Node");
        IM_CHECK(!ctx->ItemIsOpened("Closeable Node"));
    };

    //-------------------------------------------------------------------------
    // Test: Multiple tree nodes can be opened
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "tree_node", "multiple_open");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::TreeNode("Multi A")) {
            ImGui::Text("A Content");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Multi B")) {
            ImGui::Text("B Content");
            ImGui::TreePop();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("Test Window");

        // Open both
        ctx->ItemOpen("Multi A");
        ctx->ItemOpen("Multi B");

        // Both should be open
        IM_CHECK(ctx->ItemIsOpened("Multi A"));
        IM_CHECK(ctx->ItemIsOpened("Multi B"));
    };

    //-------------------------------------------------------------------------
    // Test: Nested tree nodes
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "tree_node", "nested");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::TreeNode("Parent")) {
            if (ImGui::TreeNode("Child")) {
                ImGui::Text("Grandchild");
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("Test Window");

        // Open parent
        ctx->ItemOpen("Parent");
        IM_CHECK(ctx->ItemIsOpened("Parent"));

        // Open child
        ctx->ItemOpen("Parent/Child");
        IM_CHECK(ctx->ItemIsOpened("Parent/Child"));
    };
}
