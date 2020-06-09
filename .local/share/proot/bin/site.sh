#!/bin/sh
set -eu

# configure git
git config --global user.name "PRoot"
git config --global user.email "proot_me@googlegroups.com"

# clone site repository
git clone "https://${GITHUB_USER-}:${GITHUB_ACCESS_TOKEN-}@github.com/proot-me/proot-me.github.io" doc/public_html

# compile site from documentation
make -eC doc dist

# navigate to output directory
cd doc/public_html

# add any changes
git add .

# commit changes
git commit -m "publishing changes to GitHub Pages"

# push to github
git push
