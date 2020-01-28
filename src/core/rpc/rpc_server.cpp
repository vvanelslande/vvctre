// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <httplib.h>
#include <json.hpp>
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/stb_image_write.h"
#include "common/thread.h"
#include "common/version.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/nfc/nfc_u.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/rpc/rpc_server.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

void to_json(nlohmann::json& json, const InputProfile& profile) {
    json = nlohmann::json{
        {"name", profile.name},
        {"buttons", profile.buttons},
        {"analogs", profile.analogs},
        {"motion_device", profile.motion_device},
        {"touch_device", profile.touch_device},
        {"udp_input_address", profile.udp_input_address},
        {"udp_input_port", profile.udp_input_port},
        {"udp_pad_index", profile.udp_pad_index},
    };
}

void from_json(const nlohmann::json& json, InputProfile& profile) {
    json.at("name").get_to(profile.name);
    json.at("buttons").get_to(profile.buttons);
    json.at("analogs").get_to(profile.analogs);
    json.at("motion_device").get_to(profile.motion_device);
    json.at("touch_device").get_to(profile.touch_device);
    json.at("udp_input_address").get_to(profile.udp_input_address);
    json.at("udp_input_port").get_to(profile.udp_input_port);
    json.at("udp_pad_index").get_to(profile.udp_pad_index);
}

} // namespace Settings

namespace Common {

void to_json(nlohmann::json& json, const Vec3<float>& v) {
    json = nlohmann::json{
        {"x", v.x},
        {"y", v.y},
        {"z", v.z},
    };
}

void from_json(const nlohmann::json& json, Vec3<float>& v) {
    json.at("x").get_to(v.x);
    json.at("y").get_to(v.y);
    json.at("z").get_to(v.z);
}

} // namespace Common

