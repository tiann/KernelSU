# App Profile

The App Profile is a mechanism provided by KernelSU for customizing the configuration of various apps.

For apps granted root permissions (i.e., able to use `su`), the App Profile can also be referred to as the Root Profile. It allows customization of the `uid`, `gid`, `groups`, `capabilities`, and `SELinux` rules of the `su` command, thereby restricting the privileges of the root user. For example, it can grant network permissions only to firewall apps while denying file access permissions, or it can grant shell permissions instead of full root access for freeze apps: **keeping the power confined with the principle of least privilege.**

For ordinary apps without root permissions, the App Profile can control the behavior of the kernel and module system towards these apps. For instance, it can determine whether modifications resulting from modules should be addressed. The kernel and module system can make decisions based on this configuration, such as performing operations akin to "hiding".

## Root Profile

### UID, GID, and Groups

Linux systems have two concepts: users and groups. Each user has a user ID (UID), and a user can belong to multiple groups, each with its own group ID (GID). These IDs are used to identify users in the system and determine which system resources they can access.

Users with a UID of 0 are known as root users, and groups with a GID of 0 are known as root groups. The root user group generally has the highest system privileges.

In the case of the Android system, each app functions as a separate user (except in cases of shared UIDs) with a unique UID. For example, `0` represents the root user, `1000` represents `system`, `2000` represents the ADB shell, and UIDs ranging from `10000` to `19999` represent ordinary apps.

::: info
Here, the UID mentioned isn't the same as the concept of multiple users or work profiles in the Android system. Work profiles are actually implemented by partitioning the UID range. For example, 10000-19999 represents the main user, while 110000-119999 represents a work profile. Each ordinary app among them has its own unique UID.
:::

Each app can have several groups, with the GID representing the primary group, which usually matches the UID. Other groups are known as supplementary groups. Certain permissions are controlled through groups, such as network access permissions or Bluetooth access.

For example, if we execute the `id` command in ADB shell, the output might look like this:

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

Here, the UID is `2000`, and the GID (primary group ID) is also `2000`. Additionally, it belongs to several supplementary groups, such as `inet` (indicating the ability to create `AF_INET` and `AF_INET6` sockets) and `sdcard_rw` (indicating read/write permissions for the SD card).

KernelSU's Root Profile allows customization of the UID, GID, and groups for the root process after executing `su`. For example, the Root Profile of a root app can set its UID to `2000`, which means that when using `su`, the app's actual permissions are at the ADB shell level. Additionally, the `inet` group can be removed, preventing the `su` command from accessing the network.

::: tip NOTE
The App Profile only controls the permissions of the root process after using `su` and doesn't control the app's own permissions. If an app has requested network access permission, it can still access the network even without using `su`. Removing the `inet` group from `su` only prevents `su` from accessing the network.
:::

Root Profile is enforced in the kernel and doesn't rely on the voluntary behavior of root apps, unlike switching users or groups through `su`. Granting `su` permissions is entirely controlled by the user, not the developer.

### Capabilities

Capabilities are a mechanism for privilege separation in Linux.

For the purpose of performing permission checks, traditional `UNIX` implementations distinguish two categories of processes: privileged processes (whose effective user ID is `0`, referred to as superuser or root) and unprivileged processes (whose effective UID is nonzero). Privileged processes bypass all kernel permission checks, while unprivileged processes are subject to full permission checking based on the process's credentials (usually: effective UID, effective GID, and supplementary group list).

Starting with Linux 2.2, Linux divides the privileges traditionally associated with superuser into distinct units, known as capabilities, which can be independently enabled and disabled.

Each capability represents one or more privileges. For example, `CAP_DAC_READ_SEARCH` represents the ability to bypass permission checks for file reading, as well as directory read and execute permissions. If a user with an effective UID of `0` (root user) doesn't have the `CAP_DAC_READ_SEARCH` capability or higher, this means that even as root, they cannot freely read files.

