[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | **Polski** | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_iw.md) | [हिंदी](README_IN.md)

# KernelSU

Rozwiązanie root oparte na jądrze dla urządzeń z systemem Android.

## Cechy

1. Oparte na jądrze `su` i zarządzanie dostępem roota.
2. System modułów oparty na overlayfs.

## Kompatybilność

KernelSU oficjalnie obsługuje urządzenia z Androidem GKI 2.0 (z jądrem 5.10+), starsze jądra (4.14+) są również kompatybilne, ale musisz sam skompilować jądro.

WSA i Android oparty na kontenerach również powinny działać ze zintegrowanym KernelSU.

Aktualnie obsługiwane ABI to : `arm64-v8a` i `x86_64`.

## Użycie

[Instalacja](https://kernelsu.org/guide/installation.html)

## Kompilacja

[Jak skompilować?](https://kernelsu.org/guide/how-to-build.html)

## Dyskusja

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Licencja

- Pliki w katalogu `kernel` są na licencji [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- Wszystkie inne części poza katalogiem `kernel` są na licencji [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Podziękowania

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): pomysłodawca KernelSU.
- [genuine](https://github.com/brevent/genuine/): walidacja podpisu apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): cenna znajomość rootkitów.
- [Magisk](https://github.com/topjohnwu/Magisk): implementacja sepolicy.
