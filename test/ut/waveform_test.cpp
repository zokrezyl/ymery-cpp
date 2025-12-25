// Waveform plugin unit tests
#include <boost/ut.hpp>
#include "ymery/plugin_manager.hpp"
#include "ymery/dispatcher.hpp"
#include "ymery/types.hpp"
#include "ymery/backend/audio_buffer.hpp"

using namespace boost::ut;
using namespace ymery;

// Tests run from build directory, plugins are in ./plugins
static const char* PLUGINS_PATH = "plugins";

suite waveform_tests = [] {
    "waveform_get_children_names_root"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value()) << "Waveform creation failed: " << error_msg(waveform_res);
        auto waveform = *waveform_res;

        // Root should have "available" and "opened"
        auto children_res = waveform->get_children_names(DataPath("/"));
        expect(children_res.has_value()) << "get_children_names('/') failed";

        auto children = *children_res;
        expect(children.size() == 2_ul) << "Expected 2 children, got " << children.size();

        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();

        expect(has_available) << "Missing 'available'";
        expect(has_opened) << "Missing 'opened'";
    };

    "waveform_get_children_names_available"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value());
        auto waveform = *waveform_res;

        // /available should have sine, square, triangle
        auto children_res = waveform->get_children_names(DataPath("/available"));
        expect(children_res.has_value()) << "get_children_names('/available') failed";

        auto children = *children_res;
        expect(children.size() == 3_ul) << "Expected 3 waveform types";

        bool has_sine = std::find(children.begin(), children.end(), "sine") != children.end();
        bool has_square = std::find(children.begin(), children.end(), "square") != children.end();
        bool has_triangle = std::find(children.begin(), children.end(), "triangle") != children.end();

        expect(has_sine) << "Missing 'sine'";
        expect(has_square) << "Missing 'square'";
        expect(has_triangle) << "Missing 'triangle'";
    };

    "waveform_get_children_names_opened"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value());
        auto waveform = *waveform_res;

        // /opened should have auto-started devices (sine, square, triangle)
        auto children_res = waveform->get_children_names(DataPath("/opened"));
        expect(children_res.has_value()) << "get_children_names('/opened') failed";

        auto children = *children_res;
        // Waveform auto-starts all three devices
        expect(children.size() == 3_ul) << "Expected 3 opened devices, got " << children.size();
    };

    "waveform_get_metadata_channel"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value());
        auto waveform = *waveform_res;

        // /opened/sine/0 metadata should have buffer
        auto meta_res = waveform->get_metadata(DataPath("/opened/sine/0"));
        expect(meta_res.has_value()) << "get_metadata('/opened/sine/0') failed: " << error_msg(meta_res);

        auto meta = *meta_res;
        expect(meta.find("buffer") != meta.end()) << "Missing 'buffer' in channel metadata";
    };

    "waveform_get_buffer"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value());
        auto waveform = *waveform_res;

        // get /opened/sine/0/buffer should return MediatedAudioBuffer
        auto buffer_res = waveform->get(DataPath("/opened/sine/0/buffer"));
        expect(buffer_res.has_value()) << "get('/opened/sine/0/buffer') failed: " << error_msg(buffer_res);

        auto buffer_val = *buffer_res;
        expect(buffer_val.has_value()) << "Buffer value is empty";

        // Should be a MediatedAudioBufferPtr
        auto buffer_ptr = get_as<MediatedAudioBufferPtr>(buffer_val);
        expect(buffer_ptr.has_value()) << "Buffer is not MediatedAudioBufferPtr";
    };
};

int main() {
    return 0;
}
