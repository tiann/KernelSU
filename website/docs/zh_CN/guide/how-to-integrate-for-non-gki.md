# 如何为非 GKI 内核集成 KernelSU {#introduction}

KernelSU 可以被集成到非 GKI 内核中，现在它最低支持到内核 4.14 版本；理论上也可以支持更低的版本。

由于非 GKI 内核的碎片化极其严重，因此通常没有统一的方法来编译它，所以我们也无法为非 GKI 设备提供 boot 镜像。但你完全可以自己集成 KernelSU 然后编译内核使用。

首先，你必须有能力从你设备的内核源码编译出一个可以开机并且能正常使用的内核，如果内核不开源，这通常难以做到。

如果你已经做好了上述准备，那有两个方法来集成 KernelSU 到你的内核之中。

1. 借助 `kprobe` 自动集成
2. 手动修改内核源码

## 使用 kprobe 集成 {#using-kprobes}

KernelSU 使用 kprobe 机制来做内核的相关 hook，如果 *kprobe* 可以在你编译的内核中正常运行，那么推荐用这个方法来集成。

首先，把 KernelSU 添加到你的内核源码树，在内核的根目录执行以下命令：

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.9.5
```

:::info
[KernelSU 1.0 及更高版本已经不再支持非 GKI 内核](https://github.com/tiann/KernelSU/issues/1705)，最后的支持版本为 `v0.9.5`，请注意使用正确的版本。
:::

然后，你需要检查你的内核是否开启了 *kprobe* 相关的配置，如果没有开启，需要添加以下配置：

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

最后，重新编译你的内核即可。

如果你发现KPROBES仍未生效，很有可能是因为它的依赖项`CONFIG_MODULES`没有被启用（如果还是未生效请键入`make menuconfig`搜索KPROBES 的其它依赖并启用 ）

如果你在集成 KernelSU 之后手机无法启动，那么很可能你的内核中 **kprobe 工作不正常**，你需要修复这个 bug 或者用第二种方法。

:::tip 如何验证是否是 kprobe 的问题？

注释掉 `KernelSU/kernel/ksu.c` 中 `ksu_enable_sucompat()` 和 `ksu_enable_ksud()`，如果正常开机，那么就是 kprobe 的问题；或者你可以手动尝试使用 kprobe 功能，如果不正常，手机会直接重启。
:::

## 手动修改内核源码 {#modify-kernel-source-code}

如果 kprobe 工作不正常（通常是上游的 bug 或者内核版本过低），那你可以尝试这种方法：

首先，把 KernelSU 添加到你的内核源码树，在内核的根目录执行以下命令：

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

请注意，某些设备的defconfig文件可能在`arch/arm64/configs/设备代号_defconfig`或位于`arch/arm64/configs/vendor/设备代号_defconfig`。在您的defconfig文件中,将 `CONFIG_KSU`设置为`y`以启用KernelSU,或设置为`n`以禁用。比如在某个defconfig中:
`arch/arm64/configs/...` 
```sh
+# KernelSU
+CONFIG_KSU=y
```

然后，将 KernelSU 调用添加到内核源代码中，这里有几个补丁可以参考：

::: code-group

```diff[exec.c]
diff --git a/fs/exec.c b/fs/exec.c
index ac59664eaecf..bdd585e1d2cc 100644
--- a/fs/exec.c
+++ b/fs/exec.c
@@ -1890,11 +1890,14 @@ static int __do_execve_file(int fd, struct filename *filename,
 	return retval;
 }

+#ifdef CONFIG_KSU
+extern bool ksu_execveat_hook __read_mostly;
+extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
+			void *envp, int *flags);
+extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
+				 void *argv, void *envp, int *flags);
+#endif
 static int do_execveat_common(int fd, struct filename *filename,
 			      struct user_arg_ptr argv,
 			      struct user_arg_ptr envp,
 			      int flags)
 {
+   #ifdef CONFIG_KSU
+	if (unlikely(ksu_execveat_hook))
+		ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);
+	else
+		ksu_handle_execveat_sucompat(&fd, &filename, &argv, &envp, &flags);
+   #endif
 	return __do_execve_file(fd, filename, argv, envp, flags, NULL);
 }
```
```diff[open.c]
diff --git a/fs/open.c b/fs/open.c
index 05036d819197..965b84d486b8 100644
--- a/fs/open.c
+++ b/fs/open.c
@@ -348,6 +348,8 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
 	return ksys_fallocate(fd, mode, offset, len);
 }

