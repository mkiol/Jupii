#!/usr/bin/bash

patch_file="$1"
py_dir="$2"
py_ver="$3"
out_file="$4"

if [ -f "${out_file}" ]; then
    echo "python modules already exist"
else
    rm -Rf "${py_dir}" \
    && pip3 install yt_dlp==2023.3.4 ytmusicapi==0.24.1 brotli==1.0.9 certifi==2022.12.7 charset-normalizer==2.1.1 idna==3.4 mutagen==1.46.0 pycryptodomex==3.16 requests==2.28.1 urllib3==1.26.13 websockets==10.4 --user \
    && mkdir -p "${py_dir}" \
    && mv "$HOME/.local/lib/python${py_ver}" "${py_dir}/" \
    && patch -u -b "${py_dir}/python${py_ver}/site-packages/yt_dlp/utils.py" -i "${patch_file}" \
    && cd "${py_dir}" \
    && tar cf - python${py_ver}/ | xz -z - > "${out_file}" \
    && echo "python modules created"
fi
