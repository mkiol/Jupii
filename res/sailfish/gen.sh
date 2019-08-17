#!/bin/bash

DEST_DIR=../../sailfish

if ! [ -x "$(command -v python)" ]; then
  echo 'Error: python is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v inkscape)" ]; then
  echo 'Error: inkscape is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v convert)" ]; then
  echo 'Error: convert is not installed.' >&2
  exit 1
fi

echo "Generating images..."
python gen_png.py

echo "Generating icons..."
python gen_png_app.py

echo "Generating upnp icons..."
python gen_png_upnp.py

echo "Deleting old images..."
rm -R $DEST_DIR/images

echo "Coping images to dest dir..."
cp -R images $DEST_DIR

echo "Deleting old icons..."
rm -R $DEST_DIR/icons

echo "Coping icons to dest dir..."
cp -R icons $DEST_DIR
