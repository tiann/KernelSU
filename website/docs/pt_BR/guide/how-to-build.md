# Como compilar o KernelSU?

Primeiro, você deve ler a documentação oficial do Android para compilação do kernel:

1. [Como criar kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [Builds de versão de imagem genérica do kernel (GKI)](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning AVISO
Esta página é para dispositivos GKI, se você usa um kernel antigo, consulte [Como integrar o KernelSU para kernels não GKI](how-to-integrate-for-non-gki).
:::

## Compilar o kernel

### Sincronize o código-fonte do kernel

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

O `<kernel_manifest.xml>` é um arquivo de manifesto que pode determinar uma compilação exclusivamente, você pode usar o manifesto para fazer uma compilação re-preduzível. Você deve baixar o arquivo de manifesto em [Builds de versão de imagem genérica do kernel (GKI)](https://source.android.com/docs/core/architecture/kernel/gki-release-builds).

### Construir

Por favor, verifique [Como criar kernels](https://source.android.com/docs/setup/build/building-kernels) primeiro.

Por exemplo, para compilar uma imagem de kernel `aarch64`:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Não se esqueça de adicionar o sinalizador `LTO=thin`, caso contrário a compilação poderá falhar se a memória do seu computador for inferior a 24 GB.

A partir do Android 13, o kernel é compilado pelo `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

:::info INFORMAÇÕES
Para alguns kernel do Android 14, para fazer o Wi-Fi/Bluetooth funcionar, pode ser necessário remover todas as exportações protegidas pelo GKI:

```sh
rm common/android/abi_gki_protected_exports_*
```
:::

## Compilar o kernel com KernelSU

Se você conseguir compilar o kernel com sucesso, adicionar suporte ao KernelSU a ele será relativamente fácil. Na raiz do diretório de origem do kernel, execute qualquer uma das opções listadas abaixo:

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
