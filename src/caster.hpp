/* Copyright (C) 2022-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CASTER_H
#define CASTER_H

#ifdef USE_V4L2
#include <linux/videodev2.h>
#endif

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "databuffer.hpp"
#include "testsource.hpp"

#ifdef USE_LIPSTICK_RECORDER
#include "lipstickrecordersource.hpp"
#endif
#ifdef USE_DROIDCAM
#include "droidcamsource.hpp"
#include "orientationmonitor.hpp"
#endif

class Caster {
   public:
    enum class State {
        Initing,
        Inited,
        Starting,
        Started,
        Paused,
        Terminating
    };
    friend std::ostream &operator<<(std::ostream &os, State state);

    enum class VideoOrientation {
        Auto,
        Portrait,
        InvertedPortrait,
        Landscape,
        InvertedLandscape
    };
    friend std::ostream &operator<<(std::ostream &os,
                                    VideoOrientation videoOrientation);

    enum class StreamFormat { Mp4, MpegTs, Mp3 };
    friend std::ostream &operator<<(std::ostream &os,
                                    StreamFormat streamFormat);

    enum class SensorDirection { Unknown, Front, Back };
    friend std::ostream &operator<<(std::ostream &os,
                                    SensorDirection direction);

    enum class VideoEncoder { Auto, X264, Nvenc, V4l2 };
    friend std::ostream &operator<<(std::ostream &os, VideoEncoder encoder);

    using DataReadyHandler = std::function<size_t(const uint8_t *, size_t)>;
    using StateChangedHandler = std::function<void(State state)>;
    using AudioSourceNameChangedHandler =
        std::function<void(const std::string &name)>;

    struct Dim {
        uint32_t width = 0;
        uint32_t height = 0;
        inline bool operator>(Dim dim) const {
            return (width + height) > (dim.width + dim.height);
        }
        inline bool thin() const {
            return (width > height ? width / height : height / width) > 16 / 9;
        }
        inline bool fat() const { return !thin(); }
        inline VideoOrientation orientation() const {
            return width < height ? VideoOrientation::Portrait
                                  : VideoOrientation::Landscape;
        }
    };
    friend std::ostream &operator<<(std::ostream &os, Dim dim);

    struct Config {
        StreamFormat streamFormat = StreamFormat::Mp4;
        std::string videoSource;
        std::string audioSource;
        VideoOrientation videoOrientation = VideoOrientation::Landscape;
        float audioVolume = 1.0;
        std::string streamAuthor{"Caster"};
        std::string streamTitle{"Cast session"};
        VideoEncoder videoEncoder = VideoEncoder::Auto;
        bool useNiceFormats =
            true; /*force output pixel format to nice one (yuv420p)*/
        friend std::ostream &operator<<(std::ostream &os, const Config &config);
    };

    struct AudioSourceProps {
        std::string name;
        std::string friendlyName;
    };

    struct VideoSourceProps {
        std::string name;
        std::string friendlyName;
    };

    explicit Caster(
        Config config, DataReadyHandler dataReadyHandler = {},
        StateChangedHandler stateChangedHandler = {},
        AudioSourceNameChangedHandler audioSourceNameChangedHandler = {});
    ~Caster();

    static std::vector<VideoSourceProps> videoSources();
    static std::vector<AudioSourceProps> audioSources();

    void start(bool startPaused = false);
    void pause();
    void resume();
    inline State state() const { return m_state; }
    inline auto terminating() const { return state() == State::Terminating; }
    inline const Config &config() const { return m_config; }
    SensorDirection videoDirection() const;
    void setAudioVolume(float volume);
    inline void setStateChangedHandler(StateChangedHandler cb) {
        m_stateChangedHandler = std::move(cb);
    }

    inline void setDataReadyCallback(DataReadyHandler cb) {
        m_dataReadyHandler = std::move(cb);
    }

   private:
    enum class VideoSourceType {
        Unknown,
        V4l2,
        DroidCam,
        DroidCamRaw,
        X11Capture,
        LipstickCapture,
        Test
    };
    friend std::ostream &operator<<(std::ostream &os, VideoSourceType type);

    enum class AudioSourceType { Unknown, Mic, Monitor, Playback };
    friend std::ostream &operator<<(std::ostream &os, AudioSourceType type);

    enum class Endianness { Be, Le };
    friend std::ostream &operator<<(std::ostream &os, Endianness endian);

    enum class AudioEncoder { Aac, Mp3Lame };

    enum class VideoTrans {
        Off,
        Scale,
        Vflip,
        Hflip,
        VflipHflip,
        TransClock,
        TransClockFlip,
        TransCclock,
        TransCclockFlip,
        Frame169,
        Frame169Rot90,
        Frame169Rot180,
        Frame169Rot270,
        Frame169Vflip,
        Frame169VflipRot90,
        Frame169VflipRot180,
        Frame169VflipRot270,
    };
    friend std::ostream &operator<<(std::ostream &os, VideoTrans trans);

    enum class VideoScale { Off, Down25, Down50, Down75 };
    friend std::ostream &operator<<(std::ostream &os, VideoScale scale);

    struct FrameSpec {
        Dim dim;
        std::set<uint32_t, std::greater<uint32_t>> framerates;
    };

    struct VideoFormat {
        AVCodecID codecId = AV_CODEC_ID_NONE;
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    };
    friend std::ostream &operator<<(std::ostream &os, VideoFormat format);
    struct VideoFormatExt {
        AVCodecID codecId = AV_CODEC_ID_NONE;
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
        std::vector<FrameSpec> frameSpecs;
    };
    friend std::ostream &operator<<(std::ostream &os,
                                    const VideoFormatExt &format);

    struct VideoSourceInternalProps {
        std::string name;
        std::string dev;
        std::string friendlyName;
        std::vector<VideoFormatExt> formats;
        VideoOrientation orientation = VideoOrientation::Auto;
        VideoSourceType type = VideoSourceType::Unknown;
        SensorDirection sensorDirection = SensorDirection::Unknown;
        VideoTrans trans = VideoTrans::Off;
        VideoScale scale = VideoScale::Off;
    };
    friend std::ostream &operator<<(std::ostream &os,
                                    const VideoSourceInternalProps &props);

    struct V4l2H264EncoderProps {
        std::string dev;
        std::vector<VideoFormat> formats;
    };
    friend std::ostream &operator<<(std::ostream &os,
                                    const V4l2H264EncoderProps &props);

    struct AudioSourceInternalProps {
        std::string name;
        std::string dev;
        std::string friendlyName;
        AVCodecID codec = AV_CODEC_ID_NONE;
        uint8_t channels = 0;
        uint32_t rate = 0;
        size_t bps = 0;
        Endianness endian = Endianness::Be;
        AudioSourceType type = AudioSourceType::Unknown;
        bool muteSource = false;
    };
    friend std::ostream &operator<<(std::ostream &os,
                                    const AudioSourceInternalProps &props);

    struct PaClient {
        uint32_t idx = PA_INVALID_INDEX;
        std::string name;
        std::string bin;
    };
    friend std::ostream &operator<<(std::ostream &os, const PaClient &client);

    struct PaSinkInput {
        uint32_t idx = 0;
        std::string name;
        uint32_t clientIdx = PA_INVALID_INDEX;
        uint32_t sinkIdx = PA_INVALID_INDEX;
        bool muted = false;
        bool corked = false;
        bool removed = false;
    };
    friend std::ostream &operator<<(std::ostream &os, const PaSinkInput &input);

    using VideoPropsMap =
        std::unordered_map<std::string, VideoSourceInternalProps>;
    using AudioPropsMap =
        std::unordered_map<std::string, AudioSourceInternalProps>;
    using PaClientMap = std::unordered_map<uint32_t, PaClient>;
    using PaSinkInputMap = std::unordered_map<uint32_t, PaSinkInput>;

    struct AudioSourceSearchResult {
        bool done = false;
        int micCount = 0;
        int monitorCount = 0;
        AudioPropsMap propsMap;
    };

    struct FilterCtx {
        AVFilterInOut *out = nullptr;
        AVFilterInOut *in = nullptr;
        AVFilterContext *srcCtx = nullptr;
        AVFilterContext *sinkCtx = nullptr;
        AVFilterGraph *graph = nullptr;
    };

    static constexpr const unsigned int m_videoBufSize = 0x100000;
    static constexpr const unsigned int m_audioBufSize = 0x100000;
    static constexpr const uint64_t m_avMaxAnalyzeDuration =
        5000000;  // micro s
    static constexpr const int64_t m_avProbeSize = 5000;
    static const int m_maxIters = 100;

    /* pix fmts supported by most players */
    static constexpr const std::array nicePixfmts = {AV_PIX_FMT_YUV420P};

    Config m_config;
    DataReadyHandler m_dataReadyHandler;
    StateChangedHandler m_stateChangedHandler;
    AudioSourceNameChangedHandler m_audioSourceNameChangedHandler;
    DataBuffer m_videoBuf{m_videoBufSize, m_videoBufSize * 100};
    DataBuffer m_audioBuf{m_audioBufSize, m_audioBufSize * 100};
    std::mutex m_videoMtx;
    std::mutex m_muxMtx;
    std::mutex m_audioMtx;
    std::condition_variable m_videoCv;
    std::thread m_avMuxingThread;
    std::thread m_audioPaThread;
    AVFormatContext *m_outFormatCtx = nullptr;
    AVFormatContext *m_inVideoFormatCtx = nullptr;
    AVStream *m_outVideoStream = nullptr;
    AVStream *m_outAudioStream = nullptr;
    AVCodecContext *m_inAudioCtx = nullptr;
    AVCodecContext *m_outAudioCtx = nullptr;
    AVCodecContext *m_inVideoCtx = nullptr;
    AVCodecContext *m_outVideoCtx = nullptr;
    SwrContext *m_audioSwrCtx = nullptr;
    SwsContext *m_videoSwsCtx = nullptr;
    AVBSFContext *m_videoBsfExtractExtraCtx = nullptr;
    AVBSFContext *m_videoBsfDumpExtraCtx = nullptr;
    std::vector<uint8_t> m_pktSideData;
    std::unordered_map<VideoTrans, FilterCtx> m_videoFilterCtxMap;
    uint8_t *m_videoSwsBuf = nullptr;
    AVPacket *m_keyAudioPkt = nullptr;
    AVFrame *m_audioFrameIn = nullptr;
    AVFrame *m_audioFrameOut = nullptr;
    AVFrame *m_videoFrameIn = nullptr;
    AVFrame *m_videoFrameAfterSws = nullptr;
    AVFrame *m_videoFrameAfterFilter = nullptr;
    pa_mainloop *m_paLoop = nullptr;
    pa_stream *m_paStream = nullptr;
    pa_context *m_paCtx = nullptr;
    int64_t m_audioFrameDuration = 0;  // micro s
    int64_t m_audioPktDuration = 0;    // av time_base
    int m_audioFrameSize = 0;
    int m_videoFramerate = 0;
    int m_videoRawFrameSize = 0;
    int64_t m_nextVideoPts = 0;
    int64_t m_nextAudioPts = 0;
    int64_t m_videoTimeLastFrame = 0;       // micro s
    int64_t m_audioTimeLastFrame = 0;       // micro s
    int64_t m_videoFrameDuration = 0;       // micro s
    int64_t m_videoRealFrameDuration = 0;   // micro s
    bool m_muxedFlushed = false;
    bool m_videoFlushed = false;
    bool m_audioFlushed = false;
    Dim m_inDim;
    VideoTrans m_videoTrans = VideoTrans::Off;
    AVPixelFormat m_inPixfmt = AV_PIX_FMT_NONE;
    VideoPropsMap m_videoProps;
    AudioPropsMap m_audioProps;
    PaClientMap m_paClients;
    PaSinkInputMap m_paSinkInputs;
    uint32_t m_connectedPaSinkInput = PA_INVALID_INDEX;
    State m_state = State::Initing;
    std::optional<TestSource> m_imageProvider;
