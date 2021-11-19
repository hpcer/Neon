mkdir build
cd build

export ANDROID_ABI=arm64-v8a

BUILD_TYPE=Release

if [ -z "$ANDROID_NDK" ]; then
    echo "Error: Environment variable ANDROID_NDK is not set, use default path"
    export ANDROID_NDK=~/Library/Android/sdk/ndk/android-ndk-r20b
fi

if [ -d "$ANDROID_NDK" ]; then
    echo "Using Android NDK at $ANDROID_NDK"
fi

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_NDK=$ANDROID_NDK \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE 