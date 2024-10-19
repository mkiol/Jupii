/* Copyright (C) 2022-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "caster.hpp"

#ifdef USE_X11CAPTURE
#include <X11/Xlib.h>
#endif

#ifdef USE_V4L2
#include <dirent.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <pulse/error.h>
#include <pulse/xmalloc.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include "fftools.hpp"
#include "logger.hpp"

using namespace std::literals;

static inline int64_t rescaleToUsec(int64_t time, AVRational srcTimeBase) {
    return av_rescale_q(time, srcTimeBase, AVRational{1, 1000000});
}

static inline int64_t rescaleFromUsec(int64_t time, AVRational destTimeBase) {
    return av_rescale_q(time, AVRational{1, 1000000}, destTimeBase);
}

// static bool nearlyEqual(float a, float b) {
//     return std::nextafter(a, std::numeric_limits<float>::lowest()) <= b &&
//            std::nextafter(a, std::numeric_limits<float>::max()) >= b;
// }

static void dataToHexStr(std::ostream &os, const uint8_t *data, int size) {
    os << std::hex << std::setfill('0');
    for (int i = 0; i < size; ++i)
        os << ' ' << std::setw(2) << static_cast<int>(data[i]);
}

[[maybe_unused]] static std::string dataToStr(const uint8_t *data, int size) {
    std::stringstream ss;
    dataToHexStr(ss, data, std::min(50, size));
    return ss.str();
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 AVRational r) {
    os << r.num << "/" << r.den;
    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 AVPixelFormat fmt) {
    const auto *name = av_get_pix_fmt_name(fmt);
    if (name == nullptr)
        os << "unknown";
    else
        os << name;
    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 AVSampleFormat fmt) {
    const auto *name = av_get_sample_fmt_name(fmt);
    if (name == nullptr)
        os << "unknown";
    else
        os << name;
    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const AVPixelFormat *fmts) {
    for (int i = 0;; ++i) {
        if (fmts[i] == AV_PIX_FMT_NONE) break;
        if (i != 0) os << ", ";
        os << fmts[i];
    }

    return os;
}

[[maybe_unused]] static std::ostream &operator<<(
    std::ostream &os, const AVChannelLayout *layout) {
    if (layout == nullptr) {
        os << "null";
    } else {
        std::array<char, 512> s{};
        av_channel_layout_describe(layout, s.data(), s.size());
        os << s.data();
    }
    return os;
}

[[maybe_unused]] static auto avCodecName(AVCodecID codec) {
    const auto *desc = avcodec_descriptor_get(codec);
    if (desc == nullptr) return "unknown";
    return desc->name;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 AVCodecID codec) {
    os << avCodecName(codec);
    return os;
}

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const AVPacket *pkt) {
    os << "pts=" << pkt->pts << ", dts=" << pkt->dts
       << ", duration=" << pkt->duration << ", pos=" << pkt->pos
       << ", sidx=" << pkt->stream_index << ", tb=" << pkt->time_base
       << ", size=" << pkt->size << ", data=";
    dataToHexStr(os, pkt->data, std::min(50, pkt->size));
    return os;
}

// static void logAvDevices() {
//     const AVInputFormat *d = nullptr;
//     while (true) {
//         d = av_input_video_device_next(d);
//         if (d == nullptr) break;
//         LOGD("av device: " << d->name << " (" << d->long_name << ")");

//        AVDeviceInfoList *device_list = nullptr;
//        if (avdevice_list_input_sources(d, nullptr, nullptr, &device_list) >
//                0 &&
//            device_list != nullptr) {
//            LOGD(" default source: " << device_list->default_device);
//            for (int i = 0; i < device_list->nb_devices; ++i) {
//                LOGD(" source["
//                     << i << "]: " << device_list->devices[i]->device_name
//                     << " " << device_list->devices[i]->device_description);
//            }
//            avdevice_free_list_devices(&device_list);
//        }
//    }
//}

std::ostream &operator<<(std::ostream &os, Caster::OptionsFlags flags) {
    if (flags & Caster::OptionsFlags::OnlyNiceVideoFormats)
        os << "only-nice-video-formats, ";
    if (flags & Caster::OptionsFlags::MuteAudioSource)
        os << "mute-audio-source, ";
    if (flags & Caster::OptionsFlags::V4l2VideoSources)
        os << "v4l2-video-sources, ";
    if (flags & Caster::OptionsFlags::DroidCamVideoSources)
        os << "droidcam-video-sources, ";
    if (flags & Caster::OptionsFlags::DroidCamRawVideoSources)
        os << "droidcam-raw-video-sources, ";
    if (flags & Caster::OptionsFlags::X11CaptureVideoSources)
        os << "x11-capture-video-sources, ";
    if (flags & Caster::OptionsFlags::LipstickCaptureVideoSources)
        os << "lipstick-capture-video-sources, ";
    if (flags & Caster::OptionsFlags::PaMicAudioSources)
        os << "pa-mic-audio-sources, ";
    if (flags & Caster::OptionsFlags::PaMonitorAudioSources)
        os << "pa-monitor-audio-sources, ";
    if (flags & Caster::OptionsFlags::PaPlaybackAudioSources)
        os << "pa-playback-audio-sources, ";

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::State state) {
    switch (state) {
        case Caster::State::Initing:
            os << "initing";
            break;
        case Caster::State::Inited:
            os << "inited";
            break;
        case Caster::State::Starting:
            os << "starting";
            break;
        case Caster::State::Paused:
            os << "paused";
            break;
        case Caster::State::Started:
            os << "started";
            break;
        case Caster::State::Terminating:
            os << "terminating";
            break;
    }

    return os;
}

std::ostream &operator<<(std::ostream &os,
                         Caster::VideoOrientation videoOrientation) {
    switch (videoOrientation) {
        case Caster::VideoOrientation::Auto:
            os << "auto";
            break;
        case Caster::VideoOrientation::Landscape:
            os << "landscape";
            break;
        case Caster::VideoOrientation::Portrait:
            os << "portrait";
            break;
        case Caster::VideoOrientation::InvertedLandscape:
            os << "inverted-landscape";
            break;
        case Caster::VideoOrientation::InvertedPortrait:
            os << "inverted-portrait";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::StreamFormat streamFormat) {
    switch (streamFormat) {
        case Caster::StreamFormat::Mp4:
            os << "mp4";
            break;
        case Caster::StreamFormat::MpegTs:
            os << "mpegts";
            break;
        case Caster::StreamFormat::Mp3:
            os << "mp3";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::SensorDirection direction) {
    switch (direction) {
        case Caster::SensorDirection::Back:
            os << "back";
            break;
        case Caster::SensorDirection::Front:
            os << "front";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoEncoder encoder) {
    switch (encoder) {
        case Caster::VideoEncoder::Auto:
            os << "auto";
            break;
        case Caster::VideoEncoder::X264:
            os << "x264";
            break;
        case Caster::VideoEncoder::Nvenc:
            os << "nvenc";
            break;
        case Caster::VideoEncoder::V4l2:
            os << "v4l2";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoTrans trans) {
    switch (trans) {
        case Caster::VideoTrans::Off:
            os << "off";
            break;
        case Caster::VideoTrans::Scale:
            os << "scale";
            break;
        case Caster::VideoTrans::Vflip:
            os << "vflip";
            break;
        case Caster::VideoTrans::Hflip:
            os << "hflip";
            break;
        case Caster::VideoTrans::VflipHflip:
            os << "vflip-hflip";
            break;
        case Caster::VideoTrans::TransClock:
            os << "transpose-clock";
            break;
        case Caster::VideoTrans::TransClockFlip:
            os << "transpose-clock-flip";
            break;
        case Caster::VideoTrans::TransCclock:
            os << "transpose-cclock";
            break;
        case Caster::VideoTrans::TransCclockFlip:
            os << "transpose-cclock-flip";
            break;
        case Caster::VideoTrans::Frame169:
            os << "frame-169";
            break;
        case Caster::VideoTrans::Frame169Rot90:
            os << "frame-169-rot-90";
            break;
        case Caster::VideoTrans::Frame169Rot180:
            os << "frame-169-rot-180";
            break;
        case Caster::VideoTrans::Frame169Rot270:
            os << "frame-169-rot-270";
            break;
        case Caster::VideoTrans::Frame169Vflip:
            os << "frame-169-vflip";
            break;
        case Caster::VideoTrans::Frame169VflipRot90:
            os << "frame-169-vflip-rot-90";
            break;
        case Caster::VideoTrans::Frame169VflipRot180:
            os << "frame-169-vflip-rot-180";
            break;
        case Caster::VideoTrans::Frame169VflipRot270:
            os << "frame-169-vflip-rot-270";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::AudioTrans trans) {
    switch (trans) {
        case Caster::AudioTrans::Off:
            os << "off";
            break;
        case Caster::AudioTrans::Volume:
            os << "volume";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoScale scale) {
    switch (scale) {
        case Caster::VideoScale::Off:
            os << "off";
            break;
        case Caster::VideoScale::Down25:
            os << "down-25%";
            break;
        case Caster::VideoScale::Down50:
            os << "down-50%";
            break;
        case Caster::VideoScale::Down75:
            os << "down-75%";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoSourceType type) {
    switch (type) {
        case Caster::VideoSourceType::DroidCam:
            os << "droidcam";
            break;
        case Caster::VideoSourceType::DroidCamRaw:
            os << "droidcam-raw";
            break;
        case Caster::VideoSourceType::V4l2:
            os << "v4l2";
            break;
        case Caster::VideoSourceType::X11Capture:
            os << "x11-capture";
            break;
        case Caster::VideoSourceType::LipstickCapture:
            os << "lipstick-capture";
            break;
        case Caster::VideoSourceType::Test:
            os << "test";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::AudioSourceType type) {
    switch (type) {
        case Caster::AudioSourceType::Mic:
            os << "mic";
            break;
        case Caster::AudioSourceType::Monitor:
            os << "monitor";
            break;
        case Caster::AudioSourceType::Playback:
            os << "playback";
            break;
        case Caster::AudioSourceType::File:
            os << "file";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::Dim dim) {
    os << dim.width << "x" << dim.height;
    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoFormat format) {
    os << "codec=" << format.codecId << ", pixfmt=" << format.pixfmt;
    return os;
}

std::ostream &operator<<(std::ostream &os,
                         const Caster::VideoFormatExt &format) {
    os << "codec=" << format.codecId << ", pixfmt=" << format.pixfmt << ": ";
    for (const auto &s : format.frameSpecs)
        os << fmt::format("(size={}x{}, fr=[{}]), ", s.dim.width, s.dim.height,
                          fmt::join(s.framerates, ","));
    return os;
}

std::ostream &operator<<(std::ostream &os,
                         const Caster::VideoSourceInternalProps &props) {
    os << "type=" << props.type << ", name=" << props.name
       << ", fname=" << props.friendlyName << ", dev=" << props.dev
       << ", orientation=" << props.orientation
       << ", sensor-direction=" << props.sensorDirection
       << ", trans=" << props.trans << ", scale=" << props.scale
       << ", formats=(";
    if (!props.formats.empty())
        for (const auto &f : props.formats) os << "[" << f << "], ";
    os << ")";

    return os;
}

std::ostream &operator<<(std::ostream &os,
                         const Caster::V4l2H264EncoderProps &props) {
    os << "dev=" << props.dev << ", formats=(";
    if (!props.formats.empty())
        for (const auto &f : props.formats) os << "[" << f << "], ";
    os << ")";
    return os;
}

std::ostream &operator<<(std::ostream &os,
                         const Caster::AudioSourceInternalProps &props) {
    os << "type=" << props.type << ", name=" << props.name
       << ", fname=" << props.friendlyName << ", dev=" << props.dev
       << ", codec=" << props.codec
       << ", channels=" << static_cast<int>(props.channels)
       << ", rate=" << props.rate << ", bps=" << props.bps;
    return os;
}

std::ostream &operator<<(std::ostream &os, const Caster::PaClient &client) {
    os << "idx=" << client.idx << ", name=" << client.name
       << ", bin=" << client.bin;
    return os;
}

std::ostream &operator<<(std::ostream &os, const Caster::PaSinkInput &input) {
    os << "idx=" << input.idx << ", name=" << input.name
       << ", client idx=" << input.clientIdx << ", sink idx=" << input.sinkIdx
       << ", corked=" << input.corked << ", muted=" << input.muted
       << ", removed=" << input.removed;
    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::FileSourceFlags flags) {
    if (flags & Caster::FileSourceFlags::Loop) os << "loop, ";
    if (flags & Caster::FileSourceFlags::SameFormatForAllFiles)
        os << "same-format-for-all-files, ";

    return os;
}

std::ostream &operator<<(std::ostream &os,
                         const Caster::FileSourceConfig &config) {
    os << "files=";
    if (!config.files.empty())
        for (const auto &f : config.files) os << "[" << f << "], ";
    else
        os << "empty";
    os << ", flags=[" << config.flags << "]";
    return os;
}

std::ostream &operator<<(std::ostream &os, const Caster::Config &config) {
    os << "stream-format=" << config.streamFormat << ", video-source="
       << (config.videoSource.empty() ? "off" : config.videoSource)
       << ", audio-source="
       << (config.audioSource.empty() ? "off" : config.audioSource)
       << ", video-orientation=" << config.videoOrientation
       << ", audio-volume=" << std::to_string(config.audioVolume)
       << ", stream-author=" << config.streamAuthor
       << ", stream-title=" << config.streamTitle
       << ", video-encoder=" << config.videoEncoder << ", options=["
       << static_cast<Caster::OptionsFlags>(config.options) << "]";
    if (config.fileSourceConfig) os << ", " << *config.fileSourceConfig;
    return os;
}

bool Caster::configValid(const Config &config) const {
    if (!config.videoSource.empty() &&
        m_videoProps.count(config.videoSource) == 0) {
        LOGW("video-source is invalid");
        return false;
    }

    if (!config.audioSource.empty() &&
        m_audioProps.count(config.audioSource) == 0) {
        LOGW("audio-source is invalid");
        return false;
    }

    if (!config.audioSource.empty() &&
        m_audioProps.at(config.audioSource).type == AudioSourceType::File &&
        (!config.fileSourceConfig || config.fileSourceConfig->files.empty())) {
        LOGW("no file provided for file source");
        return false;
    }

    if (config.videoSource.empty() && config.audioSource.empty()) {
        LOGW("both video-source and audio-source cannot be empty");
        return false;
    }

    if (config.videoOrientation != VideoOrientation::Auto &&
        config.videoOrientation != VideoOrientation::Landscape &&
        config.videoOrientation != VideoOrientation::InvertedLandscape &&
        config.videoOrientation != VideoOrientation::Portrait &&
        config.videoOrientation != VideoOrientation::InvertedPortrait) {
        LOGW("video-orientation is invalid");
        return false;
    }

    if (config.videoEncoder != VideoEncoder::Auto &&
        config.videoEncoder != VideoEncoder::Nvenc &&
        config.videoEncoder != VideoEncoder::V4l2 &&
        config.videoEncoder != VideoEncoder::X264) {
        LOGW("video-encoder is invalid");
        return false;
    }

    if (config.streamFormat != StreamFormat::Mp4 &&
        config.streamFormat != StreamFormat::MpegTs &&
        config.streamFormat != StreamFormat::Mp3) {
        LOGW("stream-format is invalid");
        return false;
    }

    if (config.streamFormat == StreamFormat::Mp3 &&
        !config.videoSource.empty()) {
        LOGW("stream-format does not support video");
        return false;
    }

    if (config.audioVolume < -50 || config.audioVolume > 50) {
        LOGW("audio-volume is invalid");
        return false;
    }

    if (config.streamAuthor.empty()) {
        LOGW("stream-author is invalid");
        return false;
    }

    if (config.streamTitle.empty()) {
        LOGW("stream-title is invalid");
        return false;
    }

    return true;
}

Caster::Caster(Config config, DataReadyHandler dataReadyHandler,
               StateChangedHandler stateChangedHandler,
               AudioSourceNameChangedHandler audioSourceNameChangedHandler)
    : m_config{std::move(config)}, m_dataReadyHandler{std::move(
                                       dataReadyHandler)},
      m_stateChangedHandler{std::move(stateChangedHandler)},
      m_audioSourceNameChangedHandler{
          std::move(audioSourceNameChangedHandler)} {
    LOGD("creating caster, config: " << m_config);

    try {
        detectSources(m_config.options);
#ifdef USE_V4L2
        detectV4l2Encoders();
#endif
        if (!configValid(m_config))
            throw std::runtime_error("invalid configuration");

        LOGD("audio enabled: " << audioEnabled());
        LOGD("video enabled: " << videoEnabled());

        if (audioEnabled()) initAudioSource();
        if (videoEnabled()) initVideoSource();

        initAv();
    } catch (...) {
        clean();
        throw;
    }
}

Caster::~Caster() {
    LOGD("caster termination started");
    setState(State::Terminating, false);
    m_videoCv.notify_all();
#ifdef USE_DROIDCAM
    m_droidCamSource.reset();
    m_orientationMonitor.reset();
#endif
#ifdef USE_LIPSTICK_RECORDER
    m_lipstickRecorder.reset();
#endif
    clean();
    LOGD("caster termination completed");
}

void Caster::initAudioSource() {
    initAudioTrans();

    const auto &props = audioProps();

    switch (props.type) {
        case AudioSourceType::Mic:
        case AudioSourceType::Monitor:
        case AudioSourceType::Playback:
            initPa();
            break;
        case AudioSourceType::File:
            initFiles();
            break;
        default:
            break;
    }
}

void Caster::initVideoSource() {
    initVideoTrans();

    const auto &props = videoProps();

    if (props.type == VideoSourceType::Test)
        m_imageProvider.emplace([this](const uint8_t *data, size_t size) {
            rawVideoDataReadyHandler(data, size);
        });
#ifdef USE_LIPSTICK_RECORDER
    if (props.type == VideoSourceType::LipstickCapture)
        m_lipstickRecorder.emplace(
            [this](const uint8_t *data, size_t size) {
                rawVideoDataReadyHandler(data, size);
            },
            [this] {
                LOGE("error in lipstick-recorder");
                reportError();
            });
#endif
#ifdef USE_DROIDCAM
    if (props.type == VideoSourceType::DroidCamRaw) {
        m_orientationMonitor.emplace();
        m_droidCamSource.emplace(
            false, std::stoi(props.dev),
            [this](const uint8_t *data, size_t size) {
                rawVideoDataReadyHandler(data, size);
            },
            [this] {
                LOGE("error in droidcam-source");
                reportError();
            });
    } else if (props.type == VideoSourceType::DroidCam) {
        m_droidCamSource.emplace(
            true, std::stoi(props.dev),
            [this](const uint8_t *data, size_t size) {
                compressedVideoDataReadyHandler(data, size);
            },
            [this] {
                LOGE("error in droidcam-source");
                reportError();
            });
    }
#endif
}

void Caster::reportError() {
    if (m_terminationReason == TerminationReason::Unknown)
        m_terminationReason = TerminationReason::Error;
    setState(State::Terminating);
    m_videoCv.notify_all();
}

template <typename Dev>
static bool sortByName(const Dev &rhs, const Dev &lhs) {
    return lhs.name > rhs.name;
}

void Caster::initVideoTrans() {
    const auto &props = videoProps();

    switch (props.type) {
        case VideoSourceType::Unknown:
        case VideoSourceType::DroidCam:
            return;
        case VideoSourceType::V4l2:
        case VideoSourceType::X11Capture:
        case VideoSourceType::LipstickCapture:
        case VideoSourceType::Test:
        case VideoSourceType::DroidCamRaw:
            break;
    }

    m_videoTrans = orientationToTrans(m_config.videoOrientation, props);

    if (props.trans == VideoTrans::Frame169)
        m_videoTrans = VideoTrans::Frame169;

    LOGD("initial video trans: " << m_videoTrans);
}

void Caster::initAudioTrans() {
    m_audioTrans = AudioTrans::Volume;

    LOGD("initial audio trans: " << m_audioTrans);
}

std::vector<Caster::VideoSourceProps> Caster::videoSources(uint32_t options) {
    decltype(videoSources()) sources;

    auto props = detectVideoSources(options);
    sources.reserve(props.size());

    std::transform(
        props.begin(), props.end(), std::back_inserter(sources), [](auto &p) {
            return VideoSourceProps{std::move(p.second.name),
                                    std::move(p.second.friendlyName)};
        });

    std::sort(sources.begin(), sources.end(), sortByName<VideoSourceProps>);

    return sources;
}

std::vector<Caster::AudioSourceProps> Caster::audioSources(uint32_t options) {
    decltype(audioSources()) sources;

    auto props = detectAudioSources(options);
    sources.reserve(props.size());

    std::transform(
        props.begin(), props.end(), std::back_inserter(sources),
        [](auto &p) -> AudioSourceProps {
            return {std::move(p.second.name), std::move(p.second.friendlyName)};
        });

    std::sort(sources.begin(), sources.end(), sortByName<AudioSourceProps>);

    return sources;
}

void Caster::detectSources(uint32_t options) {
    m_audioProps = detectAudioSources(options);
    m_videoProps = detectVideoSources(options);
}

Caster::AudioPropsMap Caster::detectAudioSources(uint32_t options) {
    AudioPropsMap props;

    if (options & OptionsFlags::PaMicAudioSources ||
        options & OptionsFlags::PaMonitorAudioSources ||
        options & OptionsFlags::PaPlaybackAudioSources)
        props.merge(detectPaSources(options));

    if (options & OptionsFlags::FileAudioSources)
        props.merge(detectAudioFileSources());

    return props;
}

void Caster::paSourceInfoCallback([[maybe_unused]] pa_context *ctx,
                                  const pa_source_info *info, int eol,
                                  void *userdata) {
    auto *result = static_cast<AudioSourceSearchResult *>(userdata);
    if (eol) {  // done
        result->done = true;
        return;
    }

    if (info->monitor_of_sink == PA_INVALID_INDEX &&
        info->active_port == nullptr)
        return;  // ignoring not-monitor without active
                 // ports

#ifdef USE_SFOS
    if (info->monitor_of_sink != PA_INVALID_INDEX) {
        LOGD("ignoring pa monitor on sfos: " << info->name);
        return;
    }

    if (strcmp("source.primary_input", info->name) != 0 &&
        strcmp("source.droid", info->name) != 0) {
        LOGD("ignoring pa source on sfos: " << info->name);
        return;
    }
#endif

    bool mic = info->monitor_of_sink == PA_INVALID_INDEX;

    if (mic && !(result->options & OptionsFlags::PaMicAudioSources)) return;
    if (!mic && !(result->options & OptionsFlags::PaMonitorAudioSources))
        return;

    Caster::AudioSourceInternalProps props{
#ifdef USE_SFOS
        /*name=*/"mic",
        /*dev=*/info->name,
        /*friendlyName=*/"Microphone",
