/* Copyright (C) 2022-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "lipstickrecordersource.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>

#include "logger.hpp"

std::ostream &operator<<(std::ostream &os,
                         LipstickRecorderSource::Transform transform) {
    switch (transform) {
        case LipstickRecorderSource::Transform::Normal:
            os << "normal";
            break;
        case LipstickRecorderSource::Transform::Rot90:
            os << "rot-90";
            break;
        case LipstickRecorderSource::Transform::Rot180:
            os << "rot-180";
            break;
        case LipstickRecorderSource::Transform::Rot270:
            os << "rot-270";
            break;
    }
    return os;
}

LipstickRecorderSource::LipstickRecorderSource(
    DataReadyHandler dataReadyHandler, ErrorHandler errorHandler,
    uint32_t framerate)
    : m_dataReadyHandler{std::move(dataReadyHandler)},
      m_errorHandler{std::move(errorHandler)} {
    LOGD("creating lipstick-recorder: framerate=" << framerate);

    if (!checkCredentials())
        throw std::runtime_error("no permission to use lipstick-recorder");

    m_globals.source = this;
    m_globals.props.framerate = framerate;

    wlRegister(&m_globals);

    if (!m_globals.ok()) {
        clean();
        throw std::runtime_error("failed to init");
    }
}

void LipstickRecorderSource::clean() {
    if (m_globals.lrRec != nullptr) {
        lipstick_recorder_destroy(m_globals.lrRec);
        m_globals.lrRec = nullptr;
    }
    if (m_globals.registry != nullptr) {
        wl_registry_destroy(m_globals.registry);
        m_globals.registry = nullptr;
    }
    if (m_globals.display != nullptr) {
        wl_display_disconnect(m_globals.display);
        m_globals.display = nullptr;
    }
}

LipstickRecorderSource::~LipstickRecorderSource() {
    LOGD("lipstick-recorder termination started");
    m_terminating = true;
    if (m_wlThread.joinable()) m_wlThread.join();
    LOGD("wl tread joined");
    clean();
    LOGD("lipstick-recorder termination completed");
}

bool LipstickRecorderSource::supported() noexcept {
    try {
        if (!checkCredentials()) {
            LOGW("no permission to use lipstick-recorder");
            return false;
        }
        makeGlobals(nullptr);
    } catch (const std::runtime_error &e) {
        LOGE(e.what());
        return false;
    } catch (...) {
        LOGE("unknown error");
        return false;
    }

    return true;
}

bool LipstickRecorderSource::checkCredentials() {
    auto *g = getgrgid(getegid());
    if (g == nullptr) throw std::runtime_error("failed to obtain gid");
    LOGD("process gr name: " << g->gr_name);
    return strcmp(g->gr_name, "privileged") == 0;
}

void LipstickRecorderSource::wlGlobalCallback(void *data, wl_registry *registry,
                                              uint32_t id,
                                              const char *interface,
                                              uint32_t version) {
    LOGD("wl global: interface=" << interface);

    auto *globals = static_cast<Globals *>(data);

    if (globals->ok()) return;

    if (strcmp(interface, "wl_shm") == 0) {
        globals->shm = static_cast<wl_shm *>(wl_registry_bind(
            registry, id, &wl_shm_interface, std::min(version, 1U)));
    } else if (strcmp(interface, "wl_output") == 0) {
        globals->output = static_cast<wl_output *>(wl_registry_bind(
            registry, id, &wl_output_interface, std::min(version, 1U)));
        if (globals->output != nullptr) {
            wl_output_add_listener(globals->output, &wlOutputListener, globals);
            wl_display_roundtrip(globals->display);
        }
    } else if (strcmp(interface, "lipstick_recorder_manager") == 0) {
        globals->lrManager = static_cast<lipstick_recorder_manager *>(
            wl_registry_bind(registry, id, &lipstick_recorder_manager_interface,
                             std::min(version, 1U)));
    }

    if (globals->lrRec == nullptr && globals->lrManager != nullptr &&
        globals->output != nullptr) {
        globals->lrRec = lipstick_recorder_manager_create_recorder(
            globals->lrManager, globals->output);
        if (globals->lrRec != nullptr) {
            lipstick_recorder_add_listener(globals->lrRec, &wlLrListener,
                                           globals);
            wl_display_roundtrip(globals->display);
        }
    }
}

void LipstickRecorderSource::wlGlobalRemoveCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_registry *registry,
    [[maybe_unused]] uint32_t id) {}

void LipstickRecorderSource::wlCallbackCallback(
    [[maybe_unused]] void *data, wl_callback *cb,
    [[maybe_unused]] uint32_t time) {
    wl_callback_destroy(cb);
}

void LipstickRecorderSource::wlRegister(Globals *globals) {
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

LipstickRecorderSource::Globals LipstickRecorderSource::makeGlobals(
    LipstickRecorderSource *source) {
    Globals globals;
    globals.source = source;

    wlRegister(&globals);

    LOGD("globals: shm=" << (globals.shm != nullptr)
                         << ", output=" << (globals.output != nullptr)
                         << ", lr-manager=" << (globals.lrManager != nullptr)
                         << ", lr-rec=" << (globals.lrRec != nullptr)
                         << ", props.width=" << globals.props.width
                         << ", props.height=" << globals.props.height);

    wl_registry_destroy(globals.registry);
    wl_display_disconnect(globals.display);

    if (!globals.ok())
        throw std::runtime_error("failed to get proper wl globals");

    LOGD("globals successfully done");

    return globals;
}

LipstickRecorderSource::Props LipstickRecorderSource::properties() {
    return makeGlobals(nullptr).props;
}

void LipstickRecorderSource::repaintIfNeeded(std::chrono::microseconds maxDur) {
    if (std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_buf->tp) >= maxDur) {
        LOGT("forcing repaint");
        lipstick_recorder_repaint(m_globals.lrRec);
        wl_display_flush(m_globals.display);
    }
}

void LipstickRecorderSource::start() {
    if (m_terminating) return;
    if (m_wlThread.joinable()) {
        // already started
        return;
    }

    LOGD("starting lipstick-recorder");

    m_buf.emplace(m_globals.shm, m_globals.props.width, m_globals.props.height,
                  m_globals.props.stride);

    recordFrame();
    lipstick_recorder_repaint(m_globals.lrRec);
    wl_display_flush(m_globals.display);

    m_wlThread = std::thread([this] {
        const auto sleepDur = std::chrono::microseconds{
            static_cast<int>(1000000.0 / m_globals.props.framerate)};
        const auto repaintDur = sleepDur * 5;

        LOGD("wl thread started: framerate=" << m_globals.props.framerate);

        while (!m_terminating) {
            if (wl_display_roundtrip(m_globals.display) == -1) break;
            LOGT("lr iteration");

            if (m_dataReadyHandler) {
                m_dataReadyHandler(m_buf->data, m_buf->size);
            }

            repaintIfNeeded(repaintDur);

            std::this_thread::sleep_for(sleepDur);
        }

        if (!m_terminating) {
            m_terminating = true;
            auto err = wl_display_get_error(m_globals.display);
            LOGE(fmt::format("wl error: {} ({})", strerror(err), err));
            if (m_errorHandler) m_errorHandler();
        }

        LOGD("wl thread ended");
    });

    LOGD("lipstick-recorder started");
}

LipstickRecorderSource::WlBufferWrapper::WlBufferWrapper(wl_shm *shm,
                                                         uint32_t width,
                                                         uint32_t height,
                                                         uint32_t stride) {
    size = stride * height;

    char filename[] = "/tmp/lipstick-recorder-shm-XXXXXX";

    auto fd = mkstemp(filename);
    if (fd < 0)
        throw std::runtime_error("failed to create tmp file for wl buffer");

    auto flags = fcntl(fd, F_GETFD);
    if (flags != -1) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

    if (ftruncate(fd, size) < 0) {
        close(fd);
        throw std::runtime_error(
            fmt::format("ftruncate failed: {}", strerror(errno)));
    }

    data = static_cast<unsigned char *>(
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    unlink(filename);

    if (data == static_cast<unsigned char *>(MAP_FAILED)) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }

    memset(data, 0, size);

    auto *pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                       WL_SHM_FORMAT_ARGB8888);
    wl_buffer_set_user_data(buffer, this);
    wl_shm_pool_destroy(pool);

    close(fd);
}

void LipstickRecorderSource::recordFrame() {
    LOGT("lr record frame");

    lipstick_recorder_record_frame(m_globals.lrRec, m_buf->buffer);
}

void LipstickRecorderSource::wlLrFrameCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] lipstick_recorder *recorder,
    wl_buffer *buffer, [[maybe_unused]] uint32_t timestamp, int transform) {
    LOGT("lr frame: " << transform);

    auto *buf = static_cast<WlBufferWrapper *>(wl_buffer_get_user_data(buffer));
    buf->yinverted = transform == 2;
    buf->tp = std::chrono::steady_clock::now();

    auto *globals = static_cast<Globals *>(data);
    if (globals->source) globals->source->recordFrame();
}

void LipstickRecorderSource::wlLrFailedCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] lipstick_recorder *recorder,
    [[maybe_unused]] int result, [[maybe_unused]] wl_buffer *buffer) {
    throw std::runtime_error(fmt::format("wl lr failed: {}", result));
}

void LipstickRecorderSource::wlLrCancelCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] lipstick_recorder *recorder,
    [[maybe_unused]] wl_buffer *buffer) {}

void LipstickRecorderSource::wlLrSetupCallback(
    void *data, [[maybe_unused]] lipstick_recorder *recorder, int width,
    int height, int stride, int format) {
    LOGD("lr setup: width=" << width << ", height=" << height
                            << ", stride=" << stride << ", format=" << format);

    auto *globals = static_cast<Globals *>(data);
    globals->props.width = width;
    globals->props.height = height;
    globals->props.stride = stride;
    globals->props.pixfmt = wl2FfPixfmt(static_cast<wl_shm_format>(format));
}

AVPixelFormat LipstickRecorderSource::wl2FfPixfmt(wl_shm_format wlFormat) {
    switch (wlFormat) {
        case WL_SHM_FORMAT_ARGB8888:
            return AV_PIX_FMT_0RGB;
        case WL_SHM_FORMAT_ABGR8888:
            return AV_PIX_FMT_0BGR;
        case WL_SHM_FORMAT_RGBA8888:
            return AV_PIX_FMT_RGB0;
        case WL_SHM_FORMAT_BGRA8888:
            return AV_PIX_FMT_BGR0;
        default:
            throw std::runtime_error("unknown wl pixfmt");
    }
}

wl_shm_format LipstickRecorderSource::ff2WlPixfmt(AVPixelFormat ffFormat) {
    switch (ffFormat) {
        case AV_PIX_FMT_0RGB:
            return WL_SHM_FORMAT_ARGB8888;
        case AV_PIX_FMT_0BGR:
            return WL_SHM_FORMAT_ABGR8888;
        case AV_PIX_FMT_RGB0:
            return WL_SHM_FORMAT_RGBA8888;
        case AV_PIX_FMT_BGR0:
            return WL_SHM_FORMAT_BGRA8888;
        default:
            throw std::runtime_error("unknown ff pixfmt");
    }
}

void LipstickRecorderSource::wlOutputGeometryCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, [[maybe_unused]] const char *make,
    [[maybe_unused]] const char *model, int32_t transform) {
    LOGD("wl output geometry: physical_width="
         << physical_width << ", physical_height=" << physical_height
         << ", subpixel=" << subpixel << ", x=" << x << ", y=" << y
         << ", transform=" << static_cast<Transform>(transform));

    auto *globals = static_cast<Globals *>(data);
    if (globals->source)
        globals->source->m_transform = static_cast<Transform>(transform);
}

void LipstickRecorderSource::wlOutputModeCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    [[maybe_unused]] uint32_t flags, int32_t width, int32_t height,
    int32_t refresh) {
    LOGD("wl output mode: width=" << width << ", height=" << height
                                  << ", refresh=" << refresh);
}

void LipstickRecorderSource::wlOutputDoneCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output) {}

void LipstickRecorderSource::wlOutputScaleCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    int32_t factor) {
    LOGD("wl output scale: factor=" << factor);
}

void LipstickRecorderSource::wlOutputNameCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    const char *name) {
    LOGD("wl output name: name=" << name);
}

void LipstickRecorderSource::wlOutputDescriptionCallback(
    [[maybe_unused]] void *data, [[maybe_unused]] wl_output *wl_output,
    const char *description) {
    LOGD("wl output description: description=" << description);
}
