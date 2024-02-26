# 非 GKI カーネルで KernelSU を統合する方法は？

KernelSU は非 GKI カーネルに統合することが可能であり、4.14 以下のバージョンにバックポートされました。

非 GKI カーネルの断片化のため、統一されたビルド方法がありませんので、非 GKI ブートイメージを提供することができません。しかし、KernelSU を統合して自分自身でカーネルをビルドすることができます。

まず、カーネルソースコードからブート可能なカーネルをビルドできる能力が必要です。もしカーネルがオープンソースでない場合、あなたのデバイスで KernelSU を実行することは困難です。

ブート可能なカーネルをビルドできるなら、カーネルソースコードに KernelSU を統合する方法は二つあります：

1. `kprobe` で自動的に
2. 手動で

## kprobe で統合する

KernelSU は kprobe を使ってカーネルフックを行います。もし *kprobe* があなたのカーネルでうまく動作する場合、この方法を使うことを推奨します。

まず、KernelSU をカーネルソースツリーに追加してください：

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

次に、*kprobe* がカーネル設定で有効になっているか確認してください。もし有効でなければ、これらの設定を追加してください：

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```
そしてカーネルを再度ビルドしてください。KernelSU はうまく動作するはずです。

KPROBES がまだ有効化されていない場合は、CONFIG_MODULES を有効化して試みることができます。（それでも効果がない場合は、make menuconfig を使って KPROBES の他の依存関係を検索してください）

しかし、KernelSU を統合した際にブートループに遭遇した場合、それは *kprobe* があなたのカーネルで破損している可能性があります。kprobe のバグを修正するか、二番目の方法を使用するべきです。

:::tip kprobe が破損しているかどうかを確認する方法は？

`KernelSU/kernel/ksu.c` にある `ksu_enable_sucompat()` と `ksu_enable_ksud()` をコメントアウトし、デバイスが正常にブートするか試してください。もし正常にブートするならば、kprobe が破損している可能性があります。

## カーネルソースを手動で変更する

もし kprobe があなたのカーネルで機能しない場合（上流のバグや 4.8 以下のカーネルバグが原因かもしれません）、以下の方法を試すことができます。

まず、KernelSU をカーネルソースツリーに追加してください：

::: code-group
## カーネルソースを手動で変更する

もし kprobe があなたのカーネルで機能しない場合（上流のバグや 4.8 以下のカーネルバグが原因かもしれません）、以下の方法を試すことができます。

まず、KernelSU をカーネルソースツリーに追加してください：

::: code-group

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

[ main branch(dev)]
```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

[Select tag(Such as v0.5.2)]
```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```


:::

いくつかのデバイスでは、あなたの defconfig が `arch/arm64/configs` にあったり、または他のケースでは `arch/arm64/configs/vendor/your_defconfig` にあることを念頭に置いてください。例えばあなたの defconfig で、`CONFIG_KSU` を y で有効に、または n で無効に設定します。あなたのパスは次のようになるでしょう：
`arch/arm64/configs/...`
```
# KernelSU
CONFIG_KSU=y
```
次に、KernelSU の呼び出しをカーネルソースに追加します。こちらは参照のためのパッチです：

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
カーネル ソースには 4 つの関数があるはずです。

1. do_faccessat、通常は `fs/open.c` にあります
2. do_execveat_common (通常は `fs/exec.c` にあります)
3. vfs_read (通常は `fs/read_write.c` にあります)
4. vfs_statx (通常は「fs/stat.c」にあります)

カーネルに `vfs_statx` がない場合は、代わりに `vfs_fstatat` を使用してください:

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

4.17 より前のカーネルの場合、`do faccessat` が見つからない場合は、`faccessat` システムコールの定義に移動して、そこで呼び出しを実行します。

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

KernelSU の組み込み SafeMode を有効にするには、`drivers/input/input.c` の `input_handle_event` も変更する必要があります。

:::ヒント
この機能を有効にすることを強くお勧めします。ブートループを防ぐのに非常に役立ちます!
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

最後に、カーネルを再度ビルドすると、KernelSU が正常に動作するはずです。

:::info 誤ってセーフ モードに入ってしまった場合は、
手動統合を使用し、`CONFIG_KPROBES` を無効にしない場合、ユーザーは起動後に音量を下げるボタンを押してセーフ モードをトリガーする可能性があります。 したがって、手動統合を使用する場合は、`CONFIG_KPROBES` を無効にする必要があります。
:::