namespace RPC {

RPCServer::RPCServer(const int port) {
    server = std::make_unique<httplib::Server>();

    server->Get("/version", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"vvctre", version::vvctre.to_string()},
                {"movie", version::movie},
                {"shader_cache", version::shader_cache},
            }
                .dump(),
            "application/json");
    });

    server->Post("/memory/read", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::size_t size = json["size"].get<std::size_t>();

            std::vector<u8> data(size);

            // Note: Memory read occurs asynchronously from the state of the emulator
            Core::System::GetInstance().Memory().ReadBlock(
                *Core::System::GetInstance().Kernel().GetCurrentProcess(), address, &data[0], size);

            res.set_content(nlohmann::json(data).dump(), "application/json");
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/memory/write", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::vector<u8> data = json["data"].get<std::vector<u8>>();

            // Note: Memory write occurs asynchronously from the state of the emulator
            Core::System::GetInstance().Memory().WriteBlock(
                *Core::System::GetInstance().Kernel().GetCurrentProcess(), address, &data[0],
                data.size());

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/padstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(Core::System::GetInstance());
        const Service::HID::PadState state = hid->GetPadState();

        res.set_content(
            nlohmann::json{
                {"hex", state.hex},
                {"a", static_cast<bool>(state.a)},
                {"b", static_cast<bool>(state.b)},
                {"select", static_cast<bool>(state.select)},
                {"start", static_cast<bool>(state.start)},
                {"right", static_cast<bool>(state.right)},
                {"left", static_cast<bool>(state.left)},
                {"up", static_cast<bool>(state.up)},
                {"down", static_cast<bool>(state.down)},
                {"r", static_cast<bool>(state.r)},
                {"l", static_cast<bool>(state.l)},
                {"x", static_cast<bool>(state.x)},
                {"y", static_cast<bool>(state.y)},
                {"debug", static_cast<bool>(state.debug)},
                {"gpio14", static_cast<bool>(state.gpio14)},
                {"circle_right", static_cast<bool>(state.circle_right)},
                {"circle_left", static_cast<bool>(state.circle_left)},
                {"circle_up", static_cast<bool>(state.circle_up)},
                {"circle_down", static_cast<bool>(state.circle_down)},
            }
                .dump(),
            "application/json");
    });

    server->Post("/padstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("hex")) {
                Service::HID::PadState state;
                state.hex = json["hex"].get<u32>();
                hid->SetCustomPadState(state);

                res.set_content(
                    nlohmann::json{
                        {"a", static_cast<bool>(state.a)},
                        {"b", static_cast<bool>(state.b)},
                        {"select", static_cast<bool>(state.select)},
                        {"start", static_cast<bool>(state.start)},
                        {"right", static_cast<bool>(state.right)},
                        {"left", static_cast<bool>(state.left)},
                        {"up", static_cast<bool>(state.up)},
                        {"down", static_cast<bool>(state.down)},
                        {"r", static_cast<bool>(state.r)},
                        {"l", static_cast<bool>(state.l)},
                        {"x", static_cast<bool>(state.x)},
                        {"y", static_cast<bool>(state.y)},
                        {"debug", static_cast<bool>(state.debug)},
                        {"gpio14", static_cast<bool>(state.gpio14)},
                        {"circle_right", static_cast<bool>(state.circle_right)},
                        {"circle_left", static_cast<bool>(state.circle_left)},
                        {"circle_up", static_cast<bool>(state.circle_up)},
                        {"circle_down", static_cast<bool>(state.circle_down)},
                    }
                        .dump(),
                    "application/json");
            } else {
                hid->SetCustomPadState(std::nullopt);
                res.status = 204;
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/circlepadstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(Core::System::GetInstance());
        const auto [x, y] = hid->GetCirclePadState();

        res.set_content(
            nlohmann::json{
                {"x", x},
                {"y", y},
            }
                .dump(),
            "application/json");
    });

    server->Post("/circlepadstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("x") && json.contains("y")) {
                hid->SetCustomCirclePadState(
                    std::make_tuple(json["x"].get<float>(), json["y"].get<float>()));
            } else {
                hid->SetCustomCirclePadState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/touchstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(Core::System::GetInstance());
        const auto [x, y, pressed] = hid->GetTouchState();

        res.set_content(
            nlohmann::json{
                {"x", x},
                {"y", y},
                {"pressed", pressed},
            }
                .dump(),
            "application/json");
    });

    server->Post("/touchstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("x") && json.contains("y") && json.contains("pressed")) {
                hid->SetCustomTouchState(std::make_tuple(
                    json["x"].get<float>(), json["y"].get<float>(), json["pressed"].get<bool>()));
            } else {
                hid->SetCustomTouchState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/motionstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(Core::System::GetInstance());
        const auto [accel, gyro] = hid->GetMotionState();

        res.set_content(
            nlohmann::json{
                {"accel", accel},
                {"gyro", gyro},
            }
                .dump(),
            "application/json");
    });

    server->Post("/motionstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("accel") && json.contains("gyro")) {
                hid->SetCustomMotionState(std::make_tuple(json["accel"].get<Common::Vec3<float>>(),
                                                          json["gyro"].get<Common::Vec3<float>>()));
            } else {
                hid->SetCustomMotionState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/screenshot", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        if (VideoCore::g_renderer == nullptr) {
            res.status = 503;
            res.set_content("booting", "text/plain");
            return;
        }

        const Layout::FramebufferLayout& layout =
            VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();

        Common::Event done;
        std::vector<u8> data(layout.width * layout.height * 4);
        if (VideoCore::RequestScreenshot(data.data(), [&] { done.Set(); }, layout)) {
            res.status = 503;
            res.set_content("another screenshot is pending", "text/plain");
            return;
        }
        done.Wait();

        // Rotate the image to put the pixels in correct order
        // (As OpenGL returns pixel data starting from the lowest position)
        const auto rotate = [](const std::vector<u8>& input,
                               const Layout::FramebufferLayout& layout) {
            std::vector<u8> output(input.size());

            for (std::size_t i = 0; i < layout.height; i++) {
                for (std::size_t j = 0; j < layout.width; j++) {
                    for (std::size_t k = 0; k < 4; k++) {
                        output[i * (layout.width * 4) + j * 4 + k] =
                            input[(layout.height - i - 1) * (layout.width * 4) + j * 4 + k];
                    }
                }
            }

            return output;
        };

        const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                             const Layout::FramebufferLayout& layout) {
            int offset = 0;
            std::vector<u8> output(input.size());

            for (int y = 0; y < layout.height; y++) {
                for (int x = 0; x < layout.width; x++) {
                    output[offset] = input[offset + 2];
                    output[offset + 1] = input[offset + 1];
                    output[offset + 2] = input[offset];
                    output[offset + 3] = input[offset + 3];

                    offset += 4;
                }
            }

            return output;
        };

        data = convert_bgra_to_rgba(rotate(data, layout), layout);

        std::vector<u8> out;
        stbi_write_func* f = [](void* context, void* data, int size) {
            std::vector<u8>* out = static_cast<std::vector<u8>*>(context);
            out->resize(size);
            std::memcpy(out->data(), data, size);
        };
        if (stbi_write_png_to_func(f, &out, layout.width, layout.height, 4, data.data(),
                                   layout.width * 4) == 0) {
            res.set_content("failed to encode", "text/plain");
        } else {
            res.set_content(reinterpret_cast<const char*>(out.data()), out.size(), "image/png");
        }
    });

    server->Get("/layout", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        const Layout::FramebufferLayout& layout =
            VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();
        res.set_content(
            nlohmann::json{
                {"swap_screens", Settings::values.swap_screen},
                {"is_rotated", layout.is_rotated},
                {"width", layout.width},
                {"height", layout.height},
                {"top_screen",
                 {
                     {"width", layout.top_screen.GetWidth()},
                     {"height", layout.top_screen.GetHeight()},
                     {"left", layout.top_screen.left},
                     {"top", layout.top_screen.top},
                     {"right", layout.top_screen.right},
                     {"bottom", layout.top_screen.bottom},
                 }},
                {"bottom_screen",
                 {
                     {"width", layout.bottom_screen.GetWidth()},
                     {"height", layout.bottom_screen.GetHeight()},
                     {"left", layout.bottom_screen.left},
                     {"top", layout.bottom_screen.top},
                     {"right", layout.bottom_screen.right},
                     {"bottom", layout.bottom_screen.bottom},
                 }},
            }
                .dump(),
            "application/json");
    });

    server->Post("/layout/custom", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.custom_layout = true;
            Settings::values.custom_top_left = json["top_screen"]["left"].get<u16>();
            Settings::values.custom_top_top = json["top_screen"]["top"].get<u16>();
            Settings::values.custom_top_right = json["top_screen"]["right"].get<u16>();
            Settings::values.custom_top_bottom = json["top_screen"]["bottom"].get<u16>();
            Settings::values.custom_bottom_left = json["bottom_screen"]["left"].get<u16>();
            Settings::values.custom_bottom_top = json["bottom_screen"]["top"].get<u16>();
            Settings::values.custom_bottom_right = json["bottom_screen"]["right"].get<u16>();
            Settings::values.custom_bottom_bottom = json["bottom_screen"]["bottom"].get<u16>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/layout/default", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::Default;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/singlescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/largescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::LargeScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/sidebyside", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SideScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/mediumscreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::MediumScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Post("/layout/swapscreens", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.swap_screen = json["enabled"].get<bool>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/layout/upright", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.upright_screen = json["upright"].get<bool>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/backgroundcolor", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"red", Settings::values.bg_red},
                {"green", Settings::values.bg_green},
                {"blue", Settings::values.bg_blue},
            }
                .dump(),
            "application/json");
    });

    server->Post("/backgroundcolor", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.bg_red = json["red"].get<float>();
            Settings::values.bg_green = json["green"].get<float>();
            Settings::values.bg_blue = json["blue"].get<float>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/customticks", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_custom_cpu_ticks},
                {"ticks", Settings::values.custom_cpu_ticks},
            }
                .dump(),
            "application/json");
    });

    server->Post("/customticks", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_custom_cpu_ticks = json["enabled"].get<bool>();
            Settings::values.custom_cpu_ticks = json["ticks"].get<u64>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/speedlimit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_frame_limit},
                {"percentage", Settings::values.frame_limit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/speedlimit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_frame_limit = json["enabled"].get<bool>();
            Settings::values.frame_limit = json["percentage"].get<u16>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/amiibo", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            Core::System::GetInstance()
                .ServiceManager()
                .GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc == nullptr) {
            res.status = 500;
            res.set_content("nfc:u is null", "text/plain");
        } else {
            if (req.body.empty()) {
                nfc->RemoveAmiibo();
                res.status = 204;
            } else if (req.body.size() == sizeof(Service::NFC::AmiiboData)) {
                Service::NFC::AmiiboData data;
                std::memcpy(&data, &req.body[0], sizeof(data));
                nfc->LoadAmiibo(data);
                res.status = 204;
            } else {
                res.status = 400;
                res.set_content("invalid body size. the current amiibo is removed if the body is "
                                "empty, or a amiibo is loaded if the body size is 540.",
                                "text/plain");
            }
        }
    });

    server->Get("/3d", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"mode", static_cast<int>(Settings::values.render_3d)},
                {"intensity", Settings::values.factor_3d.load()},
            }
                .dump(),
            "application/json");
    });

    server->Post("/3d", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.render_3d =
                static_cast<Settings::StereoRenderOption>(json["mode"].get<int>());
            Settings::values.factor_3d = json["intensity"].get<u8>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/microphone", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"type", static_cast<int>(Settings::values.mic_input_type)},
                {"device", Settings::values.mic_input_device},
            }
                .dump(),
            "application/json");
    });

    server->Post("/microphone", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.mic_input_type =
                static_cast<Settings::MicInputType>(json["type"].get<int>());
            Settings::values.mic_input_device = json["device"].get<std::string>();
            Settings::Apply();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/resolution", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"resolution", Settings::values.resolution_factor},
            }
                .dump(),
            "application/json");
    });

    server->Post("/resolution", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.resolution_factor = json["resolution"].get<u16>();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/frameadvancing", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Core::System::GetInstance().frame_limiter.FrameAdvancingEnabled()},
            }
                .dump(),
            "application/json");
    });

    server->Post("/frameadvancing", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Core::System::GetInstance().frame_limiter.SetFrameAdvancing(
                json["enabled"].get<bool>());

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/frameadvancing/advance",
                [&](const httplib::Request& req, httplib::Response& res) {
                    Core::System::GetInstance().frame_limiter.AdvanceFrame();
                    res.status = 204;
                });

    server->Get("/controls", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"current_profile_index", Settings::values.current_input_profile_index},
                {"current_profile", Settings::values.current_input_profile},
                {"profiles", Settings::values.input_profiles},
            }
                .dump(),
            "application/json");
    });

    server->Post("/controls", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int current_profile_index = json["current_profile_index"].get<int>();
            const std::vector<Settings::InputProfile> profiles =
                json["profiles"].get<std::vector<Settings::InputProfile>>();
            if ((current_profile_index < 0) ||
                (current_profile_index > 0 &&
                 (current_profile_index > static_cast<int>(profiles.size() - 1)))) {
                res.status = 400;
                res.set_content("current_profile_index out of range", "text/plain");
            } else {
                Settings::values.current_input_profile_index = current_profile_index;
                Settings::values.current_input_profile =
                    json["current_profile"].get<Settings::InputProfile>();
                Settings::values.input_profiles = profiles;
                Settings::Apply();

                res.status = 204;
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/cpuclockpercentage", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.cpu_clock_percentage},
            }
                .dump(),
            "application/json");
    });

    server->Post("/cpuclockpercentage", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.cpu_clock_percentage = json["value"].get<int>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/multiplayerurl", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.multiplayer_url},
            }
                .dump(),
            "application/json");
    });

    server->Post("/multiplayerurl", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.multiplayer_url = json["value"].get<std::string>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usehardwarerenderer", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_hw_renderer},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usehardwarerenderer", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_hw_renderer = json["enabled"].get<bool>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usehardwareshader", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_hw_shader},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usehardwareshader", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_hw_shader = json["enabled"].get<bool>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usediskshadercache", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_disk_shader_cache},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usediskshadercache", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_disk_shader_cache = json["enabled"].get<bool>();
            Settings::Apply();
            res.status = 202;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/shaderaccuratemultiplication",
                [&](const httplib::Request& req, httplib::Response& res) {
                    res.set_content(
                        nlohmann::json{
                            {"enabled", Settings::values.shaders_accurate_mul},
                        }
                            .dump(),
                        "application/json");
                });

    server->Post("/shaderaccuratemultiplication",
                 [&](const httplib::Request& req, httplib::Response& res) {
                     try {
                         const nlohmann::json json = nlohmann::json::parse(req.body);
                         Settings::values.shaders_accurate_mul = json["enabled"].get<bool>();
                         Settings::Apply();
                         res.status = 204;
                     } catch (nlohmann::json::exception& exception) {
                         res.status = 500;
                         res.set_content(exception.what(), "text/plain");
                     }
                 });

    server->Get("/useshaderjit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_shader_jit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/useshaderjit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_shader_jit = json["enabled"].get<bool>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/filtermode", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"mode", Settings::values.filter_mode ? "linear" : "nearest"},
            }
                .dump(),
            "application/json");
    });

    server->Get("/filtermode/nearest", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.filter_mode = false;
        Settings::Apply();
        res.status = 204;
    });

    server->Get("/filtermode/linear", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.filter_mode = true;
        Settings::Apply();
        res.status = 204;
    });

    server->Get("/postprocessingshader", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.pp_shader_name},
            }
                .dump(),
            "application/json");
    });

    server->Post("/postprocessingshader", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.pp_shader_name = json["name"].get<std::string>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/customscreenrefreshrate",
                [&](const httplib::Request& req, httplib::Response& res) {
                    res.set_content(
                        nlohmann::json{
                            {"enabled", Settings::values.use_custom_screen_refresh_rate},
                            {"value", Settings::values.custom_screen_refresh_rate},
                        }
                            .dump(),
                        "application/json");
                });

    server->Post(
        "/customscreenrefreshrate", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                const nlohmann::json json = nlohmann::json::parse(req.body);
                Settings::values.use_custom_screen_refresh_rate = json["enabled"].get<bool>();
                Settings::values.custom_screen_refresh_rate = json["value"].get<double>();
                res.status = 204;
            } catch (nlohmann::json::exception& exception) {
                res.status = 500;
                res.set_content(exception.what(), "text/plain");
            }
        });

    server->Get("/minverticesperthread", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.min_vertices_per_thread},
            }
                .dump(),
            "application/json");
    });

    server->Post("/minverticesperthread", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.min_vertices_per_thread = json["value"].get<int>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/dumptextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.dump_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/dumptextures", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.dump_textures = json["enabled"].get<bool>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/customtextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.custom_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/customtextures", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.custom_textures = json["enabled"].get<bool>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/preloadcustomtextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.preload_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/preloadcustomtextures",
                 [&](const httplib::Request& req, httplib::Response& res) {
                     try {
                         const nlohmann::json json = nlohmann::json::parse(req.body);
                         Settings::values.preload_textures = json["enabled"].get<bool>();
                         res.status = 202;
                     } catch (nlohmann::json::exception& exception) {
                         res.status = 500;
                         res.set_content(exception.what(), "text/plain");
                     }
                 });

    server->Get("/usecpujit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_cpu_jit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usecpujit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_cpu_jit = json["enabled"].get<bool>();
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/ignoreformatreinterpretation",
                [&](const httplib::Request& req, httplib::Response& res) {
                    res.set_content(
                        nlohmann::json{
                            {"enabled", Settings::values.ignore_format_reinterpretation},
                        }
                            .dump(),
                        "application/json");
                });

    server->Post(
        "/ignoreformatreinterpretation", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                const nlohmann::json json = nlohmann::json::parse(req.body);
                Settings::values.ignore_format_reinterpretation = json["enabled"].get<bool>();
                res.status = 204;
            } catch (nlohmann::json::exception& exception) {
                res.status = 500;
                res.set_content(exception.what(), "text/plain");
            }
        });

    server->Get("/dspemulation", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"emulation", Settings::values.enable_dsp_lle ? "lle" : "hle"},
                {"multithreaded",
                 Settings::values.enable_dsp_lle && Settings::values.enable_dsp_lle_multithread},
            }
                .dump(),
            "application/json");
    });

    server->Post("/dspemulation", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.enable_dsp_lle = json["emulation"].get<std::string>() == "lle";
            if (Settings::values.enable_dsp_lle) {
                Settings::values.enable_dsp_lle_multithread = json["multithreaded"].get<bool>();
            }
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audioengine", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.sink_id},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audioengine", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.sink_id = json["name"].get<std::string>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiostretching", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.enable_audio_stretching},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiostretching", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.enable_audio_stretching = json["enabled"].get<bool>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiodevice", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.audio_device_id},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiodevice", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.audio_device_id = json["value"].get<bool>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiovolume", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.volume},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiovolume", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.volume = json["value"].get<float>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiospeed", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.audio_speed},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiospeed", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.audio_speed = json["value"].get<float>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usevirtualsdcard", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_virtual_sd},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usevirtualsdcard", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_virtual_sd = json["enabled"].get<bool>();
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/isnew3ds", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.is_new_3ds},
            }
                .dump(),
            "application/json");
    });

    server->Post("/isnew3ds", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.is_new_3ds = json["enabled"].get<bool>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/region", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.region_value},
            }
                .dump(),
            "application/json");
    });

    server->Post("/region", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.region_value = json["value"].get<int>();
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/startclock", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json json;
        switch (Settings::values.init_clock) {
        case Settings::InitClock::SystemTime: {
            json["clock"] = "system";
            break;
        }
        case Settings::InitClock::FixedTime: {
            json["clock"] = "fixed";
            json["unix_timestamp"] = Settings::values.init_time;
            break;
        }
        }
        res.set_content(json.dump(), "application/json");
    });

    server->Post("/startclock", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.init_clock = json["clock"].get<std::string>() == "system"
                                              ? Settings::InitClock::SystemTime
                                              : Settings::InitClock::FixedTime;
            if (Settings::values.init_clock == Settings::InitClock::FixedTime) {
                Settings::values.init_time = json["unix_timestamp"].get<u64>();
            }
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usevsync", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_vsync_new},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usevsync", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_vsync_new = json["enabled"].get<bool>();
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/logfilter", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.log_filter},
            }
                .dump(),
            "application/json");
    });

    server->Post("/logfilter", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.log_filter = json["value"].get<std::string>();
            Log::Filter log_filter(Log::Level::Debug);
            log_filter.ParseFilterString(Settings::values.log_filter);
            Log::SetGlobalFilter(log_filter);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/recordframetimes", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.record_frame_times},
            }
                .dump(),
            "application/json");
    });

    server->Post("/recordframetimes", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.record_frame_times = json["enabled"].get<bool>();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/cameras", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.camera_name},
                {"config", Settings::values.camera_config},
                {"flip", Settings::values.camera_flip},
            }
                .dump(),
            "application/json");
    });

    server->Post("/cameras", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.camera_name =
                json["name"].get<std::array<std::string, Service::CAM::NumCameras>>();
            Settings::values.camera_config =
                json["config"].get<std::array<std::string, Service::CAM::NumCameras>>();
            Settings::values.camera_flip =
                json["flip"].get<std::array<int, Service::CAM::NumCameras>>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/gdbstub", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_gdbstub},
                {"port", Settings::values.gdbstub_port},
            }
                .dump(),
            "application/json");
    });

    server->Post("/gdbstub", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_gdbstub = json["enabled"].get<bool>();
            Settings::values.gdbstub_port = json["port"].get<u16>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/llemodules", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(nlohmann::json(Settings::values.lle_modules).dump(), "application/json");
    });

    server->Post("/llemodules", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.lle_modules = json.get<std::unordered_map<std::string, bool>>();
            if (Core::System::GetInstance().IsPoweredOn()) {
                Core::System::GetInstance().RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/movie", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"playing", Core::Movie::GetInstance().IsPlayingInput()},
                {"recording", Core::Movie::GetInstance().IsRecordingInput()},
            }
                .dump(),
            "application/json");
    });

    server->Get("/movie/stop", [&](const httplib::Request& req, httplib::Response& res) {
        Core::Movie::GetInstance().Shutdown();
        res.status = 204;
    });

    server->Post("/movie/play", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            Core::Movie::GetInstance().StartPlayback(file, [] {});
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/movie/record", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            Core::Movie::GetInstance().StartRecording(file);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/boot", [&](const httplib::Request& req, httplib::Response& res) {
        if (!Core::System::GetInstance().IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            Core::System::GetInstance().SetResetFilePath(file);
            Core::System::GetInstance().RequestReset();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/installciafile", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            const auto status = Service::AM::InstallCIA(file);
            if (status == Service::AM::InstallStatus::Success) {
                res.status = 204;
            } else {
                res.status = 500;
                res.set_content(std::to_string(static_cast<int>(status)), "text/plain");
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    request_handler_thread = std::thread([this, port] { server->listen("0.0.0.0", port); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", port);
}

RPCServer::~RPCServer() {
    server->stop();
    request_handler_thread.join();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

} // namespace RPC
