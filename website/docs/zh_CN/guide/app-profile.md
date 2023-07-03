# App Profile

App Profile 是 KernelSU 提供的一种针对各种应用自定义其使用配置的机制。

对授予了 root 权限（也即可以使用 `su`）的应用来说，App Profile 也可以称之为 Root Profile，它可以自定义 `su` 的 `uid`, `gid`, `groups`, `capabilities` 以及 `SELinux` 规则，从而限制 root 用户的权限；比如可以针对防火墙应用仅授予网络权限，而不授予文件访问权限，针对冻结类应用仅授予 shell 权限而不是直接给 root；通过最小化权限原则**把权力关进笼子里**。

对于没有被授予 root 权限的普通应用，App Profile 可以控制内核以及模块系统对此应用的行为；比如是否需要针对此应用卸载模块造成的修改等。内核和模块系统可以通过此配置决定是否要做一些类似“隐藏痕迹”类的操作。

## Root Profile

### UID、GID 和 groups

Linux 系统中有用户和组两个概念。每个用户都有一个用户 ID(UID)，一个用户可以属于多个组，每个组也有组 ID(GID)。该 ID 用于识别系统的用户并确定用户可以访问哪些系统资源。

UID 为 0 的用户被称之为 root 用户，GID 为 0 的组被称之为 root 组；root 用户组通常拥有系统的最高权限。

对于 Android 系统来说，每一个 App 都是一个单独的用户（不考虑 share uid 的情况），拥有一个唯一的 UID。比如 `0` 是 root 用户，`1000` 是 `system`，`2000` 是 ADB shell，10000-19999 的是普通用户。

:::info
此处的 UID 跟 Android 系统的多用户，或者说工作资料（Work Profile），不是一个概念。工作资料实际上是对 UID 进行分片实现的，比如 10000-19999 是主用户，110000-119999 是工作资料；他们中的任何一个普通应用都拥有自己独有的 UID。
:::

每一个 App 可以有若干个组，GID 使其主要的组，通常与 UID 一致；其他的组被称之为补充组(groups)。某些权限是通过组控制的，比如网络访问，蓝牙等。

例如，如果我们在 ADB shell 中执行 `id` 命令，会得到如下输出：

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

其中，UID 为 `2000`，GID 也即主要组 ID 也为 `2000`；除此之外它还在很多补充组里面，例如 `inet` 组代表可以创建 `AF_INET` 和 `AF_INET6` 的 socket（访问网络），`sdcard_rw` 代表可以读写 sdcard 等。

KernelSU 的 Root Profile 可以自定义执行 `su` 后 root 进程的 UID, GID 和 groups。例如，你可以设置某个 root 应用的 Root Profile 其 UID 为 `2000`，这意味着此应用在使用 `su` 的时候，它的实际权限是 ADB Shell 级别；你可以去掉 groups 中的 `inet`，这样这个 `su` 就无法访问网络。

:::tip 注意
App Profile 仅仅是控制 root 应用使用 `su` 后的权限，它并非控制 App 本身的权限！如果 App 本身申请了网络访问权限，那么它即使不使用 `su` 也可以访问网络；为 `su` 去掉 `inet` 组仅仅是让 `su` 无法访问网络。
:::

与应用通过 `su` 主动切换用户或者组不同，Root Profile 是在内核中强制实施的，不依赖 root 应用的自觉行为，`su` 权限的授予完全取决于用户而非开发者。

### Capabilities

Capabilities 是 Linux 的一种分权机制。

传统的 UNIX 系统为了执行权限检查，将进程分为两类：特权进程（其有效用户 ID 为 0，称为超级用户或 root）和非特权进程（其有效 UID 为非零）。特权进程会绕过所有内核权限检查，而非特权进程则根据其凭据（通常是有效UID、有效GID和补充组列表）进行完整的权限检查。

从 Linux 2.2开始，Linux 将传统上与超级用户关联的特权分解为独立的单元，称为 Capabilities（有的也翻译为“权能”），它们可以独立启用和禁用。

每一个 Capability 代表一个或者一类权限。比如 `CAP_DAC_READ_SEARCH` 就代表是否有能力绕过文件读取权限检查和目录读取和执行权限检查。如果一个有效 UID 为 `0` 的用户（root 用户）没有 `CAP_DAC_READ_SEARCH` 或者更高 Capalities，这意味着即使它是 root 也不能随意读取文件。