KernelSU's Root Profile allows customization of the capabilities of the root process after executing `su`, thus granting partial "root privileges". Unlike the UID and GID mentioned above, certain root apps require a UID of `0` after using `su`. In such cases, limiting the capabilities of this root user with UID `0` can restrict the operations they're allowed to perform.

::: tip STRONG RECOMMENDATION
Linux's capability [official documentation](https://man7.org/linux/man-pages/man7/capabilities.7.html) provides detailed explanations of the abilities represented by each capability. If you intend to customize capabilities, it's strongly recommended that you read this document first.
:::

### SELinux

SELinux is a powerful Mandatory Access Control (MAC) mechanism. It operates on the principle of **default deny**. Any action not explicitly allowed is denied.

SELinux can run in two global modes:

1. Permissive mode: Denial events are logged, but not enforced.
2. Enforcing mode: Denial events are logged and enforced.

::: warning
Modern Android systems heavily rely on SELinux to ensure overall system security. It's highly recommended not to use any custom systems running in "Permissive mode" since it provides no significant advantages over a completely open system.
:::

Explaining the full concept of SELinux is complex and beyond the scope of this document. It's recommended to first understand how it works through the following resources:

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU's Root Profile allows customization of the SELinux context of the root process after executing `su`. Specific access control rules can be set for this context, enabling fine-grained control over root permissions.

In typical scenarios, when an app executes `su`, it switches the process to a SELinux domain with **unrestricted access**, such as `u:r:su:s0`. Through the Root Profile, this domain can be switched to a custom domain, such as `u:r:app1:s0`, and a series of rules can be defined for this domain:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Note that the `allow app1 * * *` rule is used for demonstration purposes only. In practice, this rule shouldn't be used extensively, as it isn't much different from Permissive mode.

### Escalation

If the configuration of the Root Profile isn't set properly, an escalation scenario may occur. The restrictions imposed by the Root Profile can unintentionally fail.

For example, if you grant root permission to an ADB shell user (which is a common case) and then grant root permission to a regular app, but configure its Root Profile with UID 2000 (which is the UID of the ADB shell user), the app can obtain full root access by executing the `su` command twice:

1. The first execution of `su` will be subject to the App Profile and will switch to UID `2000` (ADB shell) instead of `0` (root).
2. The second execution of `su`, since the UID is `2000` and root access has been granted to UID `2000` (ADB shell) in the configuration, the app will gain full root privileges.

::: warning NOTE
This behavior is fully expected and isn't a bug. Therefore, we recommend the following:

If you genuinely need to grant root permissions to ADB (e.g., as a developer), it isn't advisable to change the UID to `2000` when configuring the Root Profile. Using `1000` (system) would be a better choice.
:::

## Non-root profile

### Umount modules

KernelSU provides a systemless mechanism to modify system partitions, achieved through the mounting of OverlayFS. However, some apps may be sensitive to this behavior. In this case, we can unload modules mounted in these apps by setting the "Umount modules" option.

Additionally, the KernelSU manager's settings interface provides the "Umount modules by default". By default, this option is **enabled**, which means that KernelSU or some modules will unload modules for this app unless additional settings are applied. If you don't prefer this setting or if it affects certain apps, you have the following options:

1. Keep the "Umount modules by default" option enabled and individually disable the "Umount modules" option in the App Profile for apps requiring module loading (acting as a "whitelist").
2. Disable the "Umount modules by default" option and individually enable the "Umount modules" option in the App Profile for apps requiring module loading (acting as a "blacklist").

::: info
In devices running kernel version 5.10 and above, the kernel performs without any further action the unloading of modules. However, for devices running kernel versions below 5.10, this option is merely a configuration setting, and KernelSU itself doesn't take any action. If you want to use the "Umount modules" option in kernel versions before 5.10 you need to backport the `path_umount` function in `fs/namespace.c`. You can get more information at the end of the [Intergrate for non-GKI devices](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html#how-to-backport-path_umount) page. Some modules, such as Zygisksu, may also use this option to determine if module unloading is necessary.
:::
