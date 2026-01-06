/* Copyright (C) 2022-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "caster.hpp"

#include <iterator>

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
#include <locale.h>
#include <pulse/error.h>
#include <pulse/xmalloc.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string_view>
#include <utility>

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

// static inline int64_t rescaleFromSec(int64_t time, AVRational destTimeBase) {
//     return av_rescale_q(time, AVRational{1, 1}, destTimeBase);
// }

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

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const AVFrame *pkt) {
    os << "pts=" << pkt->pts << ", duration=" << pkt->duration
       << ", tb=" << pkt->time_base << ", width=" << pkt->width
       << ", height=" << pkt->height
       << ", pixfmt=" << static_cast<AVPixelFormat>(pkt->format);
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

[[maybe_unused]] static void log_dict(const AVDictionary *dict) {
    if (!dict) return;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_iterate(dict, entry))) {
        LOGD("av dict: key=" << entry->key << ", value=" << entry->value);
    }
}

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
    if (flags & Caster::OptionsFlags::DontUsePipeWire)
        os << "dont-use-pipewire, ";

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::CapabilityFlags flags) {
    if (flags & Caster::CapabilityFlags::AdjustableAudioVolume)
        os << "adjustable-audio-volume, ";
    if (flags & Caster::CapabilityFlags::AdjustableVideoOrientation)
        os << "adjustable-video-orientation, ";
    if (flags & Caster::CapabilityFlags::AdjustableImageDuration)
        os << "adjustable-image-duration";
    if (flags & Caster::CapabilityFlags::AdjustableLoopFile)
        os << "adjustable-loop-file";

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
        case Caster::State::Stopping:
            os << "stopping";
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
        case Caster::StreamFormat::Avi:
            os << "avi";
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
        case Caster::VideoEncoder::Mjpeg:
            os << "mjpeg";
            break;
        default:
            os << "unknown";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoTrans trans) {
    [&] {
        switch (trans) {
            case Caster::VideoTrans::Off:
                os << "off";
                return;
            case Caster::VideoTrans::Scale:
                os << "scale";
                return;
            case Caster::VideoTrans::Vflip:
                os << "vflip";
                return;
            case Caster::VideoTrans::Hflip:
                os << "hflip";
                return;
            case Caster::VideoTrans::VflipHflip:
                os << "vflip-hflip";
                return;
            case Caster::VideoTrans::TransClock:
                os << "transpose-clock";
                return;
            case Caster::VideoTrans::TransClockFlip:
                os << "transpose-clock-flip";
                return;
            case Caster::VideoTrans::TransCclock:
                os << "transpose-cclock";
                return;
            case Caster::VideoTrans::TransCclockFlip:
                os << "transpose-cclock-flip";
                return;
            case Caster::VideoTrans::Frame169:
                os << "frame-169";
                return;
            case Caster::VideoTrans::Frame169Rot90:
                os << "frame-169-rot-90";
                return;
            case Caster::VideoTrans::Frame169Rot180:
                os << "frame-169-rot-180";
                return;
            case Caster::VideoTrans::Frame169Rot270:
                os << "frame-169-rot-270";
                return;
            case Caster::VideoTrans::Frame169Vflip:
                os << "frame-169-vflip";
                return;
            case Caster::VideoTrans::Frame169VflipRot90:
                os << "frame-169-vflip-rot-90";
                return;
            case Caster::VideoTrans::Frame169VflipRot180:
                os << "frame-169-vflip-rot-180";
                return;
            case Caster::VideoTrans::Frame169VflipRot270:
                os << "frame-169-vflip-rot-270";
                return;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::AudioTrans trans) {
    [&] {
        switch (trans) {
            case Caster::AudioTrans::Off:
                os << "off";
                return;
            case Caster::AudioTrans::Volume:
                os << "volume";
                return;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoScale scale) {
    [&] {
        switch (scale) {
            case Caster::VideoScale::Off:
                os << "off";
                return;
            case Caster::VideoScale::Down25:
                os << "down-25%";
                return;
            case Caster::VideoScale::Down50:
                os << "down-50%";
                return;
            case Caster::VideoScale::Down75:
                os << "down-75%";
                return;
            case Caster::VideoScale::DownAuto:
                os << "down-auto";
                return;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::UserRequestType type) {
    [&] {
        switch (type) {
            case Caster::UserRequestType::Nothing:
                os << "nothing";
                return;
            case Caster::UserRequestType::SwitchToNextFile:
                os << "switch-to-next-file";
                return;
            case Caster::UserRequestType::SwitchToPrevFile:
                os << "switch-to-prev-file";
                return;
            case Caster::UserRequestType::SwitchToFileIdx:
                os << "switch-to-file-idx";
                return;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::VideoSourceType type) {
    [&] {
        switch (type) {
            case Caster::VideoSourceType::DroidCam:
                os << "droidcam";
                return;
            case Caster::VideoSourceType::DroidCamRaw:
                os << "droidcam-raw";
                return;
            case Caster::VideoSourceType::V4l2:
                os << "v4l2";
                return;
            case Caster::VideoSourceType::X11Capture:
                os << "x11-capture";
                return;
            case Caster::VideoSourceType::LipstickCapture:
                os << "lipstick-capture";
                return;
            case Caster::VideoSourceType::File:
                os << "file";
                return;
            case Caster::VideoSourceType::Test:
                os << "test";
                return;
            case Caster::VideoSourceType::Unknown:
                break;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::AudioSourceType type) {
    [&] {
        switch (type) {
            case Caster::AudioSourceType::Mic:
                os << "mic";
                return;
            case Caster::AudioSourceType::Monitor:
                os << "monitor";
                return;
            case Caster::AudioSourceType::Playback:
                os << "playback";
                return;
            case Caster::AudioSourceType::File:
                os << "file";
                return;
            case Caster::AudioSourceType::Null:
                os << "null";
                return;
            case Caster::AudioSourceType::Unknown:
                break;
        }
        os << "unknown";
    }();

    return os;
}

std::ostream &operator<<(std::ostream &os, Caster::ImageAutoScale scale) {
    [&] {
        switch (scale) {
            case Caster::ImageAutoScale::ScaleNone:
                os << "none";
                return;
            case Caster::ImageAutoScale::Scale2160:
                os << "2160";
                return;
            case Caster::ImageAutoScale::Scale1080:
                os << "1080";
                return;
            case Caster::ImageAutoScale::Scale720:
                os << "720";
                return;
        }
        os << "unknown";
    }();

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
       << ", capabilities=[" << props.capaFlags << "]"
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
       << ", rate=" << props.rate << ", bps=" << props.bps << ", capabilities=["
       << props.capaFlags << "]";
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
    os << "files=" << config.files.size();
    //    if (!config.files.empty()) {
    //        os << "[";
    //        for (const auto &f : config.files) os << f << ",";
    //        os << "]";
    //    } else {
    //        os << "empty";
    //    }
    os << ", flags=[" << config.flags << "]";
    os << ", img-duration-sec=" << config.imgDurationSec;
    return os;
}

std::ostream &operator<<(std::ostream &os, const Caster::PerfConfig &config) {
    os << "pa-iter-cleep=" << config.paIterSleep
       << ", audio-mux-iter-sleep=" << config.audioMuxIterSleep
       << ", video-mjpeg-qmin=" << config.videoMjpegQmin
       << ", video-x264-crf=" << config.videoX264Crf
       << ", lipstick-recorder-fps=" << config.lipstickRecorderFps
       << ", img-fps=" << config.imgFps;
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
       << ", video-encoder=" << config.videoEncoder
       << ", img-auto-scale=" << config.imgAutoScale << ", perf-config=["
       << config.perfConfig << "]"
       << ", options=[" << config.options << "]";
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
        config.videoEncoder != VideoEncoder::X264 &&
        config.videoEncoder != VideoEncoder::Mjpeg) {
        LOGW("video-encoder is invalid");
        return false;
    }

    if (config.streamFormat != StreamFormat::Mp4 &&
        config.streamFormat != StreamFormat::MpegTs &&
        config.streamFormat != StreamFormat::Mp3 &&
        config.streamFormat != StreamFormat::Avi) {
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

Caster::Caster(Config config, StateChangedHandler stateChangedHandler,
               AudioSourceNameChangedHandler audioSourceNameChangedHandler)
    : m_config{std::move(config)},
      m_stateChangedHandler{std::move(stateChangedHandler)},
      m_audioSourceNameChangedHandler{
          std::move(audioSourceNameChangedHandler)} {
    LOGD("creating caster, config: " << m_config);

    try {
        detectSources(m_config.options);
#ifdef USE_V4L2
        detectV4l2Encoders();
#endif
        if (!configValid(m_config)) {
            throw std::runtime_error("invalid configuration");
        }

        m_optionsFlags = m_config.options;

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

    m_audioSourceType = props.type;

    switch (props.type) {
        case AudioSourceType::Mic:
        case AudioSourceType::Monitor:
        case AudioSourceType::Playback:
            initPa();
            break;
        case AudioSourceType::File:
            initFiles(0);
            break;
        case AudioSourceType::Null:
        case AudioSourceType::Unknown:
            break;
    }
}

void Caster::initVideoSource() {
    initVideoTrans();

    auto &props = videoProps();

    m_videoSourceType = props.type;

    switch (props.type) {
        case VideoSourceType::Test:
            m_imageProvider.emplace([this](const uint8_t *data, size_t size) {
                rawVideoDataReadyHandler(data, size);
            });
            return;
        case VideoSourceType::File:
            initFiles(0);
            return;
        case VideoSourceType::V4l2:
        case VideoSourceType::X11Capture:
            // nothing to init
            return;
        case VideoSourceType::LipstickCapture:
#ifdef USE_LIPSTICK_RECORDER
            // update framerate to requested value
            for (auto &f : props.formats) {
                for (auto &fs : f.frameSpecs) {
                    fs.framerates = {m_config.perfConfig.lipstickRecorderFps};
                }
            }
            m_lipstickRecorder.emplace(
                [this](const uint8_t *data, size_t size) {
                    rawVideoDataReadyHandler(data, size);
                },
                [this] {
                    LOGE("error in lipstick-recorder");
                    reportError();
                },
                m_config.perfConfig.lipstickRecorderFps);
            return;
#endif
        case VideoSourceType::DroidCamRaw:
#ifdef USE_DROIDCAM
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
            return;
#endif
        case VideoSourceType::DroidCam:
#ifdef USE_DROIDCAM
            m_droidCamSource.emplace(
                true, std::stoi(props.dev),
                [this](const uint8_t *data, size_t size) {
                    compressedVideoDataReadyHandler(data, size);
                },
                [this] {
                    LOGE("error in droidcam-source");
                    reportError();
                });
            return;
#endif
        case VideoSourceType::Unknown:
            break;
    }
    LOGF("invalid video source type");
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
        case VideoSourceType::File:
            break;
    }

    auto newTrans = orientationToTrans(m_config.videoOrientation, props);

    LOGD("initial video trans: config-orientation="
         << m_config.videoOrientation
         << ", props-orientation=" << props.orientation
         << ", old-trans=" << m_videoTrans << ", new-trans=" << newTrans);

    m_videoTrans = newTrans;
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

    std::transform(props.begin(), props.end(), std::back_inserter(sources),
                   [](auto &p) -> AudioSourceProps {
                       return {std::move(p.second.name),
                               std::move(p.second.friendlyName)};
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
        options & OptionsFlags::PaPlaybackAudioSources) {
        props.merge(detectPaSources(options));
    }

    if (options & OptionsFlags::FileAudioSources) {
        props.merge(detectAudioFileSources());
    }

    props.merge(detectAudioNullSources());

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
        /*type=*/mic ? AudioSourceType::Mic : AudioSourceType::Monitor,
        /*capaFlags*/ CapabilityFlags::AdjustableAudioVolume};

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

void Caster::paServerInfoCallback(pa_context *ctx, const pa_server_info *i,
                                  void *userdata) {
    if (!i) return;
    LOGD("pa server: string="
         << pa_context_get_server(ctx)
         << ", lib-proto-ver=" << pa_context_get_protocol_version(ctx)
         << ", server-proto-ver=" << pa_context_get_server_protocol_version(ctx)
         << ", local=" << pa_context_is_local(ctx)
         << ", name=" << i->server_name << ", server-ver=" << i->server_version
         << ", default-sink=" << i->default_sink_name
         << ", default-source=" << i->default_source_name);

    auto *result = static_cast<AudioSourceSearchResult *>(userdata);
    result->is_pipewire = std::string_view{i->server_name}.find("PipeWire") !=
                          std::string_view::npos;
}

