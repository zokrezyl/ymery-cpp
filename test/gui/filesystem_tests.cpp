// Filesystem demo tests using imgui_test_engine
// Tests the filesystem tree-like plugin integration

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

#include "ymery/plugins/backend/filesystem/common.hpp"
#include <ytrace/ytrace.hpp>
#include <filesystem>

void RegisterFilesystemTests(ImGuiTestEngine* engine) {
    ImGuiTest* t = nullptr;

    //-------------------------------------------------------------------------
    // Test: Filesystem plugin get_children_names works for /available
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "filesystem", "get_children_available");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Testing filesystem plugin");
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        // Create filesystem manager directly
        auto fs_res = ymery::plugins::FilesystemManager::create();
        IM_CHECK(fs_res.has_value());

        auto fs = *fs_res;

        // Test get_children_names for root
        auto root_children = fs->get_children_names(ymery::DataPath("/"));
        IM_CHECK(root_children.has_value());
        yinfo("Root children: {}", (*root_children).size());
        for (const auto& c : *root_children) {
            yinfo("  - {}", c);
        }
        IM_CHECK_EQ((*root_children).size(), 2u);  // available, opened

        // Test get_children_names for /available
        auto avail_children = fs->get_children_names(ymery::DataPath("/available"));
        IM_CHECK(avail_children.has_value());
        yinfo("Available children: {}", (*avail_children).size());
        for (const auto& c : *avail_children) {
            yinfo("  - {}", c);
        }
        IM_CHECK_EQ((*avail_children).size(), 4u);  // fs-root, home, mounts, bookmarks
    };

    //-------------------------------------------------------------------------
    // Test: Filesystem plugin metadata works
    //-------------------------------------------------------------------------
    t = IM_REGISTER_TEST(engine, "filesystem", "get_metadata");
    t->GuiFunc = [](ImGuiTestContext* ctx) {
        ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Testing filesystem metadata");
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
        auto fs_res = ymery::plugins::FilesystemManager::create();
        IM_CHECK(fs_res.has_value());

        auto fs = *fs_res;

        // Test metadata for /available
        auto meta = fs->get_metadata(ymery::DataPath("/available"));
        IM_CHECK(meta.has_value());

        auto& m = *meta;
        yinfo("Metadata for /available:");
        for (const auto& [k, v] : m) {
            if (auto s = ymery::get_as<std::string>(v)) {
                yinfo("  {}: {}", k, *s);
            }
        }

        // Check it has expected fields
        IM_CHECK(m.find("name") != m.end());
        IM_CHECK(m.find("type") != m.end());

        // Test metadata for /available/fs-root
        auto fsroot_meta = fs->get_metadata(ymery::DataPath("/available/fs-root"));
        IM_CHECK(fsroot_meta.has_value());

        auto& fm = *fsroot_meta;
        yinfo("Metadata for /available/fs-root:");
        for (const auto& [k, v] : fm) {
            if (auto s = ymery::get_as<std::string>(v)) {
                yinfo("  {}: {}", k, *s);
            }
        }
    };
}
