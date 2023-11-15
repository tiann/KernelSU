# Como construir o KernelSU?

Primeiro, você deve ler a documentação oficial do Android para construção do kernel:

1. [Como criar kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [Builds de versão de imagem genérica do kernel (GKI)](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning AVISO
Esta página é para dispositivos GKI, se você usa um kernel antigo, consulte [Como integrar o KernelSU para kernels não GKI](how-to-integrate-for-non-gki).
:::

## Construir o kernel

### Sincronize o código-fonte do kernel

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

O `<kernel_manifest.xml>` é um arquivo de manifesto que pode determinar uma construção exclusivamente, você pode usar o manifesto para fazer uma construção re-preduzível. Você deve baixar o arquivo de manifesto em [compilações de lançamento do Google GKI](https://source.android.com/docs/core/architecture/kernel/gki-release-builds).

### Construir

Por favor, verifique [Como criar kernels](https://source.android.com/docs/setup/build/building-kernels) primeiro.

Por exemplo, precisamos construir a imagem do kernel `aarch64`:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Não se esqueça de adicionar o sinalizador `LTO=thin`, caso contrário a compilação poderá falhar se a memória do seu computador for inferior a 24 GB.

A partir do Android 13, o kernel é construído pelo `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Construir o kernel com KernelSU

Se você conseguir construir o kernel com sucesso, então construir o KernelSU é muito fácil. Selecione qualquer um executado no diretório raiz de origem do kernel:

::: code-group

```sh[Tag mais recente (estável)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

```sh[Branch principal (dev)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

```sh[Selecionar tag (como v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

E então reconstrua o kernel e você obterá uma imagem do kernel com KernelSU!
