# Как собрать KernelSU?

Прежде всего, необходимо ознакомиться с официальной документацией Android по сборке ядра:

1. [Сборка ядер](https://source.android.com/docs/setup/build/building-kernels)
2. [Сборки релизов GKI](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
Эта страница предназначена для устройств GKI, если вы используете старое ядро, пожалуйста, обратитесь к [Как интегрировать KernelSU для не GKI ядер?](how-to-integrate-for-non-gki).
:::

## Сборка ядра

### Синхронизация исходного кода ядра

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

Файл `<kernel_manifest.xml>` - это файл манифеста, который может однозначно определять сборку, с его помощью можно выполнить пересборку. Файл манифеста следует загрузить с сайта [Сборки релизов Google GKI](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

### Построение

Пожалуйста, сначала ознакомьтесь с [официальной документацией](https://source.android.com/docs/setup/build/building-kernels).

Например, нам необходимо собрать образ ядра aarch64:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Не забудьте добавить флаг `LTO=thin`, иначе сборка может завершиться неудачей, если память вашего компьютера меньше 24 Гб.

Начиная с Android 13, сборка ядра осуществляется с помощью `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Сборка ядра с помощью KernelSU

Если вы успешно собрали ядро, то собрать KernelSU очень просто, выберите любой запуск в корневом каталоге исходного кода ядра:

- Последний тэг(стабильный)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- Основная ветвь(разработка)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Выбранный тэг(Например, версия v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

А затем пересоберите ядро и получите образ ядра с KernelSU!