bool Caster::hasPipeWire() {
    LOGD("pa server detection started");

    auto *loop = pa_mainloop_new();
    if (loop == nullptr) throw std::runtime_error("pa_mainloop_new error");

    auto *mla = pa_mainloop_get_api(loop);

    auto *ctx = pa_context_new(mla, "caster");
    if (ctx == nullptr) {
        pa_mainloop_free(loop);
        throw std::runtime_error("pa_context_new error");
    }

    AudioSourceSearchResult result;

    pa_context_set_state_callback(
        ctx,
        [](pa_context *ctx, void *userdata) {
            if (pa_context_get_state(ctx) == PA_CONTEXT_READY) {
                pa_operation_unref(pa_context_get_server_info(
                    ctx,
                    [](pa_context *ctx, const pa_server_info *i,
                       void *userdata) {
                        paServerInfoCallback(ctx, i, userdata);

                        auto *result =
                            static_cast<AudioSourceSearchResult *>(userdata);
                        result->done = true;
                    },
                    userdata));
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

    LOGD("pa server detection completed");

    return result.is_pipewire;
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
                pa_operation_unref(pa_context_get_server_info(
                    ctx, paServerInfoCallback, userdata));
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
            /*type=*/AudioSourceType::Playback,
            /*capaFlags*/ CapabilityFlags::AdjustableAudioVolume};
        LOGD("pa source found: " << props);
        result.propsMap.try_emplace(props.name, std::move(props));
    }

    if ((options & OptionsFlags::DontUsePipeWire) && result.is_pipewire) {
        LOGD(
            "pa pipewire server detected but it should not be used => removing "
            "pa sorces");
        result.propsMap.clear();
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

Caster::AudioPropsMap Caster::detectAudioNullSources() {
    LOGD("audio null sources detection started");

    AudioPropsMap propsMap;

    Caster::AudioSourceInternalProps props;
    props.name = "null";
    props.friendlyName = "Silence";
    props.type = AudioSourceType::Null;
    props.codec = AV_CODEC_ID_PCM_S16LE;
    props.channels = 2;
    props.rate = 16000;

    LOGD("audio null source found: " << props);

    propsMap.try_emplace(props.name, std::move(props));

    LOGD("audio null sources detection completed");

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

std::optional<size_t> Caster::start(DataReadyHandler dataReadyHandler) {
    LOGD("start caster");

    if (!dataReadyHandler) {
        LOGE("data ready handler not set");
        return std::nullopt;
    }

    auto outCtxIt =
        std::find_if(m_outCtxs.begin(), m_outCtxs.end(),
                     [](const auto &ctx) { return ctx.caster == nullptr; });
    if (outCtxIt == m_outCtxs.end()) {
        LOGE("too many outputs exist");
        return std::nullopt;
    }

    std::lock_guard lock{m_outCtxMtx};

    LOGD("starting output #"
         << 1 + std::count_if(
                    m_outCtxs.cbegin(), m_outCtxs.cend(),
                    [](const auto &ctx) { return ctx.caster != nullptr; }));

    switch (m_state) {
        case State::Initing:
        case State::Terminating:
        case State::Stopping:
            LOGE("start is only possible in inited or started state");
            return false;
        case State::Inited:
        case State::Started:
        case State::Starting:
            setState(State::Starting);
            outCtxIt->caster = this;
            outCtxIt->dataReadyHandler = std::move(dataReadyHandler);
            try {
                if (videoEnabled()) startVideoSource();
                startAv(&*outCtxIt);
                if (audioEnabled()) startAudioSource();
                setState(State::Started);
                startMuxing();
            } catch (const std::runtime_error &e) {
                LOGW("failed to start: " << e.what());
                *outCtxIt = {};
                reportError();
                return std::nullopt;
            }
            break;
    }

    LOGD("caster output started");

    if (m_config.fileSourceConfig &&
        m_config.fileSourceConfig->fileStreamingStartedHandler) {
        std::lock_guard lock{m_filesMtx};
        m_config.fileSourceConfig->fileStreamingStartedHandler(
            m_currentFileIdx, m_config.fileSourceConfig->files);
    }

    return reinterpret_cast<size_t>(outCtxIt->avFormatCtx);
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

void Caster::stop(size_t ctx) {
    if (m_state != State::Started) {
        LOGW("stop is only possible in started state");
        return;
    }

    std::lock_guard lock{m_outCtxMtx};

    auto outCtxIt = std::find_if(
        m_outCtxs.begin(), m_outCtxs.end(),
        [ctx = reinterpret_cast<decltype(OutCtx::avFormatCtx)>(ctx)](
            const auto &outCtx) { return outCtx.avFormatCtx == ctx; });
    if (outCtxIt == m_outCtxs.end()) {
        LOGW("stop is not possible as ctx is not found");
        return;
    }

    cleanAvOutputFormat(&(*outCtxIt));
    *outCtxIt = {};
}

void Caster::stopAll() {
    if (m_state != State::Started) {
        LOGW("stop is only possible in started state");
        return;
    }

    setState(State::Stopping);

    m_videoCv.notify_all();
    if (m_avMuxingThread.joinable()) m_avMuxingThread.join();
    if (m_audioPaThread.joinable()) m_audioPaThread.join();

    setState(State::Inited);
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

void Caster::cleanOutCtxs() {
    std::lock_guard lock{m_outCtxMtx};
    for (auto &outCtx : m_outCtxs) {
        cleanAvOutputFormat(&outCtx);
        outCtx = {};
    }
}

void Caster::cleanAvOutputFormat(OutCtx *outCtx) {
    if (outCtx->avFormatCtx != nullptr) {
        LOGD("clean out ctx started: ctx=" << outCtx->avFormatCtx);
        av_write_trailer(outCtx->avFormatCtx);
        if (outCtx->avFormatCtx->pb != nullptr) {
            if (outCtx->avFormatCtx->pb->buffer != nullptr &&
                outCtx->avFormatCtx->flags & AVFMT_FLAG_CUSTOM_IO) {
                av_freep(&outCtx->avFormatCtx->pb->buffer);
            }
            if (outCtx->avFormatCtx->pb != nullptr) {
                avio_context_free(&outCtx->avFormatCtx->pb);
            }
        }
        avformat_free_context(outCtx->avFormatCtx);
        outCtx->avFormatCtx = nullptr;
        LOGD("clean out ctx completed");
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
    m_videoFilterCtxMap.clear();
}

void Caster::cleanAvAudioFilters() {
    for (auto &p : m_audioFilterCtxMap) {
        if (p.second.in != nullptr) avfilter_inout_free(&p.second.in);
        if (p.second.out != nullptr) avfilter_inout_free(&p.second.out);
        if (p.second.graph != nullptr) avfilter_graph_free(&p.second.graph);
    }
    m_audioFilterCtxMap.clear();
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

    if (m_imgPkt != nullptr) {
        av_packet_free(&m_imgPkt);
    }

    cleanOutCtxs();

    cleanAvVideoInputFormat();
    cleanAvAudioInputFormat();
    cleanAvAudioEncoder();
    cleanAvAudioDecoder();
    cleanAvAudioFifo();

    if (m_outVideoCtx != nullptr) avcodec_free_context(&m_outVideoCtx);
    if (m_inVideoCtx != nullptr) avcodec_free_context(&m_inVideoCtx);
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

    while (true) {
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
    if (decoder == nullptr) {
        throw std::runtime_error("avcodec_find_decoder for audio error");
    }

    if (decoder->sample_fmts == nullptr ||
        decoder->sample_fmts[0] == AV_SAMPLE_FMT_NONE) {
        throw std::runtime_error(
            "audio decoder does not support any sample fmts");
    }

    LOGD("sample fmts supported by audio decoder:");
    for (int i = 0; decoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        LOGD("[" << i << "]: " << decoder->sample_fmts[i]);
    }

    m_inAudioCtx = avcodec_alloc_context3(decoder);
    if (m_inAudioCtx == nullptr) {
        throw std::runtime_error("avcodec_alloc_context3 for in audio error");
    }

    if (avcodec_parameters_to_context(m_inAudioCtx, stream->codecpar) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_to_context for audio error");
    }

    m_inAudioCtx->time_base = AVRational{1, m_inAudioCtx->sample_rate};

    if (avcodec_open2(m_inAudioCtx, nullptr, nullptr) != 0) {
        throw std::runtime_error("avcodec_open2 for in audio error");
    }

    LOGD("audio decoder: sample fmt="
         << m_inAudioCtx->sample_fmt
         << ", channel_layout=" << &m_inAudioCtx->ch_layout
         << ", sample_rate=" << m_inAudioCtx->sample_rate);

    m_audioFrameIn = av_frame_alloc();
    if (m_audioFrameIn == nullptr) {
        throw std::runtime_error("av_frame_alloc error");
    }
}

void Caster::initAvAudioRawDecoderFromProps() {
    LOGD("initing audio decoder");

    const auto &props = audioProps();

    const auto *decoder = avcodec_find_decoder(props.codec);
    if (decoder == nullptr) {
        throw std::runtime_error("avcodec_find_decoder for audio error");
    }

    if (decoder->sample_fmts == nullptr ||
        decoder->sample_fmts[0] == AV_SAMPLE_FMT_NONE) {
        throw std::runtime_error(
            "audio decoder does not support any sample fmts");
    }

    LOGD("sample fmts supported by audio decoder:");
    for (int i = 0; decoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        LOGD("[" << i << "]: " << decoder->sample_fmts[i]);
    }

    m_inAudioCtx = avcodec_alloc_context3(decoder);
    if (m_inAudioCtx == nullptr) {
        throw std::runtime_error("avcodec_alloc_context3 for in audio error");
    }

    av_channel_layout_default(&m_inAudioCtx->ch_layout, props.channels);
    m_inAudioCtx->sample_rate = static_cast<int>(props.rate);
    m_inAudioCtx->sample_fmt = decoder->sample_fmts[0];
    m_inAudioCtx->time_base = AVRational{1, m_inAudioCtx->sample_rate};

    LOGD("audio decoder: sample fmt="
         << m_inAudioCtx->sample_fmt
         << ", channel_layout=" << &m_inAudioCtx->ch_layout << ", sample_rate="
         << m_inAudioCtx->sample_rate << ", tb=" << m_inAudioCtx->time_base);

    if (avcodec_open2(m_inAudioCtx, nullptr, nullptr) != 0) {
        throw std::runtime_error("avcodec_open2 for in audio error");
    }

    LOGD("audio decoder: frame size=" << m_inAudioCtx->frame_size);

    m_audioFrameIn = av_frame_alloc();
    if (m_audioFrameIn == nullptr) {
        throw std::runtime_error("av_frame_alloc error");
    }
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
    if (!encoder) {
        throw std::runtime_error("no audio encoder: "s +
                                 audioEncoderAvName(type));
    }

    m_outAudioCtx = avcodec_alloc_context3(encoder);
    if (m_outAudioCtx == nullptr) {
        throw std::runtime_error("avcodec_alloc_context3 for out audio error");
    }

    // taking configuration from decoder
    if (m_inAudioCtx == nullptr) {
        LOGF("in audio ctx not is null");
    }
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

static std::pair<double, double> imageAutoScaleToNumber(
    Caster::ImageAutoScale scale) {
    switch (scale) {
        case Caster::ImageAutoScale::ScaleNone:
            break;
        case Caster::ImageAutoScale::Scale2160:
            return {3840.0, 2160.0};
        case Caster::ImageAutoScale::Scale1080:
            return {1920.0, 1080.0};
        case Caster::ImageAutoScale::Scale720:
            return {1280.0, 720.0};
    }
    return {std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max()};
}

Caster::Dim Caster::computeTransDim(Dim dim, VideoTrans trans,
                                    VideoScale scale) const {
    Dim outDim;

    auto factor = [&]() {
        switch (scale) {
            case VideoScale::Off:
                return 1.0;
            case VideoScale::Down25:
                return 0.75;
            case VideoScale::Down50:
                return 0.5;
            case VideoScale::Down75:
                return 0.25;
            case VideoScale::DownAuto: {
                auto as = imageAutoScaleToNumber(m_config.imgAutoScale);
                return std::min(
                    1.0, std::min(as.first / std::max(dim.width, dim.height),
                                  as.second / std::min(dim.width, dim.height)));
            }
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
        case VideoTrans::Frame169VflipRot270: {
            outDim.height = static_cast<uint32_t>(
                std::ceil(std::min<double>(dim.width, dim.height) * factor));
            outDim.width =
                static_cast<uint32_t>(std::ceil((16.0 / 9.0) * outDim.height));
            outDim.width += 2;
            break;
        }
    }

    outDim.height -= outDim.height % 4;
    outDim.width -= outDim.width % 4;

    LOGD("dim change: " << dim << " => " << outDim << " (factor=" << factor
                        << ", thin=" << dim.thin() << ")");

    return outDim;
}

void Caster::initAvVideoFiltersFrame169(SensorDirection direction) {
    std::string additionalFilters;
    m_videoShowProgressIndicator = false;
    m_videoShowTextIndicator = false;

    const bool showIndicators =
        m_outVideoCtx->width >= 100 && m_outVideoCtx->height >= 100;

    // progress indicator
    if ((m_optionsFlags & OptionsFlags::ShowVideoProgressIndicator) > 0 &&
        showIndicators) {
        int psize = m_outVideoCtx->height * 0.01;
        if (psize > 1) {
            // draw white progress line at the bottom of the frame
            additionalFilters += fmt::format(
                ",drawbox=x=0:y={0}:w={1}:h={2}:color=white@0.6:t=fill",
                m_outVideoCtx->height - psize, 0, psize);
            m_videoShowProgressIndicator = true;
        }
    }

    // text indicator
    if (((m_optionsFlags & OptionsFlags::ShowVideoCountIndicator) > 0 ||
         (m_optionsFlags & OptionsFlags::ShowVideoExifDateIndicator) > 0 ||
         (m_optionsFlags & OptionsFlags::ShowVideoExifModelIndicator) > 0) &&
        showIndicators) {
        int fsize = m_outVideoCtx->width * 0.02;
        int psize = m_outVideoCtx->height * 0.01;

        if (fsize > 5) {
            additionalFilters += fmt::format(
                ",drawtext=box=1:boxcolor=white@0.6:fontcolor=black@0.6:"
                "boxborderw={}:fontsize={}:"
                "text='':x={}:y=({}-text_h)",
                psize, fsize, psize, m_outVideoCtx->height - (2 * psize));
            m_videoShowTextIndicator = true;
        }
    }

    if (m_inDim.thin()) {
        initAvVideoFilter(direction, VideoTrans::Frame169,
                          "transpose=dir={2},scale=h=-1:w={0},crop=h=min(ih\\,{"
                          "1}),pad=width={0}:height="
                          "{1}"
                          ":x=-1:y=-1:color=black" +
                              additionalFilters);
        initAvVideoFilter(direction, VideoTrans::Frame169Rot90,
                          "scale=h={1}:w=-1,vflip,hflip,crop=w=min(iw\\,{0}),"
                          "pad=width={0}:height={1}:x=-1:y=-1:"
                          "color=black" +
                              additionalFilters);
        initAvVideoFilter(direction, VideoTrans::Frame169Rot180,
                          "transpose=dir={3},scale=h=-1:w={0},crop=h=min(ih\\,{"
                          "1}),pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black" +
                              additionalFilters);
        initAvVideoFilter(direction, VideoTrans::Frame169Rot270,
                          "scale=h={1}:w=-1,crop=w=min(iw\\,{0}),pad=width={0}:"
                          "height={1}:x=-1:y=-2:color="
                          "black" +
                              additionalFilters);
    } else {
        initAvVideoFilter(
            direction, VideoTrans::Frame169,
            "transpose=dir={2},scale=h={1}:w=-1,crop=w=min(iw\\,{0}),pad="
            "width={0}:height="
            "{1}"
            ":x=-1:y=-1:color=black" +
                additionalFilters);
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot90,
            "scale=h={1}:w=-1,vflip,hflip,crop=w=min(iw\\,{0}),pad=width={"
            "0}:height={1}:x=-1:y=-1:"
            "color=black" +
                additionalFilters);
        initAvVideoFilter(direction, VideoTrans::Frame169Rot180,
                          "transpose=dir={3},scale=h={1}:w=-1,crop=w=min(iw\\,{"
                          "0}),pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black" +
                              additionalFilters);
        initAvVideoFilter(
            direction, VideoTrans::Frame169Rot270,
            "scale=h={1}:w=-1,crop=w=min(iw\\,{0}),pad=width={0}:height={1}"
            ":x=-1:y=-2:color="
            "black" +
                additionalFilters);
    }
}

void Caster::initAvVideoFiltersFrame169Vflip(SensorDirection direction) {
    if (m_inDim.thin()) {
        initAvVideoFilter(direction, VideoTrans::Frame169Vflip,
                          "transpose=dir={2}_flip,scale=h=-1:w={0},crop=h=min("
                          "ih\\,{1}),pad=width={0}:height="
                          "{1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot90,
                          "scale=h={1}:w=-1,hflip,crop=w=min(iw\\,{0}),pad="
                          "width={0}:height={1}:x=-1:y=-1:"
                          "color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot180,
                          "transpose=dir={3}_flip,scale=h=-1:w={0},crop=h=min("
                          "ih\\,{1}),pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot270,
                          "scale=h={1}:w=-1,vflip,crop=w=min(iw\\,{0}),pad="
                          "width={0}:height={1}:x=-1:y=-2:color="
                          "black");
    } else {
        initAvVideoFilter(direction, VideoTrans::Frame169Vflip,
                          "transpose=dir={2}_flip,scale=h={1}:w=-1,crop=w=min("
                          "iw\\,{0}),pad=width={0}:height="
                          "{1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot90,
                          "scale=h={1}:w=-1,hflip,crop=w=min(iw\\,{0}),pad="
                          "width={0}:height={1}:x=-1:y=-1:"
                          "color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot180,
                          "transpose=dir={3}_flip,scale=h={1}:w=-1,crop=w=min("
                          "iw\\,{0}),pad=width={0}:"
                          "height={1}"
                          ":x=-1:y=-1:color=black");
        initAvVideoFilter(direction, VideoTrans::Frame169VflipRot270,
                          "scale=h={1}:w=-1,vflip,crop=w=min(iw\\,{0}),pad="
                          "width={0}:height={1}:x=-1:y=-2:color="
                          "black");
    }
}

Caster::VideoTrans Caster::orientationToTrans(
    VideoOrientation orientation, const VideoSourceInternalProps &props) {
    auto trans = [&] {
        switch (orientation) {
            case VideoOrientation::Auto:
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
        return VideoTrans::Off;
    }();

    if (props.trans == VideoTrans::Frame169 ||
        props.trans == VideoTrans::Frame169Rot90 ||
        props.trans == VideoTrans::Frame169Rot180 ||
        props.trans == VideoTrans::Frame169Rot270 ||
        props.trans == VideoTrans::Frame169Vflip ||
        props.trans == VideoTrans::Frame169VflipRot90 ||
        props.trans == VideoTrans::Frame169VflipRot180 ||
        props.trans == VideoTrans::Frame169VflipRot270) {
        switch (trans) {
            case VideoTrans::Off:
                return VideoTrans::Frame169Rot270;
            case VideoTrans::VflipHflip:
                return VideoTrans::Frame169Rot90;
            case VideoTrans::TransClock:
                return VideoTrans::Frame169;
            case VideoTrans::TransClockFlip:
                return VideoTrans::Frame169Vflip;
            case VideoTrans::TransCclock:
                return VideoTrans::Frame169Rot180;
            case VideoTrans::TransCclockFlip:
                return VideoTrans::Frame169VflipRot180;
            default:
                break;
        }
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
                              "scale=h={1}:w={0}:interl=-1");
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
        case VideoTrans::Frame169VflipRot270: {
            const char *old_locale = setlocale(LC_NUMERIC, "C");
            switch (props.type) {
                case VideoSourceType::LipstickCapture:
                    initAvVideoFiltersFrame169Vflip(props.sensorDirection);
                    [[fallthrough]];
                default:
                    initAvVideoFiltersFrame169(props.sensorDirection);
            }
            setlocale(LC_NUMERIC, old_locale);
            break;
        }
        default:
            throw std::runtime_error("unsuported video trans");
    }
}

void Caster::initAvVideoFilter(SensorDirection direction, VideoTrans trans,
                               const std::string &fmt) {
    if (m_videoFilterCtxMap.count(trans) > 0) {
        // already inited
        return;
    }

    LOGD("initing video av filter: sensor-direction="
         << direction << ", trans=" << trans << ", fmt='" << fmt << "'");
    initAvVideoFilter(
        m_videoFilterCtxMap[trans],
        fmt::format(fmt, m_outVideoCtx->width, m_outVideoCtx->height,
                    direction == SensorDirection::Front ? "cclock" : "clock",
                    direction == SensorDirection::Front ? "clock" : "cclock")
            .c_str());
}

void Caster::initAvAudioFilter(FilterCtx &ctx, const char *arg) {
    LOGD("initing audio av filter str: " << arg);

    ctx.in = avfilter_inout_alloc();
    ctx.out = avfilter_inout_alloc();
    ctx.graph = avfilter_graph_alloc();
    if (ctx.in == nullptr || ctx.out == nullptr || ctx.graph == nullptr) {
        throw std::runtime_error("failed to allocate av filter");
    }

    const auto *buffersrc = avfilter_get_by_name("abuffer");
    if (buffersrc == nullptr) throw std::runtime_error("no abuffer filter");

    if (m_inAudioCtx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_default(&m_inAudioCtx->ch_layout,
                                  m_inAudioCtx->ch_layout.nb_channels);
    }

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

AVSampleFormat Caster::bestAudioSampleFormat(
    const AVCodec *encoder, AVSampleFormat preferredSampleFmt) {
    if (encoder->sample_fmts == nullptr) {
        throw std::runtime_error(
            "audio encoder does not support any sample fmts");
    }

    AVSampleFormat bestFmt = AV_SAMPLE_FMT_NONE;

    for (int i = 0; encoder->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
        bestFmt = encoder->sample_fmts[i];
        if (bestFmt == preferredSampleFmt) {
            LOGD("sample fmt exact match");
            break;
        }
    }

    return bestFmt;
}

bool Caster::nicePixfmt(AVPixelFormat fmt, VideoEncoder type) {
    switch (type) {
        case VideoEncoder::Auto:
        case VideoEncoder::X264:
        case VideoEncoder::Nvenc:
        case VideoEncoder::V4l2:
            return std::find(nicePixfmtsMpeg.cbegin(), nicePixfmtsMpeg.cend(),
                             fmt) != nicePixfmtsMpeg.cend();
        case VideoEncoder::Mjpeg:
            return std::find(nicePixfmtsMjpeg.cbegin(), nicePixfmtsMjpeg.cend(),
                             fmt) != nicePixfmtsMjpeg.cend();
    }
    LOGF("invalid encoder type");
}

AVPixelFormat Caster::toNicePixfmt(AVPixelFormat fmt, VideoEncoder type,
                                   const AVPixelFormat *supportedFmts) {
    if (nicePixfmt(fmt, type)) return fmt;

    auto newFmt = fmt;

    for (int i = 0;; ++i) {
        if (nicePixfmt(supportedFmts[i], type)) {
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
                        VideoEncoder type, bool useNiceFormats) {
    if (encoder->pix_fmts == nullptr)
        throw std::runtime_error("encoder does not support any pixfmts");

    LOGD("pixfmts supported by encoder: " << encoder->pix_fmts);

    if (auto it =
            std::find_if(props.formats.cbegin(), props.formats.cend(),
                         [encoder, useNiceFormats, type](const auto &sf) {
                             for (int i = 0;; ++i) {
                                 if (encoder->pix_fmts[i] == AV_PIX_FMT_NONE)
                                     return false;
                                 if (useNiceFormats &&
                                     !nicePixfmt(encoder->pix_fmts[i], type))
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

    if (useNiceFormats) {
        return {props.formats.front(),
                toNicePixfmt(fmt, type, encoder->pix_fmts)};
    }
    return {props.formats.front(), fmt};
}

static std::string tempPathForX264() {
    char path[] = "/tmp/libx264-XXXXXX";
    auto fd = mkstemp(path);
    if (fd == -1) {
        LOGF("mkstemp error");
    }
    close(fd);
    return path;
}

void Caster::setVideoEncoderOpts(VideoEncoder encoder, AVCodecContext *ctx,
                                 AVDictionary **opts) const {
    switch (encoder) {
        case VideoEncoder::X264:
            av_dict_set(opts, "preset", "ultrafast", 0);
            av_dict_set(opts, "tune", "zerolatency", 0);
            av_dict_set_int(
                opts, "crf",
                std::clamp(m_config.perfConfig.videoX264Crf, 1U, 31U), 0);
            av_dict_set(opts, "passlogfile", tempPathForX264().c_str(), 0);
            return;
        case VideoEncoder::Nvenc:
            av_dict_set(opts, "preset", "p1", 0);
            av_dict_set(opts, "tune", "ull", 0);
            av_dict_set(opts, "zerolatency", "1", 0);
            av_dict_set(opts, "rc", "constqp", 0);
            return;
        case VideoEncoder::V4l2:
            // no options
            return;
        case VideoEncoder::Mjpeg:
            av_dict_set(opts, "huffman", "optimal", 0);
            LOGT("encoder initial quality settings: qmin="
                 << ctx->qmin << ", qmax=" << ctx->qmax
                 << ", global-quality=" << ctx->global_quality);
            ctx->flags |= AV_CODEC_FLAG_QSCALE;
            ctx->qmin = std::clamp(m_config.perfConfig.videoMjpegQmin, 1U, 31U);
            ctx->qmax = 31;
            return;
        case VideoEncoder::Auto:
            // invalid
            break;
    }

    LOGF("failed to set video encoder options");
}

std::string Caster::videoEncoderAvName(VideoEncoder encoder) {
    switch (encoder) {
        case VideoEncoder::X264:
            return "libx264";
        case VideoEncoder::Nvenc:
            return "h264_nvenc";
        case VideoEncoder::V4l2:
            return "h264_v4l2m2m";
        case VideoEncoder::Mjpeg:
            return "mjpeg";
        case VideoEncoder::Auto:
            break;
    }

    LOGF("invalid video encoder");
}

std::string Caster::audioEncoderAvName(AudioEncoder encoder) {
    switch (encoder) {
        case AudioEncoder::Aac:
            return "aac";
        case AudioEncoder::Mp3Lame:
            return "libmp3lame";
    }

    LOGF("invalid audio encoder");
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
        case VideoSourceType::File:
        case VideoSourceType::Unknown:
            break;
    }

    LOGF("invalid video source");
}

std::string Caster::streamFormatAvName(StreamFormat format) {
    switch (format) {
        case StreamFormat::Mp4:
            return "mp4";
        case StreamFormat::MpegTs:
            return "mpegts";
        case StreamFormat::Mp3:
            return "mp3";
        case StreamFormat::Avi:
            return "avi";
    }

    LOGF("invalid stream format");
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
    if (m_outVideoCtx == nullptr) {
        LOGF("avcodec_alloc_context3 for video error");
    }

    const auto &props = videoProps();

    const auto &bestFormat = [&]() {
#ifdef USE_V4L2
        if (type == VideoEncoder::V4l2)
            return bestVideoFormatForV4l2Encoder(props, type);
#endif
        return bestVideoFormat(
            encoder, props, type,
            m_config.options & OptionsFlags::OnlyNiceVideoFormats);
    }();

    m_outVideoCtx->pix_fmt = bestFormat.second;
    if (m_outVideoCtx->pix_fmt == AV_PIX_FMT_NONE) {
        LOGF("failed to find pixfmt for video encoder");
    }

    const auto &fs = bestFormat.first.get().frameSpecs.front();

    m_videoFramerate = static_cast<int>(*fs.framerates.begin());

    m_outVideoCtx->time_base = AVRational{1, m_videoFramerate};
    m_outVideoCtx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    m_outVideoCtx->flags &= ~AV_CODEC_FLAG_RECON_FRAME;

    m_inDim = fs.dim;
    if (!m_inDim.valid()) {
        LOGF("invalid dim");
    }

    m_inPixfmt = bestFormat.first.get().pixfmt;

    auto outDim = computeTransDim(m_inDim, m_videoTrans, props.scale);
    m_outVideoCtx->width = static_cast<int>(outDim.width);
    m_outVideoCtx->height = static_cast<int>(outDim.height);
    m_outVideoCtx->color_range =
        type == VideoEncoder::Mjpeg ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

    AVDictionary *opts = nullptr;

    setVideoEncoderOpts(type, m_outVideoCtx, &opts);

    if (avcodec_open2(m_outVideoCtx, nullptr, &opts) < 0) {
        av_dict_free(&opts);
        LOGF("avcodec_open2 for out video error");
    }

    cleanAvOpts(&opts);

    m_nextVideoInFramePts = 0;

    LOGD("video encoder: tb=" << m_outVideoCtx->time_base
                              << ", pixfmt=" << m_outVideoCtx->pix_fmt
                              << ", width=" << m_outVideoCtx->width
                              << ", height=" << m_outVideoCtx->height
                              << ", framerate=" << m_videoFramerate);

    LOGD("encoder successfuly inited");
}

void Caster::initAvVideoEncoder() {
    if (m_config.videoEncoder == VideoEncoder::Auto) {
        if (m_config.streamFormat == StreamFormat::Avi) {
            initAvVideoEncoder(VideoEncoder::Mjpeg);
            return;
        }

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

void Caster::initFiles(size_t startIdx) {
    const auto &cfiles = m_config.fileSourceConfig->files;

    if (startIdx >= cfiles.size()) {
        LOGF("start idx out of range");
    }

    LOGD("init files: startIdx=" << startIdx);

    m_files.clear();

    for (auto it = std::next(cfiles.cbegin(), startIdx); it != cfiles.cend();
         std::advance(it, 1)) {
        m_files.push_back(*it);
    }
}

void Caster::addFile(std::string file) {
    if (terminating()) return;

    std::lock_guard lock{m_filesMtx};

    LOGD("adding file: path=" << file);

    m_files.push_back(std::move(file));
}

void Caster::setFiles(std::vector<std::string> files) {
    if (terminating()) return;

    std::lock_guard lock{m_filesMtx};

    m_files.clear();
    for (auto &&file : files) {
        LOGD("adding file: " << file);
        m_files.push_back(std::move(file));
    }
}

void Caster::switchFile(bool backward) {
    if (terminating()) return;

    m_userRequestType = backward ? UserRequestType::SwitchToPrevFile
                                 : UserRequestType::SwitchToNextFile;
}

void Caster::switchFileIndex(size_t idx) {
    if (terminating()) {
        return;
    }

    if (!m_config.fileSourceConfig ||
        idx >= m_config.fileSourceConfig->files.size()) {
        LOGW("can't switch file idx");
        return;
    }

    m_userRequestType = UserRequestType::SwitchToFileIdx;
    m_userRequestedFileIdx = idx;
}

bool Caster::initAvInputFormatFromFile(bool forVideo) {
    std::lock_guard lock{m_filesMtx};

    LOGD("current file: " << m_currentFile);

    if (m_config.fileSourceConfig) {
        if (m_config.fileSourceConfig->fileStreamingDoneHandler) {
            m_config.fileSourceConfig->fileStreamingDoneHandler(m_currentFile,
                                                                m_files.size());
        }

        auto loop =
            (m_config.fileSourceConfig->flags & FileSourceFlags::Loop) > 0;

        auto requestType = m_userRequestType.exchange(UserRequestType::Nothing);
        LOGD("switch file request: type=" << requestType);
        switch (requestType) {
            case UserRequestType::Nothing:
            case UserRequestType::SwitchToNextFile:
                if (m_files.empty() && loop) {
                    initFiles(0);
                }
                break;
            case UserRequestType::SwitchToPrevFile: {
                const auto &cfiles = m_config.fileSourceConfig->files;
                auto it =
                    std::find(cfiles.cbegin(), cfiles.cend(), m_currentFile);
                if (it == cfiles.cbegin()) {
                    if (loop) {
                        it = std::prev(cfiles.cend());
                    }
                } else {
                    std::advance(it, -1);
                }
                if (it != cfiles.cend()) {
                    initFiles(std::distance(cfiles.cbegin(), it));
                }
                break;
            }
            case UserRequestType::SwitchToFileIdx: {
                size_t idx = m_userRequestedFileIdx;
                LOGD("requested idx: " << idx);
                if (idx >= m_config.fileSourceConfig->files.size()) {
                    LOGW("invalid file idx:" << m_userRequestedFileIdx);
                    if (m_files.empty() && loop) {
                        initFiles(0);
                    }
                    break;
                }
                initFiles(idx);
                break;
            }
        }
    }

    if (m_files.empty()) {
        LOGD("no more files to cast");
        return false;
    }

    LOGD("opening file: path=" << m_files.front());

    AVFormatContext *in_ctx = nullptr;

    if (avformat_open_input(&in_ctx, m_files.front().c_str(), nullptr,
                            nullptr) < 0) {
        throw std::runtime_error("avformat_open_input error");
    }

    m_currentFile = m_files.front();
    m_files.pop_front();

    if (m_config.fileSourceConfig) {
        m_currentFileIdx = std::clamp<size_t>(
            m_config.fileSourceConfig->files.size() - m_files.size() - 1, 0,
            m_config.fileSourceConfig->files.size() - 1);

        LOGD("current idx: " << m_currentFileIdx);

        if (m_config.fileSourceConfig->fileStreamingStartedHandler) {
            m_config.fileSourceConfig->fileStreamingStartedHandler(
                m_currentFileIdx, m_config.fileSourceConfig->files);
        }
    }

    if (forVideo) {
        m_inVideoFormatCtx = in_ctx;
    } else {
        m_inAudioFormatCtx = in_ctx;
    }

    if (isInputImage()) {
        LOGD("input stream is an image");
    }

    return true;
}

bool Caster::initAvVideoInputFormatFromFile() {
    return initAvInputFormatFromFile(/*forVideo=*/true);
}

bool Caster::initAvAudioInputFormatFromFile() {
    return initAvInputFormatFromFile(/*forVideo=*/false);
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

void Caster::allocAvOutputFormat(OutCtx *outCtx) {
    if (avformat_alloc_output_context2(
            &outCtx->avFormatCtx, nullptr,
            streamFormatAvName(m_config.streamFormat).c_str(), nullptr) < 0) {
        throw std::runtime_error("avformat_alloc_output_context2 error");
    }

    LOGD("allocate out ctx: ctx=" << outCtx->avFormatCtx);
}

void Caster::fixPropsForFileVideoSource(VideoSourceInternalProps &props) const {
    if (!props.formats.empty() &&
        (props.formats.front().codecId != AV_CODEC_ID_NONE ||
         props.formats.front().pixfmt != AV_PIX_FMT_NONE)) {
        // nothing to do
        return;
    }

    if (!m_inVideoFormatCtx) {
        LOGF("invalid in video format ctx");
    }

    const auto *stream = m_inVideoFormatCtx->streams[m_inVideoStreamIdx];

    if (props.formats.empty()) {
        FrameSpec fs;
        fs.framerates.insert(std::clamp(m_config.perfConfig.imgFps, 1U, 60U));
        fs.dim.width = stream->codecpar->width;
        fs.dim.height = stream->codecpar->height;
        props.formats.push_back(
            VideoFormatExt{AV_CODEC_ID_NONE,
                           static_cast<AVPixelFormat>(stream->codecpar->format),
                           {fs}});
    } else {
        auto &formatExt = props.formats.front();
        if (!formatExt.frameSpecs.empty()) {
            formatExt.frameSpecs.front().framerates.insert(
                std::clamp(m_config.perfConfig.imgFps, 1U, 60U));
        }
        formatExt.pixfmt = static_cast<AVPixelFormat>(stream->codecpar->format);
    }
}

void Caster::initAv() {
    LOGD("av init started");

    if (audioEnabled()) {
        [&] {
            switch (audioProps().type) {
                case AudioSourceType::Mic:
                case AudioSourceType::Monitor:
                case AudioSourceType::Playback:
                case AudioSourceType::Null:
                    initAvAudioRawDecoderFromProps();
                    return;
                case AudioSourceType::File:
                    if (!initAvAudioInputFormatFromFile()) {
                        throw std::runtime_error("no file to cast");
                    }
                    findAvAudioInputStreamIdx();
                    initAvAudioRawDecoderFromInputStream();
                    return;
                case AudioSourceType::Unknown:
                    break;
            }
            throw std::runtime_error("unknown audio source type");
        }();

        initAvAudioEncoder();
        initAvAudioFilters();
        initAvAudioFifo();

        m_audioInFrameSize = av_samples_get_buffer_size(
            nullptr, m_inAudioCtx->ch_layout.nb_channels,
            m_outAudioCtx->frame_size, m_inAudioCtx->sample_fmt, 0);

        m_audioOutFrameSize = av_samples_get_buffer_size(
            nullptr, m_outAudioCtx->ch_layout.nb_channels,
            m_outAudioCtx->frame_size, m_outAudioCtx->sample_fmt, 0);
    }

    if (videoEnabled()) {
        auto &props = m_videoProps.at(m_config.videoSource);
        m_videoOrigOrientation = props.orientation;
        [&] {
            switch (videoProps().type) {
                case VideoSourceType::DroidCam:
                    initAvVideoForGst();
                    return;
                case VideoSourceType::V4l2:
                case VideoSourceType::X11Capture:
                    initAvVideoEncoder();
                    initAvVideoInputRawFormat();
                    findAvVideoInputStreamIdx();
                    initAvVideoRawDecoderFromInputStream();
                    initAvVideoFilters();
                    return;
                case VideoSourceType::LipstickCapture:
                case VideoSourceType::Test:
                case VideoSourceType::DroidCamRaw:
                    initAvVideoEncoder();
                    initAvVideoRawDecoder();
                    initAvVideoFilters();
                    return;
                case VideoSourceType::File: {
                    if (!initAvVideoInputFormatFromFile()) {
                        throw std::runtime_error("no file to cast");
                    }
                    findAvVideoInputStreamIdx();
                    initAvVideoRawDecoderFromInputStream();
                    m_currentExif = extractExifFromDecoder();
                    if (props.orientation == VideoOrientation::Auto) {
                        props.orientation = m_currentExif.orientation;
                        initVideoTrans();
                    }
                    fixPropsForFileVideoSource(props);
                    initAvVideoEncoder();
                    initAvVideoFilters();
                    return;
                }
                case VideoSourceType::Unknown:
                    break;
            }
            throw std::runtime_error("unknown video source type");
        }();

        m_videoRealFrameDuration =
            rescaleToUsec(1, AVRational{1, m_videoFramerate});
        m_videoFrameDuration = m_videoRealFrameDuration;
    }

    LOGD("using muxer: " << m_config.streamFormat);

    setState(State::Inited);

    LOGD("av init completed");
}

void Caster::initAvVideoInputCompressedFormat() {
    if (m_inVideoFormatCtx) {
        // already inited
        return;
    }

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
    if (m_audioFifo == nullptr) {
        throw std::runtime_error("av_audio_fifo_alloc error");
    }
}

void Caster::initAvVideoBsf(OutCtx *outCtx) {
    if (m_videoBsfExtractExtraCtx) {
        // already inited
        return;
    }

    if (outCtx->videoStream->codecpar->codec_id !=
        AVCodecID::AV_CODEC_ID_H264) {
        // nothing to do
        return;
    }

    // extract_extradata
    const auto *extractBsf = av_bsf_get_by_name("extract_extradata");
    if (extractBsf == nullptr)
        throw std::runtime_error("no extract_extradata bsf found");

    if (av_bsf_alloc(extractBsf, &m_videoBsfExtractExtraCtx) != 0)
        throw std::runtime_error("extract_extradata av_bsf_alloc error");

    if (avcodec_parameters_copy(m_videoBsfExtractExtraCtx->par_in,
                                outCtx->videoStream->codecpar) < 0)
        throw std::runtime_error("bsf avcodec_parameters_copy error");

    m_videoBsfExtractExtraCtx->time_base_in = outCtx->videoStream->time_base;

    if (av_bsf_init(m_videoBsfExtractExtraCtx) != 0)
        throw std::runtime_error("extract_extradata av_bsf_init error");

    // dump_extra

    const auto *dumpBsf = av_bsf_get_by_name("dump_extra");
    if (dumpBsf == nullptr) throw std::runtime_error("no dump_extra bsf found");

    if (av_bsf_alloc(dumpBsf, &m_videoBsfDumpExtraCtx) != 0)
        throw std::runtime_error("dump_extra av_bsf_alloc error");

    if (avcodec_parameters_copy(m_videoBsfDumpExtraCtx->par_in,
                                outCtx->videoStream->codecpar) < 0)
        throw std::runtime_error("bsf avcodec_parameters_copy error");

    av_opt_set(m_videoBsfDumpExtraCtx, "freq", "all", 0);

    m_videoBsfDumpExtraCtx->time_base_in = outCtx->videoStream->time_base;

    if (av_bsf_init(m_videoBsfDumpExtraCtx) != 0)
        throw std::runtime_error("dump_extra av_bsf_init error");
}

void Caster::initAvVideoOutStreamFromInputFormat(OutCtx *outCtx) {
    auto idx = av_find_best_stream(m_inVideoFormatCtx, AVMEDIA_TYPE_VIDEO, -1,
                                   -1, nullptr, 0);
    if (idx < 0) throw std::runtime_error("no video stream found in input");

    auto *stream = m_inVideoFormatCtx->streams[idx];

    av_dump_format(m_inVideoFormatCtx, idx, "", 0);

    outCtx->videoStream = avformat_new_stream(outCtx->avFormatCtx, nullptr);
    if (!outCtx->videoStream)
        throw std::runtime_error("avformat_new_stream for video error");

    outCtx->videoStream->id = 0;

    if (avcodec_parameters_copy(outCtx->videoStream->codecpar,
                                stream->codecpar) < 0) {
        throw std::runtime_error("avcodec_parameters_copy for video error");
    }

    outCtx->videoStream->time_base = AVRational{1, m_videoFramerate};
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

    // if (m_inVideoCtx->width != static_cast<int>(m_inDim.width) ||
    //     m_inVideoCtx->height != static_cast<int>(m_inDim.height) ||
    //     m_inVideoCtx->pix_fmt != m_inPixfmt) {
    //     LOGE("input stream has invalid params, expected: pixfmt="
    //          << m_inPixfmt << ", width=" << m_inDim.width
    //          << ", height=" << m_inDim.height);
    //     throw std::runtime_error("decoder params are invalid");
    // }

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

void Caster::initAvVideoOutStreamFromEncoder(OutCtx *outCtx) {
    outCtx->videoStream = avformat_new_stream(outCtx->avFormatCtx, nullptr);
    if (!outCtx->videoStream)
        throw std::runtime_error("avformat_new_stream for video error");

    outCtx->videoStream->id = 0;
    outCtx->videoStream->r_frame_rate = AVRational{m_videoFramerate, 1};

    if (avcodec_parameters_from_context(outCtx->videoStream->codecpar,
                                        m_outVideoCtx) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_from_context for video error");
    }

    outCtx->videoStream->time_base = AVRational{1, m_videoFramerate};
}

void Caster::initAvAudioDurations(OutCtx *outCtx) {
    m_audioFrameDuration =
        rescaleToUsec(m_outAudioCtx->frame_size, m_inAudioCtx->time_base);
    m_audioPktDuration =
        rescaleFromUsec(m_audioFrameDuration, outCtx->audioStream->time_base);

    LOGD("audio out stream: tb=" << outCtx->audioStream->time_base
                                 << ", frame dur=" << m_audioFrameDuration
                                 << ", pkt dur=" << m_audioPktDuration);
}

void Caster::initAvAudioOutStreamFromEncoder(OutCtx *outCtx) {
    outCtx->audioStream = avformat_new_stream(outCtx->avFormatCtx, nullptr);
    if (!outCtx->audioStream) {
        throw std::runtime_error("avformat_new_stream for audio error");
    }

    outCtx->audioStream->id = 1;

    if (avcodec_parameters_from_context(outCtx->audioStream->codecpar,
                                        m_outAudioCtx) < 0) {
        throw std::runtime_error(
            "avcodec_parameters_from_context for audio error");
    }

    outCtx->audioStream->time_base = m_outAudioCtx->time_base;

    av_dump_format(outCtx->avFormatCtx, outCtx->audioStream->id, "", 1);
}

void Caster::reInitAvOutputFormat(OutCtx *outCtx) {
    cleanAvOutputFormat(outCtx);
    allocAvOutputFormat(outCtx);

    if (videoEnabled()) {
        const auto &props = videoProps();

        switch (props.type) {
            case VideoSourceType::DroidCam:
                initAvVideoOutStreamFromInputFormat(outCtx);
                break;
            case VideoSourceType::V4l2:
            case VideoSourceType::X11Capture:
            case VideoSourceType::LipstickCapture:
            case VideoSourceType::Test:
            case VideoSourceType::DroidCamRaw:
            case VideoSourceType::File:
                initAvVideoOutStreamFromEncoder(outCtx);
                break;
            default:
                throw std::runtime_error("unknown video source type");
        }
    }

    if (audioEnabled()) initAvAudioOutStreamFromEncoder(outCtx);

    initAvOutputFormat(outCtx);
}

void Caster::initAvOutputFormat(OutCtx *outCtx) {
    auto *outBuf = static_cast<uint8_t *>(av_malloc(m_videoBufSize));
    if (outBuf == nullptr) {
        av_freep(&outBuf);
        throw std::runtime_error("unable to allocate out av buf");
    }

    auto *outFormatCtx = outCtx->avFormatCtx;

    outFormatCtx->pb =
        avio_alloc_context(outBuf, m_videoBufSize, 1, outCtx, nullptr,
                           avWritePacketCallbackStatic, nullptr);
    if (outFormatCtx->pb == nullptr) {
        throw std::runtime_error("avio_alloc_context error");
    }

    AVDictionary *opts = nullptr;

    switch (m_config.streamFormat) {
        case StreamFormat::MpegTs:
            av_dict_set(&opts, "mpegts_m2ts_mode", "-1", 0);
            av_dict_set(&outFormatCtx->metadata, "service_provider",
                        m_config.streamAuthor.c_str(), 0);
            av_dict_set(&outFormatCtx->metadata, "service_name",
                        m_config.streamTitle.c_str(), 0);
            break;
        case StreamFormat::Mp4:
            av_dict_set(&opts, "movflags", "frag_custom+empty_moov+delay_moov",
                        0);
            av_dict_set(&outFormatCtx->metadata, "author",
                        m_config.streamAuthor.c_str(), 0);
            av_dict_set(&outFormatCtx->metadata, "title",
                        m_config.streamTitle.c_str(), 0);

            if (videoEnabled())
                setVideoStreamRotation(outCtx, m_config.videoOrientation);
            break;
        case StreamFormat::Mp3:
            av_dict_set(&outCtx->audioStream->metadata, "artist",
                        m_config.streamAuthor.c_str(), 0);
            av_dict_set(&outCtx->audioStream->metadata, "title",
                        m_config.streamTitle.c_str(), 0);
            break;
        case StreamFormat::Avi:
            av_dict_set(&outFormatCtx->metadata, "author",
                        m_config.streamAuthor.c_str(), 0);
            av_dict_set(&outFormatCtx->metadata, "title",
                        m_config.streamTitle.c_str(), 0);
            break;
    }

    outFormatCtx->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS |
                           AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_AUTO_BSF;

    LOGD("writting format header");
    auto ret = avformat_write_header(outFormatCtx, &opts);
    if (ret != AVSTREAM_INIT_IN_WRITE_HEADER &&
        ret != AVSTREAM_INIT_IN_INIT_OUTPUT) {
        av_dict_free(&opts);
        throw std::runtime_error("avformat_write_header error");
    }
    LOGD("format header written, ret="
         << (ret == AVSTREAM_INIT_IN_WRITE_HEADER  ? "write-header"
             : ret == AVSTREAM_INIT_IN_INIT_OUTPUT ? "init-output"
                                                   : "unknown"));

    if (audioEnabled()) initAvAudioDurations(outCtx);

    cleanAvOpts(&opts);

    if (outCtx->videoStream) {
        LOGD("video out stream: tb=" << outCtx->videoStream->time_base);
    }
    if (outCtx->audioStream) {
        LOGD("audio out stream: tb=" << outCtx->audioStream->time_base);
    }
}

void Caster::startAv(OutCtx *outCtx) {
    LOGD("starting av");

    allocAvOutputFormat(outCtx);

    if (videoEnabled()) {
        [&] {
            switch (videoProps().type) {
                case VideoSourceType::DroidCam:
                    initAvVideoInputCompressedFormat();
                    initAvVideoOutStreamFromInputFormat(outCtx);
                    initAvVideoBsf(outCtx);
                    extractVideoExtradataFromCompressedDemuxer();
                    return;
                case VideoSourceType::V4l2:
                case VideoSourceType::X11Capture:
                    initAvVideoOutStreamFromEncoder(outCtx);
                    // initAvVideoFilters();
                    initAvVideoBsf(outCtx);
                    extractVideoExtradataFromRawDemuxer();
                    return;
                case VideoSourceType::File:
                    initAvVideoOutStreamFromEncoder(outCtx);
                    // initAvVideoFilters();
                    return;
                case VideoSourceType::LipstickCapture:
                case VideoSourceType::Test:
                case VideoSourceType::DroidCamRaw:
                    initAvVideoOutStreamFromEncoder(outCtx);
                    initAvVideoBsf(outCtx);
                    extractVideoExtradataFromRawBuf();
                    return;
                case VideoSourceType::Unknown:
                    break;
            }
            throw std::runtime_error("unknown video source type");
        }();
    }

    if (audioEnabled()) {
        initAvAudioOutStreamFromEncoder(outCtx);
    }

    initAvOutputFormat(outCtx);

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

    if (m_audioPaThread.joinable()) {
        // already started
        return;
    }

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

        LOGT("pa audio after push: buf=" << m_audioBuf.size());
    }

    pa_stream_drop(stream);
}

void Caster::doPaTask() {
    const uint32_t sleep =
        m_config.perfConfig.paIterSleep * m_audioFrameDuration;

    LOGD("starting pa thread, sleep=" << sleep << ", frame-dur="
                                      << m_audioFrameDuration);

    try {
        while (!terminating()) {
            LOGT("start pa iter");
            if (pa_mainloop_iterate(m_paLoop, 0, nullptr) < 0) break;
            LOGT("stop pa iter, sleep=" << sleep);
            av_usleep(sleep);
        }
    } catch (const std::runtime_error &e) {
        LOGE("error in pa thread: " << e.what());
        reportError();
    }

    LOGD("pa thread ended");
}

void Caster::startAudioOnlyMuxing() {
    LOGD("start audio muxing");

    m_nextAudioPts = 0;
    m_audioTimeLastFrame = 0;

    m_avMuxingThread =
        std::thread([this, sleep = m_config.perfConfig.audioMuxIterSleep *
                                   m_audioFrameDuration]() {
            LOGD("starting audio muxing thread: sleep=" << sleep);

            auto *audio_pkt = av_packet_alloc();

            try {
                while (!terminating()) {
                    if (m_state != State::Started) break;
                    if (muxAudio(audio_pkt)) {
                        std::lock_guard lock{m_outCtxMtx};
                        for (auto &outCtx : m_outCtxs) {
                            if (!outCtx.avFormatCtx) continue;
                            av_write_frame(outCtx.avFormatCtx,
                                           nullptr);  // force fragment
                        }
                    }
                    av_usleep(sleep);
                }
            } catch (const std::runtime_error &e) {
                LOGE("error in audio muxing thread: " << e.what());
                reportError();
            }

            av_packet_free(&audio_pkt);

            cleanOutCtxs();
            m_audioBuf.clear();

            LOGD("muxing ended");
        });
}

void Caster::startVideoOnlyMuxing() {
    LOGD("start video muxing");

    m_nextVideoPts = 0;
    m_videoTimeLastFrame = 0;
    m_videoTimeImage = 0;
    m_videoFlushed = false;
    m_videoRealFrameDuration =
        rescaleToUsec(1, AVRational{1, m_videoFramerate});
    m_videoInputIsImage = isInputImage();
    m_videoSourceFlags = videoProps().sourceFlags;
    m_requestedVideoOrientation = m_config.videoOrientation;
    if (m_config.fileSourceConfig) {
        m_requestedImgDurationSec = m_config.fileSourceConfig->imgDurationSec;
    }

    m_avMuxingThread = std::thread([this]() {
        LOGD("starting video muxing thread");

        auto *video_pkt = av_packet_alloc();

        try {
            while (!terminating()) {
                if (m_state != State::Started) break;
                if (muxVideo(video_pkt)) {
                    std::lock_guard lock{m_outCtxMtx};
                    for (auto &outCtx : m_outCtxs) {
                        if (!outCtx.avFormatCtx) continue;
                        av_write_frame(outCtx.avFormatCtx,
                                       nullptr);  // force fragment
                    }
                }
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in video muxing thread: " << e.what());
            reportError();
        }

        av_packet_free(&video_pkt);
        cleanOutCtxs();
        m_videoBuf.clear();

        LOGD("muxing ended");
    });
}

void Caster::startVideoAudioMuxing() {
    LOGD("start video-audio muxing");

    m_nextVideoPts = 0;
    m_nextAudioPts = 0;
    m_videoTimeLastFrame = 0;
    m_videoTimeImage = 0;
    m_videoFlushed = false;
    m_videoRealFrameDuration =
        rescaleToUsec(1, AVRational{1, m_videoFramerate});
    m_videoInputIsImage = isInputImage();
    m_videoSourceFlags = videoProps().sourceFlags;
    m_requestedVideoOrientation = m_config.videoOrientation;
    m_audioTimeLastFrame = 0;
    if (m_config.fileSourceConfig) {
        m_requestedImgDurationSec = m_config.fileSourceConfig->imgDurationSec;
    }

    m_avMuxingThread = std::thread([this]() {
        LOGD("starting video-audio muxing thread");

        auto *video_pkt = av_packet_alloc();
        auto *audio_pkt = av_packet_alloc();

        try {
            while (!terminating()) {
                if (m_state != State::Started) break;

                bool pktDone = muxVideo(video_pkt);
                if (muxAudio(audio_pkt)) pktDone = true;
                if (pktDone) {
                    std::lock_guard lock{m_outCtxMtx};
                    for (auto &outCtx : m_outCtxs) {
                        if (!outCtx.avFormatCtx) continue;
                        av_write_frame(outCtx.avFormatCtx,
                                       nullptr);  // force fragment
                    }
                }
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in video-audio muxing thread: " << e.what());
            reportError();
        }

        av_packet_free(&video_pkt);
        av_packet_free(&audio_pkt);
        cleanOutCtxs();
        m_audioBuf.clear();
        m_videoBuf.clear();

        LOGD("muxing ended");
    });
}

void Caster::startMuxing() {
    if (m_avMuxingThread.joinable()) {
        // already started
        return;
    }

    if (videoEnabled()) {
        if (audioEnabled()) {
            startVideoAudioMuxing();
        } else {
            startVideoOnlyMuxing();
        }
    } else {
        if (audioEnabled()) {
            startAudioOnlyMuxing();
        } else {
            throw std::runtime_error("audio and video disabled");
        }
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

void Caster::setVideoStreamRotation(OutCtx *outCtx,
                                    VideoOrientation requestedOrientation) {
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
        case VideoSourceType::File:
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

    if (outCtx->videoStream->side_data == nullptr) {
        if (!av_stream_new_side_data(outCtx->videoStream,
                                     AV_PKT_DATA_DISPLAYMATRIX,
                                     sizeof(int32_t) * 9)) {
            throw std::runtime_error("av_stream_new_side_data error");
        }
    }

    av_display_rotation_set(
        reinterpret_cast<int32_t *>(outCtx->videoStream->side_data->data),
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

        av_usleep(m_videoFrameDuration / 2);

        return false;
    }

    if (av_new_packet(pkt, m_videoRawFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for video error");
    }

    m_videoBuf.pull(pkt->data, m_videoRawFrameSize);

    return true;
}

bool Caster::readVideoFrameFromDemuxer(AVPacket *pkt) {
    auto switchFile = [&] {
        LOGD("switch file request");

        if (reInitAvVideoInput()) {
            if (!m_config.fileSourceConfig ||
                ((m_config.fileSourceConfig->flags &
                  FileSourceFlags::SameFormatForAllFiles) == 0)) {
                reInitAvVideoDecoder();
            }
            m_videoPtsDurationOffset = m_nextVideoPts;
            return true;
        }

        return false;
    };

    if (UserRequestType userRequestType = m_userRequestType;
        m_videoSourceType == VideoSourceType::File &&
        (userRequestType == UserRequestType::SwitchToNextFile ||
         userRequestType == UserRequestType::SwitchToPrevFile ||
         userRequestType == UserRequestType::SwitchToFileIdx)) {
        if (switchFile()) {
            // file switched successfully
            return false;
        }

        // failed to switch file
        m_terminationReason = TerminationReason::Eof;
        throw source_eof_error{"no more files"};
    }

    if (auto ret = av_read_frame(m_inVideoFormatCtx, pkt); ret != 0) {
        if (ret == AVERROR_EOF) {
            LOGD("video stream eof");

            if (m_videoSourceType == VideoSourceType::File && switchFile()) {
                // file switched successfully
                return false;
            }

            m_terminationReason = TerminationReason::Eof;
            throw source_eof_error{"eof in video demuxer"};
        }

        throw std::runtime_error{"av_read_frame for video demuxer error"};
    }

    if (pkt->stream_index != m_inVideoStreamIdx) {
        av_packet_unref(pkt);
        return false;
    }

    LOGT("video frame read from input: " << pkt);

    return true;
}

bool Caster::filterVideoFrame(VideoTrans trans, AVFrame *frameIn,
                              AVFrame *frameOut) {
    LOGT("filter video frame with trans: " << trans);

    auto &ctx = m_videoFilterCtxMap.at(trans);

    if (m_videoShowProgressIndicator) {
        // update progress indicator
        auto width = static_cast<int>(
            m_outVideoCtx->width *
            std::clamp(m_requestedImgDurationSec > 0
                           ? (static_cast<double>(m_nextVideoPts -
                                                  m_videoPtsDurationOffset) /
                              (1000000 * m_requestedImgDurationSec))
                           : 0.0,
                       0.0, 1.0));

        std::array<char, 512> res{};
        if (auto ret = avfilter_graph_send_command(
                ctx.graph, "drawbox", "enable", width == 0 ? "0" : "1",
                res.data(), res.size(), 0);
            ret < 0) {
            LOGF("drawbox enable avfilter_graph_send_command error: "
                 << strForAvError(ret));
        }
        if (auto ret = avfilter_graph_send_command(
                ctx.graph, "drawbox", "width", fmt::format("{}", width).c_str(),
                res.data(), res.size(), 0);
            ret < 0) {
            LOGF("drawbox avfilter_graph_send_command error: "
                 << strForAvError(ret));
        }
    }

    if (m_videoShowTextIndicator) {
        std::array<char, 512> res{};
        std::string text;
        text.reserve(50);
        if ((m_optionsFlags & OptionsFlags::ShowVideoCountIndicator) > 0 &&
            m_config.fileSourceConfig) {
            text += fmt::format("{} / {}", m_currentFileIdx + 1,
                                m_config.fileSourceConfig->files.size());
        }
        if ((m_optionsFlags & OptionsFlags::ShowVideoExifDateIndicator) > 0) {
            if (!m_currentExif.dateTime.empty()) {
                if (!text.empty()) {
                    text.append(" · ", 4);
                }
                text += m_currentExif.dateTime;
            }
        }
        if ((m_optionsFlags & OptionsFlags::ShowVideoExifModelIndicator) > 0) {
            if (!m_currentExif.model.empty()) {
                if (!text.empty()) {
                    text.append(" · ", 4);
                }
                text += m_currentExif.model;
            }
        }

        if (!text.empty()) {
            text.append(" ", 1);
        }

        if (auto ret = avfilter_graph_send_command(ctx.graph, "drawtext",
                                                   "text", text.c_str(),
                                                   res.data(), res.size(), 0);
            ret < 0) {
            LOGF("drawtext avfilter_graph_send_command error: "
                 << strForAvError(ret));
        }
    }

    if (auto ret = av_buffersrc_add_frame_flags(ctx.srcCtx, frameIn,
                                                AV_BUFFERSRC_FLAG_PUSH);
        ret < 0) {
        LOGF(
            "video av_buffersrc_add_frame_flags error: " << strForAvError(ret));
    }

    auto ret = av_buffersink_get_frame(ctx.sinkCtx, frameOut);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return false;
    if (ret < 0) {
        LOGF("video av_buffersink_get_frame error: " << strForAvError(ret));
    }

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
    auto trans = [&] {
        switch (m_videoTrans) {
            case VideoTrans::Off:
            case VideoTrans::Scale:
            case VideoTrans::Vflip:
            case VideoTrans::Hflip:
            case VideoTrans::VflipHflip:
            case VideoTrans::TransClock:
            case VideoTrans::TransClockFlip:
            case VideoTrans::TransCclock:
            case VideoTrans::TransCclockFlip: {
#ifdef USE_LIPSTICK_RECORDER
                if (m_lipstickRecorder) {
                    return transForYinverted(m_videoTrans,
                                             m_lipstickRecorder->yinverted());
                }
#endif
                break;
            }
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
                return orientationToTrans(m_requestedVideoOrientation,
                                          videoProps());
        }
        return m_videoTrans;
    }();

    LOGT("requested trans: " << trans);

    if (trans == VideoTrans::Off) return frameIn;

    if (!filterVideoFrame(trans, frameIn, m_videoFrameAfterFilter)) {
        av_frame_unref(frameIn);
        av_frame_unref(m_videoFrameAfterFilter);
        return nullptr;
    }

    av_frame_unref(frameIn);
    return m_videoFrameAfterFilter;
}

static std::string escapeColons(const std::string &input) {
    std::string result;
    result.reserve(input.size() + std::count(input.begin(), input.end(), ':'));
    for (char c : input) {
        if (c == ':') {
            result.append("\\:", 2);
        } else {
            result.push_back(c);
        }
    }
    return result;
}

Caster::ExifData Caster::extractExifFromDecoder() {
    ExifData exif;

    auto *pkt = av_packet_alloc();
    if (!readVideoFrameFromDemuxer(pkt)) {
        av_packet_unref(pkt);
        return exif;
    }

    // reiniting to rewind input to the begining
    if (!m_currentFile.empty()) {
        {
            std::lock_guard lock{m_filesMtx};
            m_files.push_front(m_currentFile);
        }
        if (!reInitAvVideoInput()) {
            LOGF("failed to reinit video input");
        }
    }

    if (avcodec_send_packet(m_inVideoCtx, pkt) != 0) {
        av_packet_unref(pkt);
        return exif;
    }

    auto *frame = av_frame_alloc();
    if (avcodec_receive_frame(m_inVideoCtx, frame) != 0) {
        av_frame_unref(frame);
        av_frame_free(&frame);
        av_packet_unref(pkt);
        return exif;
    }

    //    LOGD("exif all raw:");
    //    log_dict(frame->metadata);

    // orientation
    auto *entry = av_dict_get(frame->metadata, "Orientation", NULL, 0);
    if (entry) {
        LOGD("exif orientation raw: '" << entry->value << "'");
        switch (std::clamp(atoi(entry->value), 1, 8)) {
            case 1:  // 0 deg
            case 2:  // 0 deg, mirrored
                exif.orientation = VideoOrientation::Landscape;
                break;
            case 3:  // 180 deg
            case 4:  // 180 deg, mirrored
                exif.orientation = VideoOrientation::InvertedLandscape;
                break;
            case 5:  // 90 deg
            case 6:  // 90 deg, mirrored
                exif.orientation = VideoOrientation::InvertedPortrait;
                break;
            case 7:  // 270 deg
            case 8:  // 270 deg, mirrored
                exif.orientation = VideoOrientation::Portrait;
                break;
            default:
                break;
        }
    } else {
        LOGD("exif orientation raw: null");
    }

    // make
    entry = av_dict_get(frame->metadata, "Make", NULL, 0);
    if (entry) {
        LOGD("exif make raw: '" << entry->value << "'");
        exif.model = escapeColons(entry->value);
        if (!exif.model.empty()) {
            exif.model[0] = std::toupper(exif.model[0]);
        }
    }

    // model
    entry = av_dict_get(frame->metadata, "Model", NULL, 0);
    if (entry) {
        LOGD("exif model raw: '" << entry->value << "'");
        auto model = escapeColons(entry->value);
        if (!model.empty() && !exif.model.empty()) {
            exif.model += ' ';
        }
        exif.model += model;
    }

    // date-time
    entry = av_dict_get(frame->metadata, "DateTime", NULL, 0);
    if (entry) {
        LOGD("exif datetime raw: '" << entry->value << "'");
        if (m_config.fileSourceConfig &&
            m_config.fileSourceConfig->fileDateTimeTranslateHandler) {
            exif.dateTime = escapeColons(
                m_config.fileSourceConfig->fileDateTimeTranslateHandler(
                    entry->value));
        } else {
            exif.dateTime = escapeColons(entry->value);
        }
    }

    av_frame_unref(frame);
    av_frame_free(&frame);
    av_packet_unref(pkt);

    return exif;
}

bool Caster::encodeVideoFrame(AVPacket *pkt) {
    LOGT("video pkt to decoder: " << pkt);

    if (auto ret = avcodec_send_packet(m_inVideoCtx, pkt);
        ret != 0 && ret != AVERROR(EAGAIN)) {
        av_packet_unref(pkt);
        if (ret == AVERROR_EOF) {
            m_terminationReason = TerminationReason::Eof;
        }
        LOGE("avcodec_send_packet for video error: " << strForAvError(ret));
        return false;
    }

    av_packet_unref(pkt);

    if (auto ret = avcodec_receive_frame(m_inVideoCtx, m_videoFrameIn);
        ret != 0) {
        if (ret == AVERROR_EOF) {
            m_terminationReason = TerminationReason::Eof;
        }
        throw std::runtime_error(fmt::format(
            "video avcodec_receive_frame error ({})", strForAvError(ret)));
    }

    LOGT("video frame from decoder: " << m_videoFrameIn);

    m_videoFrameIn->format = m_inVideoCtx->pix_fmt;
    m_videoFrameIn->width = m_inVideoCtx->width;
    m_videoFrameIn->height = m_inVideoCtx->height;
    m_videoFrameIn->time_base = m_inVideoCtx->time_base;
    m_videoFrameIn->pts = m_nextVideoInFramePts;
    m_videoFrameIn->duration = 1;
    m_nextVideoInFramePts += m_videoFrameIn->duration;

    auto *frameOut = filterVideoIfNeeded(m_videoFrameIn);
    if (frameOut == nullptr) return false;

    if (auto ret = avcodec_send_frame(m_outVideoCtx, frameOut);
        ret != 0 && ret != AVERROR(EAGAIN)) {
        av_frame_unref(frameOut);
        throw std::runtime_error(fmt::format(
            "video avcodec_send_frame error ({})", strForAvError(ret)));
    }

    av_frame_unref(frameOut);

    if (auto ret = avcodec_receive_packet(m_outVideoCtx, pkt); ret != 0) {
        if (ret == AVERROR(EAGAIN)) {
            LOGD("video pkt not ready");
            return false;
        }

        throw std::runtime_error(fmt::format(
            "video avcodec_receive_packet error ({})", strForAvError(ret)));
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
    if (!m_pktSideData.empty()) {
        // already extracted
        return;
    }

    auto *pkt = av_packet_alloc();

    readVideoFrameFromDemuxer(pkt);

    if (!extractExtradata(pkt)) {
        LOGW("failed to extract extradata from video pkt");
    }

    av_packet_unref(pkt);
}

void Caster::extractVideoExtradataFromRawDemuxer() {
    if (!m_pktSideData.empty()) {
        // already extracted
        return;
    }

    if (!m_videoBsfExtractExtraCtx) {
        // bsf not inited
        return;
    }

    auto *pkt = av_packet_alloc();

    readVideoFrameFromDemuxer(pkt);
    if (encodeVideoFrame(pkt))
        if (!extractExtradata(pkt))
            LOGW("first time failed to extract extradata from video pkt");

    av_packet_unref(pkt);
}

void Caster::extractVideoExtradataFromRawBuf() {
    if (!m_pktSideData.empty()) {
        // already extracted
        return;
    }

    if (!m_videoBsfExtractExtraCtx) {
        // bsf not inited
        return;
    }

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

bool Caster::isInputImage() const {
    return m_inVideoFormatCtx &&
           strcmp(m_inVideoFormatCtx->iformat->name, "image2") == 0;
}

bool Caster::muxVideo(AVPacket *pkt) {
    const auto now = av_gettime();

    LOGT("video read real frame");

    if ((m_videoSourceFlags & SourceFlags::VideoThrottleFastStream) > 0 &&
        m_videoStreamTimeLastFrame > 0) {
        auto diff = now - m_videoStreamTimeLastFrame - m_videoFrameDuration;

        if (diff < (m_config.perfConfig.videoMaxFastFrames *
                    m_videoFrameDuration * -1)) {
            LOGT("streaming is to fast: diff="
                 << diff << ", ref=" << m_videoFrameDuration
                 << ", max-fast-frames="
                 << m_config.perfConfig.videoMaxFastFrames);
            av_usleep(m_videoFrameDuration);
            return false;
        }
        if (diff > m_videoFrameDuration) {
            LOGD("streaming is to slow: diff=" << diff << ", ref="
                                               << m_videoFrameDuration);
        }
    }

    switch (m_videoSourceType) {
        case VideoSourceType::DroidCam:
            if (!readVideoFrameFromDemuxer(pkt)) return false;
            break;
        case VideoSourceType::V4l2:
        case VideoSourceType::X11Capture:
        case VideoSourceType::File:
            if (m_videoInputIsImage) {
                // if input is an image, decode only once and clone pkt
                UserRequestType userRequestType = m_userRequestType;
                if ((userRequestType == UserRequestType::SwitchToNextFile ||
                     userRequestType == UserRequestType::SwitchToPrevFile ||
                     userRequestType == UserRequestType::SwitchToFileIdx) &&
                    m_imgPkt != nullptr) {
                    av_packet_free(&m_imgPkt);
                }
                if (m_imgPkt == nullptr) {
                    LOGT("decode img pkt");
                    try {
                        if (!readVideoFrameFromDemuxer(pkt)) return false;
                        m_imgPkt = av_packet_clone(pkt);
                        m_videoTimeImage = now;
                        if (!m_imgPkt) {
                            LOGF("failed to clone img pkt");
                        }
                    } catch (const source_eof_error &err) {
                        LOGD("source eof");
                        // null pkt to flush
                        pkt->size = 0;
                        pkt->data = nullptr;
                    }
                } else {
                    // if decoded img pkt exists, just copy data
                    LOGT("copy img pkt");
                    pkt->pts = m_imgPkt->pts;
                    pkt->dts = m_imgPkt->dts;
                    pkt->duration = m_imgPkt->duration;
                    pkt->stream_index = m_imgPkt->stream_index;
                    pkt->flags = m_imgPkt->flags;
                    pkt->pos = m_imgPkt->pos;

                    if (m_requestedImgDurationSec > 0 &&
                        m_nextVideoPts - m_videoPtsDurationOffset >
                            (1000000 * m_requestedImgDurationSec)) {
                        LOGD("eof for img");
                        av_packet_free(&m_imgPkt);
                        return false;
                    } else {
                        pkt->size = m_imgPkt->size;
                        pkt->data = m_imgPkt->data;
                    }
                }
                if (!encodeVideoFrame(pkt)) return false;
            } else {
                try {
                    if (!readVideoFrameFromDemuxer(pkt)) return false;
                } catch (const source_eof_error &err) {
                    LOGD("source eof");
                    // null pkt to flush
                    pkt->size = 0;
                    pkt->data = nullptr;
                }
                if (!encodeVideoFrame(pkt)) return false;
            }
            break;
        case VideoSourceType::LipstickCapture:
        case VideoSourceType::Test:
        case VideoSourceType::DroidCamRaw:
            if (!readVideoFrameFromBuf(pkt)) return false;
            if (!encodeVideoFrame(pkt)) return false;
            break;
        default:
            LOGF("unknown video source type");
    }

    if (!avPktOk(pkt)) {
        av_packet_unref(pkt);
        return false;
    }

    if (!insertExtradata(pkt)) return false;

    updateVideoSampleStats(now);

    m_nextVideoPts += m_videoRealFrameDuration;

    {
        std::lock_guard lock{m_outCtxMtx};
        for (auto &outCtx : m_outCtxs) {
            if (!outCtx.videoStream || !outCtx.avFormatCtx) continue;

            pkt->pts = outCtx.nextVideoPts;
            pkt->dts = outCtx.nextVideoPts;
            pkt->stream_index = outCtx.videoStream->index;
            pkt->duration = rescaleFromUsec(m_videoRealFrameDuration,
                                            outCtx.videoStream->time_base);
            pkt->time_base = outCtx.videoStream->time_base;
            outCtx.nextVideoPts += pkt->duration;

            LOGT("video: frd=" << m_videoRealFrameDuration
                               << ", npts_usec=" << m_nextVideoPts
                               << ", npts=" << outCtx.nextVideoPts
                               << ", lft=" << m_videoTimeLastFrame
                               << ", os_tb=" << outCtx.videoStream->time_base
                               << ", pkt=" << pkt);
            if (auto ret = av_write_frame(outCtx.avFormatCtx, pkt); ret < 0) {
                LOGF("av_interleaved_write_frame for video error: "
                     << strForAvError(ret));
            }

            if (!outCtx.videoFlushed) {
                LOGD("first av video data: ctx=" << outCtx.avFormatCtx);
                outCtx.videoFlushed = true;
                m_videoFlushed = true;
            }
        }
    }

    av_packet_unref(pkt);

    return true;
}

int64_t Caster::videoAudioDelay() const {
    for (auto &outCtx : m_outCtxs) {
        if (!outCtx.videoStream || !outCtx.audioStream) continue;
        auto video_pts =
            rescaleToUsec(outCtx.nextVideoPts, outCtx.videoStream->time_base);
        auto audio_pts =
            rescaleToUsec(outCtx.nextAudioPts, outCtx.audioStream->time_base);
        return video_pts - audio_pts;
    }

    return 0;
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

    if (!initAvAudioInputFormatFromFile()) {
        return false;
    }

    findAvAudioInputStreamIdx();

    LOGD("audio input re-inited");

    return true;
}

bool Caster::reInitAvVideoInput() {
    cleanAvVideoInputFormat();

    if (!initAvVideoInputFormatFromFile()) {
        return false;
    }

    findAvVideoInputStreamIdx();

    LOGD("video input re-inited");

    return true;
}

void Caster::reInitAvVideoDecoder() {
    if (m_inVideoCtx != nullptr) {
        avcodec_free_context(&m_inVideoCtx);
    }
    if (m_videoFrameIn != nullptr) {
        av_frame_free(&m_videoFrameIn);
    }
    if (m_videoFrameAfterFilter != nullptr) {
        av_frame_free(&m_videoFrameAfterFilter);
    }
    if (m_imgPkt != nullptr) {
        av_packet_free(&m_imgPkt);
    }
    cleanAvVideoFilters();
    initAvVideoRawDecoderFromInputStream();

    m_currentExif = extractExifFromDecoder();
    if (m_videoOrigOrientation == VideoOrientation::Auto) {
        auto &props = m_videoProps.at(m_config.videoSource);
        props.orientation = m_currentExif.orientation;
        initVideoTrans();
    }

    initAvVideoFilters();

    LOGD("video decoder re-inited");
}

void Caster::reInitAvAudioDecoder() {
    cleanAvAudioDecoder();
    cleanAvAudioFifo();
    cleanAvAudioFilters();

    initAvAudioRawDecoderFromInputStream();
    initAvAudioFilters();
    initAvAudioFifo();

    m_audioInFrameSize = av_samples_get_buffer_size(
        nullptr, m_inAudioCtx->ch_layout.nb_channels, m_outAudioCtx->frame_size,
        m_inAudioCtx->sample_fmt, 0);

    LOGD("audio decoder re-inited");
}

bool Caster::readAudioPktFromBuf(AVPacket *pkt, bool nullWhenNoEnoughData) {
    std::lock_guard lock{m_audioMtx};

    LOGT("audio read from buf: has-enough-data="
         << m_audioBuf.hasEnoughData(m_audioInFrameSize) << ", push-null="
         << nullWhenNoEnoughData << ", frame-size=" << m_audioInFrameSize
         << ", buff-size=" << m_audioBuf.size() << ", diff-size="
         << static_cast<long>(m_audioBuf.size()) - m_audioInFrameSize);

    if (!m_audioBuf.hasEnoughData(m_audioInFrameSize)) {
        const auto pushNull = m_paStream == nullptr || nullWhenNoEnoughData;
        if (!pushNull) {
            return false;
        }
        LOGT("audio push null: " << (m_audioInFrameSize - m_audioBuf.size()));
        m_audioBuf.pushNullExactForce(m_audioInFrameSize - m_audioBuf.size());
    }

    if (av_new_packet(pkt, m_audioInFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for audio error");
    }

    if (!m_audioBuf.pullExact(pkt->data, m_audioInFrameSize)) {
        throw std::runtime_error("failed to pull from buf");
    }

    LOGT("audio read from buff successful");

    return true;
}

void Caster::readAudioPktFromNull(AVPacket *pkt) {
    if (av_new_packet(pkt, m_audioInFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for audio error");
    }

    std::fill(pkt->data, pkt->data + m_audioInFrameSize, 0);
}

void Caster::readAudioPktFromNoise(AVPacket *pkt) {
    if (av_new_packet(pkt, m_audioInFrameSize) < 0) {
        throw std::runtime_error("av_new_packet for audio error");
    }

    std::random_device rnd_device;
    std::mt19937 engine{rnd_device()};
    std::uniform_int_distribution<uint8_t> dist{0, 255};

    std::generate(pkt->data, pkt->data + m_audioInFrameSize,
                  [&]() { return dist(engine); });
}

bool Caster::readAudioPktFromDemuxer(AVPacket *pkt) {
    if (auto ret = av_read_frame(m_inAudioFormatCtx, pkt); ret != 0) {
        if (ret == AVERROR_EOF) {
            LOGD("audio stream eof");

            if (audioProps().type == AudioSourceType::File) {
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

bool Caster::readAudioFrame(AVPacket *pkt, DataSource source) {
    while (av_audio_fifo_size(m_audioFifo) < m_outAudioCtx->frame_size) {
        if (terminating()) return false;

        switch (source) {
            case DataSource::Demuxer:
                if (!readAudioPktFromDemuxer(pkt)) continue;
                break;
            case DataSource::Buf:
                if (!readAudioPktFromBuf(pkt, false)) return false;
                break;
            case DataSource::BufWithNull:
                if (!readAudioPktFromBuf(pkt, true)) return false;
                break;
            case DataSource::Null:
                readAudioPktFromNull(pkt);
                break;
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
            0) {
            throw std::runtime_error("av_audio_fifo_realloc error");
        }

        if (av_audio_fifo_write(
                m_audioFifo, reinterpret_cast<void **>(m_audioFrameIn->data),
                m_audioFrameIn->nb_samples) < m_audioFrameIn->nb_samples) {
            throw std::runtime_error("av_audio_fifo_write error");
        }

        av_frame_unref(m_audioFrameIn);
    }

    m_audioFrameIn->nb_samples = m_outAudioCtx->frame_size;
    av_channel_layout_copy(&m_audioFrameIn->ch_layout,
                           &m_inAudioCtx->ch_layout);
    m_audioFrameIn->format = m_inAudioCtx->sample_fmt;
    m_audioFrameIn->sample_rate = m_inAudioCtx->sample_rate;

    if (av_frame_get_buffer(m_audioFrameIn, 0) != 0) {
        throw std::runtime_error("av_frame_get_buffer error");
    }

    if (av_audio_fifo_read(
            m_audioFifo, reinterpret_cast<void **>(m_audioFrameIn->data),
            m_outAudioCtx->frame_size) < m_outAudioCtx->frame_size) {
        av_frame_free(&m_audioFrameIn);
        throw std::runtime_error("av_audio_fifo_read error");
    }

    return true;
}

bool Caster::readAudioFrameFromBuf(AVPacket *pkt, bool nullWhenNoEnoughData) {
    return readAudioFrame(
        pkt, nullWhenNoEnoughData ? DataSource::BufWithNull : DataSource::Buf);
}

bool Caster::readAudioFrameFromNull(AVPacket *pkt) {
    return readAudioFrame(pkt, DataSource::Null);
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

    while (!terminating()) {
        auto now = av_gettime();
        const auto maxAudioDelay =
            m_videoRealFrameDuration > 0
                ? std::max(m_videoRealFrameDuration, 2 * m_audioFrameDuration)
                : 2 * m_audioFrameDuration;
        const auto delay =
            m_videoRealFrameDuration > 0 ? videoAudioDelay() : audioDelay(now);

        LOGT("audio: delay="
             << delay << ", audio frame-dur=" << m_audioFrameDuration
             << ", alf=" << m_audioTimeLastFrame
             << ", vrfd=" << m_videoRealFrameDuration << ", too-much="
             << (delay < -maxAudioDelay) << ", low-delay=" << (delay <= 0)
             << ", push-null=" << (delay > maxAudioDelay && m_videoFlushed));
        if (delay < -maxAudioDelay) {
            LOGW("too much audio, delay=" << delay);
            break;
        }

        if (delay < m_audioFrameDuration) break;

        if (m_audioSourceType == AudioSourceType::File) {
            if (!readAudioFrameFromDemuxer(pkt)) break;
        } else if (m_audioSourceType == AudioSourceType::Null) {
            if (!readAudioFrameFromNull(pkt)) break;
        } else {
            if (!readAudioFrameFromBuf(
                    pkt, delay > maxAudioDelay && m_videoFlushed)) {
                break;
            }
        }

        if (!encodeAudioFrame(pkt)) continue;

        if (!avPktOk(pkt)) {
            av_packet_unref(pkt);
            continue;
        }

        if (m_audioTimeLastFrame == 0) {
            m_audioTimeLastFrame = now;
        } else {
            m_audioTimeLastFrame += m_audioFrameDuration;
        }

        pkt->duration = m_audioPktDuration;

        {
            std::lock_guard lock{m_outCtxMtx};
            for (auto &outCtx : m_outCtxs) {
                if (!outCtx.audioStream || !outCtx.avFormatCtx) continue;

                pkt->pts = outCtx.nextAudioPts;
                pkt->dts = outCtx.nextAudioPts;
                pkt->stream_index = outCtx.audioStream->index;
                outCtx.nextAudioPts += pkt->duration;

                LOGT("audio mux: ctx=" << outCtx.avFormatCtx << ", os-tb="
                                       << outCtx.audioStream->time_base
                                       << ", pkt=" << pkt);

                if (auto ret = av_write_frame(outCtx.avFormatCtx, pkt);
                    ret < 0) {
                    LOGF("av_interleaved_write_frame for audio error: "
                         << strForAvError(ret));
                }

                if (!outCtx.audioFlushed) {
                    LOGD("first av audio data: ctx=" << outCtx.avFormatCtx);
                    outCtx.audioFlushed = true;
                }
            }
        }

        av_packet_unref(pkt);

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
    auto *outCtx = static_cast<OutCtx *>(opaque);
    return outCtx->caster->avWritePacketCallback(outCtx, buf, bufSize);
}

int Caster::avWritePacketCallback(OutCtx *ctx, ff_buf_type buf, int bufSize) {
    if (bufSize < 0)
        throw std::runtime_error("invalid read packet callback buf size");

    LOGT("write packet: size=" << bufSize
                               << ", data=" << dataToStr(buf, bufSize));

    if (!terminating() && ctx->dataReadyHandler) {
        if (!ctx->muxedFlushed && m_avMuxingThread.joinable()) {
            LOGD("first av muxed data: ctx=" << ctx->avFormatCtx);
            ctx->muxedFlushed = true;
        }
        return static_cast<int>(ctx->dataReadyHandler(buf, bufSize));
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
    m_videoCv.wait(lock,
                   [this] { return terminating() || !m_videoBuf.empty(); });

    if (terminating()) {
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
        if ((m_videoSourceFlags & SourceFlags::VideoAdjustVideoFrameDuration) >
            0) {
            auto lastDur = now - m_videoTimeLastFrame;
            if (lastDur != m_videoFrameDuration) {
                LOGD("adjusting video real frame duration: "
                     << m_videoRealFrameDuration << " => " << lastDur
                     << ", ref=" << m_videoFrameDuration
                     << ", codec=" << m_outVideoCtx->codec_id);
                m_videoRealFrameDuration = lastDur;
            }
        }
        m_videoStreamTimeLastFrame += m_videoRealFrameDuration;
    } else {
        m_videoStreamTimeLastFrame = now;
    }
    m_videoTimeLastFrame = now;
}

void Caster::setAudioVolume(int volume) {
    if ((capabilities() & CapabilityFlags::AdjustableAudioVolume) == 0) {
        LOGW("source doesn't support:"
             << CapabilityFlags::AdjustableAudioVolume);
        return;
    }

    if (volume < -50 || volume > 50) {
        LOGW("audio-volume is invalid");
        return;
    }

    if (volume == m_config.audioVolume) return;

    m_config.audioVolume = volume;
    m_audioVolumeUpdated = true;
}

void Caster::setLoopFile(bool enabled) {
    if ((capabilities() & CapabilityFlags::AdjustableLoopFile) == 0) {
        LOGW("source doesn't support:" << CapabilityFlags::AdjustableLoopFile);
        return;
    }

    if (!m_config.fileSourceConfig) {
        LOGW("no file source config");
        return;
    }

    bool loopEnabled = m_config.fileSourceConfig->flags & FileSourceFlags::Loop;

    if (loopEnabled == enabled) {
        // already set
        return;
    }

    if (enabled) {
        m_config.fileSourceConfig->flags |= FileSourceFlags::Loop;
    } else {
        m_config.fileSourceConfig->flags &= ~FileSourceFlags::Loop;
    }
}

void Caster::setShowVideoProgressIndicator(bool enabled) {
    if (enabled) {
        m_optionsFlags |= OptionsFlags::ShowVideoProgressIndicator;
    } else {
        m_optionsFlags &= ~(OptionsFlags::ShowVideoProgressIndicator);
    }
}

void Caster::setShowVideoCountIndicator(bool enabled) {
    if (enabled) {
        m_optionsFlags |= OptionsFlags::ShowVideoCountIndicator;
    } else {
        m_optionsFlags &= ~OptionsFlags::ShowVideoCountIndicator;
    }
}

void Caster::setShowVideoExifDateIndicator(bool enabled) {
    if (enabled) {
        m_optionsFlags |= OptionsFlags::ShowVideoExifDateIndicator;
    } else {
        m_optionsFlags &= ~OptionsFlags::ShowVideoExifDateIndicator;
    }
}

void Caster::setShowVideoExifModelIndicator(bool enabled) {
    if (enabled) {
        m_optionsFlags |= OptionsFlags::ShowVideoExifModelIndicator;
    } else {
        m_optionsFlags &= ~OptionsFlags::ShowVideoExifModelIndicator;
    }
}

void Caster::setVideoOrientation(VideoOrientation orientation) {
    if (capabilities() & CapabilityFlags::AdjustableVideoOrientation) {
        m_requestedVideoOrientation = orientation;
    } else {
        LOGW("source doesn't support:"
             << CapabilityFlags::AdjustableVideoOrientation);
    }
}
void Caster::setImgDurationSec(uint32_t duration, bool restart) {
    if (capabilities() & CapabilityFlags::AdjustableImageDuration) {
        if (restart && duration > 0 && m_nextVideoPts > 0) {
            m_videoPtsDurationOffset = m_nextVideoPts;
        }
        m_requestedImgDurationSec = duration;
    } else {
        LOGW("source doesn't support:"
             << CapabilityFlags::AdjustableImageDuration);
    }
}

uint32_t Caster::hash(std::string_view str) {
    return std::accumulate(str.cbegin(), str.cend(), 0U) % 999;
}

Caster::VideoPropsMap Caster::detectVideoSources(uint32_t options) {
    avdevice_register_all();

    VideoPropsMap props;
#ifdef USE_DROIDCAM
    if (options & OptionsFlags::DroidCamVideoSources ||
        options & OptionsFlags::DroidCamRawVideoSources) {
        props.merge(detectDroidCamVideoSources(options));
    }
#endif
#ifdef USE_V4L2
    if (options & OptionsFlags::V4l2VideoSources) {
        props.merge(detectV4l2VideoSources());
    }
#endif
#ifdef USE_X11CAPTURE
    if (options & OptionsFlags::X11CaptureVideoSources) {
        props.merge(detectX11VideoSources());
    }
#endif
#ifdef USE_LIPSTICK_RECORDER
    if (options & OptionsFlags::LipstickCaptureVideoSources) {
        props.merge(detectLipstickRecorderVideoSources());
    }
#endif
#ifdef USE_TESTSOURCE
    props.merge(detectTestVideoSources());
#endif
    if (options & OptionsFlags::FileVideoSources) {
        props.merge(detectVideoFileSources());
    }

    return props;
}

Caster::VideoPropsMap Caster::detectVideoFileSources() {
    LOGD("video file sources detection started");

    VideoPropsMap propsMap;

    {
        Caster::VideoSourceInternalProps props;
        props.name = "file";
        props.friendlyName = "File streaming";
        props.type = VideoSourceType::File;
        props.orientation = Caster::VideoOrientation::Auto;
        props.trans = VideoTrans::Off;
        props.scale = VideoScale::DownAuto;
        props.capaFlags = CapabilityFlags::AdjustableImageDuration |
                          CapabilityFlags::AdjustableLoopFile;

        LOGD("video file source found: " << props);

        propsMap.try_emplace(props.name, std::move(props));
    }

    {
        Caster::VideoSourceInternalProps props;
        props.name = "file-rotate";
        props.friendlyName = "File streaming (rotate)";
        props.type = VideoSourceType::File;
        props.orientation = Caster::VideoOrientation::Auto;
        props.trans = VideoTrans::Frame169;
        props.scale = VideoScale::DownAuto;
        props.capaFlags = CapabilityFlags::AdjustableVideoOrientation |
                          CapabilityFlags::AdjustableImageDuration |
                          CapabilityFlags::AdjustableLoopFile;
        props.sourceFlags = SourceFlags::VideoThrottleFastStream;

        LOGD("video file source found: " << props);

        propsMap.try_emplace(props.name, std::move(props));
    }

    {
        Caster::VideoSourceInternalProps props;
        props.name = "file-fixed-size";
        props.friendlyName = "File streaming (fixed size)";
        props.type = VideoSourceType::File;
        props.orientation = Caster::VideoOrientation::Auto;
        props.trans = VideoTrans::Frame169;
        props.scale = VideoScale::DownAuto;
        props.capaFlags = CapabilityFlags::AdjustableVideoOrientation |
                          CapabilityFlags::AdjustableImageDuration |
                          CapabilityFlags::AdjustableLoopFile;
        props.sourceFlags = SourceFlags::VideoThrottleFastStream;
        FrameSpec fs;
        fs.framerates.insert(5U);
        fs.dim.width = 1920U;
        fs.dim.height = 1080U;
        props.formats.push_back(
            VideoFormatExt{AV_CODEC_ID_NONE, AV_PIX_FMT_NONE, {fs}});

        LOGD("video file source found: " << props);

        propsMap.try_emplace(props.name, std::move(props));
    }

    LOGD("video file sources detection completed");

    return propsMap;
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
        return terminating() || m_videoBuf.hasFreeSpace(size);
    });

    if (terminating()) {
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

uint32_t Caster::capabilities() const {
    return (audioEnabled() ? audioProps().capaFlags : 0) |
           (videoEnabled() ? videoProps().capaFlags : 0);
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
        props.sourceFlags = SourceFlags::VideoAdjustVideoFrameDuration;
        props.scale = VideoScale::DownAuto;

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
        p.sourceFlags = SourceFlags::VideoAdjustVideoFrameDuration;

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
        p.sourceFlags = SourceFlags::VideoAdjustVideoFrameDuration;

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
        p.sourceFlags = SourceFlags::VideoAdjustVideoFrameDuration;

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
        p.sourceFlags = SourceFlags::VideoAdjustVideoFrameDuration;

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
Caster::bestVideoFormatForV4l2Encoder(const VideoSourceInternalProps &props,
                                      VideoEncoder type) {
    if (m_v4l2Encoders.empty()) throw std::runtime_error("no v4l2 encoder");

    if (auto it = std::find_if(
            props.formats.cbegin(), props.formats.cend(),
            [&](const auto &sf) {
                return std::any_of(
                    m_v4l2Encoders.cbegin(), m_v4l2Encoders.cend(),
                    [&sf, type,
                     useNiceFormats =
                         m_config.options &
                         OptionsFlags::OnlyNiceVideoFormats](const auto &e) {
                        return std::any_of(e.formats.cbegin(), e.formats.cend(),
                                           [&](const auto &ef) {
                                               if (sf.codecId != ef.codecId)
                                                   return false;
                                               if (useNiceFormats &&
                                                   !nicePixfmt(ef.pixfmt, type))
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
        auto fmt = [this, type] {
            for (const auto &f : m_v4l2Encoders.front().formats)
                if (f.codecId == AV_CODEC_ID_RAWVIDEO &&
                    nicePixfmt(f.pixfmt, type))
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
            // props.scale = VideoScale::Down50;
            props.scale = VideoScale::DownAuto;
            props.sourceFlags = SourceFlags::VideoThrottleFastStream |
                                SourceFlags::VideoAdjustVideoFrameDuration;

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
            // props.scale = VideoScale::Down50;
            props.scale = VideoScale::DownAuto;
            props.sourceFlags = SourceFlags::VideoThrottleFastStream |
                                SourceFlags::VideoAdjustVideoFrameDuration;

            LOGD("lipstick recorder source found: " << props);

            map.try_emplace(props.name, std::move(props));
        }
    };

    LOGD("lipstick-recorder video source detecton completed");

    return map;
}
#endif
