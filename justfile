alias bk := build_ksud
alias bm := build_manager

build_ksud:
    cross build --target aarch64-linux-android --release --manifest-path ./userspace/ksud/Cargo.toml

build_manager: build_ksud
    cp userspace/ksud/target/aarch64-linux-android/release/ksud manager/app/src/main/jniLibs/arm64-v8a/libksud.so
    cd manager && ./gradlew aDebug

clippy:
    cargo fmt --manifest-path ./userspace/ksud/Cargo.toml
    cross clippy --target x86_64-pc-windows-gnu --release --manifest-path ./userspace/ksud/Cargo.toml
    cross clippy --target aarch64-linux-android --release --manifest-path ./userspace/ksud/Cargo.toml
