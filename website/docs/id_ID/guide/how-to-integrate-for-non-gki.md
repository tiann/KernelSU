# Bagaimana Caranya untuk mengintegrasikan KernelSU ke kernel non GKI?

KernelSU dapat diintegrasikan ke kernel non GKI, dan saat ini sudah di-backport ke 4.14, dan juga dapat dijalankan pada kernel di bawah 4.14.

Karena fragmentasi kernel non GKI, kami tidak memiliki cara yang seragam untuk membangunnya, sehingga kami tidak dapat menyediakan gambar boot non GKI. Tetapi Anda dapat membangun kernel sendiri dengan KernelSU yang terintegrasi.

Pertama, Anda harus dapat membangun kernel yang dapat di-boot dari kode sumber kernel, jika kernel tersebut tidak open source, maka akan sulit untuk menjalankan KernelSU untuk perangkat Anda.

Jika Anda dapat membuat kernel yang dapat di-booting, ada dua cara untuk mengintegrasikan KernelSU ke kode sumber kernel:

1. Secara otomatis dengan `kprobe`
2. Secara manual

## Integrasikan dengan kprobe

KernelSU menggunakan kprobe untuk melakukan hook kernel, jika *kprobe* berjalan dengan baik pada kernel Anda, maka disarankan untuk menggunakan cara ini.

Pertama, tambahkan KernelSU ke dalam berkas kernel source tree:

- Latest tag(stable)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main branch(dev)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Select tag(Such as v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Kemudian, Anda harus memeriksa apakah *kprobe* diaktifkan dalam konfigurasi kernel Anda, jika tidak, tambahkan konfigurasi ini ke dalamnya:

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

Dan build kernel Anda lagi, KernelSU seharusnya bekerja dengan baik.

Jika Anda menemukan bahwa KPROBES masih belum diaktifkan, Anda dapat mencoba mengaktifkan `CONFIG_MODULES`. (Jika masih belum berlaku, gunakan `make menuconfig` untuk mencari ketergantungan KPROBES yang lain)

etapi jika Anda mengalami boot loop saat mengintegrasikan KernelSU, itu mungkin *kprobe rusak di kernel Anda*, Anda harus memperbaiki bug kprobe atau menggunakan cara kedua.

## Memodifikasi sumber kernel secara manual

Jika kprobe tidak dapat bekerja pada kernel Anda (mungkin karena bug di upstream atau kernel di bawah 4.8), maka Anda dapat mencoba cara ini:

Pertama, tambahkan KernelSU ke dalam direktori kernel source tree:

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

Kemudian, tambahkan panggilan KernelSU ke source kernel, berikut ini adalah patch yang dapat dirujuk:

```diff
diff --git a/fs/exec.c b/fs/exec.c
index ac59664eaecf..bdd585e1d2cc 100644
--- a/fs/exec.c
+++ b/fs/exec.c
@@ -1890,11 +1890,14 @@ static int __do_execve_file(int fd, struct filename *filename,
 	return retval;
 }
 
+extern bool ksu_execveat_hook __read_mostly;
+extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
+			void *envp, int *flags);
+extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
+				 void *argv, void *envp, int *flags);
 static int do_execveat_common(int fd, struct filename *filename,
 			      struct user_arg_ptr argv,
 			      struct user_arg_ptr envp,
 			      int flags)
 {
+	if (unlikely(ksu_execveat_hook))
+		ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);
+	else
+		ksu_handle_execveat_sucompat(&fd, &filename, &argv, &envp, &flags);
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
 
+extern bool ksu_vfs_read_hook __read_mostly;
+extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
+			size_t *count_ptr, loff_t **pos);
 ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
 {
 	ssize_t ret;
 
+	if (unlikely(ksu_vfs_read_hook))
+		ksu_handle_vfs_read(&file, &buf, &count, &pos);
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

Anda harus menemukan empat fungsi dalam kernel source:

1. do_faccessat, usually in `fs/open.c`
2. do_execveat_common, usually in `fs/exec.c`
3. vfs_read, usually in `fs/read_write.c`
4. vfs_statx, usually in `fs/stat.c`

Jika kernel anda tidak memiliki `vfs_statx`, maka gunakan `vfs_fstatat` alih-alih:

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

Untuk kernel lebih awal dari 4.17, jika anda menemukan `do_faccessat`, hanya pergi ke definisi yang sama `faccessat` syscall dan tempatkan pemanggil di sini:

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

To enable KernelSU's builtin SafeMode, You should also modify `input_handle_event` in `drivers/input/input.c`:

:::tip
Fitur ini sangat direkomendasikan, serta sangat membantu untuk memulihkan pada saat bootloop!
:::

```diff
diff --git a/drivers/input/input.c b/drivers/input/input.c
index 45306f9ef247..815091ebfca4 100755
--- a/drivers/input/input.c
+++ b/drivers/input/input.c
@@ -367,10 +367,13 @@ static int input_get_disposition(struct input_dev *dev,
 	return disposition;
 }
 
+extern bool ksu_input_hook __read_mostly;
+extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);
+
 static void input_handle_event(struct input_dev *dev,
 			       unsigned int type, unsigned int code, int value)
 {
	int disposition = input_get_disposition(dev, type, code, &value);
+
+	if (unlikely(ksu_input_hook))
+		ksu_handle_input_handle_event(&type, &code, &value);
 
 	if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN)
 		add_input_randomness(type, code, value);
```

Terakhir, edit `KernelSU/kernel/ksu.c` dan beri komentar pada `enable_sucompat()` lalu build kernel Anda lagi, KernelSU akan bekerja dengan baik.
