/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "databuffer.hpp"

#include "logger.hpp"

DataBuffer::DataBuffer(size_t initialSize, size_t maxSize)
    : m_maxBufSize{maxSize}, m_buf(initialSize) {}

bool DataBuffer::hasFreeSpace(size_t neededSize) {
    if (freeSpaceSize() < neededSize) optimize();
    return freeSpaceSize() >= neededSize;
}

bool DataBuffer::pushExact(const BufType *data, size_t dataSize) {
    if (!hasFreeSpace(dataSize)) return false;

    memcpy(&m_buf[m_tail], data, dataSize);
    m_tail += dataSize;

    return true;
}

void DataBuffer::optimize() {
    if (m_head == 0 || empty()) return;
    memmove(&m_buf[0], &m_buf[m_head], size());
    m_tail = size();
    m_head = 0;
}

void DataBuffer::resize(size_t neededSize) {
    if (m_tail + neededSize > m_maxBufSize)
        throw std::runtime_error(std::string{"buf max size reached: "} +
                                 std::to_string(m_tail + neededSize) + "/" +
                                 std::to_string(m_maxBufSize));

    auto old = m_buf.size();
    m_buf.resize(m_tail + neededSize);
    LOGD("buf resize: " << old << " => " << m_buf.size());
}

void DataBuffer::pushExactForce(const BufType *data, size_t dataSize) {
    if (!hasFreeSpace(dataSize)) resize(dataSize);

    memcpy(&m_buf[m_tail], data, dataSize);
    m_tail += dataSize;
}

size_t DataBuffer::push(const BufType *data, size_t dataMaxSize) {
    if (freeSpaceSize() < dataMaxSize) optimize();
    if (full()) return 0;

    auto sizeToPush = std::min(dataMaxSize, freeSpaceSize());

    memcpy(&m_buf[m_tail], data, sizeToPush);
    m_tail += sizeToPush;

    return sizeToPush;
}

bool DataBuffer::pushOverwriteTail(const BufType *data, size_t size) {
    if (freeSpaceSize() < size) return false;

    memcpy(&m_buf[m_tail - size], data, size);

    return true;
}

bool DataBuffer::pushNullExactForce(size_t dataSize) {
    if (!hasFreeSpace(dataSize)) resize(dataSize);

    memset(&m_buf[m_tail], 0, dataSize);
    m_tail += dataSize;

    return true;
}

bool DataBuffer::pushNullExact(size_t dataSize) {
    if (!hasFreeSpace(dataSize)) return false;

    memset(&m_buf[m_tail], 0, dataSize);
    m_tail += dataSize;

    return true;
}

size_t DataBuffer::pull(BufType *data, size_t dataMaxSize) {
    if (empty()) return 0;

    auto sizeToRead = std::min(dataMaxSize, size());

    memcpy(data, &m_buf[m_head], sizeToRead);

    if (sizeToRead == size()) {
        m_head = 0;
        m_tail = 0;
    } else {
        m_head += sizeToRead;
    }

    return sizeToRead;
}

bool DataBuffer::pullExact(BufType *data, size_t dataSize) {
    if (empty() || size() < dataSize) return false;

    memcpy(data, &m_buf[m_head], dataSize);

    if (dataSize == size()) {
        m_head = 0;
        m_tail = 0;
    } else {
        m_head += dataSize;
    }

    return true;
}

std::pair<DataBuffer::BufType *, size_t> DataBuffer::ptrForPull() {
    return {&m_buf[m_head], size()};
}

size_t DataBuffer::discard(size_t dataMaxSize) {
    auto sizeToRead = std::min(dataMaxSize, size());

    if (sizeToRead == size()) {
        m_head = 0;
        m_tail = 0;
    } else {
        m_head += sizeToRead;
    }

    return sizeToRead;
}

bool DataBuffer::discardExact(size_t dataSize) {
    if (size() < dataSize) return false;

    if (dataSize == size()) {
        m_head = 0;
        m_tail = 0;
    } else {
        m_head += dataSize;
    }

    return true;
}

void DataBuffer::clear() {
    m_head = 0;
    m_tail = 0;
}
