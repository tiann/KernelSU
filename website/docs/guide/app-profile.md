# App Profile

The App Profile is a mechanism provided by KernelSU for customizing the configuration of various applications.

For applications granted root permissions (i.e., able to use `su`), the App Profile can also be referred to as the Root Profile. It allows customization of the `uid`, `gid`, `groups`, `capabilities`, and `SELinux` rules of the `su` command, thereby restricting the privileges of the root user. For example, it can grant network permissions only to firewall applications while denying file access permissions, or it can grant shell permissions instead of full root access for freeze applications: **keeping the power confined with the principle of least privilege.**

For ordinary applications without root permissions, the App Profile can control the behavior of the kernel and module system towards these applications. For instance, it can determine whether modifications resulting from modules should be addressed. The kernel and module system can make decisions based on this configuration, such as performing operations akin to "hiding"

## Root Profile

### UID, GID, and Groups

Linux systems have two concepts: users and groups. Each user has a user ID (UID), and a user can belong to multiple groups, each with its own group ID (GID). These IDs are used to identify users in the system and determine which system resources they can access.

Users with a UID of 0 are known as root users, and groups with a GID of 0 are known as root groups. The root user group typically holds the highest system privileges.

In the case of the Android system, each app is a separate user (excluding shared UID scenarios) with a unique UID. For example, `0` represents the root user, `1000` represents `system`, `2000` represents the ADB shell, and UIDs ranging from 10000 to 19999 represent ordinary apps.

:::info
Here, the UID mentioned is not the same as the concept of multiple users or work profiles in the Android system. Work profiles are actually implemented by partitioning the UID range. For example, 10000-19999 represents the main user, while 110000-119999 represents a work profile. Each ordinary app among them has its own unique UID.
:::

Each app can have several groups, with the GID representing the primary group, which usually matches the UID. Other groups are known as supplementary groups. Certain permissions are controlled through groups, such as network access permissions or Bluetooth access.

For example, if we execute the `id` command in ADB shell, the output might look like this:

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

Here, the UID is `2000`, and the GID (primary group ID) is also `2000`. Additionally, it belongs to several supplementary groups, such as `inet` (indicating the ability to create `AF_INET` and `AF_INET6` sockets) and `sdcard_rw` (indicating read/write permissions for the SD card).

KernelSU's Root Profile allows customization of the UID, GID, and groups for the root process after executing `su`. For example, the Root Profile of a root app can set its UID to `2000`, which means that when using `su`, the app's actual permissions are at the ADB shell level. The `inet` group can be removed, preventing the `su` command from accessing the network.

:::tip Note
The App Profile only controls the permissions of the root process after using `su`; it does not control the permissions of the app itself. If an app has requested network access permission, it can still access the network even without using `su`. Removing the `inet` group from `su` only prevents `su` from accessing the network.
:::

Root Profile is enforced in the kernel and does not rely on the voluntary behavior of root applications, unlike switching users or groups through `su`, the granting of `su` permission is entirely up to the user rather than the developer.

### Capabilities

Capabilities are a mechanism for privilege separation in Linux.

For the purpose of performing permission checks, traditional UNIX implementations distinguish two categories of processes: privileged processes (whose effective user ID is 0, referred to as superuser or root), and unprivileged processes (whose effective UID is nonzero).  Privileged processes bypass all kernel permission checks, while unprivileged processes are subject to full permission checking based on the process's credentials (usually: effective UID, effective GID, and supplementary group list).

Starting with Linux 2.2, Linux divides the privileges traditionally associated with superuser into distinct units, known as capabilities, which can be independently enabled and disabled.

Each Capability represents one or more privileges. For example, `CAP_DAC_READ_SEARCH` represents the ability to bypass permission checks for file reading, as well as directory reading and execution permissions. If a user with an effective UID of `0` (root user) lacks `CAP_DAC_READ_SEARCH` or higher capabilities, this means that even though they are root, they cannot read files at will.

KernelSU's Root Profile allows customization of the Capabilities of the root process after executing `su`, thereby achieving partially granting "root permissions." Unlike the aforementioned UID and GID, certain root apps require a UID of `0` after using `su`. In such cases, limiting the Capabilities of this root user with UID `0` can restrict their allowed operations.

:::tip Strong Recommendation
Linux's Capability [official documentation](https://man7.org/linux/man-pages/man7/capabilities.7.html) provides detailed explanations of the abilities represented by each Capability. If you intend to customize Capabilities, it is strongly recommended that you read this document first.
:::

### SELinux

SELinux is a powerful Mandatory Access Control (MAC) mechanism. It operates on the principle of **default deny**: any action not explicitly allowed is denied.

SELinux can run in two global modes:

1. Permissive mode: Denial events are logged but not enforced.
2. Enforcing mode: Denial events are logged and enforced.

:::warning Warning
Modern Android systems heavily rely on SELinux to ensure overall system security. It is highly recommended not to use any custom systems running in "permissive mode" since it provides no significant advantages over a completely open system.
:::

Explaining the full concept of SELinux is complex and beyond the scope of this document. It is recommended to first understand its workings through the following resources:

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU's Root Profile allows customization of the SELinux context of the root process after executing `su`. Specific access control rules can be set for this context to enable fine-grained control over root permissions.

In typical scenarios, when an app executes `su`, it switches the process to a SELinux domain with **unrestricted access**, such as `u:r:su:s0`. Through the Root Profile, this domain can be switched to a custom domain, such as `u:r:app1:s0`, and a series of rules can be defined for this domain:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Note that the `allow app1 * * *` rule is used for demonstration purposes only. In practice, this rule should not be used extensively since it doesn't differ much from permissive mode.

### Escalation

If the configuration of the Root Profile is not set properly, an escalation scenario may occur: the restrictions imposed by the Root Profile can unintentionally fail.

For example, if you grant root permission to an ADB shell user (which is a common case), and then you grant root permission to a regular application but configure its root profile with UID 2000 (which is the UID of the ADB shell user), the application can obtain full root access by executing the `su` command twice:

1. The first `su` execution is subject to the enforcement of the App Profile and will switch to UID `2000` (adb shell) instead of `0` (root).
2. The second `su` execution, since the UID is `2000`, and you have granted root access to the UID `2000` (adb shell) in the configuration, the application will gain full root privileges.

:::warning Note
This behavior is entirely expected and not a bug. Therefore, we recommend the following:

If you genuinely need to grant root permissions to ADB (e.g., as a developer), it is not advisable to change the UID to `2000` when configuring the Root Profile. Using `1000` (system) would be a better choice.
:::

## Non-Root Profile

### Umount Modules

KernelSU provides a systemless mechanism for modifying system partitions, achieved through overlayfs mounting. However, some apps may be sensitive to such behavior. Thus, we can unload modules mounted on these apps by setting the "umount modules" option.

Additionally, the settings interface of the KernelSU manager provides a switch for "umount modules by default". By default, this switch is **enabled**, which means that KernelSU or some modules will unload modules for this app unless additional settings are applied. If you do not prefer this setting or if it affects certain apps, you have the following options:

1. Keep the switch for "umount modules by default" and individually disable the "umount modules" option in the App Profile for apps requiring module loading (acting as a "whitelist").
2. Disable the switch for "umount modules by default" and individually enable the "umount modules" option in the App Profile for apps requiring module unloading (acting as a "blacklist").

:::info
In devices using kernel version 5.10 and above, the kernel performs the unloading of modules. However, for devices running kernel versions below 5.10, this switch is merely a configuration option, and KernelSU itself does not take any action. Some modules, such as Zygisksu, may use this switch to determine whether module unloading is necessary.
:::