KernelSU 的 Root Profile 可以自定义执行 `su` 后 root 进程的 Capabilities，从而实现只授予“部分 root 权限”。与上面介绍的 UID, GID 不同，某些 root 应用就是需要 `su` 后 UID 是 `0`，此时我们可以通过限制这个 UID 为 `0` 的 root 用户的 Capabilities，就可以限制它能够执行的操作。

:::tip 强烈建议
Linux 系统关于 Capability 的 [官方文档](https://man7.org/linux/man-pages/man7/capabilities.7.html)，解释了每一项 Capability 所代表的能力，写的非常详细，如果你想要自定义 Capabilities，请务必先阅读此文档。
:::

### SELinux

SELinux 是一种强大的强制性权限访问控制（MAC）机制。它按照**默认拒绝**的原则运行：任何未经明确允许的行为都会被拒绝。

SELinux 可按两种全局模式运行：

1. 宽容模式：权限拒绝事件会被记录下来，但不会被强制执行。
2. 强制模式：权限拒绝事件会被记录下来**并**强制执行。

:::warning 警告
现代的 Android 系统极度依赖 SELinux 来保障整个系统的安全性，我们强烈建议您不要使用任何以“宽容模式”运行的自定义系统，因为那样与裸奔没什么区别。
:::

SELinux 的完整概念比较复杂，我们这里不打算讲解它的具体工作方式，建议你先通过以下资料来了解其工作原理：

1. [wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Redhat: what-is-selinux](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU 的 Root Profile 可以自定义执行 `su` 后 root 进程的 SELinux context，并且可以针对这个 context 设置特定的访问控制规则，从而更加精细化地控制 root 权限。

通常情况下，应用执行 `su` 后，会将进程切换到一个 **不受任何限制** 的 SELinux 域，比如 `u:r:su:s0`，通过 Root Profile，我们可以将它切换到一个自定义的域，比如 `u:r:app1:s0`，然后为这个域制定一系列规则：

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

注意：此处的 `allow app1 * * *` 仅仅作为演示方便而使用，实际过程中不应使用这个规则，因为它跟 permissive 区别不大。

### 逃逸

如果 Root Profile 的配置不合理，那么可能会发生逃逸的情况：Root Profile 的限制会意外失效。

比如，如果你为 ADB shell 用户设置允许 root 权限（这是相当常见的情况）；然后你给某个普通应用允许 root 权限，但是配置它的 root profile 中的 UID 为 2000（ADB shell 用户的 UID）；那么此时，这个 App 可以通过执行两次 `su` 来获得完整的 root 权限：

1. 第一次执行 `su`，由于 App Profile 强制生效，会正常切换到 UID 为 `2000(adb shell)` 而非 `0(root)`。
2. 第二次执行 `su`，由于此时它 UID 是 `2000`，而你给 `2000(adb shell)` 配置了允许 root，它会获得完整的 root 权限！

:::warning 注意
这是完全符合预期的行为，并非 BUG！因此我们建议：

如果你的确需要给 adb 授予 root 权限（比如你是开发者），那么不建议你在配置 Root Profile 的时候将 UID 改成 `2000`，用 `1000(system)` 会更好。
:::

## Non Root Profile

### 卸载模块

KernelSU 提供了一种 systemless 的方式来修改系统分区，这是通过挂载 overlayfs 来实现的。但有些情况下，App 可能会对这种行为比较敏感；因此，我们可以通过设置“卸载模块”来卸载挂载在这些 App 上的模块。

另外，KernelSU 管理器的设置界面还提供了一个“默认卸载模块”的开关，这个开关默认情况下是**开启**的，这意味着**如果不对 App 做额外的设置**，默认情况下 KernelSU 或者某些模块会对此 App 执行卸载操作。当然，如果你不喜欢这个设置或者这个设置会影响某些 App，可以有如下选择：

1. 保持“默认卸载模块”的开关，然后针对不需要“卸载模块”的 App 进行单独的设置，在 App Profile 中关闭“卸载模块”；（相当于“白名单“）。
2. 关闭“默认卸载模块”的开关，然后针对需要“卸载模块”的 App 进行单独的设置，在 App Profile 中开启“卸载模块”；（相当于“黑名单“）。

:::info
KernelSU 在 5.10 及以上内核上，内核会执行“卸载模块”的操作；但在 5.10 以下的设备上，这个开关仅仅是一个“配置项”，KernelSU 本身不会做任何动作，一些模块（如 Zygisksu 会通过这个模块决定是否需要卸载）
:::
