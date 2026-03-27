alias bk := build_ksud
alias bm := build_manager

build_ksud:
    cross build --target aarch64-linux-android --release

build_manager: build_ksud
    cp target/aarch64-linux-android/release/ksud manager/app/src/main/jniLibs/arm64-v8a/libksud.so
    cd manager && ./gradlew aDebug

clippy:
    cargo fmt
    cross clippy --target aarch64-linux-android --release
