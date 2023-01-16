/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TESTSOURCE_HPP
#define TESTSOURCE_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

class TestSource {
   public:
    using DataReadyHandler = std::function<void(const uint8_t *, size_t)>;

    struct Props {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t framerate = 0;
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    };

    explicit TestSource(DataReadyHandler dataReadyHandler);
    ~TestSource();
    void start();
    static bool supported() noexcept;
    static Props properties();

   private:
    DataReadyHandler m_dataReadyHandler;
    Props m_props;
    std::thread m_thread;
    bool m_termination = false;

    static std::vector<uint8_t> generateImage(uint32_t width, uint32_t height);
};

#endif  // TESTSOURCE_HPP
