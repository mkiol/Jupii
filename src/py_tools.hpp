/* Copyright (C) 2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PY_TOOLS_HPP
#define PY_TOOLS_HPP

#include <iostream>

namespace py_tools {
inline static const auto python_site_path = "python/site-packages";

struct libs_availability_t {
    bool ytmusicapi = false;
    bool ytdlp = false;
};

libs_availability_t libs_availability();
bool init_module();
}  // namespace py_tools

std::ostream& operator<<(std::ostream& os,
                         const py_tools::libs_availability_t& availability);

#endif  // PY_TOOLS_HPP
