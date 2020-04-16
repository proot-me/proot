#!/bin/sh
set -eu

# create directories
mkdir -p public/bin
mkdir -p public/reports
mkdir -p public/reports/lcov
mkdir -p public/reports/scan-build

# copy distributable artifacts
cp dist/* public/bin/

# copy redirect template
cp doc/template/redirect.html public/index.html

# copy reports
cp -R gcov-latest public/reports/
cp -R scan-build-latest public/reports/

# copy templates
cp public/index.html public/reports/scan-build/
cp public/index.html public/reports/lcov/

# insert job id
sed -i "s|{{ page.source_url }}|https://proot-me.github.io|g" public/index.html
sed -i "s|{{ page.source_url }}|https://proot.gitlab.io/-/proot/-/jobs/${CI_JOB_ID}/artifacts/public/reports/scan-build-latest/index.html|g" public/reports/scan-build/index.html
sed -i "s|{{ page.source_url }}|https://proot.gitlab.io/-/proot/-/jobs/${CI_JOB_ID}/artifacts/public/reports/gcov-latest/index.html|g" public/reports/lcov/index.html

