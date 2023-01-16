/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "testsource.hpp"

#include <chrono>
#include <cstring>

#include "logger.hpp"

TestSource::TestSource(DataReadyHandler dataReadyHandler)
    : m_dataReadyHandler{std::move(dataReadyHandler)}, m_props(properties()) {
    LOGD("creating test source");
}

TestSource::~TestSource() {
    LOGD("test source termination started");
    m_termination = true;
    if (m_thread.joinable()) m_thread.join();
    LOGD("test source termination completed");
}

bool TestSource::supported() noexcept { return true; }

TestSource::Props TestSource::properties() {
    return {200, 800, 15, AV_PIX_FMT_0RGB};
}

std::vector<uint8_t> TestSource::generateImage(uint32_t width,
                                               uint32_t height) {
    const size_t stride = 4 * static_cast<size_t>(width);

    const uint8_t red[] = {0x00, 0xFF, 0x00, 0x00};
    const uint8_t green[] = {0x00, 0x00, 0xFF, 0x00};
    const uint8_t blue[] = {0x00, 0x00, 0x00, 0xFF};

    std::vector<uint8_t> data;
    data.resize(stride * height);

    const auto* pixel = red;
    const auto rowHeight = height / 3;

    for (size_t i = 0; i < height; ++i) {
        auto* beg = &data[i * stride];

        for (size_t ii = 0; ii < stride; ii += 4) memcpy(&beg[ii], pixel, 4);

        if (i > 0 && i % rowHeight == 0) {
            if (pixel == red)
                pixel = green;
            else if (pixel == green)
                pixel = blue;
            else
                pixel = red;
        }
    }

    return data;
}

void TestSource::start() {
    m_thread = std::thread([this] {
        LOGD("test source thread started");
        const auto sleepDur = std::chrono::microseconds{
            static_cast<int>(1000000.0 / m_props.framerate)};

        auto data = generateImage(m_props.width, m_props.height);

        while (!m_termination) {
            if (m_dataReadyHandler) {
                m_dataReadyHandler(data.data(), data.size());
            }
            std::this_thread::sleep_for(sleepDur);
        }
        LOGD("test source thread ended");
    });
}
