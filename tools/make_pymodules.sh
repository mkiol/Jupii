#!/usr/bin/bash

out_dir="$1"
out_file="$2"
xz_path="$3"

if [ -f "${out_file}" ]; then
    echo "python module already exists"
else
    rm -Rf "${out_dir}" \
    && pip3 install --user \
        yt_dlp==2023.12.30 \
        ytmusicapi==1.5.0 \
        brotli==1.0.9 \
        certifi==2022.12.7 \
        charset-normalizer==2.1.1 \
        idna==3.4 \
        mutagen==1.46.0 \
        pycryptodomex==3.17 \
        requests==2.31.0 \
        urllib3==1.26.17 \
        websockets==12.0 \
    && mkdir -p "${out_dir}" \
    && mv --no-target-directory "$(find "$HOME/.local/lib" -name "python*" | head -n 1)" "${out_dir}/python" \
    && strip -s $(find "${out_dir}" -type f -name "*.so*" | sed 's/.*/&/' | tr '\n' ' ') \
    && cd "${out_dir}" \
    && tar cf - python/ | "${xz_path}" -z -T 0 - > "${out_file}" \
    && echo "python module created"
fi
