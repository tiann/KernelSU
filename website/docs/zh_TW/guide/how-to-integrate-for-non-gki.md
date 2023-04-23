# 如何為非 GKI 核心整合 KernelSU {#introduction}

KernelSU 可以被整合到非 GKI 核心中，現在它最低支援到核心 4.14 版本；理論上也可以支援更低的版本。

由於非 GKI 核心的片段化極其嚴重，因此通常沒有統一的方法來建置它，所以我們也無法為非 GKI 裝置提供 Boot 映像。但您完全可以自行整合 KernelSU 並建置核心以繼續使用。

首先，您必須有能力從您裝置的核心原始碼建置出一個可以開機並且能夠正常使用的核心，如果核心並非開放原始碼，這通常難以做到。

如果您已經做好了上述準備，那有兩個方法來將 KernelSU 整合至您的核心之中。

1. 藉助 `kprobe` 自動整合
2. 手動修改核心原始碼

## 使用 kprobe 整合 {#using-kprobes}

KernelSU 使用 kprobe 機制來處理核心的相關 hook，如果 *kprobe* 可以在您建置的核心中正常運作，那麼建議使用這個方法進行整合。

首先，把 KernelSU 新增至您的核心來源樹狀結構，再核心的根目錄執行以下命令：

- 最新 tag (稳定版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main 分支(開發版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- 選取 tag (例如 v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

然後，您需要檢查您的核心是否啟用 *kprobe* 相關組態，如果未啟用，則需要新增以下組態：

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

最後，重新建置您的核心即可。

如果您發現 KPROBES 仍未生效，很有可能是因為它的相依性 `CONFIG_MODULES` 並未被啟用 (如果還是未生效請輸入 `make menuconfig` 搜尋 KPROBES 的其他相依性並啟用)

如果您在整合 KernelSU 之後手機無法啟動，那麼很可能您的核心中 **kprobe 無法正常運作**，您需要修正這個錯誤，或者使用第二種方法。

:::tip 如何檢查 kprobe 是否損毀？

將 `KernelSU/kernel/ksu.c` 中的 `ksu_enable_sucompat()` 和 `ksu_enable_ksud()` 取消註解，如果正常開機，即 kprobe 已損毀；或者您可以手動嘗試使用 kprobe 功能，如果不正常，手機會直接重新啟動。
:::

## 手動修改核心原始碼 {#modify-kernel-source-code}

如果 kprobe 無法正常運作 (可能是上游的錯誤或核心版本過低)，那您可以嘗試這種方法：

首先，將 KernelSU 新增至您的原始碼樹狀結構，再核心的根目錄執行以下命令：

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

然後，手動修改核心原始碼，您可以參閱下方的 patch：

```diff
diff --git a/fs/exec.c b/fs/exec.c
index ac59664eaecf..bdd585e1d2cc 100644
--- a/fs/exec.c
+++ b/fs/exec.c
@@ -1890,11 +1890,14 @@ static int __do_execve_file(int fd, struct filename *filename,
 	return retval;
 }
 
+extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
+			void *envp, int *flags);
 static int do_execveat_common(int fd, struct filename *filename,
 			      struct user_arg_ptr argv,
 			      struct user_arg_ptr envp,
 			      int flags)
 {
+	ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);
 	return __do_execve_file(fd, filename, argv, envp, flags, NULL);
 }
 
diff --git a/fs/open.c b/fs/open.c
index 05036d819197..965b84d486b8 100644
--- a/fs/open.c
+++ b/fs/open.c
@@ -348,6 +348,8 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
 	return ksys_fallocate(fd, mode, offset, len);
 }
 
+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
+			 int *flags);
 /*
  * access() needs to use the real uid/gid, not the effective uid/gid.
  * We do this by temporarily clearing all FS-related capabilities and
@@ -355,6 +357,7 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
  */
 long do_faccessat(int dfd, const char __user *filename, int mode)
 {
+	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
 	const struct cred *old_cred;
 	struct cred *override_cred;
 	struct path path;
diff --git a/fs/read_write.c b/fs/read_write.c
index 650fc7e0f3a6..55be193913b6 100644
--- a/fs/read_write.c
+++ b/fs/read_write.c
@@ -434,10 +434,14 @@ ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
 }
 EXPORT_SYMBOL(kernel_read);
 
+extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
+			size_t *count_ptr, loff_t **pos);
 ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
 {
 	ssize_t ret;
 
+	ksu_handle_vfs_read(&file, &buf, &count, &pos);
+	
 	if (!(file->f_mode & FMODE_READ))
 		return -EBADF;
 	if (!(file->f_mode & FMODE_CAN_READ))
diff --git a/fs/stat.c b/fs/stat.c
index 376543199b5a..82adcef03ecc 100644
--- a/fs/stat.c
+++ b/fs/stat.c
@@ -148,6 +148,8 @@ int vfs_statx_fd(unsigned int fd, struct kstat *stat,
 }
 EXPORT_SYMBOL(vfs_statx_fd);
 
+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);
+
 /**
  * vfs_statx - Get basic and extra attributes by filename
  * @dfd: A file descriptor representing the base dir for a relative filename
@@ -170,6 +172,7 @@ int vfs_statx(int dfd, const char __user *filename, int flags,
 	int error = -EINVAL;
 	unsigned int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;
 
+	ksu_handle_stat(&dfd, &filename, &flags);
 	if ((flags & ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |
 		       AT_EMPTY_PATH | KSTAT_QUERY_FLAGS)) != 0)
 		return -EINVAL;
```

主要修改四個項目：

1. do_faccessat，通常位於 `fs/open.c`
2. do_execveat_common，通常位於 `fs/exec.c`
3. vfs_read，通常位於 `fs/read_write.c`
4. vfs_statx，通常位於 `fs/stat.c`

如果您的核心沒有 `vfs_statx`，使用 `vfs_fstatat` 將其取代：

```diff
diff --git a/fs/stat.c b/fs/stat.c
index 068fdbcc9e26..5348b7bb9db2 100644
--- a/fs/stat.c
+++ b/fs/stat.c
@@ -87,6 +87,8 @@ int vfs_fstat(unsigned int fd, struct kstat *stat)
 }
 EXPORT_SYMBOL(vfs_fstat);
 
+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);
+
 int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,
 		int flag)
 {
@@ -94,6 +96,8 @@ int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,
 	int error = -EINVAL;
 	unsigned int lookup_flags = 0;
 
+	ksu_handle_stat(&dfd, &filename, &flag);
+
 	if ((flag & ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |
 		      AT_EMPTY_PATH)) != 0)
 		goto out;
```

對於早於 4.17 的核心，如果沒有 `do_faccessat`，可以直接找到 `faccessat` 系統呼叫的定義並進行修改：

```diff
diff --git a/fs/open.c b/fs/open.c
index 2ff887661237..e758d7db7663 100644
--- a/fs/open.c
+++ b/fs/open.c
@@ -355,6 +355,9 @@ SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)
 	return error;
 }
 
+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
+			        int *flags);
+
 /*
  * access() needs to use the real uid/gid, not the effective uid/gid.
  * We do this by temporarily clearing all FS-related capabilities and
@@ -370,6 +373,8 @@ SYSCALL_DEFINE3(faccessat, int, dfd, const char __user *, filename, int, mode)
 	int res;
 	unsigned int lookup_flags = LOOKUP_FOLLOW;
 
+	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
+
 	if (mode & ~S_IRWXO)	/* where's F_OK, X_OK, W_OK, R_OK? */
 		return -EINVAL;
```

若要啟用 KernelSU 內建的安全模式，您還需要修改 `drivers/input/input.c` 中的 `input_handle_event` 方法：

:::tip
強烈建議啟用此功能，如果遇到開機迴圈，這將會非常有用！
:::

```diff
diff --git a/drivers/input/input.c b/drivers/input/input.c
index 45306f9ef247..815091ebfca4 100755
--- a/drivers/input/input.c
+++ b/drivers/input/input.c
@@ -367,10 +367,13 @@ static int input_get_disposition(struct input_dev *dev,
        return disposition;
 }

+extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);
+
 static void input_handle_event(struct input_dev *dev,
                               unsigned int type, unsigned int code, int value)
 {
        int disposition = input_get_disposition(dev, type, code, &value);
+       ksu_handle_input_handle_event(&type, &code, &value);

        if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN)
                add_input_randomness(type, code, value);
```

最後，再次建置您的核心，KernelSU 將會如期運作。
