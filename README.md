# SygiSU

SygiSU is a fork of [KernelSU](https://github.com/tiann/KernelSU), a kernel-based root solution for
Android. It adds a redesigned manager app and importable theme packs.

- One SygiSU UI (the Miuix and Material modes were merged into one).
- Floating pill navigation and a Shizuku-Next / ColorOS-style look.
- **Themes**: import a `.zip` to change colors, fonts, icons, shapes and sizes. See
  [docs/SYGISU_THEMES.md](docs/SYGISU_THEMES.md).

## Credits and license

SygiSU is based on KernelSU by tiann and contributors and is not affiliated with or endorsed by the
upstream project. It stays under GPLv2; upstream copyright notices are kept.

## Important: kernel and manager signature

KernelSU's kernel only accepts a manager app signed with a key it was built to trust. A SygiSU
manager signed with your key works only with kernels built from this repo with your key's size and
hash configured (see upstream KernelSU docs for `KSU_EXPECTED_SIZE` / `KSU_EXPECTED_HASH`).