#else
        /*name=*/mic ? fmt::format("mic-{:03}", hash(info->name))
                     : fmt::format("monitor-{:03}", hash(info->name)),
        /*dev=*/info->name,
        /*friendlyName=*/info->description,
#endif
        /*codec=*/
        ff_tools::ff_pulse_format_to_codec_id(info->sample_spec.format),
        /*channels=*/info->sample_spec.channels,
        /*rate=*/info->sample_spec.rate,
        /*bps=*/
        pa_sample_size(&info->sample_spec),
        /*type=*/mic ? AudioSourceType::Mic : AudioSourceType::Monitor};

    if (props.codec == AV_CODEC_ID_NONE) {
        LOGW("invalid codec: " << props.dev);
        return;
    }

    LOGD("pa source found: " << props);

    result->propsMap.try_emplace(props.name, std::move(props));
}

std::optional<std::reference_wrapper<Caster::PaSinkInput>>
Caster::bestPaSinkInput() {
    if (m_paSinkInputs.count(m_connectedPaSinkInput) > 0) {
        auto &si = m_paSinkInputs.at(m_connectedPaSinkInput);
        if (!si.removed && !si.corked) {
            LOGD("best pa sink input is current sink input");
            return si;
        }
    }

    for (auto it = m_paSinkInputs.begin(); it != m_paSinkInputs.end(); ++it) {
        if (!it->second.removed && !it->second.corked) return it->second;
    }

    return std::nullopt;
}

Caster::AudioPropsMap Caster::detectPaSources(uint32_t options) {
    LOGD("pa sources detection started");

    auto *loop = pa_mainloop_new();
    if (loop == nullptr) throw std::runtime_error("pa_mainloop_new error");

    auto *mla = pa_mainloop_get_api(loop);

    auto *ctx = pa_context_new(mla, "caster");
    if (ctx == nullptr) {
        pa_mainloop_free(loop);
        throw std::runtime_error("pa_context_new error");
    }

    AudioSourceSearchResult result;
    result.options = options;

    pa_context_set_state_callback(
        ctx,
        [](pa_context *ctx, void *userdata) {
            if (pa_context_get_state(ctx) == PA_CONTEXT_READY) {
                pa_operation_unref(pa_context_get_source_info_list(
                    ctx, paSourceInfoCallback, userdata));
            }
        },
        &result);

    if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        auto err = pa_context_errno(ctx);
        pa_context_unref(ctx);
        pa_mainloop_free(loop);
        throw std::runtime_error("pa_context_connect error: "s +
                                 pa_strerror(err));
    }

    while (true) {
        if (result.done || pa_mainloop_iterate(loop, 0, nullptr) < 0) break;
    }

    if (options & OptionsFlags::PaPlaybackAudioSources) {
        Caster::AudioSourceInternalProps props{
            /*name=*/"playback",
            /*dev=*/{},
            /*friendlyName=*/"Playback capture",
            /*codec=*/AV_CODEC_ID_PCM_S16LE,
            /*channels=*/2,
            /*rate=*/44100,
            /*bps=*/2,
            /*type=*/AudioSourceType::Playback};
        LOGD("pa source found: " << props);
        result.propsMap.try_emplace(props.name, std::move(props));
    }

    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(loop);

    LOGD("pa sources detection completed");

    return std::move(result.propsMap);
}

Caster::AudioPropsMap Caster::detectAudioFileSources() {
    LOGD("audio file sources detection started");

    AudioPropsMap propsMap;

    Caster::AudioSourceInternalProps props;
    props.name = "file";
    props.friendlyName = "File streaming";
    props.type = AudioSourceType::File;

    LOGD("audio file source found: " << props);

    propsMap.try_emplace(props.name, std::move(props));

    LOGD("audio file sources detection completed");

    return propsMap;
}

bool Caster::audioBoosted() const { return m_config.audioVolume != 0; }

void Caster::setState(State newState, bool notify) {
    if (m_state != newState) {
        LOGD("changing state: " << m_state << " => " << newState);
        m_state = newState;
        if (notify && m_stateChangedHandler) m_stateChangedHandler(newState);
    }
}

void Caster::start(bool startPaused) {
    if (m_state == State::Paused && !startPaused) {
        LOGW("resuming instead of starting");
        resume();
        return;
    }

    if (m_state != State::Inited) {
        LOGW("start is only possible in inited state");
        return;
    }

    setState(State::Starting);

    try {
        if (videoEnabled()) startVideoSource();

        startAv();

        if (audioEnabled()) startAudioSource();

        if (startPaused) {
            setState(State::Paused);
        } else {
            setState(State::Started);
            startMuxing();
        }
    } catch (const std::runtime_error &e) {
        LOGW("failed to start: " << e.what());
        reportError();
    }
}

void Caster::startVideoSource() {
    const auto &props = videoProps();

    if (props.type == VideoSourceType::Test) m_imageProvider->start();
#ifdef USE_LIPSTICK_RECORDER
    if (m_lipstickRecorder) m_lipstickRecorder->start();
#endif
#ifdef USE_DROIDCAM
    if (m_orientationMonitor) m_orientationMonitor->start();
    if (m_droidCamSource) m_droidCamSource->start();
#endif
}

void Caster::startAudioSource() {
    const auto &props = audioProps();

    switch (props.type) {
        case AudioSourceType::Mic:
        case AudioSourceType::Monitor:
        case AudioSourceType::Playback:
            startPa();
            break;
        default:
            break;
    }
}

void Caster::pause() {
    if (m_state != State::Started) {
        LOGW("pause is only possible in not started state");
        return;
    }

    setState(State::Paused);

    m_videoCv.notify_all();
    if (m_avMuxingThread.joinable()) m_avMuxingThread.join();
}

void Caster::resume() {
    if (m_state != State::Paused) {
        LOGW("resume is only possible in paused state");
        return;
    }

    reInitAvOutputFormat();
    setState(State::Started);
    startMuxing();
}

void Caster::clean() {
    if (m_avMuxingThread.joinable()) m_avMuxingThread.join();
    LOGD("av muxing thread joined");
    if (m_audioPaThread.joinable()) m_audioPaThread.join();
    LOGD("pa thread joined");
    cleanPa();
    LOGD("pa cleaned");
    cleanAv();
    LOGD("av cleaned");
}

std::string Caster::strForAvOpts(const AVDictionary *opts) {
    if (opts == nullptr) return {};

    std::ostringstream os;

    AVDictionaryEntry *t = nullptr;
    while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
        os << "[" << t->key << "=" << t->value << "],";
    }

    return os.str();
}

void Caster::cleanAvOutputFormat() {
    if (m_outFormatCtx != nullptr) {
        if (m_outFormatCtx->pb != nullptr) {
            if (m_outFormatCtx->pb->buffer != nullptr &&
                m_outFormatCtx->flags & AVFMT_FLAG_CUSTOM_IO)
                av_freep(&m_outFormatCtx->pb->buffer);
            avio_context_free(&m_outFormatCtx->pb);
        }
        avformat_free_context(m_outFormatCtx);
        m_outFormatCtx = nullptr;
    }
}

void Caster::cleanAvAudioInputFormat() {
    if (m_inAudioFormatCtx != nullptr) {
        avformat_close_input(&m_inAudioFormatCtx);
    }
}

void Caster::cleanAvVideoInputFormat() {
    if (m_inVideoFormatCtx != nullptr) {
        if (m_inVideoFormatCtx->pb != nullptr &&
            m_inVideoFormatCtx->flags & AVFMT_FLAG_CUSTOM_IO) {
            if (m_inVideoFormatCtx->pb->buffer != nullptr)
                av_freep(&m_inVideoFormatCtx->pb->buffer);
            avio_context_free(&m_inVideoFormatCtx->pb);
        }
        avformat_close_input(&m_inVideoFormatCtx);
    }
}

void Caster::cleanAvAudioDecoder() {
    if (m_audioFrameIn != nullptr) av_frame_free(&m_audioFrameIn);
    if (m_inAudioCtx != nullptr) avcodec_free_context(&m_inAudioCtx);
}

void Caster::cleanAvAudioEncoder() {
    if (m_outAudioCtx != nullptr) avcodec_free_context(&m_outAudioCtx);
}

void Caster::cleanAvAudioFifo() {
    if (m_audioFifo != nullptr) {
        av_audio_fifo_free(m_audioFifo);
        m_audioFifo = nullptr;
    }
}

void Caster::cleanAvVideoFilters() {
    for (auto &p : m_videoFilterCtxMap) {
        if (p.second.in != nullptr) avfilter_inout_free(&p.second.in);
        if (p.second.out != nullptr) avfilter_inout_free(&p.second.out);
        if (p.second.graph != nullptr) avfilter_graph_free(&p.second.graph);
    }
}

void Caster::cleanAvAudioFilters() {
    for (auto &p : m_audioFilterCtxMap) {
        if (p.second.in != nullptr) avfilter_inout_free(&p.second.in);
        if (p.second.out != nullptr) avfilter_inout_free(&p.second.out);
        if (p.second.graph != nullptr) avfilter_graph_free(&p.second.graph);
    }
}

void Caster::cleanAv() {
    cleanAvVideoFilters();
    cleanAvAudioFilters();

    if (m_videoFrameIn != nullptr) av_frame_free(&m_videoFrameIn);
    if (m_videoFrameAfterFilter != nullptr)
        av_frame_free(&m_videoFrameAfterFilter);

    if (m_audioFrameIn != nullptr) av_frame_free(&m_audioFrameIn);
    if (m_audioFrameAfterFilter != nullptr)
        av_frame_free(&m_audioFrameAfterFilter);

    cleanAvOutputFormat();
    cleanAvVideoInputFormat();
    cleanAvAudioInputFormat();
    cleanAvAudioEncoder();
    cleanAvAudioDecoder();
    cleanAvAudioFifo();

    if (m_outVideoCtx != nullptr) avcodec_free_context(&m_outVideoCtx);
    if (m_inVideoCtx != nullptr) avcodec_free_context(&m_inVideoCtx);

    m_outAudioStream = nullptr;
    m_outVideoStream = nullptr;
}

void Caster::unmuteAllPaSinkInputs() {
    for (auto it = m_paSinkInputs.begin(); it != m_paSinkInputs.end(); ++it) {
        if (it->second.muted) unmutePaSinkInput(it->second);
    }

    while (true) {
        auto ret = pa_mainloop_iterate(m_paLoop, 0, nullptr);
        if (ret <= 0) break;
    }
}

void Caster::cleanPa() {
    if (m_paCtx != nullptr) {
        if (m_paStream != nullptr) {
            pa_stream_disconnect(m_paStream);
            pa_stream_unref(m_paStream);
            m_paStream = nullptr;
        }

        unmuteAllPaSinkInputs();

        pa_context_unref(m_paCtx);
        m_paCtx = nullptr;
    }

    if (m_paLoop != nullptr) {
        pa_mainloop_free(m_paLoop);
        m_paLoop = nullptr;
    }
}

void Caster::cleanAvOpts(AVDictionary **opts) {
    if (*opts != nullptr) {
        LOGW("rejected av options: " << strForAvOpts(*opts));
        av_dict_free(opts);
    }
}

void Caster::paSubscriptionCallback(pa_context *ctx,
                                    pa_subscription_event_type_t t,
                                    uint32_t idx, void *userdata) {
    auto *caster = static_cast<Caster *>(userdata);

    if (caster->terminating()) return;

    auto facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    auto type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    switch (facility) {
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if (type == PA_SUBSCRIPTION_EVENT_NEW ||
                type == PA_SUBSCRIPTION_EVENT_CHANGE) {
                if (type == PA_SUBSCRIPTION_EVENT_NEW)
                    LOGD("pa sink input created: " << idx);
                else
                    LOGD("pa sink input changed: " << idx);
                pa_operation_unref(pa_context_get_sink_input_info(
                    ctx, idx, paSinkInputInfoCallback, userdata));
            } else if (type == PA_SUBSCRIPTION_EVENT_REMOVE)
                if (caster->m_paSinkInputs.count(idx) > 0) {
                    LOGD("pa sink input removed: " << idx);
                    if (caster->m_paSinkInputs.count(idx))
                        caster->m_paSinkInputs.at(idx).removed = true;
                    caster->reconnectPaSinkInput();
                }
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            if (type == PA_SUBSCRIPTION_EVENT_NEW ||
                type == PA_SUBSCRIPTION_EVENT_CHANGE) {
                if (type == PA_SUBSCRIPTION_EVENT_NEW)
                    LOGD("pa client created: " << idx);
                else
                    LOGD("pa client changed: " << idx);
                pa_operation_unref(pa_context_get_client_info(
                    ctx, idx, paClientInfoCallback, userdata));
            } else if (type == PA_SUBSCRIPTION_EVENT_REMOVE)
                if (caster->m_paClients.count(idx) > 0) {
                    LOGD("pa client removed: " << idx);
                    caster->m_paClients.erase(idx);
                }
            break;
    }
}

bool Caster::paClientShouldBeIgnored(
    [[maybe_unused]] const pa_client_info *info) {
    bool me = [&] {
        const auto *cpid =
            pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_ID);
        if (cpid == nullptr) return true;
        static auto pid = ::getpid();
        return strtoimax(cpid, nullptr, 10) == pid;
    }();

    if (me) return true;

#ifdef USE_SFOS
    if (!strcmp(info->name, "ngfd") || !strcmp(info->name, "feedback-event") ||
        !strcmp(info->name, "keyboard_0") ||
        !strcmp(info->name, "keyboard_1") ||
        !strcmp(info->name, "ngf-tonegen-plugin") ||
        !strcmp(info->name, "jolla keyboard")) {
        return true;
    }
#endif
    if (!strcmp(info->name, "speech-dispatcher")) return true;

    return false;
}

void Caster::paClientInfoCallback([[maybe_unused]] pa_context *ctx,
                                  const pa_client_info *info, int eol,
                                  void *userdata) {
    if (eol || paClientShouldBeIgnored(info)) return;

    auto *caster = static_cast<Caster *>(userdata);

    auto &client = caster->m_paClients[info->index];

    client.idx = info->index;
    client.name = info->name;

    if (const auto *binary = pa_proplist_gets(
            info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY)) {
        client.bin = binary;
    }

    LOGD("pa client: " << client);

    //    auto *props = pa_proplist_to_string(info->proplist);
    //    LOGD(" props:\n" << props);
    //    pa_xfree(props);
}

void Caster::paSinkInputInfoCallback([[maybe_unused]] pa_context *ctx,
                                     const pa_sink_input_info *info, int eol,
                                     void *userdata) {
    if (eol) return;

    auto *caster = static_cast<Caster *>(userdata);

    if (caster->m_paClients.count(info->client) == 0) return;

    auto &input = caster->m_paSinkInputs[info->index];

    input.idx = info->index;
    input.name = info->name;
    input.clientIdx = info->client;
    input.corked = info->corked != 0;
    if (!input.muted) input.sinkIdx = info->sink;

    LOGD("pa sink input: " << input);

    caster->reconnectPaSinkInput();
}

void Caster::reconnectPaSinkInput() {
    if (!m_audioPaThread.joinable()) return;  // muxing not started yet
    if (audioProps().type != AudioSourceType::Playback) return;
    connectPaSinkInput();
}

std::string Caster::paCorrectedClientName(uint32_t idx) const {
    if (m_paClients.count(idx) == 0) return {};

    const auto &client = m_paClients.at(idx);

#ifdef USE_SFOS
    if (client.name == "CubebUtils" && !client.bin.empty()) return client.bin;

    if (client.name == "aliendalvik_audio_glue" ||
        client.name == "AlienAudioService") {
        return "Android";
    }
#endif

    return client.name;
}

