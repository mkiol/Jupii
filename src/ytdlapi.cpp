/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ytdlapi.h"

#include <archive.h>
#include <archive_entry.h>
#include <lzma.h>
#undef slots
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#define slots Q_SLOTS
#include <stdlib.h>

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <fstream>

#include "downloader.h"
#include "info.h"
#include "settings.h"
#include "utils.h"

YtdlApi::State YtdlApi::state = YtdlApi::State::Unknown;

#ifdef SAILFISH
const QString YtdlApi::pythonArchivePath =
    QStringLiteral("/usr/share/%1/lib/python.tar.xz").arg(Jupii::APP_BINARY_ID);

inline static QString pythonUnpackDir() {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
}

static bool xz_decode(const QString& file_in, const QString& file_out) {
    qDebug() << "extracting xz archive:" << file_in << file_out;

    if (std::ifstream input{file_in.toStdString(),
                            std::ios::in | std::ifstream::binary}) {
        if (std::ofstream output{file_out.toStdString(),
                                 std::ios::out | std::ifstream::binary}) {
            lzma_stream strm = LZMA_STREAM_INIT;
            if (lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
                ret != LZMA_OK) {
                qWarning() << "error initializing the xz decoder:" << ret;
                return false;
            }

            lzma_action action = LZMA_RUN;

            char buff_out[std::numeric_limits<unsigned short>::max()];
            char buff_in[std::numeric_limits<unsigned short>::max()];

            strm.next_in = reinterpret_cast<uint8_t*>(buff_in);
            strm.next_out = reinterpret_cast<uint8_t*>(buff_out);
            strm.avail_out = sizeof buff_out;
            strm.avail_in = 0;

            while (true) {
                if (strm.avail_in == 0 && input) {
                    strm.next_in = reinterpret_cast<uint8_t*>(buff_in);
                    input.read(buff_in, sizeof buff_in);
                    strm.avail_in = input.gcount();
                }

                if (!input) action = LZMA_FINISH;

                auto ret = lzma_code(&strm, action);

                if (strm.avail_out == 0 || ret == LZMA_STREAM_END) {
                    output.write(buff_out, sizeof buff_out - strm.avail_out);

                    strm.next_out = reinterpret_cast<uint8_t*>(buff_out);
                    strm.avail_out = sizeof buff_out;
                }

                if (ret == LZMA_STREAM_END) break;

                if (ret != LZMA_OK) {
                    qWarning() << "xz decoder error:" << ret;
                    lzma_end(&strm);
                    return false;
                }
            }

            lzma_end(&strm);

            return true;
        }
        qWarning() << "error opening out-file:" << file_out;
    } else {
        qWarning() << "error opening in-file:" << file_in;
    }

    return false;
}

// source: https://github.com/libarchive/libarchive/blob/master/examples/untar.c
static int copy_data(struct archive* ar, struct archive* aw) {
    int r;
    const void* buff;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return (ARCHIVE_OK);
        if (r != ARCHIVE_OK) return (r);
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            return (r);
        }
    }
}

static bool tar_decode(const QString& file_in, const QString& dir_out) {
    qDebug() << "extracting tar archive:" << file_in << dir_out;

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    archive_read_support_format_tar(a);

    bool ok = true;

    if (archive_read_open_filename(a, file_in.toStdString().c_str(), 10240)) {
        qWarning() << "error opening in-file:" << file_in
                   << archive_error_string(a);
        ok = false;
    } else {
        struct archive_entry* entry;
        while (true) {
            int ret = archive_read_next_header(a, &entry);
            if (ret == ARCHIVE_EOF) break;
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_read_next_header:" << file_in
                           << archive_error_string(a);
                ok = false;
                break;
            }

            const QString entry_path{archive_entry_pathname_utf8(entry)};
            const QString out_path = dir_out + "/" + entry_path;

            const auto std_file_out = out_path.toStdString();
            archive_entry_set_pathname(entry, std_file_out.c_str());

            ret = archive_write_header(ext, entry);
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_write_header:" << file_in
                           << archive_error_string(ext);
                ok = false;
                break;
            }

            copy_data(a, ext);
            ret = archive_write_finish_entry(ext);
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_write_finish_entry:" << file_in
                           << archive_error_string(ext);
                ok = false;
                break;
            }
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return ok;
}

inline static void setPythonPath() {
    ::setenv(
        "PYTHONPATH",
        QStringLiteral("%1/%2")
            .arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation),
                 QStringLiteral("/python3.8/site-packages"))
            .toStdString()
            .c_str(),
        true);
}
#endif

