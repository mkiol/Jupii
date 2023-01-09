#!/usr/bin/bash

patch_file="$1"
py_dir="$2"
py_ver="$3"
out_file="$4"

if [ -f "${out_file}" ]; then
    echo "python modules already exist"
else
    rm -Rf "${py_dir}" \
    && pip3 install yt_dlp ytmusicapi --user \
    && mkdir -p "${py_dir}" \
    && mv "$HOME/.local/lib/python${py_ver}" "${py_dir}/" \
    && patch -u -b "${py_dir}/python${py_ver}/site-packages/yt_dlp/utils.py" -i "${patch_file}" \
    && cd "${py_dir}" \
    && tar cf - python${py_ver}/ | xz -z - > "${out_file}" \
    && echo "python modules created"
fi
