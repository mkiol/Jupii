#!/usr/bin/bash

out_dir="$1"
out_file="$2"
xz_path="$3"

ytmusicapi_ver="1.8.2"

patch_ytmusicapi() {
    python3 --version | grep -q "3.8"
    if [ $? -ne 0 ]; then
        echo "ytmusicapi pathing not needed"
        return 0
    fi

    local py_files_to_patch=$(find . -maxdepth 5 -type f -name *.py)

    for file in $py_files_to_patch; do
        if ! grep -q 'from typing import' "$file"; then
            sed -i '1i from typing import List, Dict, Tuple, Set' "$file"
        fi
        sed -i 's/\blist\[/List[/g' "$file"
        sed -i 's/\bdict\[/Dict[/g' "$file"
        sed -i 's/\btuple\[/Tuple[/g' "$file"
        sed -i 's/\bset\[/Set[/g' "$file"
        sed -i '1i from __future__ import annotations' "$file"
    done
}

download_and_patch_ytmusicapi() {
    ytmusicapi_filename="ytmusicapi-${ytmusicapi_ver}-py3-none-any.whl"
    local ytmusicapi_url="https://files.pythonhosted.org/packages/7e/eb/23417507ba393545e98ba7c6d01e136cb3b9426d496a5eb216de7dc79adb/${ytmusicapi_filename}"
    local ytmusicapi_meta="ytmusicapi-${ytmusicapi_ver}.dist-info/METADATA"

    rm -rf ytmusicapi \
    && rm -rf ytmusicapi-${ytmusicapi_ver}.dist-info \
    && rm -f $ytmusicapi_filename \
    && curl -o $ytmusicapi_filename "$ytmusicapi_url" \
    && unzip $ytmusicapi_filename \
    && grep -v "Requires-Python" "$ytmusicapi_meta" > "${ytmusicapi_meta}.tmp" \
    && patch_ytmusicapi \
    && mv "${ytmusicapi_meta}.tmp" "$ytmusicapi_meta" \
    && rm $ytmusicapi_filename \
    && zip -r $ytmusicapi_filename ytmusicapi ytmusicapi-${ytmusicapi_ver}.dist-info \
    && rm -rf ytmusicapi \
    && rm -rf ytmusicapi-${ytmusicapi_ver}.dist-info

    return $?
}

if [ -f "${out_file}" ]; then
    echo "python module already exists"
else
    rm -Rf "${out_dir}" \
    && download_and_patch_ytmusicapi \
    && pip3 install --user \
        ./$ytmusicapi_filename \
        yt_dlp==2024.10.7 \
        brotli==1.0.9 \
        certifi==2022.12.7 \
        charset-normalizer==2.1.1 \
        idna==3.4 \
        mutagen==1.46.0 \
        pycryptodomex==3.17 \
        requests==2.32.3 \
        urllib3==1.26.17 \
        websockets==13.1 \
    && mkdir -p "${out_dir}" \
    && mv --no-target-directory "$(find "$HOME/.local/lib" -name "python*" | head -n 1)" "${out_dir}/python" \
    && strip -s $(find "${out_dir}" -type f -name "*.so*" | sed 's/.*/&/' | tr '\n' ' ') \
    && cd "${out_dir}" \
    && tar cf - python/ | "${xz_path}" -z -T 0 - > "${out_file}" \
    && echo "python module created"
fi