QDebug operator<<(QDebug debug, const std::string& s) {
    QDebugStateSaver saver{debug};
    debug.nospace() << s.c_str();
    return debug;
}

YtdlApi::YtdlApi(QObject* parent) : QObject{parent} {
#ifdef SAILFISH
    setPythonPath();
#endif
    init();
}

bool YtdlApi::unpack() {
#ifdef SAILFISH
    qDebug() << "unpacking ytdl modules";

    if (!QFileInfo::exists(pythonArchivePath)) {
        qWarning() << "python archive does not exist";
        return false;
    }

    auto unpackPath = pythonUnpackDir() + "/python.tar";
    if (QFileInfo::exists(unpackPath)) QFile::remove(unpackPath);

    if (!xz_decode(pythonArchivePath, unpackPath)) {
        qWarning() << "cannot extract python archive";
        return false;
    }

    if (!tar_decode(unpackPath, pythonUnpackDir())) {
        qWarning() << "cannot extract python tar archive";
        QFile::remove(unpackPath);
        return false;
    }

    QFile::remove(unpackPath);

    qDebug() << "ytdl modules successfully unpacked";
    return true;
#else
    return true;
#endif
}

bool YtdlApi::check() {
    using namespace pybind11;
    scoped_interpreter guard{};

    try {
        module::import("ytmusicapi");
        module::import("yt_dlp");
    } catch (const std::runtime_error& e) {
        qDebug() << "ytdl check failed:" << e.what();
        return false;
    }

    qDebug() << "ytdl modules work fine";
    return true;
}

bool YtdlApi::init() {
    if (state == State::Enabled) return true;
    if (state == State::Disabled) return false;

    if (check()) {
        state = State::Enabled;
        return true;
    }

    if (unpack() && check()) {
        state = State::Enabled;
        return true;
    }

    state = State::Disabled;

    return false;
}

static void logVideoInfo(const video_info::VideoInfo& info) {
    qDebug() << "extract_video_info:";
    qDebug() << "id:" << info.id;
    qDebug() << "title:" << info.title;
    qDebug() << "thumbnail:" << info.thumbnail;
    for (const auto& f : info.formats) {
        qDebug() << " -> format:";
        qDebug() << "    quality:" << f.quality.value_or(0);
        qDebug() << "    acodec:" << f.acodec;
        qDebug() << "    vcodec:" << f.vcodec;
        qDebug() << "    url:" << f.url;
    }
}

QUrl YtdlApi::bestAudioUrl(const std::vector<video_info::Format>& formats) {
    std::vector<video_info::Format> audioFormats{};
    std::copy_if(formats.cbegin(), formats.cend(),
                 std::back_inserter(audioFormats), [](const auto& format) {
                     return format.vcodec == "none" &&
                            format.acodec.find("mp4a") != std::string::npos &&
                            !format.url.empty();
                 });

    auto it = std::max_element(audioFormats.cbegin(), audioFormats.cend(),
                               [](const auto& f1, const auto& f2) {
                                   return f1.quality.value_or(0) <
                                          f2.quality.value_or(0);
                               });

    if (it == audioFormats.cend()) return {};

    return QUrl{QString::fromStdString(it->url)};
}

QUrl YtdlApi::bestVideoUrl(const std::vector<video_info::Format>& formats) {
    std::vector<video_info::Format> audioFormats{};

    std::copy_if(formats.cbegin(), formats.cend(),
                 std::back_inserter(audioFormats), [](const auto& format) {
                     return format.vcodec.find("avc1") != std::string::npos &&
                            format.acodec.find("mp4a") != std::string::npos &&
                            !format.url.empty();
                 });

    auto it = std::max_element(audioFormats.cbegin(), audioFormats.cend(),
                               [](const auto& f1, const auto& f2) {
                                   return f1.quality.value_or(0) <
                                          f2.quality.value_or(0);
                               });

    if (it == audioFormats.cend()) return {};

    return QUrl{QString::fromStdString(it->url)};
}

std::optional<YtdlApi::Track> YtdlApi::track(const QUrl& url) const {
    if (state != State::Enabled) return std::nullopt;

    auto info = YTMusic{}.extract_video_info(url.toString().toStdString());

    logVideoInfo(info);

    Track track;

    track.streamUrl = bestVideoUrl(info.formats);
    track.streamAudioUrl = bestAudioUrl(info.formats);
    if (track.streamUrl.isEmpty() && track.streamAudioUrl.isEmpty())
        return std::nullopt;

    track.title = QString::fromStdString(info.title);
    track.webUrl = url;
    track.image = QUrl{QString::fromStdString(info.thumbnail)};

    return track;
}
