#!/usr/bin/env bash
set -e

SCRIPT_NAME=$(basename $0)
NGX_WASM_DIR=${NGX_WASM_DIR:-"$(
    cd $(dirname $(dirname $(dirname ${0})))
    pwd -P
)"}
if [[ ! -f "${NGX_WASM_DIR}/util/_lib.sh" ]]; then
    echo "Unable to source util/_lib.sh" >&2
    exit 1
fi

source $NGX_WASM_DIR/util/_lib.sh

###############################################################################

build_cwabt() {
    local target="$1"

    notice "building lib/cwabt..."

    cd $NGX_WASM_DIR/lib/cwabt
    make
    make install TARGET="$target"
}

check_libwee8_build_dependencies() {
    # Running as root causes issues when the V8 build process uses tar to
    # extract archives: tar sets file permissions differently when run as root.
    if ! [ -n "$ACT" ]; then
        [ "$UID" = 0 ] && {
            fatal "must not run as root."
        }
    fi

    python3 --help >/dev/null 2>/dev/null || {
        fatal "python3 is required in your path."
    }

    git --help >/dev/null 2>/dev/null || {
        fatal "git is required in your path."
    }

    xz --help >/dev/null 2>/dev/null || {
        fatal "xz (from xz-utils) is required in your path."
    }

    pkg-config --help >/dev/null 2>/dev/null || {
        fatal "pkg-config is required in your path."
    }

    curl --help >/dev/null 2>/dev/null || {
        fatal "curl is required in your path."
    }

    ninja --help >/dev/null 2>/dev/null
    [ "$?" = 1 ] || {
        fatal "ninja (from ninja-build) required in your path."
    }
}

build_libwee8() {
    local target="$1"

    notice "building libwee8..."

    check_libwee8_build_dependencies || exit 1

    mkdir -p "$DIR_LIBWEE8"
    cd "$DIR_LIBWEE8"

    mkdir -p tools

    if [ ! -e tools/depot_tools ]; then
        cd tools
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
        cd ..
    fi

    export PATH="$DIR_LIBWEE8/tools/depot_tools:$PATH"

    if [ -n "$ACT" ]; then
        local tar="$DIR_LIBWEE8/tools/depot_tools/tar"
        echo "#!/bin/sh" > "$tar"
        echo "exec $(which tar)" '"$@"' "--no-same-owner --no-same-permissions" >> "$tar"
        chmod +x "$tar"
    fi

    ### fetch

    mkdir -p repos

    if [ ! -e repos/v8 ]; then
        cd repos
        notice "fetching V8 repository..."
        fetch v8
        cd ..
    fi

    cd repos/v8

    git checkout .
    git checkout "$V8_VER"

    notice "synchronizing V8 repository..."
    gclient sync

    ### build

    local build_mode="$V8_PLATFORM.release.sample"

    notice "generating V8 build files..."
    tools/dev/v8gen.py "$build_mode" -vv -- use_custom_libcxx=false

    notice "building V8..."
    ninja -C out.gn/"$build_mode" wee8

    ### install to target

    mkdir -p "$target/lib"
    cp out.gn/"$build_mode"/obj/libwee8.a "$target/lib"

    mkdir -p "$target/include"
    cp ./third_party/wasm-api/wasm.h "$target/include"
}

check_cached_v8() {
    local target="$1"
    local cachedir="$2"

    if [[ -e "$cachedir/wasm.h" ]]; then
        notice "cache exists in $cachedir - using it..."
        mkdir -p "$target/include"
        mkdir -p "$target/lib"
        cp -av "$cachedir/wasm.h" "$target/include"
        cp -av "$cachedir/libwee8.a" "$target/lib"
        cp -av "$cachedir/cwabt.h" "$target/include"
        cp -av "$cachedir/libcwabt.a" "$target/lib"
        return 0
    fi

    return 1
}

cache_v8() {
    local target="$1"
    local cachedir="$2"

    notice "caching built assets in $cachedir..."
    mkdir -p "$cachedir"
    cp -av "$target/include/wasm.h" "$cachedir"
    cp -av "$target/lib/libwee8.a" "$cachedir"
    cp -av "$target/include/cwabt.h" "$cachedir"
    cp -av "$target/lib/libcwabt.a" "$cachedir"
}

build_v8() {
    local target="$1"
    local cachedir="$2"

    if check_cached_v8 "$target" "$cachedir"; then
        return 0
    fi

    build_cwabt "$target"
    build_libwee8 "$target" "$V8_VER"

    cache_v8 "$target" "$cachedir"
}

###############################################################################

get_from_release() {
    local var_name="$1"
    local release_file="$NGX_WASM_DIR/.github/workflows/release.yml"

    awk '/'$var_name':/ { print $2 }' "$release_file"
}

V8_PLATFORM="${V8_PLATFORM:-x64}"
V8_VER="${V8_VER:-$(get_from_release V8_VER)}"
target="${1:-$DIR_WORK}"
cachedir="${2:-$DIR_DOWNLOAD/v8-$V8_VER}"

build_v8 "$target" "$cachedir"