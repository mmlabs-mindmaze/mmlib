#!/bin/sh -e

# Go the the package root folder (the one where this script is located)
cd $(dirname $0)

# Generate the build scripts
autoreconf -fi