#ifdef USE_V4L2
    std::vector<V4l2H264EncoderProps> m_v4l2Encoders;
#endif
#ifdef USE_DROIDCAM
    std::optional<DroidCamSource> m_droidCamSource;
    std::optional<OrientationMonitor> m_orientationMonitor;
#endif
#ifdef USE_LIPSTICK_RECORDER
    std::optional<LipstickRecorderSource> m_lipstickRecorder;
#endif
    static std::string strForAvError(int err);
    static std::string strForAvOpts(const AVDictionary *opts);
    static std::string strForPaSubscriptionEvent(
        pa_subscription_event_type_t event);
    static int avReadPacketCallbackStatic(void *opaque, uint8_t *buf,
                                          int bufSize);
    static int avWritePacketCallbackStatic(void *opaque, uint8_t *buf,
                                           int bufSize);
    static void paStreamRequestCallbackStatic(pa_stream *stream, size_t nbytes,
                                              void *userdata);
    static bool paClientShouldBeIgnored(const pa_client_info *info);
    static void paSourceInfoCallback(pa_context *ctx,
                                     const pa_source_info *info, int eol,
                                     void *userdata);

    template <size_t SampleSize>
    void changeAudioVolume(Endianness endian, AVPacket *pkt) const {
        using Sample = std::conditional_t<
            SampleSize == 1, int8_t,
            std::conditional_t<SampleSize == 2, int16_t, int64_t>>;
        using Sample2x = std::conditional_t<
            SampleSize == 1, int16_t,
            std::conditional_t<SampleSize == 2, int32_t, int64_t>>;

        auto *buf = reinterpret_cast<Sample *>(pkt->data);

        auto buf_size = m_audioFrameSize / sizeof(Sample);
        for (auto i = 0U; i < buf_size; ++i) {
            if (SampleSize > 1 && endian == Endianness::Le) {
                auto *p = reinterpret_cast<int8_t *>(&buf[i]);

                std::reverse(p, p + SampleSize - 1);

                buf[i] = static_cast<Sample>(std::clamp<Sample2x>(
                    static_cast<Sample2x>(buf[i]) * m_config.audioVolume,
                    std::numeric_limits<Sample>::min(),
                    std::numeric_limits<Sample>::max()));

                std::reverse(p, p + SampleSize - 1);
            } else {
                buf[i] = static_cast<Sample>(std::clamp<Sample2x>(
                    static_cast<Sample2x>(buf[i]) * m_config.audioVolume,
                    std::numeric_limits<Sample>::min(),
                    std::numeric_limits<Sample>::max()));
            }
        }
    }

    int avReadPacketCallback(uint8_t *buf, int bufSize);
    int avWritePacketCallback(uint8_t *buf, int bufSize);
    void paStreamRequestCallback(pa_stream *stream, size_t nbytes);
    static void paClientInfoCallback(pa_context *ctx,
                                     const pa_client_info *info, int eol,
                                     void *userdata);
    static void paSinkInputInfoCallback(pa_context *ctx,
                                        const pa_sink_input_info *info, int eol,
                                        void *userdata);
    static void paSubscriptionCallback(pa_context *ctx,
                                       pa_subscription_event_type_t t,
                                       uint32_t idx, void *userdata);
    static void paStateCallback(pa_context *ctx, void *userdata);
    std::string paCorrectedClientName(uint32_t idx) const;
    void doPaTask();
    void initAvAudio();
    void initAvAudioDecoder();
    void initAvAudioEncoder();
    void initAvAudioResampler();
    void initAvVideoForGst();
    void initAvVideoEncoder();
    void initAvVideoInputRawFormat();
    void initAvVideoInputCompressedFormat();
    void initAvVideoEncoder(VideoEncoder type);
    void initAvVideoRawDecoder();
    void initAvVideoRawDecoderFromInputStream(int idx);
    void initAvVideoFilters();
    // void initAvVideoFiltersLipstickRecorder();
    void initAvVideoFiltersFrame169(SensorDirection direction);
    void initAvVideoFiltersFrame169Vflip(SensorDirection direction);
    void initAvVideoFilter(SensorDirection direction, VideoTrans trans,
                           const std::string &fmt);
    void initAvVideoFilter(FilterCtx &ctx, const char *arg);
    void initAvVideoOutStreamFromEncoder();
    void initAvVideoOutStreamFromInputFormat();
    void initAvVideoBsf();
    void allocAvOutputFormat();
    void initAvOutputFormat();
    void reinitAvOutputFormat();
    void initVideoTrans();
    void initAv();
    void initPa();
    void initAvAudioOutStream();
    int findAvVideoInputStreamIdx();
    void startAv();
    void startPa();
    void connectPaSource();
    void connectPaSinkInput();
    void disconnectPaSinkInput();
    void reconnectPaSinkInput();
    void startMuxing();
    void startVideoOnlyMuxing();
    void startAudioOnlyMuxing();
    void startVideoAudioMuxing();
    void startAudioSourceThread();
    bool muxVideo(AVPacket *pkt);
    bool muxAudio(AVPacket *pkt);
    bool readRawAudioPkt(AVPacket *pkt, int64_t now);
    void clean();
    void cleanAv();
    void cleanAvOutputFormat();
    void cleanPa();
    static void cleanAvOpts(AVDictionary **opts);
    void setVideoStreamRotation(VideoOrientation requestedOrientation);
    void updateVideoSampleStats(int64_t now);
    int64_t videoAudioDelay() const;
    int64_t videoDelay(int64_t now) const;
    int64_t audioDelay(int64_t now) const;
    void reportError();
    bool audioMuted() const;
    bool audioBoosted() const;
    void detectSources();
    static VideoPropsMap detectVideoSources();
    static AudioPropsMap detectPaSources();
    static AudioPropsMap detectAudioSources();
    void initAvAudioDurations();
    bool readVideoFrameFromBuf(AVPacket *pkt);
    void readNullFrame(AVPacket *pkt);
    void readVideoFrameFromDemuxer(AVPacket *pkt);
    bool encodeVideoFrame(AVPacket *pkt);
    bool filterVideoFrame(VideoTrans trans, AVFrame *frameIn,
                          AVFrame *frameOut);
    AVFrame *filterVideoIfNeeded(AVFrame *frameIn);
    void convertVideoFramePixfmt(AVFrame *frameIn, AVFrame *frameOut);
    bool configValid(const Config &config) const;
    inline bool audioEnabled() const { return !m_config.audioSource.empty(); }
    inline bool videoEnabled() const { return !m_config.videoSource.empty(); }
    inline const auto &videoProps() const {
        return m_videoProps.at(m_config.videoSource);
    }
    inline const auto &audioProps() const {
        return m_audioProps.at(m_config.audioSource);
    }
    static std::pair<std::reference_wrapper<const VideoFormatExt>,
                     AVPixelFormat>
    bestVideoFormat(const AVCodec *encoder,
                    const VideoSourceInternalProps &props, bool useNiceFormats);
    static AVSampleFormat bestAudioSampleFormat(
        const AVCodec *encoder, const AudioSourceInternalProps &props);
    static void setVideoEncoderOpts(VideoEncoder encoder, AVDictionary **opts);
    static void setAudioEncoderOpts(AudioEncoder encoder, AVDictionary **opts);
    static std::string videoEncoderAvName(VideoEncoder encoder);
    static std::string audioEncoderAvName(AudioEncoder encoder);
    static std::string videoFormatAvName(VideoSourceType type);
    static std::string streamFormatAvName(StreamFormat format);
    std::optional<std::reference_wrapper<PaSinkInput>> bestPaSinkInput();
    void mutePaSinkInput(PaSinkInput &si);
    void unmutePaSinkInput(PaSinkInput &si);
    void unmuteAllPaSinkInputs();
    void setState(State newState, bool notify = true);
    static VideoPropsMap detectTestVideoSources();
    void rawVideoDataReadyHandler(const uint8_t *data, size_t size);
    void compressedVideoDataReadyHandler(const uint8_t *data, size_t size);
    static Dim computeTransDim(Dim dim, VideoTrans trans, VideoScale scale);
    static uint32_t hash(std::string_view str);
    static bool nicePixfmt(AVPixelFormat fmt);
    static AVPixelFormat toNicePixfmt(AVPixelFormat fmt,
                                      const AVPixelFormat *supportedFmts);
    static bool videoPktOk(const AVPacket *pkt);
    void extractVideoExtradataFromCompressedDemuxer();
    void extractVideoExtradataFromRawDemuxer();
    void extractVideoExtradataFromRawBuf();
    bool extractExtradata(AVPacket *pkt);
    bool insertExtradata(AVPacket *pkt) const;
    static int orientationToRot(VideoOrientation orientation);
    static VideoTrans orientationToTrans(VideoOrientation orientation,
                                         const VideoSourceInternalProps &props);
    static VideoTrans transForYinverted(VideoTrans trans, bool yinverted);
#ifdef USE_X11CAPTURE
    static VideoPropsMap detectX11VideoSources();
#endif
#ifdef USE_DROIDCAM
    static VideoPropsMap detectDroidCamVideoSources();
#endif
#ifdef USE_V4L2
    static VideoPropsMap detectV4l2VideoSources();
    static void addV4l2VideoFormats(int fd, v4l2_buf_type type,
                                    std::vector<VideoFormat> &formats);
    static void addV4l2VideoFormatsExt(int fd, v4l2_buf_type type,
                                       std::vector<VideoFormatExt> &formats);
    void detectV4l2Encoders();
    static std::vector<FrameSpec> detectV4l2FrameSpecs(int fd,
                                                       uint32_t pixelformat);
    std::pair<std::reference_wrapper<const VideoFormatExt>, AVPixelFormat>
    bestVideoFormatForV4l2Encoder(const VideoSourceInternalProps &cam);
#endif
#ifdef USE_LIPSTICK_RECORDER
    static VideoPropsMap detectLipstickRecorderVideoSources();
#endif
};

#endif  // CASTER_H
