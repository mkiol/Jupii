#!/usr/bin/bash

name="$1"
prefix="$2"
shift 2
locales="$@"

echo "<RCC><qresource prefix=\"${prefix}\">"
for locale in ${locales}; do
    echo "<file>${name}-${locale}.qm</file>"
done
echo "</qresource></RCC>"
