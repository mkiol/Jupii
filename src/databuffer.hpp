/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

class DataBuffer {
   public:
    using BufType = uint8_t;

    DataBuffer(size_t initialSize, size_t maxSize);

    inline bool empty() const { return m_tail == 0; }
    inline size_t size() const { return m_tail - m_head; }
    inline bool hasEnoughData(size_t neededSize) const {
        return size() >= neededSize;
    }

    bool hasFreeSpace(size_t neededSize);
    bool pushExact(const BufType *data, size_t dataSize);
    void pushExactForce(const BufType *data, size_t dataSize);
    size_t push(const BufType *data, size_t dataMaxSize);
    bool pushOverwriteTail(const BufType *data, size_t size);
    bool pushNullExact(size_t dataSize);
    bool pushNullExactForce(size_t dataSize);
    size_t pull(BufType *data, size_t dataMaxSize);
    bool pullExact(BufType *data, size_t dataSize);
    std::pair<BufType *, size_t> ptrForPull();
    size_t discard(size_t dataMaxSize);
    bool discardExact(size_t dataSize);
    void clear();

   private:
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_maxBufSize = 0;

    std::vector<BufType> m_buf;

    void resize(size_t neededSize);
    void optimize();

    inline bool full() const { return m_buf.size() == m_tail; }
    inline size_t freeSpaceSize() const { return m_buf.size() - m_tail; }
};

#endif  // DATABUFFER_H
