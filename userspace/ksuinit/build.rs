fn main() {
    // Fix getauxval linking issue for aarch64-unknown-linux-musl
    // The compiler_builtins crate needs getauxval from libc, but due to link order
    // issues, we need to link libc again at the end
    let target = std::env::var("TARGET").unwrap();

    if target == "aarch64-unknown-linux-musl" || target == "x86_64-unknown-linux-musl" {
        // Link libc at the end to resolve symbols from compiler_builtins
        println!("cargo:rustc-link-arg=-lc");
    }
}
