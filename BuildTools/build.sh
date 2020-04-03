#!/bin/bash -e

if [ "$1" = "" ]; then
    echo "Provide at least one argument"
    exit 1
fi

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

if [ "$2" = "full" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_MAPPER=1 -DFONLINE_BUILD_BAKER=1"
    BUILD_DIR="build-$1-full"
elif [ "$2" = "server" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_SERVER=1"
    BUILD_DIR="build-$1-server"
elif [ "$2" = "mapper" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_MAPPER=1"
    BUILD_DIR="build-$1-mapper"
elif [ "$2" = "baker" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_BAKER=1"
    BUILD_DIR="build-$1-baker"
elif [ "$2" = "unit-tests" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_UNIT_TESTS=1"
    BUILD_DIR="build-$1-unit-tests"
elif [ "$2" = "code-coverage" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_CODE_COVERAGE=1"
    BUILD_DIR="build-$1-code-coverage"
else
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=1"
    BUILD_DIR="build-$1"
fi

CONFIG="Release"
if [ "$2" = "debug" ] || [ "$3" = "debug" ]; then
    CONFIG="Debug"
    BUILD_TARGETS="$BUILD_TARGETS -DFONLINE_DEBUG_BUILD=1"
    BUILD_DIR="$BUILD_DIR-debug"
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ "$1" = "win64" ] || [ "$1" = "win32" ] || [ "$1" = "uwp" ]; then
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    OUTPUT_PATH_WIN=`wsl_path_to_windows "$OUTPUT_PATH"`
    FO_CMAKE_CONTRIBUTION_WIN=`wsl_path_to_windows "$FO_CMAKE_CONTRIBUTION"`

    if [ "$1" = "win64" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION_WIN" "$FO_ROOT_WIN"
        cmake.exe --build . --config $CONFIG --parallel
    elif [ "$1" = "win32" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A Win32 -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION_WIN" "$FO_ROOT_WIN"
        cmake.exe --build . --config $CONFIG --parallel
    elif [ "$1" = "uwp" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A x64 -C "$FO_ROOT_WIN/BuildTools/uwp.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION_WIN" "$FO_ROOT_WIN"
        cmake.exe --build . --config $CONFIG --parallel
    fi

elif [ "$1" = "linux" ]; then
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config Release --parallel

elif [ "$1" = "web" ]; then
    source $FO_WORKSPACE/emsdk/emsdk_env.sh

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config Release --parallel

elif [ "$1" = "android" ] || [ "$1" = "android-arm64" ] || [ "$1" = "android-x86" ]; then
    if [ "$1" = "android" ]; then
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/android-arm-toolchain
        export ANDROID_ABI=armeabi-v7a
    elif [ "$1" = "android-arm64" ]; then
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/android-arm64-toolchain
        export ANDROID_ABI=arm64-v8a
    elif [ "$1" = "android-x86" ]; then
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/android-x86-toolchain
        export ANDROID_ABI=x86
    fi

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config Release --parallel

elif [ "$1" = "mac" ] || [ "$1" = "ios" ]; then
    if [ "$1" = "mac" ] && [ -d "$FO_WORKSPACE/osxcross" ]; then
        echo "OSXCross cross compilation"
        "$FO_WORKSPACE/osxcross/target/bin/x86_64-apple-darwin19-cmake" -G "Unix Makefiles" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
        cmake --build . --config Release --parallel

    elif [ "$1" = "ios" ] && [ -d "$FO_WORKSPACE/ios-toolchain" ]; then
        echo "iOS cross compilation"
        "$FO_WORKSPACE/ios-toolchain/target/bin/x86_64-apple-darwin19-cmake" -G "Unix Makefiles" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
        cmake --build . --config Release --parallel

    else
        if [ -x "$(command -v cmake)" ]; then
            CMAKE=cmake
        else
            CMAKE=/Applications/CMake.app/Contents/bin/cmake
        fi

        if [ "$1" = "mac" ]; then
            $CMAKE -G "Xcode" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
            $CMAKE --build . --config Release --parallel
        else
            $CMAKE -G "Xcode" -C "$FO_ROOT/BuildTools/ios.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGETS -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
            $CMAKE --build . --config Release --parallel
        fi
    fi
fi
