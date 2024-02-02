/* Copyright (C) 2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PYEXECUTOR_H
#define PYEXECUTOR_H

#undef slots
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>
#define slots Q_SLOTS

#include <any>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

#include "py_tools.hpp"
#include "singleton.h"

namespace py = pybind11;

class py_executor : public Singleton<py_executor> {
   public:
    using task_t = std::function<std::any()>;
    std::optional<py_tools::libs_availability_t> libs_availability;
    py_executor() = default;
    ~py_executor() override;
    std::optional<std::future<std::any>> execute(task_t task);
    void start();
    void stop();

   private:
    bool m_shutting_down = false;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    std::optional<py::scoped_interpreter> m_py_interpreter;
    std::optional<task_t> m_task;
    std::optional<std::promise<std::any>> m_promise;

    void loop();
};

#endif  // PYEXECUTOR_H
