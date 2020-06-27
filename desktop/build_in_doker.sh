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
  print "Builds $NAME in Docker container."
  print ""
  print "Options:"
  print "  -r <DIR>     root directory"
  print "  -d <DIR>     docker file directory"
  print "  -f <name>    docker file name"
  print "  -i <name>    docker image name"
  print "  -h           display this help and exit"
}

work_dir=$(pwd) # current working dir
script_name=$(basename "$0")

while getopts ":r:d:f:i:h" options; do
case "${options}" in
  r)
    ROOT_DIR="$(readlink -f "${OPTARG}")";;
  d)
    DOCKER_DIR="$(readlink -f "${OPTARG}")";;
  f)
    DOCKER_FILE="$(readlink -f "${OPTARG}")";;
  i)
    DOCKER_IMG="${OPTARG}";;
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
if [ -z "$DOCKER_DIR" ]; then
  exit_abnormal "Error: Docker directory was not set."
fi
if [ -z "$DOCKER_FILE" ]; then
  exit_abnormal "Error: Docker file name was not set."
fi
if [ -z "$DOCKER_IMG" ]; then
  exit_abnormal "Error: Docker image name was not set."
fi

print "Starting $script_name..."
print "Root directory: $ROOT_DIR"
print "Docker directory: $DOCKER_DIR"
print "Docker image name: $DOCKER_IMG"

if ! docker build -t $DOCKER_IMG -f $DOCKER_FILE $DOCKER_DIR; then
  exit_abnormal "Error. Cannot build Docker image."
fi
if ! docker run --name "$DOCKER_IMG-$NAME" --rm -ti -u "$(id -u):$(id -g)" -v $ROOT_DIR:/mnt $DOCKER_IMG /mnt/desktop/build.sh -r /mnt -b /mnt/build/$DOCKER_IMG; then
  exit_abnormal "Error. Cannot run Docker image."
fi

print "Done $script_name"
exit_normal
