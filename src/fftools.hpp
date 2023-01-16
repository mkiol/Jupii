// source copied from FFmpeg project

#ifndef FFTOOLS_H
#define FFTOOLS_H

#ifdef USE_V4L2
#include <linux/videodev2.h>
#endif
#include <pulse/pulseaudio.h>

#include <cstdint>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

namespace ff_tools {
#ifdef USE_V4L2
AVPixelFormat ff_fmt_v4l2ff(uint32_t v4l2_fmt, AVCodecID codec_id);
AVCodecID ff_fmt_v4l2codec(uint32_t v4l2_fmt);
#endif
#ifdef USE_X11CAPTURE
AVPixelFormat ff_fmt_x112ff(int bo, int depth, int bpp);
#endif
pa_sample_format_t ff_codec_id_to_pulse_format(AVCodecID codec_id);
AVCodecID ff_pulse_format_to_codec_id(pa_sample_format format);
}  // namespace ff_tools

#endif  // FFTOOLS_H