+#ifdef CONFIG_KSU
+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
+			 int *flags);
+#endif
 /*
  * access() needs to use the real uid/gid, not the effective uid/gid.
  * We do this by temporarily clearing all FS-related capabilities and
@@ -355,6 +357,7 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
  */
 long do_faccessat(int dfd, const char __user *filename, int mode)
 {
 	const struct cred *old_cred;
 	struct cred *override_cred;
 	struct path path;
 	struct inode *inode;
 	struct vfsmount *mnt;
 	int res;
 	unsigned int lookup_flags = LOOKUP_FOLLOW;
+   #ifdef CONFIG_KSU
+	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
+   #endif
 
 	if (mode & ~S_IRWXO)	/* where's F_OK, X_OK, W_OK, R_OK? */
 		return -EINVAL;
```
```diff[read_write.c]
diff --git a/fs/read_write.c b/fs/read_write.c
index 650fc7e0f3a6..55be193913b6 100644
--- a/fs/read_write.c
+++ b/fs/read_write.c
@@ -434,10 +434,14 @@ ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
 }
 EXPORT_SYMBOL(kernel_read);

+#ifdef CONFIG_KSU
+extern bool ksu_vfs_read_hook __read_mostly;
+extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
+			size_t *count_ptr, loff_t **pos);
+#endif
 ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
 {
 	ssize_t ret;
+   #ifdef CONFIG_KSU 
+	if (unlikely(ksu_vfs_read_hook))
+		ksu_handle_vfs_read(&file, &buf, &count, &pos);
+   #endif
+
 	if (!(file->f_mode & FMODE_READ))
 		return -EBADF;
 	if (!(file->f_mode & FMODE_CAN_READ))
```
```diff[stat.c]
diff --git a/fs/stat.c b/fs/stat.c
index 376543199b5a..82adcef03ecc 100644
--- a/fs/stat.c
+++ b/fs/stat.c
@@ -148,6 +148,8 @@ int vfs_statx_fd(unsigned int fd, struct kstat *stat,
 }
 EXPORT_SYMBOL(vfs_statx_fd);

+#ifdef CONFIG_KSU
+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);
+#endif
+
 /**
  * vfs_statx - Get basic and extra attributes by filename
  * @dfd: A file descriptor representing the base dir for a relative filename
@@ -170,6 +172,7 @@ int vfs_statx(int dfd, const char __user *filename, int flags,
 	int error = -EINVAL;
 	unsigned int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;

+   #ifdef CONFIG_KSU
+	ksu_handle_stat(&dfd, &filename, &flags);
+   #endif
 	if ((flags & ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |
 		       AT_EMPTY_PATH | KSTAT_QUERY_FLAGS)) != 0)
 		return -EINVAL;
```
:::

主要是要改四个地方：

1. do_faccessat，通常位于 `fs/open.c`
2. do_execveat_common，通常位于 `fs/exec.c`
3. vfs_read，通常位于 `fs/read_write.c`
4. vfs_statx，通常位于 `fs/stat.c`

如果你的内核没有 `vfs_statx`, 使用 `vfs_fstatat` 来代替它：
```diff
diff --git a/fs/stat.c b/fs/stat.c
index 068fdbcc9e26..5348b7bb9db2 100644
--- a/fs/stat.c
+++ b/fs/stat.c
@@ -87,6 +87,8 @@ int vfs_fstat(unsigned int fd, struct kstat *stat)
 }
 EXPORT_SYMBOL(vfs_fstat);
 
+#ifdef CONFIG_KSU 
+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);
+#endif
+
 int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,
 		int flag)
 {
@@ -94,6 +96,8 @@ int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,
 	int error = -EINVAL;
 	unsigned int lookup_flags = 0;
+   #ifdef CONFIG_KSU 
+	ksu_handle_stat(&dfd, &filename, &flag);
+   #endif
+
 	if ((flag & ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |
 		      AT_EMPTY_PATH)) != 0)
 		goto out;
```

对于早于 4.17 的内核，如果没有 `do_faccessat`，可以直接找到 `faccessat` 系统调用的定义然后修改：

```diff
diff --git a/fs/open.c b/fs/open.c
index 2ff887661237..e758d7db7663 100644
--- a/fs/open.c
+++ b/fs/open.c
@@ -355,6 +355,9 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
 	return error;
 }

+#ifdef CONFIG_KSU
+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
+			        int *flags);
+#endif
+
 /*
  * access() needs to use the real uid/gid, not the effective uid/gid.
  * We do this by temporarily clearing all FS-related capabilities and
@@ -370,6 +373,8 @@ SYSCALL_DEFINE3(faccessat, int, dfd, const char __user *, filename, int, mode)
 	int res;
 	unsigned int lookup_flags = LOOKUP_FOLLOW;
+   #ifdef CONFIG_KSU
+	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
+   #endif
+
 	if (mode & ~S_IRWXO)	/* where's F_OK, X_OK, W_OK, R_OK? */
 		return -EINVAL;
