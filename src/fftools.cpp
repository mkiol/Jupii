// source partially copied from FFmpeg project

#include "fftools.hpp"

#ifdef USE_X11CAPTURE
#include <X11/X.h>
#endif

namespace ff_tools {
#ifdef USE_V4L2
struct fmt_map {
    enum AVPixelFormat ff_fmt;
    enum AVCodecID codec_id;
    uint32_t v4l2_fmt;
};

static const struct fmt_map ff_fmt_conversion_table[] = {
    // ff_fmt              codec_id              v4l2_fmt
    {AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV420},
    {AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU420},
    {AV_PIX_FMT_YUV422P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV422P},
    {AV_PIX_FMT_YUYV422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUYV},
    {AV_PIX_FMT_UYVY422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_UYVY},
    {AV_PIX_FMT_YUV411P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV411P},
    {AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV410},
    {AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU410},
    {AV_PIX_FMT_RGB555LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555},
    {AV_PIX_FMT_RGB555BE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555X},
    {AV_PIX_FMT_RGB565LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565},
    {AV_PIX_FMT_RGB565BE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565X},
    {AV_PIX_FMT_BGR24, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR24},
    {AV_PIX_FMT_RGB24, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB24},
    {AV_PIX_FMT_BGR0, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_XBGR32},
    {AV_PIX_FMT_0RGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_XRGB32},
    {AV_PIX_FMT_BGRA, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_ABGR32},
    {AV_PIX_FMT_ARGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_ARGB32},
    {AV_PIX_FMT_BGR0, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR32},
    {AV_PIX_FMT_0RGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB32},
    {AV_PIX_FMT_GRAY8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_GREY},
    {AV_PIX_FMT_GRAY16LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_Y16},
    {AV_PIX_FMT_NV12, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_NV12},
    //    {AV_PIX_FMT_NONE, AV_CODEC_ID_MJPEG, V4L2_PIX_FMT_MJPEG},
    //    {AV_PIX_FMT_NONE, AV_CODEC_ID_MJPEG, V4L2_PIX_FMT_JPEG},
    {AV_PIX_FMT_NONE, AV_CODEC_ID_H264, V4L2_PIX_FMT_H264},
    //    {AV_PIX_FMT_NONE, AV_CODEC_ID_MPEG4, V4L2_PIX_FMT_MPEG4},
    //    {AV_PIX_FMT_NONE, AV_CODEC_ID_CPIA, V4L2_PIX_FMT_CPIA1},
    {AV_PIX_FMT_BAYER_BGGR8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SBGGR8},
    {AV_PIX_FMT_BAYER_GBRG8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SGBRG8},
    {AV_PIX_FMT_BAYER_GRBG8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SGRBG8},
    {AV_PIX_FMT_BAYER_RGGB8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SRGGB8},
    {AV_PIX_FMT_NONE, AV_CODEC_ID_NONE, 0},
};

AVPixelFormat ff_fmt_v4l2ff(uint32_t v4l2_fmt, AVCodecID codec_id) {
    int i;

    for (i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++) {
        if (ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt &&
            ff_fmt_conversion_table[i].codec_id == codec_id) {
            return ff_fmt_conversion_table[i].ff_fmt;
        }
    }

    return AV_PIX_FMT_NONE;
}

AVCodecID ff_fmt_v4l2codec(uint32_t v4l2_fmt) {
    int i;

    for (i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++) {
        if (ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt) {
            return ff_fmt_conversion_table[i].codec_id;
        }
    }

    return AV_CODEC_ID_NONE;
}
#endif  // USE_V4L2

#ifdef USE_X11CAPTURE
AVPixelFormat ff_fmt_x112ff(int bo, int depth, int bpp) {
    switch (depth) {
        case 32:
            if (bpp == 32)
                return bo == LSBFirst ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_0RGB;
            break;
        case 24:
            if (bpp == 32)
                return bo == LSBFirst ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_0RGB;
            else if (bpp == 24)
                return bo == LSBFirst ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24;
            break;
        case 16:
            if (bpp == 16)
                return bo == LSBFirst ? AV_PIX_FMT_RGB565LE
                                      : AV_PIX_FMT_RGB565BE;
            break;
        case 15:
            if (bpp == 16)
                return bo == LSBFirst ? AV_PIX_FMT_RGB555LE
                                      : AV_PIX_FMT_RGB555BE;
            break;
        case 8:
            if (bpp == 8) return AV_PIX_FMT_RGB8;
            break;
    }

    return AV_PIX_FMT_NONE;
}
#endif  // USE_X11CAPTURE

pa_sample_format_t ff_codec_id_to_pulse_format(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_PCM_U8:
            return PA_SAMPLE_U8;
        case AV_CODEC_ID_PCM_ALAW:
            return PA_SAMPLE_ALAW;
        case AV_CODEC_ID_PCM_MULAW:
            return PA_SAMPLE_ULAW;
        case AV_CODEC_ID_PCM_S16LE:
            return PA_SAMPLE_S16LE;
        case AV_CODEC_ID_PCM_S16BE:
            return PA_SAMPLE_S16BE;
        case AV_CODEC_ID_PCM_F32LE:
            return PA_SAMPLE_FLOAT32LE;
        case AV_CODEC_ID_PCM_F32BE:
            return PA_SAMPLE_FLOAT32BE;
        case AV_CODEC_ID_PCM_S32LE:
            return PA_SAMPLE_S32LE;
        case AV_CODEC_ID_PCM_S32BE:
            return PA_SAMPLE_S32BE;
        case AV_CODEC_ID_PCM_S24LE:
            return PA_SAMPLE_S24LE;
        case AV_CODEC_ID_PCM_S24BE:
            return PA_SAMPLE_S24BE;
        default:
            return PA_SAMPLE_INVALID;
    }
}

AVCodecID ff_pulse_format_to_codec_id(pa_sample_format format) {
    switch (format) {
        case PA_SAMPLE_U8:
            return AV_CODEC_ID_PCM_U8;
        case PA_SAMPLE_ALAW:
            return AV_CODEC_ID_PCM_ALAW;
        case PA_SAMPLE_ULAW:
            return AV_CODEC_ID_PCM_MULAW;
        case PA_SAMPLE_S16LE:
            return AV_CODEC_ID_PCM_S16LE;
        case PA_SAMPLE_S16BE:
            return AV_CODEC_ID_PCM_S16BE;
        case PA_SAMPLE_FLOAT32LE:
            return AV_CODEC_ID_PCM_F32LE;
        case PA_SAMPLE_FLOAT32BE:
            return AV_CODEC_ID_PCM_F32BE;
        case PA_SAMPLE_S32LE:
            return AV_CODEC_ID_PCM_S32LE;
        case PA_SAMPLE_S32BE:
            return AV_CODEC_ID_PCM_S32BE;
        case PA_SAMPLE_S24LE:
            return AV_CODEC_ID_PCM_S24LE;
        case PA_SAMPLE_S24BE:
            return AV_CODEC_ID_PCM_S24BE;
        default:
            return AV_CODEC_ID_NONE;
    }
}

}  // namespace ff_tools
