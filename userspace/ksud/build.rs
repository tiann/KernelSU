fn main() {
    cc::Build::new()
        .file("src/apk_sign.c")
        .compile("apk_sign");
}