```

### 安全模式 

要使用 KernelSU 内置的安全模式，你还需要修改 `drivers/input/input.c` 中的 `input_handle_event` 方法：

:::tip
强烈建议开启此功能，对用户救砖会非常有帮助！
:::

:::info 莫名其妙进入安全模式？
如果你采用手动集成的方式，并且没有禁用`CONFIG_KPROBES`，那么用户在开机之后按音量下，也可能触发安全模式！因此如果使用手动集成，你需要关闭 `CONFIG_KPROBES`！
:::

```diff
diff --git a/drivers/input/input.c b/drivers/input/input.c
index 45306f9ef247..815091ebfca4 100755
--- a/drivers/input/input.c
+++ b/drivers/input/input.c
@@ -367,10 +367,13 @@ static int input_get_disposition(struct input_dev *dev,
 	return disposition;
 }

+#ifdef CONFIG_KSU
+extern bool ksu_input_hook __read_mostly;
+extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);
+#endif
+
 static void input_handle_event(struct input_dev *dev,
 			       unsigned int type, unsigned int code, int value)
 {
	int disposition = input_get_disposition(dev, type, code, &value);
+   #ifdef CONFIG_KSU
+	if (unlikely(ksu_input_hook))
+		ksu_handle_input_handle_event(&type, &code, &value);
+   #endif
 
 	if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN)
 		add_input_randomness(type, code, value);
```

### pm 命令执行失败？

你需要同时修改 `fs/devpts/inode.c`，补丁如下：

```diff
diff --git a/fs/devpts/inode.c b/fs/devpts/inode.c
index 32f6f1c68..d69d8eca2 100644
--- a/fs/devpts/inode.c
+++ b/fs/devpts/inode.c
@@ -602,6 +602,8 @@ struct dentry *devpts_pty_new(struct pts_fs_info *fsi, int index, void *priv)
        return dentry;
 }

+extern int ksu_handle_devpts(struct inode*);
+
 /**
  * devpts_get_priv -- get private data for a slave
  * @pts_inode: inode of the slave
@@ -610,6 +612,7 @@ struct dentry *devpts_pty_new(struct pts_fs_info *fsi, int index, void *priv)
  */
 void *devpts_get_priv(struct dentry *dentry)
 {
+       ksu_handle_devpts(dentry->d_inode);
        if (dentry->d_sb->s_magic != DEVPTS_SUPER_MAGIC)
                return NULL;
        return dentry->d_fsdata;
```

### path_umount {#how-to-backport-path-umount}

你可以通过从K5.9向旧版本移植`path_umount`，在GKI之前的内核上获得卸载模块的功能。你可以通过以下补丁作为参考:

```diff
--- a/fs/namespace.c
+++ b/fs/namespace.c
@@ -1739,6 +1739,39 @@ static inline bool may_mandlock(void)
 }
 #endif

+static int can_umount(const struct path *path, int flags)
+{
+	struct mount *mnt = real_mount(path->mnt);
+
+	if (flags & ~(MNT_FORCE | MNT_DETACH | MNT_EXPIRE | UMOUNT_NOFOLLOW))
+		return -EINVAL;
+	if (!may_mount())
+		return -EPERM;
+	if (path->dentry != path->mnt->mnt_root)
+		return -EINVAL;
+	if (!check_mnt(mnt))
+		return -EINVAL;
+	if (mnt->mnt.mnt_flags & MNT_LOCKED) /* Check optimistically */
+		return -EINVAL;
+	if (flags & MNT_FORCE && !capable(CAP_SYS_ADMIN))
+		return -EPERM;
+	return 0;
+}
+
+int path_umount(struct path *path, int flags)
+{
+	struct mount *mnt = real_mount(path->mnt);
+	int ret;
+
+	ret = can_umount(path, flags);
+	if (!ret)
+		ret = do_umount(mnt, flags);
+
+	/* we mustn't call path_put() as that would clear mnt_expiry_mark */
+	dput(path->dentry);
+	mntput_no_expire(mnt);
+	return ret;
+}
 /*
  * Now umount can handle mount points as well as block devices.
  * This is important for filesystems which use unnamed block devices.
```

改完之后重新编译内核即可。
