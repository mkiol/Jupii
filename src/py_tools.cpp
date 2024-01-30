/* Copyright (C) 2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "py_tools.hpp"

#ifdef USE_PY
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>
#define slots Q_SLOTS
#endif

#include <QDebug>
#include <QStandardPaths>
#include <QString>

#include "logger.hpp"

#ifdef USE_PYTHON_MODULE
#include "module_tools.hpp"
#endif

std::ostream& operator<<(std::ostream& os,
                         const py_tools::libs_availability_t& availability) {
    os << "ytmusicapi=" << availability.ytmusicapi
       << ", ytdlp=" << availability.ytdlp;

    return os;
}

namespace py_tools {
libs_availability_t libs_availability() {
    // run only in py thread

    libs_availability_t availability{};

#ifdef USE_PY
    namespace py = pybind11;
    using namespace pybind11::literals;

    try {
        LOGD("checking: ytmusicapi");
        auto ytmusic_mod = py::module_::import("ytmusicapi");
        LOGD("ytmusicapi version: "
             << ytmusic_mod.attr("version")("ytmusicapi").cast<std::string>());
        availability.ytmusicapi = true;
    } catch (const std::exception& err) {
        LOGD("ytmusicapi check py error: " << err.what());
    }

    try {
        LOGD("checking: ytmusicapi");
        auto yt_dlp_mod = py::module_::import("yt_dlp");
        LOGD("yt_dlp version: " << yt_dlp_mod.attr("version")
                                       .attr("__version__")
                                       .cast<std::string>());
        availability.ytdlp = true;
    } catch (const std::exception& err) {
        LOGD("yt_dlp check py error: " << err.what());
    }

    LOGD("py libs availability: [" << availability << "]");
#endif

    return availability;
}

bool init_module() {
#ifdef USE_PYTHON_MODULE
    if (!module_tools::init_module(QStringLiteral("python"))) return false;

    auto py_path =
        QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" +
        python_site_path;

    qDebug() << "setting env PYTHONPATH=" << py_path;

    setenv("PYTHONPATH", py_path.toStdString().c_str(), true);

    return true;
#else
    return true;
#endif
}
}  // namespace py_tools
