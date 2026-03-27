# shellcheck shell=bash
TRIPLE=$1
ANDROID_SDK_LEVEL=$2
LLVM_PATH="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
LLVM_BIN="$LLVM_PATH/bin"
CLANG_PATH="$LLVM_BIN/${TRIPLE}${ANDROID_SDK_LEVEL}-clang"
UTRIPLE="$(echo $TRIPLE | sed 's/-/_/g')"
UUTRIPLE="$(echo $UTRIPLE | tr a-z A-Z)"

export CC_$UTRIPLE="$CLANG_PATH"
export CXX_$UTRIPLE="${CLANG_PATH}++"
export AR_$UTRIPLE="$LLVM_BIN/llvm-ar"
export CARGO_TARGET_${UUTRIPLE}_LINKER="$CLANG_PATH"
export BINDGEN_EXTRA_CLANG_ARGS_$UTRIPLE="--sysroot=$LLVM_PATH/sysroot -I$LLVM_PATH/sysroot/usr/include/$TRIPLE"
