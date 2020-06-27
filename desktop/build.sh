#!/bin/bash

NAME="Jupii"

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
  print "Builds $NAME binary."
  print ""
  print "Options:"
  print "  -r <DIR>     root directory"
  print "  -b <DIR>     build directory"
  print "  -h           display this help and exit"
}

work_dir=$(pwd) # current working dir
script_name=$(basename "$0")

while getopts ":r:b:h" options; do
case "${options}" in
  r)
    ROOT_DIR="$(readlink -f "${OPTARG}")";;
  b)
    BUILD_DIR="$(readlink -f "${OPTARG}")";;
  h)
    usage
    exit_normal;;
  :)
    exit_abnormal "Error: Option -$OPTARG requires an argument.";;
  ?)
    exit_abnormal "Error: Unknown option -$OPTARG.";;
  esac
done

if [ -z "$ROOT_DIR" ]; then
  exit_abnormal "Error: Root directory was not set."
fi
if [ -z "$BUILD_DIR" ]; then
  exit_abnormal "Error: Build directory was not set."
fi

PRO_FILE=$ROOT_DIR/desktop/jupii.pro

print "Starting $script_name..."
print "Root directory: $ROOT_DIR"
print "Build dir: $BUILD_DIR"
print "Project file: $PRO_FILE"

print "Building ffmpeg..."
if ! "$(dirname "$0")"/build_ffmpeg.sh -f -c -r "$ROOT_DIR"; then
  exit_abnormal "Error: Cannot build ffmpeg."
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR
if ! qmake-qt5 $PRO_FILE; then
  if ! qmake $PRO_FILE; then
    exit_abnormal "Error: qmake"
  fi
fi
if ! make; then
  exit_abnormal "Error: make"
fi

print "Done $script_name"
exit_normal
