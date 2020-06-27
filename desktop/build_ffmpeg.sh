#!/bin/bash

DEBUG=0
CLEAN=0

# -----------------------

work_dir="$(pwd)"
script_name=$(basename "$0")

print() {
  echo "$1"
}

print_error() {
  echo "$1" 1>&2
}

exit_abnormal() {
  local txt="$1"
  if [ -n "$txt" ]; then
    print_error "$1"
  fi
  cd "$work_dir"
  exit 1
}

exit_normal() {
  cd "$work_dir"
  exit 0
}

usage() {
  print "Usage: $script_name [OPTION]..."
  print "Build static ffmpeg libs."
  print ""
  print "Options:"
  print "  -r <DIR>     root directory"
  print "  -d           enable debug build"
  print "  -f           build when ffmpeg libs already exist"
  print "  -c           clean before build"
  print "  -h           display this help and exit"
}

set_arch() {
  local cpu_arch=$(uname -m)
  
  if [ $cpu_arch == "aarch64" ] || [[ $cpu_arch == *arm* ]]; then
    arch=armv7hl
  elif [[ $cpu_arch == i?86 ]]; then
    arch=i486
  elif [ $cpu_arch == "x86_64" ]; then
    arch=amd64
  fi
  
  if [ -z $arch ]; then
    exit_abnormal "Error: Unknown CPU architecture."
  fi
}

while getopts ":dcfhr:" options; do
case "${options}" in
  r)
    ROOT_DIR="$(readlink -f "${OPTARG}")";;
  d)
    DEBUG=1;;
  c)
    CLEAN=1;;
  f)
    FORCE=1;;
  h)
    usage
    exit_normal;;
  :)
    exit_abnormal "Error: Option -$OPTARG requires an argument.";;
  ?)
    exit_abnormal "Error: Unknown option -$OPTARG.";;
  esac
done


if [ -z $ROOT_DIR ]; then
  exit_abnormal "Error: Root directory was not set."
fi

archive_file="$ROOT_DIR/libs/ffmpeg/ffmpeg-4.1.3.tar.bz2"
out_dir="$ROOT_DIR/libs/ffmpeg/build"
archive_name="$(basename "$archive_file")"
src_dir="$ROOT_DIR/libs/ffmpeg/${archive_name%.tar*}"

print "Root directory: $ROOT_DIR"
print "Output directory: $out_dir"
print "Source code archive file: $archive_file"
print "Source code dirrectory: $src_dir"

# -----------------------

set_arch

if [ $FORCE -ne 1 ] && [ -d "$out_dir/$arch" ]; then
  print "Directory $out_dir/$arch exists, so assuming that ffmpeg is already built."
  print "Done $script_name"
  exit_normal
fi

if [ ! -d "$src_dir" ]; then
  print "Extracting sources from $archive_file..."
  
  cd "$(dirname "$archive_file")"

  if ! tar -xvf "$archive_file"; then
    exit_abnormal "Error: Cannot extract $archive_file."
  fi
  
  cd "$work_dir"
fi

cd "$src_dir"

if [ $CLEAN == 1 ]; then
  if ! make clean; then
    exit_abnormal "Error: Cannot clean."
  fi
fi

if [ $DEBUG == 1 ]; then
  print "Debug build is enabled."
  debug_option=--enable-debug
else
  debug_option=--disable-debug
fi

if ! ./configure --disable-programs --disable-doc --disable-everything --enable-pic --enable-protocol=file --enable-encoder=libx264 --enable-encoder=aac --enable-decoder=rawvideo --enable-muxer=mp4 --enable-parser=h264 --disable-x86asm --enable-nonfree --enable-encoder=libx264rgb --enable-indev=xcbgrab --enable-rpath --enable-gpl --enable-libx264 --enable-muxer=mpegts --enable-demuxer=aac --enable-demuxer=avi --enable-demuxer=h264 --enable-demuxer=m4v --enable-demuxer=mov --enable-demuxer=ogg --enable-demuxer=mpegvideo --enable-demuxer=matroska  --enable-demuxer=wav --enable-decoder=pcm_u8 --enable-decoder=pcm_u32le --enable-decoder=pcm_u32be --enable-decoder=pcm_u24le --enable-decoder=pcm_u24be --enable-decoder=pcm_u16le --enable-decoder=pcm_u16be --enable-decoder=pcm_s8 --enable-decoder=pcm_s32le --enable-decoder=pcm_s32be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24be --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=pcm_f64le --enable-decoder=pcm_f64be --enable-decoder=pcm_f32le --enable-decoder=pcm_f32be --enable-demuxer=pcm_u32be --enable-demuxer=pcm_u32le --enable-demuxer=pcm_u8 --enable-demuxer=pcm_alaw --enable-demuxer=pcm_f32be --enable-demuxer=pcm_f32le --enable-demuxer=pcm_f64be --enable-demuxer=pcm_f64le --enable-demuxer=pcm_s16be --enable-demuxer=pcm_s16le --enable-demuxer=pcm_s24be --enable-demuxer=pcm_s24le  --enable-demuxer=pcm_s32be --enable-demuxer=pcm_s32le --enable-demuxer=pcm_s8 --enable-demuxer=pcm_u16be --enable-demuxer=pcm_u16le --enable-demuxer=pcm_u24be --enable-demuxer=pcm_u24le --enable-libmp3lame --enable-encoder=libmp3lame --enable-muxer=mp3 --enable-static --disable-shared $debug_option; then
  exit_abnormal "Error: Cannot configure."
fi

if ! make; then
  exit_abnormal "Error: Cannot make."
fi

mkdir -p "$out_dir/$arch"

cp --dereference -f "./libavcodec/libavcodec.a" "$out_dir/$arch/libavcodec.a"
cp --dereference -f "./libavformat/libavformat.a" "$out_dir/$arch/libavformat.a"
cp --dereference -f "./libavutil/libavutil.a" "$out_dir/$arch/libavutil.a"
cp --dereference -f "./libswresample/libswresample.a" "$out_dir/$arch/libswresample.a"
cp --dereference -f "./libswscale/libswscale.a" "$out_dir/$arch/libswscale.a"
cp --dereference -f "./libavdevice/libavdevice.a" "$out_dir/$arch/libavdevice.a"

print "Done $script_name"
exit_normal