void Caster::paStateCallback(pa_context *ctx, [[maybe_unused]] void *userdata) {
    auto *caster = static_cast<Caster *>(userdata);

    if (caster->terminating()) return;

    switch (pa_context_get_state(ctx)) {
        case PA_CONTEXT_CONNECTING:
            LOGD("pa connecting");
            break;
        case PA_CONTEXT_AUTHORIZING:
            LOGD("pa authorizing");
            break;
        case PA_CONTEXT_SETTING_NAME:
            LOGD("pa setting name");
            break;
        case PA_CONTEXT_READY:
            LOGD("pa ready");
            break;
        case PA_CONTEXT_TERMINATED:
            LOGD("pa terminated");
            break;
        case PA_CONTEXT_FAILED:
            LOGD("pa failed");
            throw std::runtime_error("pa failed");
        default:
            LOGD("pa unknown state");
    }
}

void Caster::initPa() {
    LOGD("pa init started");

    m_paLoop = pa_mainloop_new();
    if (m_paLoop == nullptr) throw std::runtime_error("pa_mainloop_new error");

    auto *mla = pa_mainloop_get_api(m_paLoop);

    m_paCtx = pa_context_new(mla, m_config.streamAuthor.c_str());
    if (m_paCtx == nullptr) throw std::runtime_error("pa_context_new error");

    if (pa_context_connect(m_paCtx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        throw std::runtime_error("pa_context_connect error: "s +
                                 pa_strerror(pa_context_errno(m_paCtx)));
    }

    pa_context_set_state_callback(m_paCtx, paStateCallback, this);

    while(true) {
        auto ret = pa_mainloop_iterate(m_paLoop, 0, nullptr);
        auto state = pa_context_get_state(m_paCtx);
        if (ret < 0 || state == PA_CONTEXT_FAILED ||
            state == PA_CONTEXT_TERMINATED)
            throw std::runtime_error("pa error");
        if (state == PA_CONTEXT_READY) break;
    }

    if (audioProps().type == AudioSourceType::Playback) {
        pa_context_set_subscribe_callback(m_paCtx, paSubscriptionCallback,
                                          this);
        auto mask = static_cast<pa_subscription_mask_t>(
            PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_CLIENT);

        auto *op = pa_context_subscribe(
            m_paCtx, mask,
            [](pa_context *ctx, int success, void *userdata) {
                if (success) {
                    pa_operation_unref(pa_context_get_client_info_list(
                        ctx, paClientInfoCallback, userdata));
                    pa_operation_unref(pa_context_get_sink_input_info_list(
                        ctx, paSinkInputInfoCallback, userdata));
                }
            },
            this);
        if (op == nullptr)
            throw std::runtime_error("pa_context_subscribe error");
        pa_operation_unref(op);
    }

    LOGD("pa init completed");
}

void Caster::initAvAudioRawDecoderFromInputStream() {
    LOGD("initing audio decoder");

    const auto *stream = m_inAudioFormatCtx->streams[m_inAudioStreamIdx];

    const auto *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (decoder == nullptr)
        throw std::runtime_error("avcodec_find_decoder for audio error");

    if (decoder->sample_fmts == nullptr ||
        decoder->sample_fmts[0] == AV_SAMPLE_FMT_NONE)
        throw std::runtime_error(
            "audio decoder does not support any sample fmts");

    LOGD("sample fmts supported by audio decoder:");
    for (int i = 0; decoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        LOGD("[" << i << "]: " << decoder->sample_fmts[i]);
    }

    m_inAudioCtx = avcodec_alloc_context3(decoder);
    if (m_inAudioCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for in audio error");

    if (avcodec_parameters_to_context(m_inAudioCtx, stream->codecpar) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_to_context for audio error");
    }

    m_inAudioCtx->time_base = AVRational{1, m_inAudioCtx->sample_rate};

    if (avcodec_open2(m_inAudioCtx, nullptr, nullptr) != 0)
        throw std::runtime_error("avcodec_open2 for in audio error");

    LOGD("audio decoder: sample fmt="
         << m_inAudioCtx->sample_fmt
         << ", channel_layout=" << &m_inAudioCtx->ch_layout
         << ", sample_rate=" << m_inAudioCtx->sample_rate);

    m_audioFrameIn = av_frame_alloc();
    if (m_audioFrameIn == nullptr)
        throw std::runtime_error("av_frame_alloc error");
}

void Caster::initAvAudioRawDecoderFromProps() {
    LOGD("initing audio decoder");

    const auto &props = audioProps();

    const auto *decoder = avcodec_find_decoder(props.codec);
    if (decoder == nullptr)
        throw std::runtime_error("avcodec_find_decoder for audio error");

    if (decoder->sample_fmts == nullptr ||
        decoder->sample_fmts[0] == AV_SAMPLE_FMT_NONE)
        throw std::runtime_error(
            "audio decoder does not support any sample fmts");

    LOGD("sample fmts supported by audio decoder:");
    for (int i = 0; decoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        LOGD("[" << i << "]: " << decoder->sample_fmts[i]);
    }

    m_inAudioCtx = avcodec_alloc_context3(decoder);
    if (m_inAudioCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for in audio error");

    av_channel_layout_default(&m_inAudioCtx->ch_layout, props.channels);
    m_inAudioCtx->sample_rate = static_cast<int>(props.rate);
    m_inAudioCtx->sample_fmt = decoder->sample_fmts[0];
    m_inAudioCtx->time_base = AVRational{1, m_inAudioCtx->sample_rate};

    LOGD("audio decoder: sample fmt="
         << m_inAudioCtx->sample_fmt
         << ", channel_layout=" << &m_inAudioCtx->ch_layout << ", sample_rate="
         << m_inAudioCtx->sample_rate << ", tb=" << m_inAudioCtx->time_base);

    if (avcodec_open2(m_inAudioCtx, nullptr, nullptr) != 0)
        throw std::runtime_error("avcodec_open2 for in audio error");

    LOGD("audio decoder: frame size=" << m_inAudioCtx->frame_size);

    m_audioFrameIn = av_frame_alloc();
    if (m_audioFrameIn == nullptr)
        throw std::runtime_error("av_frame_alloc error");
}

void Caster::setAudioEncoderOpts(AudioEncoder encoder, AVDictionary **opts) {
    switch (encoder) {
        case AudioEncoder::Aac:
            av_dict_set(opts, "aac_coder", "fast", 0);
            break;
        case AudioEncoder::Mp3Lame:
            av_dict_set(opts, "b", "128k", 0);
            av_dict_set(opts, "compression_level", "9", 0);
            break;
        default:
            break;
    }
}

void Caster::initAvAudioEncoder() {
    LOGD("initing audio encoder");

    auto type = m_config.streamFormat == StreamFormat::Mp3
                    ? AudioEncoder::Mp3Lame
                    : AudioEncoder::Aac;

    const auto *encoder =
        avcodec_find_encoder_by_name(audioEncoderAvName(type).c_str());
    if (!encoder)
        throw std::runtime_error("no audio encoder: "s +
                                 audioEncoderAvName(type));

    m_outAudioCtx = avcodec_alloc_context3(encoder);
    if (m_outAudioCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for out audio error");

    m_outAudioCtx->sample_fmt =
        bestAudioSampleFormat(encoder, m_inAudioCtx->sample_fmt);

    av_channel_layout_default(&m_outAudioCtx->ch_layout,
                              m_inAudioCtx->ch_layout.nb_channels == 1 ? 1 : 2);
    m_outAudioCtx->sample_rate = m_inAudioCtx->sample_rate;
    m_outAudioCtx->time_base = AVRational{1, m_outAudioCtx->sample_rate};

    LOGD("audio encoder: sample fmt="
         << m_outAudioCtx->sample_fmt
         << ", ch_layout=" << &m_outAudioCtx->ch_layout << ", sample_rate="
         << m_outAudioCtx->sample_rate << ", tb=" << m_outAudioCtx->time_base);

    AVDictionary *opts = nullptr;

    setAudioEncoderOpts(type, &opts);

    if (avcodec_open2(m_outAudioCtx, encoder, &opts) < 0) {
        av_dict_free(&opts);
        throw std::runtime_error("avcodec_open2 for out audio error");
    }

    LOGD("audio encoder: frame size=" << m_outAudioCtx->frame_size
                                      << ", tb=" << m_outAudioCtx->time_base);

    cleanAvOpts(&opts);
}

void Caster::initAvVideoForGst() {
    LOGD("initing video for gst");

    auto *in_buf = static_cast<uint8_t *>(av_malloc(m_videoBufSize));
    if (in_buf == nullptr) {
        av_freep(&in_buf);
        throw std::runtime_error("unable to allocate in av buf");
    }

    auto *in_ctx = avformat_alloc_context();

    in_ctx->max_analyze_duration = m_avMaxAnalyzeDuration;
    in_ctx->probesize = m_avProbeSize;

    in_ctx->pb =
        avio_alloc_context(in_buf, m_videoBufSize, 0, this,
                           avReadPacketCallbackStatic, nullptr, nullptr);
    if (in_ctx->pb == nullptr) {
        avformat_free_context(in_ctx);
        av_freep(&in_buf);
        throw std::runtime_error("avio_alloc_context error");
    }

    in_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    m_inVideoFormatCtx = in_ctx;

    m_videoFramerate = static_cast<int>(
        *videoProps().formats.front().frameSpecs.front().framerates.begin());
}

Caster::Dim Caster::computeTransDim(Dim dim, VideoTrans trans,
                                    VideoScale scale) {
    Dim outDim;

    auto factor = [scale]() {
        switch (scale) {
            case VideoScale::Off:
                return 1.0;
            case VideoScale::Down25:
                return 0.75;
            case VideoScale::Down50:
                return 0.5;
            case VideoScale::Down75:
                return 0.25;
        }
        return 1.0;
    }();

    switch (trans) {
        case VideoTrans::Off:
        case VideoTrans::Vflip:
        case VideoTrans::Hflip:
        case VideoTrans::VflipHflip:
        case VideoTrans::Scale:
            outDim.width = std::ceil(dim.width * factor);
            outDim.height = std::ceil(dim.height * factor);
            break;
        case VideoTrans::TransClock:
        case VideoTrans::TransClockFlip:
        case VideoTrans::TransCclock:
        case VideoTrans::TransCclockFlip:
            outDim.height = std::ceil(dim.width * factor);
            outDim.width = std::ceil(dim.height * factor);
            break;
        case VideoTrans::Frame169:
        case VideoTrans::Frame169Rot90:
        case VideoTrans::Frame169Rot180:
        case VideoTrans::Frame169Rot270:
        case VideoTrans::Frame169Vflip:
        case VideoTrans::Frame169VflipRot90:
        case VideoTrans::Frame169VflipRot180:
        case VideoTrans::Frame169VflipRot270:
            outDim.height = static_cast<uint32_t>(
                std::ceil(std::max(dim.width, dim.height) * factor));
            outDim.width =
                static_cast<uint32_t>(std::ceil((16.0 / 9.0) * outDim.height));
            break;
    }

    outDim.height -= outDim.height % 2;
    outDim.width -= outDim.width % 2;

    LOGD("dim change: " << dim << " => " << outDim << " (thin=" << dim.thin()
                        << ")");

    return outDim;
}

void Caster::initAvVideoFiltersFrame169(SensorDirection direction) {
    if (m_inDim.thin()) {
        initAvVideoFilter(
            direction, VideoTrans::Frame169,
            "transpose=dir={2},scale=h=-1:w={0},pad=width={0}:height="
            "{1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot90,
            "scale=h={1}:w=-1,vflip,hflip,pad=width={0}:height={1}:x=-1:y=-1:"
            "color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169Rot180,
                          "transpose=dir={3},scale=h=-1:w={0},pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot270,
            "scale=h={1}:w=-1,pad=width={0}:height={1}:x=-1:y=-2:color="
            "black");
    } else {
        initAvVideoFilter(
            direction, VideoTrans::Frame169,
            "transpose=dir={2},scale=h={1}:w=-1,pad=width={0}:height="
            "{1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot90,
            "scale=h={1}:w=-1,vflip,hflip,pad=width={0}:height={1}:x=-1:y=-1:"
            "color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169Rot180,
                          "transpose=dir={3},scale=h={1}:w=-1,pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot270,
            "scale=h={1}:w=-1,pad=width={0}:height={1}:x=-1:y=-2:color="
            "black");
    }
}

void Caster::initAvVideoFiltersFrame169Vflip(SensorDirection direction) {
    if (m_inDim.thin()) {
        initAvVideoFilter(
            direction, VideoTrans::Frame169Vflip,
            "transpose=dir={2}_flip,scale=h=-1:w={0},pad=width={0}:height="
            "{1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot90,
            "scale=h={1}:w=-1,hflip,pad=width={0}:height={1}:x=-1:y=-1:"
            "color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot180,
            "transpose=dir={3}_flip,scale=h=-1:w={0},pad=width={0}:"
            "height={1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot270,
            "scale=h={1}:w=-1,vflip,pad=width={0}:height={1}:x=-1:y=-2:color="
            "black");
    } else {
        initAvVideoFilter(
            direction, VideoTrans::Frame169Vflip,
            "transpose=dir={2}_flip,scale=h={1}:w=-1,pad=width={0}:height="
            "{1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot90,
            "scale=h={1}:w=-1,hflip,pad=width={0}:height={1}:x=-1:y=-1:"
            "color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot180,
            "transpose=dir={3}_flip,scale=h={1}:w=-1,pad=width={0}:"
            "height={1}"
            ":x=-1:y=-1:color=black");
        initAvVideoFilter(
            direction, VideoTrans::Frame169VflipRot270,
            "scale=h={1}:w=-1,vflip,pad=width={0}:height={1}:x=-1:y=-2:color="
            "black");
    }
}

Caster::VideoTrans Caster::orientationToTrans(
    VideoOrientation orientation, const VideoSourceInternalProps &props) {
    switch (orientation) {
        case VideoOrientation::Auto:
            return props.trans;
        case VideoOrientation::Landscape:
            switch (props.orientation) {
                case VideoOrientation::Portrait:
                    return VideoTrans::TransCclock;
                case VideoOrientation::InvertedLandscape:
                    return VideoTrans::VflipHflip;
                case VideoOrientation::InvertedPortrait:
                    return VideoTrans::TransClock;
                default:
                    break;
            }
            break;
        case VideoOrientation::Portrait:
            switch (props.orientation) {
                case VideoOrientation::Landscape:
                    return VideoTrans::TransClock;
                case VideoOrientation::InvertedLandscape:
                    return VideoTrans::TransCclock;
                case VideoOrientation::InvertedPortrait:
                    return VideoTrans::VflipHflip;
                default:
                    break;
            }
            break;
        case VideoOrientation::InvertedLandscape:
            switch (props.orientation) {
                case VideoOrientation::Portrait:
                    return VideoTrans::TransClock;
                case VideoOrientation::Landscape:
                    return VideoTrans::VflipHflip;
                case VideoOrientation::InvertedPortrait:
                    return VideoTrans::TransCclock;
                default:
                    break;
            }
            break;
        case VideoOrientation::InvertedPortrait:
            switch (props.orientation) {
                case VideoOrientation::Portrait:
                    return VideoTrans::VflipHflip;
                case VideoOrientation::Landscape:
                    return VideoTrans::TransCclock;
                case VideoOrientation::InvertedLandscape:
                    return VideoTrans::TransClock;
                default:
                    break;
            }
            break;
    }

    return props.trans;
}

void Caster::initAvAudioFilters() {
    if (m_audioTrans == AudioTrans::Off) {
        LOGD("audio filtering is not needed");
        return;
    }

    m_audioFrameAfterFilter = av_frame_alloc();

    switch (m_audioTrans) {
        case AudioTrans::Volume:
            initAvAudioFilter(
                m_audioFilterCtxMap[AudioTrans::Volume],
                fmt::format("volume=volume={}dB", m_config.audioVolume)
                    .c_str());
            break;
        default:
            throw std::runtime_error("unsuported audio trans");
    }
}

void Caster::initAvVideoFilters() {
    const auto &props = videoProps();

    if (m_videoTrans == VideoTrans::Off) {
        if (m_inVideoCtx->pix_fmt != m_outVideoCtx->pix_fmt) {
            LOGD("pixfmt conversion required: "
                 << m_inVideoCtx->pix_fmt << " => " << m_outVideoCtx->pix_fmt);
            m_videoTrans = VideoTrans::Scale;
        } else if (m_inVideoCtx->width != m_outVideoCtx->width ||
                   m_inVideoCtx->height != m_outVideoCtx->height) {
            LOGD("dim conversion required");
            m_videoTrans = VideoTrans::Scale;
        }
    }

    if (m_videoTrans == VideoTrans::Off) {
        LOGD("video filtering is not needed");
        return;
    }

    m_videoFrameAfterFilter = av_frame_alloc();

    switch (m_videoTrans) {
        case VideoTrans::Scale:
        case VideoTrans::Vflip:
        case VideoTrans::Hflip:
        case VideoTrans::VflipHflip:
        case VideoTrans::TransClock:
        case VideoTrans::TransClockFlip:
        case VideoTrans::TransCclock:
        case VideoTrans::TransCclockFlip:
            initAvVideoFilter(props.sensorDirection, VideoTrans::Scale,
                              "scale=h={1}:w={0}");
            initAvVideoFilter(props.sensorDirection, VideoTrans::Vflip,
                              "scale=h={1}:w={0},vflip");
            initAvVideoFilter(props.sensorDirection, VideoTrans::Hflip,
                              "scale=h={1}:w={0},hflip");
            initAvVideoFilter(props.sensorDirection, VideoTrans::VflipHflip,
                              "scale=h={1}:w={0},vflip,hflip");
            initAvVideoFilter(props.sensorDirection, VideoTrans::TransClock,
                              "scale=h={0}:w={1},transpose=dir={2}");
            initAvVideoFilter(props.sensorDirection, VideoTrans::TransCclock,
                              "scale=h={0}:w={1},transpose=dir={3}");
            initAvVideoFilter(props.sensorDirection, VideoTrans::TransClockFlip,
                              "scale=h={0}:w={1},transpose=dir={2}_flip");
            initAvVideoFilter(props.sensorDirection,
                              VideoTrans::TransCclockFlip,
                              "scale=h={0}:w={1},transpose=dir={3}_flip");
            break;
        case VideoTrans::Frame169:
        case VideoTrans::Frame169Rot90:
        case VideoTrans::Frame169Rot180:
        case VideoTrans::Frame169Rot270:
        case VideoTrans::Frame169Vflip:
        case VideoTrans::Frame169VflipRot90:
        case VideoTrans::Frame169VflipRot180:
        case VideoTrans::Frame169VflipRot270:
            switch (props.type) {
                case VideoSourceType::LipstickCapture:
                    initAvVideoFiltersFrame169Vflip(props.sensorDirection);
                    [[fallthrough]];
                default:
                    initAvVideoFiltersFrame169(props.sensorDirection);
            }
            break;
        default:
            throw std::runtime_error("unsuported video trans");
    }
}

void Caster::initAvVideoFilter(SensorDirection direction, VideoTrans trans,
                               const std::string &fmt) {
    initAvVideoFilter(
        m_videoFilterCtxMap[trans],
        fmt::format(fmt, m_outVideoCtx->width, m_outVideoCtx->height,
                    direction == SensorDirection::Front ? "cclock" : "clock",
                    direction == SensorDirection::Front ? "clock" : "cclock")
            .c_str());
}

void Caster::initAvAudioFilter(FilterCtx &ctx, const char *arg) {
    LOGD("initing audio av filter: " << arg);

    ctx.in = avfilter_inout_alloc();
    ctx.out = avfilter_inout_alloc();
    ctx.graph = avfilter_graph_alloc();
    if (ctx.in == nullptr || ctx.out == nullptr || ctx.graph == nullptr)
        throw std::runtime_error("failed to allocate av filter");

    const auto *buffersrc = avfilter_get_by_name("abuffer");
    if (buffersrc == nullptr) throw std::runtime_error("no abuffer filter");

    if (m_inAudioCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&m_inAudioCtx->ch_layout,
                                  m_inAudioCtx->ch_layout.nb_channels);

    std::array<char, 512> srcArgs{};
    auto s =
        snprintf(srcArgs.data(), srcArgs.size(),
                 "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=",
                 m_inAudioCtx->sample_rate,
                 av_get_sample_fmt_name(m_inAudioCtx->sample_fmt),
                 m_inAudioCtx->time_base.num, m_inAudioCtx->time_base.den);
    av_channel_layout_describe(&m_inAudioCtx->ch_layout, &srcArgs.at(s),
                               srcArgs.size() - s);
    LOGD("filter bufsrc: " << srcArgs.data());

    if (avfilter_graph_create_filter(&ctx.srcCtx, buffersrc, "in",
                                     srcArgs.data(), nullptr, ctx.graph) < 0) {
        throw std::runtime_error(
            "audio src avfilter_graph_create_filter error");
    }

    const auto *buffersink = avfilter_get_by_name("abuffersink");
    if (buffersink == nullptr)
        throw std::runtime_error("no abuffersink filter");

    if (avfilter_graph_create_filter(&ctx.sinkCtx, buffersink, "out", nullptr,
                                     nullptr, ctx.graph) < 0) {
        throw std::runtime_error(
            "audio sink avfilter_graph_create_filter error");
    }

    const AVSampleFormat out_sample_fmts[] = {m_outAudioCtx->sample_fmt,
                                              AV_SAMPLE_FMT_NONE};
    if (av_opt_set_int_list(ctx.sinkCtx, "sample_fmts", out_sample_fmts, -1,
                            AV_OPT_SEARCH_CHILDREN) < 0) {
        throw std::runtime_error("av_opt_set_int_list error");
    }

    std::array<char, 512> chName{};
    av_channel_layout_describe(&m_outAudioCtx->ch_layout, chName.data(),
                               chName.size());
    if (av_opt_set(ctx.sinkCtx, "ch_layouts", chName.data(),
                   AV_OPT_SEARCH_CHILDREN) < 0) {
        throw std::runtime_error("av_opt_set error");
    }

    const int out_sample_rates[] = {m_outAudioCtx->sample_rate, -1};
    if (av_opt_set_int_list(ctx.sinkCtx, "sample_rates", out_sample_rates, -1,
                            AV_OPT_SEARCH_CHILDREN) < 0) {
        throw std::runtime_error("av_opt_set_int_list error");
    }

    ctx.out->name = av_strdup("in");
    ctx.out->filter_ctx = ctx.srcCtx;
    ctx.out->pad_idx = 0;
    ctx.out->next = nullptr;

    ctx.in->name = av_strdup("out");
    ctx.in->filter_ctx = ctx.sinkCtx;
    ctx.in->pad_idx = 0;
    ctx.in->next = nullptr;

    if (avfilter_graph_parse_ptr(ctx.graph, arg, &ctx.in, &ctx.out, nullptr) <
        0)
        throw std::runtime_error("audio avfilter_graph_parse_ptr error");

    if (avfilter_graph_config(ctx.graph, nullptr) < 0)
        throw std::runtime_error("audio avfilter_graph_config error");

    auto outlink = ctx.sinkCtx->inputs[0];
    LOGD("filtering output: sample rate="
         << outlink->sample_rate
         << ", sample fmt=" << static_cast<AVSampleFormat>(outlink->format)
         << ", channel layout=" << &outlink->ch_layout);

    LOGD("audio av filter successfully inited");
}

void Caster::initAvVideoFilter(FilterCtx &ctx, const char *arg) {
    LOGD("initing video av filter: " << arg);

    ctx.in = avfilter_inout_alloc();
    ctx.out = avfilter_inout_alloc();
    ctx.graph = avfilter_graph_alloc();
    if (ctx.in == nullptr || ctx.out == nullptr || ctx.graph == nullptr)
        throw std::runtime_error("failed to allocate av filter");

    const auto *buffersrc = avfilter_get_by_name("buffer");
    if (buffersrc == nullptr) throw std::runtime_error("no buffer filter");

    std::array<char, 512> srcArgs{};
    snprintf(srcArgs.data(), srcArgs.size(),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d", m_inVideoCtx->width,
             m_inVideoCtx->height, m_inVideoCtx->pix_fmt,
             m_inVideoCtx->time_base.num, m_inVideoCtx->time_base.den);
    LOGD("filter bufsrc: " << srcArgs.data());

    if (avfilter_graph_create_filter(&ctx.srcCtx, buffersrc, "in",
                                     srcArgs.data(), nullptr, ctx.graph) < 0) {
        throw std::runtime_error(
            "video src avfilter_graph_create_filter error");
    }

    const auto *buffersink = avfilter_get_by_name("buffersink");
    if (buffersink == nullptr) throw std::runtime_error("no buffersink filter");

    if (avfilter_graph_create_filter(&ctx.sinkCtx, buffersink, "out", nullptr,
                                     nullptr, ctx.graph) < 0) {
        throw std::runtime_error(
            "video sink avfilter_graph_create_filter error");
    }

    AVPixelFormat pix_fmts[] = {m_outVideoCtx->pix_fmt, AV_PIX_FMT_NONE};
    if (av_opt_set_int_list(ctx.sinkCtx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE,
                            AV_OPT_SEARCH_CHILDREN) < 0) {
        throw std::runtime_error("av_opt_set_int_list error");
    }

    ctx.out->name = av_strdup("in");
    ctx.out->filter_ctx = ctx.srcCtx;
    ctx.out->pad_idx = 0;
    ctx.out->next = nullptr;

    ctx.in->name = av_strdup("out");
    ctx.in->filter_ctx = ctx.sinkCtx;
    ctx.in->pad_idx = 0;
    ctx.in->next = nullptr;

    if (avfilter_graph_parse_ptr(ctx.graph, arg, &ctx.in, &ctx.out, nullptr) <
        0)
        throw std::runtime_error("video avfilter_graph_parse_ptr error");

    if (avfilter_graph_config(ctx.graph, nullptr) < 0)
        throw std::runtime_error("video avfilter_graph_config error");

    LOGD("video av filter successfully inited");
}

AVSampleFormat Caster::bestAudioSampleFormat(const AVCodec *encoder,
                                             AVSampleFormat decoderSampleFmt) {
    if (encoder->sample_fmts == nullptr)
        throw std::runtime_error(
            "audio encoder does not support any sample fmts");

    AVSampleFormat bestFmt = AV_SAMPLE_FMT_NONE;

    for (int i = 0; encoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        bestFmt = encoder->sample_fmts[i];
        if (bestFmt == decoderSampleFmt) {
            LOGD("sample fmt exact match");
            break;
        }
    }

    return bestFmt;
}

bool Caster::nicePixfmt(AVPixelFormat fmt) {
    return std::find(nicePixfmts.cbegin(), nicePixfmts.cend(), fmt) !=
           nicePixfmts.cend();
}

AVPixelFormat Caster::toNicePixfmt(AVPixelFormat fmt,
                                   const AVPixelFormat *supportedFmts) {
    if (nicePixfmt(fmt)) return fmt;

    auto newFmt = fmt;

    for (int i = 0;; ++i) {
        if (nicePixfmt(supportedFmts[i])) {
            newFmt = supportedFmts[i];
            break;
        }
        if (supportedFmts[i] == AV_PIX_FMT_NONE) break;
    }

    if (fmt == newFmt)
        throw std::runtime_error("encoder does not support any nice formats");

    LOGD("changing encoder pixel format to nice one: " << fmt << " => "
                                                       << newFmt);

    return newFmt;
}

std::pair<std::reference_wrapper<const Caster::VideoFormatExt>, AVPixelFormat>
Caster::bestVideoFormat(const AVCodec *encoder,
                        const VideoSourceInternalProps &props,
                        bool useNiceFormats) {
    if (encoder->pix_fmts == nullptr)
        throw std::runtime_error("encoder does not support any pixfmts");

    LOGD("pixfmts supported by encoder: " << encoder->pix_fmts);

    if (auto it = std::find_if(
            props.formats.cbegin(), props.formats.cend(),
            [encoder, useNiceFormats](const auto &sf) {
                for (int i = 0;; ++i) {
                    if (encoder->pix_fmts[i] == AV_PIX_FMT_NONE) return false;
                    if (useNiceFormats && !nicePixfmt(encoder->pix_fmts[i]))
                        return false;
                    return encoder->pix_fmts[i] == sf.pixfmt;
                }
            });
        it != props.formats.cend()) {
        LOGD("pixfmt exact match: " << it->pixfmt);

        return {*it, it->pixfmt};
    }

    auto fmt = avcodec_find_best_pix_fmt_of_list(
        encoder->pix_fmts, props.formats.front().pixfmt, 0, nullptr);

    if (useNiceFormats)
        return {props.formats.front(), toNicePixfmt(fmt, encoder->pix_fmts)};
    return {props.formats.front(), fmt};
}

static std::string tempPathForX264() {
    char path[] = "/tmp/libx264-XXXXXX";
    auto fd = mkstemp(path);
    if (fd == -1) throw std::runtime_error("mkstemp error");
    close(fd);
    return path;
}

void Caster::setVideoEncoderOpts(VideoEncoder encoder, AVDictionary **opts) {
    switch (encoder) {
        case VideoEncoder::Nvenc:
            av_dict_set(opts, "preset", "p1", 0);
            av_dict_set(opts, "tune", "ull", 0);
            av_dict_set(opts, "zerolatency", "1", 0);
            av_dict_set(opts, "rc", "constqp", 0);
            break;
        case VideoEncoder::X264:
            av_dict_set(opts, "preset", "ultrafast", 0);
            av_dict_set(opts, "tune", "zerolatency", 0);
            av_dict_set(opts, "passlogfile", tempPathForX264().c_str(), 0);
            break;
        default:
            LOGW("failed to set video encoder options");
    }
}

std::string Caster::videoEncoderAvName(VideoEncoder encoder) {
    switch (encoder) {
        case VideoEncoder::X264:
            return "libx264";
        case VideoEncoder::Nvenc:
            return "h264_nvenc";
        case VideoEncoder::V4l2:
            return "h264_v4l2m2m";
        case VideoEncoder::Auto:
            break;
    }

    throw std::runtime_error("invalid video encoder");
}

std::string Caster::audioEncoderAvName(AudioEncoder encoder) {
    switch (encoder) {
        case AudioEncoder::Aac:
            return "aac";
        case AudioEncoder::Mp3Lame:
            return "libmp3lame";
    }

    throw std::runtime_error("invalid audio encoder");
}

std::string Caster::videoFormatAvName(VideoSourceType type) {
    switch (type) {
        case VideoSourceType::V4l2:
            return "video4linux2";
        case VideoSourceType::X11Capture:
            return "x11grab";
        case VideoSourceType::LipstickCapture:
        case VideoSourceType::Test:
        case VideoSourceType::DroidCamRaw:
            return "rawvideo";
        case VideoSourceType::DroidCam:
        case VideoSourceType::Unknown:
            break;
    }

    throw std::runtime_error("invalid video source");
}

std::string Caster::streamFormatAvName(StreamFormat format) {
    switch (format) {
        case StreamFormat::Mp4:
            return "mp4";
        case StreamFormat::MpegTs:
            return "mpegts";
        case StreamFormat::Mp3:
            return "mp3";
    }

    throw std::runtime_error("invalid stream format");
}

void Caster::initAvVideoRawDecoder() {
    const auto *decoder = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    if (decoder == nullptr)
        throw std::runtime_error("avcodec_find_decoder for video error");

    m_inVideoCtx = avcodec_alloc_context3(decoder);
    if (m_inVideoCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for video error");

    m_inVideoCtx->pix_fmt = m_inPixfmt;
    m_inVideoCtx->width = static_cast<int>(m_inDim.width);
    m_inVideoCtx->height = static_cast<int>(m_inDim.height);
    m_inVideoCtx->time_base = AVRational{1, m_videoFramerate};

    m_videoRawFrameSize = av_image_get_buffer_size(
        m_inVideoCtx->pix_fmt, m_inVideoCtx->width, m_inVideoCtx->height, 32);

    if (avcodec_open2(m_inVideoCtx, nullptr, nullptr) != 0) {
        throw std::runtime_error("avcodec_open2 for in video error");
    }

    LOGD("video decoder: tb=" << m_inVideoCtx->time_base
                              << ", pixfmt=" << m_inVideoCtx->pix_fmt
                              << ", width=" << m_inVideoCtx->width
                              << ", height=" << m_inVideoCtx->height
                              << ", raw frame size=" << m_videoRawFrameSize);

    m_videoFrameIn = av_frame_alloc();
}

void Caster::initAvVideoEncoder(VideoEncoder type) {
    auto enc = videoEncoderAvName(type);

    LOGD("initing video encoder: " << enc);

    const auto *encoder = avcodec_find_encoder_by_name(enc.c_str());
    if (!encoder) throw std::runtime_error(fmt::format("no {} encoder", enc));

    m_outVideoCtx = avcodec_alloc_context3(encoder);
    if (m_outVideoCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for video error");

    const auto &props = videoProps();

    const auto &bestFormat = [&]() {
#ifdef USE_V4L2
        if (type == VideoEncoder::V4l2)
            return bestVideoFormatForV4l2Encoder(props);
#endif
        return bestVideoFormat(
            encoder, props,
            m_config.options & OptionsFlags::OnlyNiceVideoFormats);
    }();

    m_outVideoCtx->pix_fmt = bestFormat.second;
    if (m_outVideoCtx->pix_fmt == AV_PIX_FMT_NONE)
        throw std::runtime_error("failed to find pixfmt for video encoder");

    const auto &fs = bestFormat.first.get().frameSpecs.front();

    m_videoFramerate = static_cast<int>(*fs.framerates.begin());

    m_outVideoCtx->time_base = AVRational{1, m_videoFramerate};
    m_outVideoCtx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;

    m_inDim = fs.dim;
    m_inPixfmt = bestFormat.first.get().pixfmt;

    auto outDim = computeTransDim(m_inDim, m_videoTrans, props.scale);
    m_outVideoCtx->width = static_cast<int>(outDim.width);
    m_outVideoCtx->height = static_cast<int>(outDim.height);

    AVDictionary *opts = nullptr;

    setVideoEncoderOpts(type, &opts);

    if (avcodec_open2(m_outVideoCtx, nullptr, &opts) < 0) {
        av_dict_free(&opts);
        throw std::runtime_error("avcodec_open2 for out video error");
    }

    cleanAvOpts(&opts);

    LOGD("video encoder: tb=" << m_outVideoCtx->time_base
                              << ", pixfmt=" << m_outVideoCtx->pix_fmt
                              << ", width=" << m_outVideoCtx->width
                              << ", height=" << m_outVideoCtx->height
                              << ", framerate=" << m_videoFramerate);

    LOGD("encoder successfuly inited");
}

void Caster::initAvVideoEncoder() {
    if (m_config.videoEncoder == VideoEncoder::Auto) {
        try {
            initAvVideoEncoder(VideoEncoder::V4l2);
        } catch (const std::runtime_error &e) {
            LOGW("failed to init h264_v4l2m2m encoder: " << e.what());
            try {
                initAvVideoEncoder(VideoEncoder::Nvenc);
            } catch (const std::runtime_error &e) {
                LOGW("failed to init h264_nvenc encoder: " << e.what());
                initAvVideoEncoder(VideoEncoder::X264);
            }
        }
        return;
    }

    initAvVideoEncoder(m_config.videoEncoder);
}

void Caster::initFiles() {
    for (const auto &f : m_config.fileSourceConfig->files) m_files.push(f);
}

void Caster::addFile(std::string file) {
    if (terminating()) return;

    std::lock_guard lock{m_filesMtx};

    LOGD("adding file: " << file);

    m_files.push(std::move(file));
}

bool Caster::initAvAudioInputFormatFromFile() {
    std::lock_guard lock{m_filesMtx};

    if (m_files.empty() && m_config.fileSourceConfig &&
        m_config.fileSourceConfig->flags & FileSourceFlags::Loop)
        initFiles();

    if (m_files.empty()) {
        LOGD("no more files to cast");
        return false;
    }

    LOGD("opening file: " << m_files.front());

    if (avformat_open_input(&m_inAudioFormatCtx, m_files.front().c_str(),
                            nullptr, nullptr) < 0)
        throw std::runtime_error("avformat_open_input for audio error");

    m_currentFile = m_files.front();
    m_files.pop();

    return true;
}

void Caster::initAvVideoInputRawFormat() {
    const auto &props = videoProps();

    const auto *in_video_format =
        av_find_input_format(videoFormatAvName(props.type).c_str());
    if (in_video_format == nullptr)
        throw std::runtime_error("av_find_input_format for video error");

    AVDictionary *opts = nullptr;

    auto dim = fmt::format("{}x{}", m_inDim.width, m_inDim.height);
    av_dict_set(&opts, "video_size", dim.c_str(), 0);
    av_dict_set_int(&opts, "framerate", m_videoFramerate, 0);

    if (props.type == VideoSourceType::V4l2)
        av_dict_set(&opts, "input_format", av_get_pix_fmt_name(m_inPixfmt), 0);

    AVFormatContext *in_cxt = nullptr;
    if (avformat_open_input(&in_cxt, props.dev.c_str(), in_video_format,
                            &opts) < 0) {
        av_dict_free(&opts);
        throw std::runtime_error("avformat_open_input for video error");
    }

    cleanAvOpts(&opts);

    m_inVideoFormatCtx = in_cxt;
}

void Caster::allocAvOutputFormat() {
    if (avformat_alloc_output_context2(
            &m_outFormatCtx, nullptr,
            streamFormatAvName(m_config.streamFormat).c_str(), nullptr) < 0) {
        throw std::runtime_error("avformat_alloc_output_context2 error");
    }
}

void Caster::initAv() {
    LOGD("av init started");

    if (audioEnabled()) {
        const auto &props = audioProps();

        switch (props.type) {
            case AudioSourceType::Mic:
            case AudioSourceType::Monitor:
            case AudioSourceType::Playback:
                initAvAudioRawDecoderFromProps();
                break;
            case AudioSourceType::File:
                if (!initAvAudioInputFormatFromFile())
                    throw std::runtime_error("no file to cast");
                findAvAudioInputStreamIdx();
                initAvAudioRawDecoderFromInputStream();
                break;
            default:
                throw std::runtime_error("unknown audio source type");
        }

        initAvAudioEncoder();
        initAvAudioFifo();
        initAvAudioFilters();

        m_audioInFrameSize = av_samples_get_buffer_size(
            nullptr, m_inAudioCtx->ch_layout.nb_channels,
            m_outAudioCtx->frame_size, m_inAudioCtx->sample_fmt, 0);

        m_audioOutFrameSize = av_samples_get_buffer_size(
            nullptr, m_outAudioCtx->ch_layout.nb_channels,
            m_outAudioCtx->frame_size, m_outAudioCtx->sample_fmt, 0);
    }

    if (videoEnabled()) {
        const auto &props = videoProps();

        switch (props.type) {
            case VideoSourceType::DroidCam:
                initAvVideoForGst();
                break;
            case VideoSourceType::V4l2:
            case VideoSourceType::X11Capture:
                initAvVideoEncoder();
                initAvVideoInputRawFormat();
                findAvVideoInputStreamIdx();
                initAvVideoRawDecoderFromInputStream();
                break;
            case VideoSourceType::LipstickCapture:
            case VideoSourceType::Test:
            case VideoSourceType::DroidCamRaw:
                initAvVideoEncoder();
                initAvVideoRawDecoder();
                initAvVideoFilters();
                break;
            default:
                throw std::runtime_error("unknown video source type");
        }

        m_videoRealFrameDuration =
            rescaleToUsec(1, AVRational{1, m_videoFramerate});
        m_videoFrameDuration = m_videoRealFrameDuration / 2;
    }

    LOGD("using muxer: " << m_config.streamFormat);

    allocAvOutputFormat();

    setState(State::Inited);

    LOGD("av init completed");
}

void Caster::initAvVideoInputCompressedFormat() {
    AVDictionary *opts = nullptr;

    av_dict_set_int(&opts, "framerate", m_videoFramerate, 0);

    if (auto ret = avformat_open_input(&m_inVideoFormatCtx, "", nullptr, &opts);
        ret != 0) {
        av_dict_free(&opts);
        throw std::runtime_error("avformat_open_input for video error: " +
                                 strForAvError(ret));
    }

    cleanAvOpts(&opts);

    if (avformat_find_stream_info(m_inVideoFormatCtx, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info for video error");
}

void Caster::initAvAudioFifo() {
    m_audioFifo = av_audio_fifo_alloc(m_inAudioCtx->sample_fmt,
                                      m_inAudioCtx->ch_layout.nb_channels, 1);
    if (m_audioFifo == nullptr)
        throw std::runtime_error("av_audio_fifo_alloc error");
}

void Caster::initAvVideoBsf() {
    // extract_extradata

    const auto *extractBsf = av_bsf_get_by_name("extract_extradata");
    if (extractBsf == nullptr)
        throw std::runtime_error("no extract_extradata bsf found");

    if (av_bsf_alloc(extractBsf, &m_videoBsfExtractExtraCtx) != 0)
        throw std::runtime_error("extract_extradata av_bsf_alloc error");

    if (avcodec_parameters_copy(m_videoBsfExtractExtraCtx->par_in,
                                m_outVideoStream->codecpar) < 0)
        throw std::runtime_error("bsf avcodec_parameters_copy error");

    m_videoBsfExtractExtraCtx->time_base_in = m_outVideoStream->time_base;

    if (av_bsf_init(m_videoBsfExtractExtraCtx) != 0)
        throw std::runtime_error("extract_extradata av_bsf_init error");

    // dump_extra

    const auto *dumpBsf = av_bsf_get_by_name("dump_extra");
    if (dumpBsf == nullptr) throw std::runtime_error("no dump_extra bsf found");

    if (av_bsf_alloc(dumpBsf, &m_videoBsfDumpExtraCtx) != 0)
        throw std::runtime_error("dump_extra av_bsf_alloc error");

    if (avcodec_parameters_copy(m_videoBsfDumpExtraCtx->par_in,
                                m_outVideoStream->codecpar) < 0)
        throw std::runtime_error("bsf avcodec_parameters_copy error");

    av_opt_set(m_videoBsfDumpExtraCtx, "freq", "all", 0);

    m_videoBsfDumpExtraCtx->time_base_in = m_outVideoStream->time_base;

    if (av_bsf_init(m_videoBsfDumpExtraCtx) != 0)
        throw std::runtime_error("dump_extra av_bsf_init error");
}

void Caster::initAvVideoOutStreamFromInputFormat() {
    auto idx = av_find_best_stream(m_inVideoFormatCtx, AVMEDIA_TYPE_VIDEO, -1,
                                   -1, nullptr, 0);
    if (idx < 0) throw std::runtime_error("no video stream found in input");

    auto *stream = m_inVideoFormatCtx->streams[idx];

    av_dump_format(m_inVideoFormatCtx, idx, "", 0);

    m_outVideoStream = avformat_new_stream(m_outFormatCtx, nullptr);
    if (!m_outVideoStream)
        throw std::runtime_error("avformat_new_stream for video error");

    m_outVideoStream->id = 0;

    if (avcodec_parameters_copy(m_outVideoStream->codecpar, stream->codecpar) <
        0) {
        throw std::runtime_error("avcodec_parameters_copy for video error");
    }

    m_outVideoStream->time_base = AVRational{1, m_videoFramerate};
}

void Caster::initAvVideoRawDecoderFromInputStream() {
    const auto *stream = m_inVideoFormatCtx->streams[m_inVideoStreamIdx];

    m_inPixfmt = static_cast<AVPixelFormat>(stream->codecpar->format);

    const auto *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (decoder == nullptr)
        throw std::runtime_error("avcodec_find_decoder for video error");

    m_inVideoCtx = avcodec_alloc_context3(decoder);
    if (m_inVideoCtx == nullptr)
        throw std::runtime_error("avcodec_alloc_context3 for in video error");

    if (avcodec_parameters_to_context(m_inVideoCtx, stream->codecpar) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_to_context for video error");
    }

    m_inVideoCtx->time_base = stream->time_base;
    m_videoRawFrameSize = av_image_get_buffer_size(
        m_inVideoCtx->pix_fmt, m_inVideoCtx->width, m_inVideoCtx->height, 32);

    if (avcodec_open2(m_inVideoCtx, nullptr, nullptr) != 0) {
        throw std::runtime_error("avcodec_open2 for in video error");
    }

    LOGD("video decoder: tb=" << m_inVideoCtx->time_base
                              << ", pixfmt=" << m_inVideoCtx->pix_fmt
                              << ", width=" << m_inVideoCtx->width
                              << ", height=" << m_inVideoCtx->height
                              << ", raw frame size=" << m_videoRawFrameSize);

    if (m_inVideoCtx->width != static_cast<int>(m_inDim.width) ||
        m_inVideoCtx->height != static_cast<int>(m_inDim.height) ||
        m_inVideoCtx->pix_fmt != m_inPixfmt) {
        LOGE("input stream has invalid params, expected: pixfmt="
             << m_inPixfmt << ", width=" << m_inDim.width
             << ", height=" << m_inDim.height);
        throw std::runtime_error("decoder params are invalid");
    }

    m_videoFrameIn = av_frame_alloc();
}

void Caster::findAvAudioInputStreamIdx() {
    if (avformat_find_stream_info(m_inAudioFormatCtx, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info for audio error");

    auto audioIdx = av_find_best_stream(m_inAudioFormatCtx, AVMEDIA_TYPE_AUDIO,
                                        -1, -1, nullptr, 0);
    if (audioIdx < 0)
        throw std::runtime_error("no audio stream found in input");

    auto dataIdx = av_find_best_stream(m_inAudioFormatCtx, AVMEDIA_TYPE_DATA,
                                       -1, -1, nullptr, 0);
    if (dataIdx >= 0)
        LOGD("data stream found, type="
             << m_inAudioFormatCtx->streams[dataIdx]->codecpar->codec_id);

    av_dump_format(m_inAudioFormatCtx, audioIdx, "", 0);

    m_inAudioStreamIdx = audioIdx;
}

void Caster::findAvVideoInputStreamIdx() {
    if (avformat_find_stream_info(m_inVideoFormatCtx, nullptr) < 0)
        throw std::runtime_error("avformat_find_stream_info for video error");

    auto idx = av_find_best_stream(m_inVideoFormatCtx, AVMEDIA_TYPE_VIDEO, -1,
                                   -1, nullptr, 0);
    if (idx < 0) throw std::runtime_error("no video stream found in input");

    av_dump_format(m_inVideoFormatCtx, idx, "", 0);

    m_inVideoStreamIdx = idx;
}

void Caster::initAvVideoOutStreamFromEncoder() {
    m_outVideoStream = avformat_new_stream(m_outFormatCtx, nullptr);
    if (!m_outVideoStream)
        throw std::runtime_error("avformat_new_stream for video error");

    m_outVideoStream->id = 0;
    m_outVideoStream->r_frame_rate = AVRational{m_videoFramerate, 1};

    if (avcodec_parameters_from_context(m_outVideoStream->codecpar,
                                        m_outVideoCtx) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_from_context for video error");
    }

    m_outVideoStream->time_base = AVRational{1, m_videoFramerate};
}

void Caster::initAvAudioDurations() {
    m_audioFrameDuration =
        rescaleToUsec(m_outAudioCtx->frame_size, m_inAudioCtx->time_base);
    m_audioPktDuration =
        rescaleFromUsec(m_audioFrameDuration, m_outAudioStream->time_base);

    LOGD("audio out stream: tb=" << m_outAudioStream->time_base
                                 << ", frame dur=" << m_audioFrameDuration
                                 << ", pkt dur=" << m_audioPktDuration);
}

void Caster::initAvAudioOutStreamFromEncoder() {
    m_outAudioStream = avformat_new_stream(m_outFormatCtx, nullptr);
    if (!m_outAudioStream) {
        throw std::runtime_error("avformat_new_stream for audio error");
    }

    m_outAudioStream->id = 1;

    if (avcodec_parameters_from_context(m_outAudioStream->codecpar,
                                        m_outAudioCtx) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_from_context for audio error");
    }

    m_outAudioStream->time_base = m_outAudioCtx->time_base;

    av_dump_format(m_outFormatCtx, m_outAudioStream->id, "", 1);
}

void Caster::reInitAvOutputFormat() {
    cleanAvOutputFormat();
    allocAvOutputFormat();

    if (videoEnabled()) {
        const auto &props = videoProps();

        switch (props.type) {
            case VideoSourceType::DroidCam:
                initAvVideoOutStreamFromInputFormat();
                break;
            case VideoSourceType::V4l2:
            case VideoSourceType::X11Capture:
            case VideoSourceType::LipstickCapture:
            case VideoSourceType::Test:
            case VideoSourceType::DroidCamRaw:
                initAvVideoOutStreamFromEncoder();
                break;
            default:
                throw std::runtime_error("unknown video source type");
        }
    }

    if (audioEnabled()) initAvAudioOutStreamFromEncoder();

    initAvOutputFormat();
}

void Caster::initAvOutputFormat() {
    auto *outBuf = static_cast<uint8_t *>(av_malloc(m_videoBufSize));
    if (outBuf == nullptr) {
        av_freep(&outBuf);
        throw std::runtime_error("unable to allocate out av buf");
    }

    m_outFormatCtx->pb =
        avio_alloc_context(outBuf, m_videoBufSize, 1, this, nullptr,
                           avWritePacketCallbackStatic, nullptr);
    if (m_outFormatCtx->pb == nullptr) {
        throw std::runtime_error("avio_alloc_context error");
    }

    AVDictionary *opts = nullptr;

    if (m_config.streamFormat == StreamFormat::MpegTs) {
        av_dict_set(&opts, "mpegts_m2ts_mode", "-1", 0);
        av_dict_set(&m_outFormatCtx->metadata, "service_provider",
                    m_config.streamAuthor.c_str(), 0);
        av_dict_set(&m_outFormatCtx->metadata, "service_name",
                    m_config.streamTitle.c_str(), 0);
    } else if (m_config.streamFormat == StreamFormat::Mp4) {
        av_dict_set(&opts, "movflags", "frag_custom+empty_moov+delay_moov", 0);
        av_dict_set(&m_outFormatCtx->metadata, "author",
                    m_config.streamAuthor.c_str(), 0);
        av_dict_set(&m_outFormatCtx->metadata, "title",
                    m_config.streamTitle.c_str(), 0);

        if (videoEnabled()) setVideoStreamRotation(m_config.videoOrientation);
    } else if (m_config.streamFormat == StreamFormat::Mp3) {
        av_dict_set(&m_outAudioStream->metadata, "artist",
                    m_config.streamAuthor.c_str(), 0);
        av_dict_set(&m_outAudioStream->metadata, "title",
                    m_config.streamTitle.c_str(), 0);
    } else {
        throw std::runtime_error("invalid stream format for video");
    }

    m_outFormatCtx->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS |
                             AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_AUTO_BSF;

    LOGD("writting format header");
    auto ret = avformat_write_header(m_outFormatCtx, &opts);
    if (ret != AVSTREAM_INIT_IN_WRITE_HEADER &&
        ret != AVSTREAM_INIT_IN_INIT_OUTPUT) {
        av_dict_free(&opts);
        throw std::runtime_error("avformat_write_header error");
    }
    LOGD("format header written, ret="
         << (ret == AVSTREAM_INIT_IN_WRITE_HEADER  ? "write-header"
             : ret == AVSTREAM_INIT_IN_INIT_OUTPUT ? "init-output"
                                                   : "unknown"));

    if (audioEnabled()) initAvAudioDurations();

    cleanAvOpts(&opts);
}

void Caster::startAv() {
    LOGD("starting av");

    if (videoEnabled()) {
        const auto &props = videoProps();

        switch (props.type) {
            case VideoSourceType::DroidCam:
                initAvVideoInputCompressedFormat();
                initAvVideoOutStreamFromInputFormat();
                initAvVideoBsf();
                extractVideoExtradataFromCompressedDemuxer();
                break;
            case VideoSourceType::V4l2:
            case VideoSourceType::X11Capture:
                initAvVideoOutStreamFromEncoder();
                initAvVideoFilters();
                initAvVideoBsf();
                extractVideoExtradataFromRawDemuxer();
                break;
            case VideoSourceType::LipstickCapture:
            case VideoSourceType::Test:
            case VideoSourceType::DroidCamRaw:
                initAvVideoOutStreamFromEncoder();
                initAvVideoBsf();
                extractVideoExtradataFromRawBuf();
                break;
            default:
                throw std::runtime_error("unknown video source type");
        }
    }

    if (audioEnabled()) {
        initAvAudioOutStreamFromEncoder();
    }

    initAvOutputFormat();

    LOGD("av start completed");
}

void Caster::disconnectPaSinkInput() {
    if (m_paStream == nullptr) return;

    LOGD("disconnecting pa stream");

    if (m_connectedPaSinkInput != PA_INVALID_INDEX &&
        m_paSinkInputs.count(m_connectedPaSinkInput) > 0) {
        if (m_config.options & OptionsFlags::MuteAudioSource)
            unmutePaSinkInput(m_paSinkInputs.at(m_connectedPaSinkInput));
    }

    pa_stream_disconnect(m_paStream);
    pa_stream_unref(m_paStream);
    m_paStream = nullptr;
    m_connectedPaSinkInput = PA_INVALID_INDEX;

    for (auto it = m_paSinkInputs.begin(); it != m_paSinkInputs.end();) {
        if (it->second.removed)
            it = m_paSinkInputs.erase(it);
        else
            ++it;
    }

    if (m_audioSourceNameChangedHandler) m_audioSourceNameChangedHandler({});
}

void Caster::mutePaSinkInput([[maybe_unused]] PaSinkInput &si) {
#ifdef USE_SFOS
    auto *o = pa_context_move_sink_input_by_name(
        m_paCtx, si.idx, "sink.null",
        []([[maybe_unused]] pa_context *ctx, int success,
           [[maybe_unused]] void *userdata) {
            if (success)
                LOGD("pa sink input successfully muted");
            else
                LOGW("failed to mute pa sink input");
        },
        nullptr);
    if (o != nullptr) pa_operation_unref(o);
    si.muted = true;
#endif
}

void Caster::unmutePaSinkInput([[maybe_unused]] PaSinkInput &si) {
#ifdef USE_SFOS
    auto *o = pa_context_move_sink_input_by_index(
        m_paCtx, si.idx, si.sinkIdx,
        []([[maybe_unused]] pa_context *ctx, int success,
           [[maybe_unused]] void *userdata) {
            if (success)
                LOGD("pa sink input successfully unmuted");
            else
                LOGW("failed to unmute pa sink input");
        },
        nullptr);
    if (o != nullptr) pa_operation_unref(o);
    si.muted = false;
#endif
}

void Caster::connectPaSinkInput() {
    auto si = bestPaSinkInput();
    if (!si) {
        LOGD("no active pa sink input");
        disconnectPaSinkInput();
        return;
    }

    const auto idx = si->get().idx;

    LOGD("best pa sink input: " << idx);

    if (m_paStream != nullptr && m_connectedPaSinkInput != PA_INVALID_INDEX) {
        LOGD("connected pa sink input: " << m_connectedPaSinkInput);
        if (m_connectedPaSinkInput == idx) {
            LOGD("best pa sink input is already connected");
            return;
        }
        disconnectPaSinkInput();
    }

    const auto &props = audioProps();

    pa_sample_spec spec{ff_tools::ff_codec_id_to_pulse_format(props.codec),
                        props.rate, props.channels};

#ifdef USE_SFOS
    m_paStream = pa_stream_new(m_paCtx, "notiftone", &spec, nullptr);
#else
    m_paStream =
        pa_stream_new(m_paCtx, m_config.streamTitle.c_str(), &spec, nullptr);
#endif

    pa_stream_set_read_callback(m_paStream, paStreamRequestCallbackStatic,
                                this);

    bool muteSource = m_config.options & OptionsFlags::MuteAudioSource;

    if (muteSource) mutePaSinkInput(*si);

    const pa_buffer_attr attr{
        /*maxlength=*/
        static_cast<uint32_t>(-1),
        /*tlength=*/static_cast<uint32_t>(-1),
        /*prebuf=*/static_cast<uint32_t>(-1),
        /*minreq=*/static_cast<uint32_t>(-1),
        /*fragsize=*/static_cast<uint32_t>(m_audioInFrameSize)};

    if (pa_stream_set_monitor_stream(m_paStream, idx) < 0) {
        if (muteSource) unmutePaSinkInput(*si);
        throw std::runtime_error("pa_stream_set_monitor_stream error");
    }

    LOGD("connecting pa sink input: " << si->get());

    m_connectedPaSinkInput = idx;

    if (pa_stream_connect_record(m_paStream, nullptr, &attr,
                                 PA_STREAM_ADJUST_LATENCY) != 0) {
        if (muteSource) unmutePaSinkInput(*si);
        throw std::runtime_error("pa_stream_connect_record error");
    }

    if (m_audioSourceNameChangedHandler)
        m_audioSourceNameChangedHandler(
            paCorrectedClientName(si->get().clientIdx));
}

void Caster::connectPaSource() {
    const auto &props = audioProps();

    pa_sample_spec spec{ff_tools::ff_codec_id_to_pulse_format(props.codec),
                        props.rate, props.channels};

    m_paStream =
        pa_stream_new(m_paCtx, m_config.streamTitle.c_str(), &spec, nullptr);
    pa_stream_set_read_callback(m_paStream, paStreamRequestCallbackStatic,
                                this);

    const pa_buffer_attr attr{
        /*maxlength=*/static_cast<uint32_t>(-1),
        /*tlength=*/static_cast<uint32_t>(-1),
        /*prebuf=*/static_cast<uint32_t>(-1),
        /*minreq=*/static_cast<uint32_t>(-1),
        /*fragsize=*/static_cast<uint32_t>(m_audioInFrameSize)};

    LOGD("connecting pa source: " << props.dev);

    if (pa_stream_connect_record(
            m_paStream, props.dev.empty() ? nullptr : props.dev.c_str(), &attr,
            PA_STREAM_ADJUST_LATENCY) != 0) {
        throw std::runtime_error("pa_stream_connect_record error");
    }
}

void Caster::startPa() {
    LOGD("starting pa");

    switch (audioProps().type) {
        case AudioSourceType::Mic:
        case AudioSourceType::Monitor:
            connectPaSource();
            break;
        case AudioSourceType::Playback:
            connectPaSinkInput();
            break;
        default:
            throw std::runtime_error("invalid audio source type");
    }

    m_paDataReceived = false;

    m_audioPaThread = std::thread(&Caster::doPaTask, this);

    LOGD("pa started");
}

void Caster::paStreamRequestCallbackStatic(pa_stream *stream, size_t nbytes,
                                           void *userdata) {
    static_cast<Caster *>(userdata)->paStreamRequestCallback(stream, nbytes);
}

void Caster::paStreamRequestCallback(pa_stream *stream, size_t nbytes) {
    std::lock_guard lock{m_audioMtx};

    LOGT("pa audio sample: " << nbytes << ", buf=" << m_audioBuf.size());

    const void *data;
    if (pa_stream_peek(stream, &data, &nbytes) != 0) {
        LOGW("pa_stream_peek error");
        return;
    }

    if (data == nullptr || nbytes == 0) {
        LOGW("no pa data");
        return;
    }

    if (m_state == State::Started) {
        m_audioBuf.pushExactForce(
            static_cast<const decltype(m_audioBuf)::BufType *>(data), nbytes);
        if (!m_paDataReceived) {
            m_paDataReceived = true;
            LOGD("first pa data received");
        }
    }

    pa_stream_drop(stream);
}

void Caster::doPaTask() {
    const uint32_t sleep = m_audioFrameDuration;

    LOGD("starting pa thread, sleep=" << sleep);

    try {
        while (!terminating()) {
            if (pa_mainloop_iterate(m_paLoop, 0, nullptr) < 0) break;
            av_usleep(sleep);
        }
    } catch (const std::runtime_error &e) {
        LOGE("error in pa thread: " << e.what());
        reportError();
    }

    LOGD("pa thread ended");
}

void Caster::startAudioOnlyMuxing() {
    m_avMuxingThread = std::thread([this, sleep = m_audioFrameDuration / 2]() {
        LOGD("starting muxing");

        auto *audio_pkt = av_packet_alloc();
        m_nextAudioPts = 0;
        m_audioFlushed = false;

        try {
            while (!terminating()) {
                if (m_state != State::Started) break;
                if (muxAudio(audio_pkt)) {
                    av_write_frame(m_outFormatCtx,
                                   nullptr);  // force fragment
                }
                av_usleep(sleep);
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in audio muxing thread: " << e.what());
            reportError();
        }

        av_packet_free(&audio_pkt);

        LOGD("muxing ended");
    });
}

void Caster::startVideoOnlyMuxing() {
    m_avMuxingThread = std::thread([this]() {
        LOGD("starting muxing");

        auto *video_pkt = av_packet_alloc();
        m_nextVideoPts = 0;
        m_videoFlushed = false;
        m_videoTimeLastFrame = 0;
        m_videoRealFrameDuration =
            rescaleToUsec(1, AVRational{1, m_videoFramerate});

        try {
            while (!terminating()) {
                if (m_state != State::Started) break;
                if (muxVideo(video_pkt)) {
                    av_write_frame(m_outFormatCtx,
                                   nullptr);  // force fragment
                }
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in video muxing thread: " << e.what());
            reportError();
        }

        av_packet_free(&video_pkt);

        LOGD("muxing ended");
    });
}

void Caster::startVideoAudioMuxing() {
    m_avMuxingThread = std::thread([this]() {
        LOGD("starting muxing");

        auto *video_pkt = av_packet_alloc();
        auto *audio_pkt = av_packet_alloc();
        m_nextVideoPts = 0;
        m_nextAudioPts = 0;
        m_audioFlushed = false;
        m_videoFlushed = false;
        m_videoTimeLastFrame = 0;
        m_videoRealFrameDuration =
            rescaleToUsec(1, AVRational{1, m_videoFramerate});

        try {
            while (!terminating()) {
                if (m_state != State::Started) break;

                bool pktDone = muxVideo(video_pkt);
                if (muxAudio(audio_pkt)) pktDone = true;
                if (pktDone)
                    av_write_frame(m_outFormatCtx,
                                   nullptr);  // force fragment
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in video-audio muxing thread: " << e.what());
            reportError();
        }

        av_packet_free(&video_pkt);
        av_packet_free(&audio_pkt);

        LOGD("muxing ended");
    });
}

void Caster::startMuxing() {
    if (videoEnabled()) {
        if (audioEnabled())
            startVideoAudioMuxing();
        else
            startVideoOnlyMuxing();
    } else {
        if (audioEnabled())
            startAudioOnlyMuxing();
        else
            throw std::runtime_error("audio and video disabled");
    }
}

int Caster::orientationToRot(VideoOrientation orientation) {
    switch (orientation) {
        case VideoOrientation::Auto:
        case VideoOrientation::Landscape:
            return 0;
        case VideoOrientation::Portrait:
            return 90;
        case VideoOrientation::InvertedLandscape:
            return 180;
        case VideoOrientation::InvertedPortrait:
            return 270;
    }

    return 0;
}

void Caster::setVideoStreamRotation(VideoOrientation requestedOrientation) {
    const auto &props = videoProps();

    switch (props.type) {
        case VideoSourceType::DroidCam:
            break;
        case VideoSourceType::Unknown:
        case VideoSourceType::V4l2:
        case VideoSourceType::X11Capture:
        case VideoSourceType::LipstickCapture:
        case VideoSourceType::Test:
        case VideoSourceType::DroidCamRaw:
            return;
    }

    auto rotation = [ro = requestedOrientation, o = props.orientation]() {
        if (ro == VideoOrientation::Auto || ro == o) return 0;
        return (orientationToRot(ro) + orientationToRot(o)) % 360;
    }();

    LOGD("video rotation: " << rotation << ", o=" << props.orientation << " ("
                            << orientationToRot(props.orientation)
                            << "), ro=" << requestedOrientation << " ("
                            << orientationToRot(requestedOrientation) << ")");

    if (rotation == 0) return;

    if (m_outVideoStream->side_data == nullptr) {
        if (!av_stream_new_side_data(m_outVideoStream,
                                     AV_PKT_DATA_DISPLAYMATRIX,
                                     sizeof(int32_t) * 9)) {
            throw std::runtime_error("av_stream_new_side_data error");
        }
    }

    av_display_rotation_set(
        reinterpret_cast<int32_t *>(m_outVideoStream->side_data->data),
        rotation);
}

void Caster::readNullFrame(AVPacket *pkt) {
    if (av_new_packet(pkt, m_videoRawFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for video error");
    }

    memset(pkt->data, 0, m_videoRawFrameSize);
}

bool Caster::readVideoFrameFromBuf(AVPacket *pkt) {
    std::unique_lock lock{m_videoMtx};

    if (!m_videoBuf.hasEnoughData(m_videoRawFrameSize)) {
        LOGT("video buff dont have enough data");
        lock.unlock();

        av_usleep(m_videoFrameDuration);

        return false;
    }

    if (av_new_packet(pkt, m_videoRawFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for video error");
    }

    m_videoBuf.pull(pkt->data, m_videoRawFrameSize);

    return true;
}

void Caster::readVideoFrameFromDemuxer(AVPacket *pkt) {
    if (auto ret = av_read_frame(m_inVideoFormatCtx, pkt); ret != 0) {
        if (ret == AVERROR_EOF) {
            LOGD("video stream eof");
            m_terminationReason = TerminationReason::Eof;
        }

        throw std::runtime_error("av_read_frame for video error");
    }
}

bool Caster::filterVideoFrame(VideoTrans trans, AVFrame *frameIn,
                              AVFrame *frameOut) {
    LOGT("filter video frame with trans: " << trans);

    auto &ctx = m_videoFilterCtxMap.at(trans);

    if (av_buffersrc_add_frame_flags(ctx.srcCtx, frameIn,
                                     AV_BUFFERSRC_FLAG_PUSH) < 0)
        throw std::runtime_error("video av_buffersrc_add_frame_flags error");

    auto ret = av_buffersink_get_frame(ctx.sinkCtx, frameOut);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    if (ret < 0)
        throw std::runtime_error("video av_buffersink_get_frame error");

    return true;
}

Caster::VideoTrans Caster::transForYinverted(VideoTrans trans, bool yinverted) {
    if (!yinverted) return trans;

    switch (trans) {
        case VideoTrans::Off:
        case VideoTrans::Scale:
            return VideoTrans::Vflip;
        case VideoTrans::Vflip:
            return VideoTrans::Scale;
        case VideoTrans::Hflip:
            return VideoTrans::VflipHflip;
        case VideoTrans::VflipHflip:
            return VideoTrans::Hflip;
        case VideoTrans::TransClock:
            return VideoTrans::TransClockFlip;
        case VideoTrans::TransClockFlip:
            return VideoTrans::TransClock;
        case VideoTrans::TransCclock:
            return VideoTrans::TransCclockFlip;
        case VideoTrans::TransCclockFlip:
            return VideoTrans::TransCclock;
        case VideoTrans::Frame169:
            return VideoTrans::Frame169Vflip;
        case VideoTrans::Frame169Rot90:
            return VideoTrans::Frame169VflipRot90;
        case VideoTrans::Frame169Rot180:
            return VideoTrans::Frame169VflipRot180;
        case VideoTrans::Frame169Rot270:
            return VideoTrans::Frame169VflipRot270;
        case VideoTrans::Frame169Vflip:
            return VideoTrans::Frame169;
        case VideoTrans::Frame169VflipRot90:
            return VideoTrans::Frame169Rot90;
        case VideoTrans::Frame169VflipRot180:
            return VideoTrans::Frame169Rot180;
        case VideoTrans::Frame169VflipRot270:
            return VideoTrans::Frame169Rot270;
    }

    return trans;
}

AVFrame *Caster::filterVideoIfNeeded(AVFrame *frameIn) {
    auto trans = [this] {
        switch (m_videoTrans) {
            case VideoTrans::Off:
            case VideoTrans::Scale:
            case VideoTrans::Vflip:
            case VideoTrans::Hflip:
            case VideoTrans::VflipHflip:
            case VideoTrans::TransClock:
            case VideoTrans::TransClockFlip:
            case VideoTrans::TransCclock:
            case VideoTrans::TransCclockFlip:
#ifdef USE_LIPSTICK_RECORDER
                if (m_lipstickRecorder) {
                    return transForYinverted(m_videoTrans,
                                             m_lipstickRecorder->yinverted());
                }
#endif
                break;
            case VideoTrans::Frame169:
            case VideoTrans::Frame169Rot90:
            case VideoTrans::Frame169Rot180:
            case VideoTrans::Frame169Rot270:
            case VideoTrans::Frame169Vflip:
            case VideoTrans::Frame169VflipRot90:
            case VideoTrans::Frame169VflipRot180:
            case VideoTrans::Frame169VflipRot270:
#ifdef USE_LIPSTICK_RECORDER
                if (m_lipstickRecorder) {
                    switch (m_lipstickRecorder->transform()) {
                        case LipstickRecorderSource::Transform::Normal:
                            return transForYinverted(
                                VideoTrans::Frame169Rot270,
                                m_lipstickRecorder->yinverted());
                        case LipstickRecorderSource::Transform::Rot90:
                            return transForYinverted(
                                VideoTrans::Frame169,
                                m_lipstickRecorder->yinverted());
                        case LipstickRecorderSource::Transform::Rot180:
                            return transForYinverted(
                                VideoTrans::Frame169Rot90,
                                m_lipstickRecorder->yinverted());
                        case LipstickRecorderSource::Transform::Rot270:
                            return transForYinverted(
                                VideoTrans::Frame169Rot180,
                                m_lipstickRecorder->yinverted());
                    }
                }
#endif
#ifdef USE_DROIDCAM
                if (m_orientationMonitor) {
                    switch (m_orientationMonitor->transform()) {
                        case OrientationMonitor::Transform::Normal:
                            return VideoTrans::Frame169;
                        case OrientationMonitor::Transform::Rot90:
                            return VideoTrans::Frame169Rot90;
                        case OrientationMonitor::Transform::Rot180:
                            return VideoTrans::Frame169Rot180;
                        case OrientationMonitor::Transform::Rot270:
                            return VideoTrans::Frame169Rot270;
                    }
                }
#endif
                break;
        }
        return m_videoTrans;
    }();

    if (trans == VideoTrans::Off) return frameIn;

    if (!filterVideoFrame(trans, frameIn, m_videoFrameAfterFilter)) {
        av_frame_unref(frameIn);
        av_frame_unref(m_videoFrameAfterFilter);
        return nullptr;
    }

    av_frame_unref(frameIn);
    return m_videoFrameAfterFilter;
}

bool Caster::encodeVideoFrame(AVPacket *pkt) {
    if (auto ret = avcodec_send_packet(m_inVideoCtx, pkt);
        ret != 0 && ret != AVERROR(EAGAIN)) {
        av_packet_unref(pkt);
        throw std::runtime_error(fmt::format(
            "avcodec_send_packet for video error ({})", strForAvError(ret)));
    }

    av_packet_unref(pkt);

    if (auto ret = avcodec_receive_frame(m_inVideoCtx, m_videoFrameIn);
        ret != 0) {
        throw std::runtime_error("video avcodec_receive_frame error");
    }

    m_videoFrameIn->format = m_inVideoCtx->pix_fmt;
    m_videoFrameIn->width = m_inVideoCtx->width;
    m_videoFrameIn->height = m_inVideoCtx->height;

    auto *frameOut = filterVideoIfNeeded(m_videoFrameIn);
    if (frameOut == nullptr) return false;

    if (auto ret = avcodec_send_frame(m_outVideoCtx, frameOut);
        ret != 0 && ret != AVERROR(EAGAIN)) {
        av_frame_unref(frameOut);
        throw std::runtime_error("video avcodec_send_frame error");
    }

    av_frame_unref(frameOut);

    if (auto ret = avcodec_receive_packet(m_outVideoCtx, pkt); ret != 0) {
        if (ret == AVERROR(EAGAIN)) {
            LOGD("video pkt not ready");
            return false;
        }

        throw std::runtime_error("video avcodec_receive_packet error");
    }

    return true;
}

bool Caster::avPktOk(const AVPacket *pkt) {
    if (pkt->flags & AV_PKT_FLAG_CORRUPT) {
        LOGW("corrupted pkt detected");
        return false;
    }

    if (pkt->flags & AV_PKT_FLAG_DISCARD) {
        LOGW("discarded pkt detected");
        return false;
    }

    return true;
}

void Caster::extractVideoExtradataFromCompressedDemuxer() {
    auto *pkt = av_packet_alloc();

    readVideoFrameFromDemuxer(pkt);

    if (!extractExtradata(pkt))
        LOGW("failed to extract extradata from video pkt");

    av_packet_unref(pkt);
}

void Caster::extractVideoExtradataFromRawDemuxer() {
    auto *pkt = av_packet_alloc();

    readVideoFrameFromDemuxer(pkt);
    if (encodeVideoFrame(pkt))
        if (!extractExtradata(pkt))
            LOGW("first time failed to extract extradata from video pkt");

    av_packet_unref(pkt);
}

void Caster::extractVideoExtradataFromRawBuf() {
    auto *pkt = av_packet_alloc();

    while (!terminating()) {
        readNullFrame(pkt);
        if (!encodeVideoFrame(pkt)) continue;
        if (!extractExtradata(pkt))
            LOGW("failed to extract extradata from video pkt");
        break;
    }

    av_packet_unref(pkt);
}

bool Caster::extractExtradata(AVPacket *pkt) {
    if (!m_videoBsfExtractExtraCtx || !m_pktSideData.empty()) return false;

    if (auto ret = av_bsf_send_packet(m_videoBsfExtractExtraCtx, pkt);
        ret != 0 && ret != AVERROR(EAGAIN))
        throw std::runtime_error("av_bsf_send_packet error");

    if (auto ret = av_bsf_receive_packet(m_videoBsfExtractExtraCtx, pkt);
        ret != 0)
        return false;

    size_t size = 0;
    auto *sd = av_packet_get_side_data(pkt, AV_PKT_DATA_NEW_EXTRADATA, &size);
    if (size > 0 && sd != nullptr) {
        LOGD("extradata extracted");
        m_pktSideData.resize(size);
        memcpy(&m_pktSideData.front(), sd, size);
    }

    return true;
}

bool Caster::insertExtradata(AVPacket *pkt) const {
    if (!m_videoBsfDumpExtraCtx || m_pktSideData.empty()) return true;

    auto *sd = av_packet_new_side_data(pkt, AV_PKT_DATA_NEW_EXTRADATA,
                                       m_pktSideData.size());
    if (sd == nullptr)
        throw std::runtime_error("av_packet_new_side_data error");

    memcpy(sd, &m_pktSideData.front(), m_pktSideData.size());

    if (auto ret = av_bsf_send_packet(m_videoBsfDumpExtraCtx, pkt);
        ret != AVERROR(EAGAIN) && ret < 0)
        throw std::runtime_error("av_bsf_send_packet error");

    if (auto ret = av_bsf_receive_packet(m_videoBsfDumpExtraCtx, pkt);
        ret < 0 && ret != AVERROR(EAGAIN))
        return false;

    return true;
}

bool Caster::muxVideo(AVPacket *pkt) {
    const auto now = av_gettime();

    LOGT("video read real frame");

    switch (videoProps().type) {
        case VideoSourceType::DroidCam:
            readVideoFrameFromDemuxer(pkt);
            break;
        case VideoSourceType::V4l2:
        case VideoSourceType::X11Capture:
            readVideoFrameFromDemuxer(pkt);
            if (!encodeVideoFrame(pkt)) return false;
            break;
        case VideoSourceType::LipstickCapture:
        case VideoSourceType::Test:
        case VideoSourceType::DroidCamRaw:
            if (!readVideoFrameFromBuf(pkt)) return false;
            if (!encodeVideoFrame(pkt)) return false;
            break;
        default:
            throw std::runtime_error("unknown video source type");
    }

    if (!avPktOk(pkt)) {
        av_packet_unref(pkt);
        return false;
    }

    if (!insertExtradata(pkt)) return false;

    updateVideoSampleStats(now);

    LOGT("video: frd=" << m_videoRealFrameDuration << ", npts="
                       << m_nextVideoPts << ", lft=" << m_videoTimeLastFrame
                       << ", os_tb=" << m_outVideoStream->time_base
                       << ", data=" << dataToStr(pkt->data, pkt->size));

    pkt->stream_index = m_outVideoStream->index;
    pkt->pts = m_nextVideoPts;
    pkt->dts = m_nextVideoPts;

    pkt->duration =
        rescaleFromUsec(m_videoRealFrameDuration, m_outVideoStream->time_base);

    m_nextVideoPts += pkt->duration;

    if (auto ret = av_write_frame(m_outFormatCtx, pkt); ret < 0)
        throw std::runtime_error("av_interleaved_write_frame for video error");

    av_packet_unref(pkt);

    if (!m_videoFlushed) {
        LOGD("first av video data");
        m_videoFlushed = true;
    }

    return true;
}

int64_t Caster::videoAudioDelay() const {
    auto video_pts = rescaleToUsec(m_nextVideoPts, m_outVideoStream->time_base);
    auto audio_pts = rescaleToUsec(m_nextAudioPts, m_outAudioStream->time_base);
    return video_pts - audio_pts;
}

int64_t Caster::videoDelay(int64_t now) const {
    if (m_videoTimeLastFrame == 0) return m_videoRealFrameDuration;
    return now - (m_videoTimeLastFrame + m_videoRealFrameDuration);
}

int64_t Caster::audioDelay(int64_t now) const {
    if (m_audioTimeLastFrame == 0) return m_audioFrameDuration;
    return now - (m_audioTimeLastFrame + m_audioFrameDuration);
}

bool Caster::reInitAvAudioInput() {
    cleanAvAudioInputFormat();

    if (!initAvAudioInputFormatFromFile()) return false;

    findAvAudioInputStreamIdx();

    LOGD("audio input re-inited");

    return true;
}

void Caster::reInitAvAudioDecoder() {
    cleanAvAudioDecoder();
    cleanAvAudioFifo();
    cleanAvAudioFilters();

    initAvAudioRawDecoderFromInputStream();
    initAvAudioFifo();
    initAvAudioFilters();

    m_audioInFrameSize = av_samples_get_buffer_size(
        nullptr, m_inAudioCtx->ch_layout.nb_channels, m_outAudioCtx->frame_size,
        m_inAudioCtx->sample_fmt, 0);

    LOGD("audio decoder re-inited");
}

bool Caster::readAudioPktFromBuf(AVPacket *pkt, bool nullWhenNoEnoughData) {
    std::lock_guard lock{m_audioMtx};

    if (!m_audioBuf.hasEnoughData(m_audioInFrameSize)) {
        const auto pushNull = m_paStream == nullptr || nullWhenNoEnoughData;

        if (pushNull) {
            LOGD("audio push null: "
                 << (m_audioInFrameSize - m_audioBuf.size()));
            m_audioBuf.pushNullExactForce(m_audioInFrameSize -
                                          m_audioBuf.size());
        } else {
            return false;
        }
    }

    if (av_new_packet(pkt, m_audioInFrameSize) < 0)
        throw std::runtime_error("av_new_packet for audio error");

    if (!m_audioBuf.pullExact(pkt->data, m_audioInFrameSize))
        throw std::runtime_error("failed to pull from buf");

    return true;
}

bool Caster::readAudioPktFromDemuxer(AVPacket *pkt) {
    if (auto ret = av_read_frame(m_inAudioFormatCtx, pkt); ret != 0) {
        if (ret == AVERROR_EOF) {
            LOGD("audio stream eof");

            if (audioProps().type == AudioSourceType::File) {
                cleanAvAudioInputFormat();
                if (m_config.fileSourceConfig &&
                    m_config.fileSourceConfig->fileStreamingDoneHandler)
                    m_config.fileSourceConfig->fileStreamingDoneHandler(
                        m_currentFile, m_files.size());

                if (reInitAvAudioInput()) {
                    if (!m_config.fileSourceConfig ||
                        !(m_config.fileSourceConfig->flags &
                          FileSourceFlags::SameFormatForAllFiles))
                        reInitAvAudioDecoder();
                    return false;
                }
            }

            m_terminationReason = TerminationReason::Eof;
        }
        throw std::runtime_error("av_read_frame from audio demuxer error");
    }

    if (pkt->stream_index != m_inAudioStreamIdx) {
        av_packet_unref(pkt);
        return false;
    }

    return true;
}

bool Caster::readAudioFrameFromDemuxer(AVPacket *pkt) {
    return readAudioFrame(pkt, DataSource::Demuxer);
}

bool Caster::readAudioFrame(AVPacket *pkt, DataSource source,
                            bool nullWhenNoEnoughData) {
    while (av_audio_fifo_size(m_audioFifo) < m_outAudioCtx->frame_size) {
        if (terminating() || m_state == State::Paused) return false;

        if (source == DataSource::Demuxer) {
            if (!readAudioPktFromDemuxer(pkt)) continue;
        } else {
            if (!readAudioPktFromBuf(pkt, nullWhenNoEnoughData)) return false;
        }

        if (auto ret = avcodec_send_packet(m_inAudioCtx, pkt);
            ret != 0 && ret != AVERROR(EAGAIN)) {
            LOGT("audio decoding error")

            av_packet_unref(pkt);
            return false;
        }

        av_packet_unref(pkt);

        if (auto ret = avcodec_receive_frame(m_inAudioCtx, m_audioFrameIn);
            ret != 0) {
            if (ret == AVERROR(EAGAIN)) continue;

            throw std::runtime_error(
                "avcodec_receive_frame from audio decoder error");
        }

        if (av_audio_fifo_realloc(m_audioFifo, av_audio_fifo_size(m_audioFifo) +
                                                   m_audioFrameIn->nb_samples) <
            0)
            throw std::runtime_error("av_audio_fifo_realloc error");

        if (av_audio_fifo_write(
                m_audioFifo, reinterpret_cast<void **>(m_audioFrameIn->data),
                m_audioFrameIn->nb_samples) < m_audioFrameIn->nb_samples)
            throw std::runtime_error("av_audio_fifo_write error");

        av_frame_unref(m_audioFrameIn);
    }

    m_audioFrameIn->nb_samples = m_outAudioCtx->frame_size;
    av_channel_layout_copy(&m_audioFrameIn->ch_layout,
                           &m_inAudioCtx->ch_layout);
    m_audioFrameIn->format = m_inAudioCtx->sample_fmt;
    m_audioFrameIn->sample_rate = m_inAudioCtx->sample_rate;

    if (av_frame_get_buffer(m_audioFrameIn, 0) != 0)
        throw std::runtime_error("av_frame_get_buffer error");

    if (av_audio_fifo_read(
            m_audioFifo, reinterpret_cast<void **>(m_audioFrameIn->data),
            m_outAudioCtx->frame_size) < m_outAudioCtx->frame_size) {
        av_frame_free(&m_audioFrameIn);
        throw std::runtime_error("av_audio_fifo_read error");
    }

    return true;
}

bool Caster::readAudioFrameFromBuf(AVPacket *pkt, bool nullWhenNoEnoughData) {
    return readAudioFrame(pkt, DataSource::Buf, nullWhenNoEnoughData);
}

bool Caster::filterAudioFrame(AudioTrans trans, AVFrame *frameIn,
                              AVFrame *frameOut) {
    LOGT("filter audio frame with trans: " << trans);

    auto &ctx = m_audioFilterCtxMap.at(trans);

    if (av_buffersrc_add_frame_flags(ctx.srcCtx, frameIn,
                                     AV_BUFFERSRC_FLAG_PUSH) < 0) {
        av_frame_unref(frameIn);
        throw std::runtime_error("audio av_buffersrc_add_frame_flags error");
    }

    auto ret = av_buffersink_get_frame(ctx.sinkCtx, frameOut);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    if (ret < 0) {
        av_frame_unref(frameIn);
        throw std::runtime_error("audio av_buffersink_get_frame error");
    }

    return true;
}

AVFrame *Caster::filterAudioIfNeeded(AVFrame *frameIn) {
    if (m_audioTrans == AudioTrans::Off) return frameIn;

    updateAudioVolumeFilter();

    if (!filterAudioFrame(m_audioTrans, frameIn, m_audioFrameAfterFilter)) {
        av_frame_unref(frameIn);
        av_frame_unref(m_audioFrameAfterFilter);
        return nullptr;
    }

    av_frame_unref(frameIn);
    return m_audioFrameAfterFilter;
}

void Caster::updateAudioVolumeFilter() {
    if (!m_audioVolumeUpdated) return;

    m_audioVolumeUpdated = false;

    if (!m_audioFilterCtxMap.count(AudioTrans::Volume)) return;

    auto ctx = m_audioFilterCtxMap.at(AudioTrans::Volume);

    std::array<char, 512> res{};
    if (avfilter_graph_send_command(
            ctx.graph, "volume", "volume",
            fmt::format("{}dB", m_config.audioVolume).c_str(), res.data(),
            res.size(), 0) < 0) {
        throw std::runtime_error("audio avfilter_graph_send_command error");
    }
}

bool Caster::encodeAudioFrame(AVPacket *pkt) {
    auto *frameOut = filterAudioIfNeeded(m_audioFrameIn);
    if (frameOut == nullptr) return false;

    if (auto ret = avcodec_send_frame(m_outAudioCtx, frameOut);
        ret != 0 && ret != AVERROR(EAGAIN)) {
        av_frame_unref(frameOut);
        throw std::runtime_error("audio avcodec_send_frame error");
    }

    av_frame_unref(frameOut);

    if (auto ret = avcodec_receive_packet(m_outAudioCtx, pkt); ret != 0) {
        if (ret == AVERROR(EAGAIN)) {
            LOGD("audio pkt not ready");
            return false;
        }

        throw std::runtime_error("audio avcodec_receive_packet error");
    }

    return true;
}

bool Caster::muxAudio(AVPacket *pkt) {
    bool pktDone = false;

    while (!terminating() && m_state != State::Paused) {
        auto now = av_gettime();
        const auto maxAudioDelay =
            m_videoRealFrameDuration > 0
                ? std::max(m_videoRealFrameDuration, 2 * m_audioFrameDuration)
                : 2 * m_audioFrameDuration;
        const auto delay =
            m_videoRealFrameDuration > 0 ? videoAudioDelay() : audioDelay(now);

        LOGT("audio: delay=" << delay
                             << ", audio frame dur=" << m_audioFrameDuration
                             << ", push null=" << (delay > maxAudioDelay));
        if (delay < -maxAudioDelay) {
            LOGW("too much audio, delay=" << delay);
            break;
        }

        if (delay < m_audioFrameDuration) break;

        if (audioProps().type == AudioSourceType::File) {
            if (!readAudioFrameFromDemuxer(pkt)) break;
        } else {
            if (!readAudioFrameFromBuf(pkt,
                                       delay > maxAudioDelay && m_videoFlushed))
                break;
        }

        if (!encodeAudioFrame(pkt)) continue;

        if (!avPktOk(pkt)) {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = m_outAudioStream->index;
        pkt->pts = m_nextAudioPts;
        pkt->dts = m_nextAudioPts;
        pkt->duration = m_audioPktDuration;
        m_nextAudioPts += pkt->duration;

        if (pkt->pts == 0)
            m_audioTimeLastFrame = now;
        else
            m_audioTimeLastFrame += m_audioFrameDuration;

        LOGT("audio mux: tb=" << m_outAudioStream->time_base
                              << ", pkt=" << pkt);

        if (auto ret = av_write_frame(m_outFormatCtx, pkt); ret < 0)
            throw std::runtime_error(
                "av_interleaved_write_frame for audio error");

        av_packet_unref(pkt);

        if (!m_audioFlushed) {
            LOGD("first av audio data");
            m_audioFlushed = true;
        }

        pktDone = true;
    }

    return pktDone;
}

std::string Caster::strForAvError(int err) {
    char str[AV_ERROR_MAX_STRING_SIZE];

    if (av_strerror(err, str, AV_ERROR_MAX_STRING_SIZE) < 0) {
        return std::to_string(err);
    }

    return std::string(str);
}

int Caster::avWritePacketCallbackStatic(void *opaque, ff_buf_type buf,
                                        int bufSize) {
    return static_cast<Caster *>(opaque)->avWritePacketCallback(buf, bufSize);
}

int Caster::avWritePacketCallback(ff_buf_type buf, int bufSize) {
    if (bufSize < 0)
        throw std::runtime_error("invalid read packet callback buf size");

    LOGT("write packet: size=" << bufSize
                               << ", data=" << dataToStr(buf, bufSize));

    if (!terminating() && m_dataReadyHandler) {
        if (!m_muxedFlushed && m_avMuxingThread.joinable()) {
            LOGD("first av muxed data");
            m_muxedFlushed = true;
        }
        return static_cast<int>(m_dataReadyHandler(buf, bufSize));
    }

    return bufSize;
}

int Caster::avReadPacketCallbackStatic(void *opaque, uint8_t *buf,
                                       int bufSize) {
    return static_cast<Caster *>(opaque)->avReadPacketCallback(buf, bufSize);
}

int Caster::avReadPacketCallback(uint8_t *buf, int bufSize) {
    if (bufSize < 0)
        throw std::runtime_error("invalid read_packet_callback buf size");

    LOGT("read packet: request");

    std::unique_lock lock{m_videoMtx};
    m_videoCv.wait(lock, [this] {
        return terminating() || m_state == State::Paused || !m_videoBuf.empty();
    });

    if (terminating() || m_state == State::Paused) {
        m_videoBuf.clear();
        lock.unlock();
        m_videoCv.notify_one();
        LOGT("read packet: eof");
        return AVERROR_EOF;
    }

    auto pulledSize = m_videoBuf.pull(buf, bufSize);

    LOGT("read packet: done, size=" << pulledSize
                                    << ", data=" + dataToStr(buf, pulledSize));

    lock.unlock();
    m_videoCv.notify_one();

    return static_cast<decltype(bufSize)>(pulledSize);
}

void Caster::updateVideoSampleStats(int64_t now) {
    if (m_videoTimeLastFrame > 0) {
        auto lastDur = now - m_videoTimeLastFrame;
        if (lastDur >= m_videoRealFrameDuration / 4)
            m_videoRealFrameDuration = lastDur; 
    }
    m_videoTimeLastFrame = now;
}

void Caster::setAudioVolume(int volume) {
    if (volume < -50 || volume > 50) {
        LOGW("audio-volume is invalid");
        return;
    }

    if (volume == m_config.audioVolume) return;

    m_config.audioVolume = volume;
    m_audioVolumeUpdated = true;
}

uint32_t Caster::hash(std::string_view str) {
    return std::accumulate(str.cbegin(), str.cend(), 0U) % 999;
}

Caster::VideoPropsMap Caster::detectVideoSources(uint32_t options) {
    avdevice_register_all();

    VideoPropsMap props;
#ifdef USE_DROIDCAM
    if (options & OptionsFlags::DroidCamVideoSources ||
        options & OptionsFlags::DroidCamRawVideoSources)
        props.merge(detectDroidCamVideoSources(options));
#endif
#ifdef USE_V4L2
    if (options & OptionsFlags::V4l2VideoSources)
        props.merge(detectV4l2VideoSources());
#endif
#ifdef USE_X11CAPTURE
    if (options & OptionsFlags::X11CaptureVideoSources)
        props.merge(detectX11VideoSources());
#endif
#ifdef USE_LIPSTICK_RECORDER
    if (options & OptionsFlags::LipstickCaptureVideoSources)
        props.merge(detectLipstickRecorderVideoSources());
#endif
#ifdef USE_TESTSOURCE
    props.merge(detectTestVideoSources());
#endif
    return props;
}

Caster::SensorDirection Caster::videoDirection() const {
    return videoProps().sensorDirection;
}

Caster::VideoPropsMap Caster::detectTestVideoSources() {
    LOGD("test video source detecton started");
    VideoPropsMap map;

    if (TestSource::supported()) {
        auto iprops = TestSource::properties();

        {
            VideoSourceInternalProps props;
            props.type = VideoSourceType::Test;
            props.formats.push_back(
                {AV_CODEC_ID_RAWVIDEO,
                 iprops.pixfmt,
                 {{{iprops.width, iprops.height}, {iprops.framerate}}}});
            props.name = "test";
            props.friendlyName = "Test";
            props.orientation = iprops.width < iprops.height
                                    ? VideoOrientation::Portrait
                                    : VideoOrientation::Landscape;

            LOGD("test source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }

        {
            VideoSourceInternalProps props;
            props.type = VideoSourceType::Test;
            props.formats.push_back(
                {AV_CODEC_ID_RAWVIDEO,
                 iprops.pixfmt,
                 {{{iprops.width, iprops.height}, {iprops.framerate}}}});
            props.name = "test-169l";
            props.friendlyName = "Test (16:9 landscape)";
            props.trans = VideoTrans::Frame169;
            props.orientation = iprops.width < iprops.height
                                    ? VideoOrientation::Portrait
                                    : VideoOrientation::Landscape;

            LOGD("test source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }

        {
            VideoSourceInternalProps props;
            props.type = VideoSourceType::Test;
            props.formats.push_back(
                {AV_CODEC_ID_RAWVIDEO,
                 iprops.pixfmt,
                 {{{iprops.width, iprops.height}, {iprops.framerate}}}});
            props.name = "test-169p";
            props.friendlyName = "Test (16:9 portrait)";
            props.trans = VideoTrans::Frame169Rot90;
            props.orientation = iprops.width < iprops.height
                                    ? VideoOrientation::Portrait
                                    : VideoOrientation::Landscape;

            LOGD("test source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }
    };

    LOGD("test video source detecton completed");

    return map;
}

void Caster::rawVideoDataReadyHandler(const uint8_t *data, size_t size) {
    if (terminating()) return;

    LOGT("raw video data ready: size=" << size);

    std::lock_guard lock{m_videoMtx};

    if (m_videoBuf.hasEnoughData(size)) {
        m_videoBuf.pushOverwriteTail(data, size);
    } else {
        m_videoBuf.pushExactForce(data, size);
    }
}

void Caster::compressedVideoDataReadyHandler(const uint8_t *data, size_t size) {
    if (terminating()) return;

    LOGT("compressed video data ready: size=" << size);

    std::unique_lock lock{m_videoMtx};
    m_videoCv.wait(lock, [this, size] {
        return terminating() || m_state == State::Paused ||
               m_videoBuf.hasFreeSpace(size);
    });

    if (terminating() || m_state == State::Paused) {
        m_videoBuf.clear();
    } else {
        m_videoBuf.pushExact(data, size);
        LOGT("compressed data written to video buf");
    }

    if (!m_avMuxingThread.joinable()) /* video muxing not started yet */ {
        updateVideoSampleStats(av_gettime());
    }

    lock.unlock();
    m_videoCv.notify_one();
}

#ifdef USE_X11CAPTURE
static std::vector<AVPixelFormat> x11Pixfmts(Display *dpy) {
    std::vector<AVPixelFormat> fmts;
    auto bo = BitmapBitOrder(dpy);
    int n = 0;
    auto *pmf = XListPixmapFormats(dpy, &n);
    if (pmf) {
        fmts.reserve(n);
        for (int i = 0; i < n; ++i) {
            auto pixfmt = ff_tools::ff_fmt_x112ff(bo, pmf[i].depth,
                                                  pmf[i].bits_per_pixel);
            if (pixfmt != AV_PIX_FMT_NONE) fmts.push_back(pixfmt);
        }
        XFree(pmf);
    }
    return fmts;
}

Caster::VideoPropsMap Caster::detectX11VideoSources() {
    LOGD("x11 source detecton started");

    VideoPropsMap map;

    if (av_find_input_format("x11grab") == nullptr) return map;

    auto *dpy = XOpenDisplay(nullptr);
    if (dpy == nullptr) return map;

    if (DisplayString(dpy) == nullptr) {
        LOGW("x11 display string is null");
        XCloseDisplay(dpy);
        return map;
    }

    auto pixfmts = x11Pixfmts(dpy);

    int count = ScreenCount(dpy);

    LOGD("x11 screen count: " << count);

    for (int i = 0; i < count; ++i) {
        VideoSourceInternalProps props;
        props.type = VideoSourceType::X11Capture;
        props.name = fmt::format("screen-{}", i + 1);
        props.friendlyName = fmt::format("Screen {} capture", i + 1);
        props.dev = fmt::format("{}.{}", DisplayString(dpy), i);

        FrameSpec fs{Dim{static_cast<uint32_t>(XDisplayWidth(dpy, i)),
                         static_cast<uint32_t>(XDisplayHeight(dpy, i))},
                     {30}};

        props.orientation = fs.dim.orientation();

        for (auto pixfmt : pixfmts) {
            props.formats.push_back(
                VideoFormatExt{AV_CODEC_ID_RAWVIDEO, pixfmt, {fs}});
        }

        LOGD("x11 source found: " << props);

        map.try_emplace(props.name, std::move(props));
    }

    XCloseDisplay(dpy);

    LOGD("x11 source detecton completed");

    return map;
}
#endif  // USE_X11CAPTURE

#ifdef USE_DROIDCAM
Caster::VideoPropsMap Caster::detectDroidCamVideoSources(uint32_t options) {
    LOGD("droidcam detection started");

    VideoPropsMap props;

    if (!DroidCamSource::supported()) return props;

    auto dp = DroidCamSource::properties();

    if (options & OptionsFlags::DroidCamVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCam;
        p.dev = "0";
        p.formats.push_back(
            {dp.second.codecId,
             dp.second.pixfmt,
             {{{dp.second.width, dp.second.height}, {dp.second.framerate}}}});
        p.sensorDirection = SensorDirection::Back;
        p.name = "cam-back";
        p.friendlyName = "Back camera";
        p.orientation = VideoOrientation::Landscape;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    if (options & OptionsFlags::DroidCamVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCam;
        p.dev = "1";
        p.formats.push_back(
            {dp.second.codecId,
             dp.second.pixfmt,
             {{{dp.second.width, dp.second.height}, {dp.second.framerate}}}});
        p.sensorDirection = SensorDirection::Front;
        p.name = "cam-front";
        p.friendlyName = "Front camera";
        p.orientation = VideoOrientation::InvertedLandscape;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    if (options & OptionsFlags::DroidCamRawVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCamRaw;
        p.dev = "0";
        p.formats.push_back(
            {dp.first.codecId,
             dp.first.pixfmt,
             {{{dp.first.width, dp.first.height}, {dp.first.framerate}}}});
        p.sensorDirection = SensorDirection::Back;
        p.name = "cam-raw-back";
        p.friendlyName = "Back camera";
        p.orientation = VideoOrientation::Landscape;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    if (options & OptionsFlags::DroidCamRawVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCamRaw;
        p.dev = "1";
        p.formats.push_back(
            {dp.first.codecId,
             dp.first.pixfmt,
             {{{dp.first.width, dp.first.height}, {dp.first.framerate}}}});
        p.sensorDirection = SensorDirection::Front;
        p.name = "cam-raw-front";
        p.friendlyName = "Front camera";
        p.orientation = VideoOrientation::Landscape;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    if (options & OptionsFlags::DroidCamRawVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCamRaw;
        p.dev = "0";
        p.formats.push_back(
            {dp.first.codecId,
             dp.first.pixfmt,
             {{{dp.first.width, dp.first.height}, {dp.first.framerate}}}});
        p.sensorDirection = SensorDirection::Back;
        p.name = "cam-raw-back-rotate";
        p.friendlyName = "Back camera (auto rotate)";
        p.orientation = VideoOrientation::Landscape;
        p.trans = VideoTrans::Frame169;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    if (options & OptionsFlags::DroidCamRawVideoSources) {
        VideoSourceInternalProps p;
        p.type = VideoSourceType::DroidCamRaw;
        p.dev = "1";
        p.formats.push_back(
            {dp.first.codecId,
             dp.first.pixfmt,
             {{{dp.first.width, dp.first.height}, {dp.first.framerate}}}});
        p.sensorDirection = SensorDirection::Front;
        p.name = "cam-raw-front-rotate";
        p.friendlyName = "Front camera (auto rotate)";
        p.orientation = VideoOrientation::Landscape;
        p.trans = VideoTrans::Frame169;

        LOGD("droidcam source found: " << p);

        props.try_emplace(p.name, std::move(p));
    }

    LOGD("droidcam detection completed");

    return props;
}
#endif  // USE_DROIDCAM

#ifdef USE_V4L2
static inline auto isV4lDev(const char *name) {
    return av_strstart(name, "video", nullptr);
}

static std::optional<std::string> readLinkTarget(const std::string &file) {
    char link[64];
    auto linkLen = readlink(file.c_str(), link, sizeof(link));

    if (linkLen < 0) return std::nullopt;  // not a link

    std::string target;
    if (link[0] != '/') target.assign("/dev/");
    target += std::string(link, linkLen);

    return target;
}

static auto v4lDevFiles() {
    std::vector<std::string> files;

    auto *dir = opendir("/dev");
    if (dir == nullptr) throw std::runtime_error("failed to open /dev dir");

    while (auto *entry = readdir(dir)) {
        if (isV4lDev(entry->d_name)) {
            files.push_back("/dev/"s + entry->d_name);
            if (files.size() > 1000) break;
        }
    }

    closedir(dir);

    for (auto it = files.begin(); it != files.end();) {
        auto target = readLinkTarget(*it);
        if (!target) {
            ++it;
            continue;
        }

        if (std::find(files.cbegin(), files.cend(), *target) == files.end()) {
            ++it;
            continue;
        }

        it = files.erase(it);
    }

    std::sort(files.begin(), files.end());

    return files;
}

std::vector<Caster::FrameSpec> Caster::detectV4l2FrameSpecs(
    int fd, uint32_t pixelformat) {
    std::vector<FrameSpec> specs;

    v4l2_frmsizeenum vfse{};
    vfse.type = V4L2_FRMIVAL_TYPE_DISCRETE;
    vfse.pixel_format = pixelformat;

    for (int i = 0; i < m_maxIters && !ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &vfse);
         ++i) {
        if (vfse.type != V4L2_FRMSIZE_TYPE_DISCRETE) break;

        v4l2_frmivalenum vfie{};
        vfie.type = V4L2_FRMIVAL_TYPE_DISCRETE;
        vfie.pixel_format = pixelformat;
        vfie.height = vfse.discrete.height;
        vfie.width = vfse.discrete.width;

        FrameSpec spec;
        for (int ii = 0;
             ii < m_maxIters && !ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &vfie);
             ++ii) {
            if (vfie.type != V4L2_FRMIVAL_TYPE_DISCRETE) break;

            if (vfie.discrete.numerator == 1)
                spec.framerates.insert(vfie.discrete.denominator);

            vfie.index++;
        }

        if (!spec.framerates.empty()) {
            spec.dim = {vfse.discrete.width, vfse.discrete.height};
            specs.push_back(std::move(spec));
        }

        vfse.index++;
    }

    std::sort(specs.begin(), specs.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.dim > rhs.dim;
    });

    return specs;
}

void Caster::addV4l2VideoFormats(int fd, v4l2_buf_type type,
                                 std::vector<VideoFormat> &formats) {
    v4l2_fmtdesc vfmt{};
    vfmt.type = type;

    for (int i = 0; i < m_maxIters && !ioctl(fd, VIDIOC_ENUM_FMT, &vfmt); ++i) {
        if (vfmt.type == 0) break;

        vfmt.index++;

        auto c = ff_tools::ff_fmt_v4l2codec(vfmt.pixelformat);
        if (c == AV_CODEC_ID_NONE) continue;

        formats.push_back({c, ff_tools::ff_fmt_v4l2ff(vfmt.pixelformat, c)});
    }
}

void Caster::addV4l2VideoFormatsExt(int fd, v4l2_buf_type type,
                                    std::vector<VideoFormatExt> &formats) {
    v4l2_fmtdesc vfmt{};
    vfmt.type = type;

    for (int i = 0; i < m_maxIters && !ioctl(fd, VIDIOC_ENUM_FMT, &vfmt); ++i) {
        if (vfmt.type == 0) break;

        vfmt.index++;

        auto c = ff_tools::ff_fmt_v4l2codec(vfmt.pixelformat);
        if (c == AV_CODEC_ID_NONE) continue;

        auto pf = ff_tools::ff_fmt_v4l2ff(vfmt.pixelformat, c);
        if (pf == AV_PIX_FMT_NONE) continue;

        auto fs = detectV4l2FrameSpecs(fd, vfmt.pixelformat);
        if (fs.empty()) continue;

        formats.push_back({c, pf, std::move(fs)});
    }
}

static auto v4l2Caps(uint32_t caps) {
    std::ostringstream os;
    if (caps & V4L2_CAP_VIDEO_CAPTURE) os << "V4L2_CAP_VIDEO_CAPTURE, ";
    if (caps & V4L2_CAP_VIDEO_OUTPUT) os << "V4L2_CAP_VIDEO_OUTPUT, ";
    if (caps & V4L2_CAP_VIDEO_OVERLAY) os << "V4L2_CAP_VIDEO_OVERLAY, ";
    if (caps & V4L2_CAP_VBI_CAPTURE) os << "V4L2_CAP_VBI_CAPTURE, ";
    if (caps & V4L2_CAP_VBI_OUTPUT) os << "V4L2_CAP_VBI_OUTPUT, ";
    if (caps & V4L2_CAP_SLICED_VBI_CAPTURE)
        os << "V4L2_CAP_SLICED_VBI_CAPTURE, ";
    if (caps & V4L2_CAP_SLICED_VBI_OUTPUT) os << "V4L2_CAP_SLICED_VBI_OUTPUT, ";
    if (caps & V4L2_CAP_RDS_CAPTURE) os << "V4L2_CAP_RDS_CAPTURE, ";
    if (caps & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
        os << "V4L2_CAP_VIDEO_OUTPUT_OVERLAY, ";
    if (caps & V4L2_CAP_HW_FREQ_SEEK) os << "V4L2_CAP_HW_FREQ_SEEK, ";
    if (caps & V4L2_CAP_RDS_OUTPUT) os << "V4L2_CAP_RDS_OUTPUT, ";
    if (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        os << "V4L2_CAP_VIDEO_CAPTURE_MPLANE, ";
    if (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
        os << "V4L2_CAP_VIDEO_OUTPUT_MPLANE, ";
    if (caps & V4L2_CAP_VIDEO_M2M_MPLANE) os << "V4L2_CAP_VIDEO_M2M_MPLANE, ";
    if (caps & V4L2_CAP_VIDEO_M2M) os << "V4L2_CAP_VIDEO_M2M, ";
    if (caps & V4L2_CAP_TUNER) os << "V4L2_CAP_TUNER, ";
    if (caps & V4L2_CAP_AUDIO) os << "V4L2_CAP_AUDIO, ";
    if (caps & V4L2_CAP_RADIO) os << "V4L2_CAP_RADIO, ";
    if (caps & V4L2_CAP_MODULATOR) os << "V4L2_CAP_MODULATOR, ";
    if (caps & V4L2_CAP_SDR_CAPTURE) os << "V4L2_CAP_SDR_CAPTURE, ";
    if (caps & V4L2_CAP_EXT_PIX_FORMAT) os << "V4L2_CAP_EXT_PIX_FORMAT, ";
    if (caps & V4L2_CAP_SDR_OUTPUT) os << "V4L2_CAP_SDR_OUTPUT, ";
    if (caps & V4L2_CAP_META_CAPTURE) os << "V4L2_CAP_META_CAPTURE, ";
    if (caps & V4L2_CAP_READWRITE) os << "V4L2_CAP_READWRITE, ";
    if (caps & V4L2_CAP_ASYNCIO) os << "V4L2_CAP_ASYNCIO, ";
    if (caps & V4L2_CAP_STREAMING) os << "V4L2_CAP_STREAMING, ";
    if (caps & V4L2_CAP_META_OUTPUT) os << "V4L2_CAP_META_OUTPUT, ";
    if (caps & V4L2_CAP_TOUCH) os << "V4L2_CAP_TOUCH, ";
    if (caps & V4L2_CAP_IO_MC) os << "V4L2_CAP_IO_MC, ";
    if (caps & V4L2_CAP_DEVICE_CAPS) os << "V4L2_CAP_DEVICE_CAPS, ";
    return os.str();
}

static bool mightBeV4l2m2mEncoder(uint32_t caps) {
    return !(caps & V4L2_CAP_VIDEO_CAPTURE) &&
           !(caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) &&
           (caps & V4L2_CAP_VIDEO_M2M || caps & V4L2_CAP_VIDEO_M2M_MPLANE);
}

static bool mightBeV4l2Cam(uint32_t caps) {
    return (caps & V4L2_CAP_VIDEO_CAPTURE ||
            caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
}

Caster::VideoPropsMap Caster::detectV4l2VideoSources() {
    LOGD("v4l2 sources detection started");
    auto files = v4lDevFiles();

    decltype(detectV4l2VideoSources()) cards;  // bus_info => props

    v4l2_capability vcap = {};

    for (auto &file : files) {
        auto fd = open(file.c_str(), O_RDWR);

        if (fd < 0) continue;

        if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0 ||
            !mightBeV4l2Cam(vcap.device_caps)) {
            close(fd);
            continue;
        }

        LOGD("found v4l2 dev: file="
             << file
             << ", card=" << reinterpret_cast<const char *>(vcap.bus_info)
             << ", caps=[" << v4l2Caps(vcap.device_caps) << "]");

        std::vector<VideoFormatExt> outFormats;

        addV4l2VideoFormatsExt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, outFormats);
        addV4l2VideoFormatsExt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                               outFormats);

        if (outFormats.empty()) {
            close(fd);
            continue;
        }

        Caster::VideoSourceInternalProps props;
        props.type = VideoSourceType::V4l2;
        props.name = fmt::format(
            "cam-{:03}", hash(reinterpret_cast<const char *>(vcap.card)));
        props.dev = std::move(file);
        props.friendlyName = reinterpret_cast<const char *>(vcap.card);
        props.orientation =
            outFormats.front().frameSpecs.front().dim.orientation();
        props.formats = std::move(outFormats);

        LOGD("v4l2 source found: " << props);

        cards.try_emplace(reinterpret_cast<const char *>(vcap.bus_info),
                          std::move(props));
        close(fd);
    }

    decltype(detectV4l2VideoSources()) cams;

    for (auto &&pair : cards) {
        cams.try_emplace(pair.second.name, std::move(pair.second));
    }

    LOGD("v4l2 sources detection completed");

    return cams;
}

void Caster::detectV4l2Encoders() {
    LOGD("v4l2 encoders detection started");

    auto files = v4lDevFiles();

    v4l2_capability vcap{};

    for (auto &file : files) {
        auto fd = open(file.c_str(), O_RDWR);

        if (fd < 0) continue;

        if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0 ||
            !mightBeV4l2m2mEncoder(vcap.device_caps)) {
            close(fd);
            continue;
        }

        LOGD("found v4l2 dev: file="
             << file
             << ", card=" << reinterpret_cast<const char *>(vcap.bus_info)
             << ", caps=[" << v4l2Caps(vcap.device_caps) << "]");

        std::vector<VideoFormat> outFormats;
        addV4l2VideoFormats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, outFormats);
        addV4l2VideoFormats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, outFormats);
        if (outFormats.empty()) {
            LOGD("v4l2 encoder does not support h264");
            close(fd);
            continue;
        }

        std::vector<VideoFormat> formats;
        addV4l2VideoFormats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, formats);
        addV4l2VideoFormats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, formats);

        if (!formats.empty()) {
            auto props = m_v4l2Encoders.emplace_back(
                V4l2H264EncoderProps{file, std::move(formats)});
            LOGD("found v4l2 encoder: " << props);
        }

        close(fd);
    }

    LOGD("v4l2 encoders detection completed");
}

std::pair<std::reference_wrapper<const Caster::VideoFormatExt>, AVPixelFormat>
Caster::bestVideoFormatForV4l2Encoder(const VideoSourceInternalProps &props) {
    if (m_v4l2Encoders.empty()) throw std::runtime_error("no v4l2 encoder");

    if (auto it = std::find_if(
            props.formats.cbegin(), props.formats.cend(),
            [&](const auto &sf) {
                return std::any_of(
                    m_v4l2Encoders.cbegin(), m_v4l2Encoders.cend(),
                    [&sf, useNiceFormats = m_config.options &
                                           OptionsFlags::OnlyNiceVideoFormats](
                        const auto &e) {
                        return std::any_of(
                            e.formats.cbegin(), e.formats.cend(),
                            [&](const auto &ef) {
                                if (sf.codecId != ef.codecId) return false;
                                if (useNiceFormats && !nicePixfmt(ef.pixfmt))
                                    return false;
                                return sf.pixfmt == ef.pixfmt;
                            });
                    });
            });
        it != props.formats.cend()) {
        LOGD("pixfmt exact match: " << it->pixfmt);

        return {*it, it->pixfmt};
    }

    if (m_config.options & OptionsFlags::OnlyNiceVideoFormats) {
        auto fmt = [this] {
            for (const auto &f : m_v4l2Encoders.front().formats)
                if (f.codecId == AV_CODEC_ID_RAWVIDEO && nicePixfmt(f.pixfmt))
                    return f.pixfmt;
            throw std::runtime_error(
                "encoder does not support any nice formats");
        }();
        return {props.formats.front(), fmt};
    }

    return {props.formats.front(),
            m_v4l2Encoders.front().formats.front().pixfmt};
}
#endif  // USE_V4L2
#ifdef USE_LIPSTICK_RECORDER
Caster::VideoPropsMap Caster::detectLipstickRecorderVideoSources() {
    LOGD("lipstick-recorder video source detecton started");
    VideoPropsMap map;

    if (LipstickRecorderSource::supported()) {
        auto lprops = LipstickRecorderSource::properties();

        {
            VideoSourceInternalProps props;
            props.type = VideoSourceType::LipstickCapture;
            props.orientation = VideoOrientation::Portrait;
            props.formats.push_back(
                {AV_CODEC_ID_RAWVIDEO,
                 lprops.pixfmt,
                 {{{lprops.width, lprops.height}, {lprops.framerate}}}});
            props.sensorDirection = SensorDirection::Front;
            props.name = "screen";
            props.friendlyName = "Screen capture";
            props.scale = VideoScale::Down50;

            LOGD("lipstick recorder source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }

        {
            VideoSourceInternalProps props;
            props.type = VideoSourceType::LipstickCapture;
            props.orientation = VideoOrientation::Landscape;
            props.formats.push_back(
                {AV_CODEC_ID_RAWVIDEO,
                 lprops.pixfmt,
                 {{{lprops.width, lprops.height}, {lprops.framerate}}}});
            props.sensorDirection = SensorDirection::Front;
            props.name = "screen-rotate";
            props.friendlyName = "Screen capture (auto rotate)";
            props.trans = VideoTrans::Frame169;
            props.scale = VideoScale::Down50;

            LOGD("lipstick recorder source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }
    };

    LOGD("lipstick-recorder video source detecton completed");

    return map;
}
#endif
