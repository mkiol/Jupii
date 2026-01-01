/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orientationmonitor.hpp"

#include <fmt/format.h>

#include <cerrno>
#include <cstdlib>

#include "logger.hpp"

std::ostream &operator<<(std::ostream &os,
                         OrientationMonitor::Transform transform) {
    switch (transform) {
        case OrientationMonitor::Transform::Normal:
            os << "normal";
            break;
        case OrientationMonitor::Transform::Rot90:
            os << "rot-90";
            break;
        case OrientationMonitor::Transform::Rot180:
            os << "rot-180";
            break;
        case OrientationMonitor::Transform::Rot270:
            os << "rot-270";
            break;
    }
    return os;
}

OrientationMonitor::OrientationMonitor() {
    LOGD("creating orientation-monitor");

    m_globals.source = this;

    wlRegister(&m_globals);

    if (!m_globals.ok()) {
        clean();
        throw std::runtime_error("failed to init");
    }
}

void OrientationMonitor::clean() {
    if (m_globals.registry != nullptr) {
        wl_registry_destroy(m_globals.registry);
        m_globals.registry = nullptr;
    }
    if (m_globals.display != nullptr) {
        wl_display_disconnect(m_globals.display);
        m_globals.display = nullptr;
    }
}

OrientationMonitor::~OrientationMonitor() {
    LOGD("orientation-monitor termination started");
    m_terminating = true;
    if (m_wlThread.joinable()) m_wlThread.join();
    LOGD("wl tread joined");
    clean();
    LOGD("orientation-monitor termination completed");
}

void OrientationMonitor::wlGlobalCallback(void *data, wl_registry *registry,
                                          uint32_t id, const char *interface,
                                          uint32_t version) {
    LOGD("wl global: interface=" << interface);

    auto *globals = static_cast<Globals *>(data);

    if (globals->ok()) return;

    if (strcmp(interface, "wl_output") == 0) {
        globals->output = static_cast<wl_output *>(wl_registry_bind(
            registry, id, &wl_output_interface, std::min(version, 1U)));
        if (globals->output != nullptr) {
            wl_output_add_listener(globals->output, &wlOutputListener, globals);
            wl_display_roundtrip(globals->display);
        }
    }
}

void OrientationMonitor::wlGlobalRemoveCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_registry *registry,
    [[maybe_unused]] uint32_t id) {}

void OrientationMonitor::wlCallbackCallback([[maybe_unused]] void *data,
                                            wl_callback *cb,
                                            [[maybe_unused]] uint32_t time) {
    wl_callback_destroy(cb);
}

void OrientationMonitor::wlRegister(Globals *globals) {
    globals->display = wl_display_connect(0);
    if (globals->display == nullptr)
        throw std::runtime_error("failed to get wl display");

    globals->registry = wl_display_get_registry(globals->display);
    if (globals->registry == nullptr) {
        wl_display_disconnect(globals->display);
        globals->display = nullptr;
        throw std::runtime_error("failed to get wl registry");
    }

    wl_registry_add_listener(globals->registry, &wlGlobalListener, globals);

    auto *cb = wl_display_sync(globals->display);
    wl_callback_add_listener(cb, &wlCallback, nullptr);

    wl_display_roundtrip(globals->display);
}

void OrientationMonitor::start() {
    if (m_terminating) return;
    if (m_wlThread.joinable()) {
        // already started
        return;
    }

    LOGD("starting orientation-monitor");

    m_wlThread = std::thread([this] {
        LOGD("orientation-monitor wl thread started");

        const auto sleepDur =
            std::chrono::microseconds{static_cast<int>(1000000.0 / framerate)};

        while (!m_terminating) {
            if (wl_display_roundtrip(m_globals.display) == -1) break;
            std::this_thread::sleep_for(sleepDur);
        }

        if (!m_terminating) {
            m_terminating = true;
            auto err = wl_display_get_error(m_globals.display);
            LOGE(fmt::format("wl error: {} ({})", strerror(err), err));
        }

        LOGD("orientation-monitor wl thread ended");
    });

    LOGD("orientation-monitor started");
}

void OrientationMonitor::wlOutputGeometryCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
    [[maybe_unused]] int32_t physical_width,
    [[maybe_unused]] int32_t physical_height, [[maybe_unused]] int32_t subpixel,
    [[maybe_unused]] const char *make, [[maybe_unused]] const char *model,
    int32_t transform) {
    LOGD("wl output geometry: transform=" << static_cast<Transform>(transform));

    auto *globals = static_cast<Globals *>(data);
    if (globals->source)
        globals->source->m_transform = static_cast<Transform>(transform);
}

void OrientationMonitor::wlOutputModeCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] uint32_t flags, [[maybe_unused]] int32_t width,
    [[maybe_unused]] int32_t height, [[maybe_unused]] int32_t refresh) {}

void OrientationMonitor::wlOutputDoneCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output) {}

void OrientationMonitor::wlOutputScaleCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] int32_t factor) {}

void OrientationMonitor::wlOutputNameCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] const char *name) {}

void OrientationMonitor::wlOutputDescriptionCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] const char *description) {}
