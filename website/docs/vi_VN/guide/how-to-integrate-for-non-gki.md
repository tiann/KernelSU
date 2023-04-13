# Làm thế nào để tích hợp KernelSU vào thiết bị không sử dụng GKI ?

KernelSU có thể được tích hợp vào kernel không phải GKI và hiện tại nó đã được backport xuống 4.14, thậm chí cũng có thể chạy trên kernel thấp hơn 4.14.

Do các kernel không phải GKI bị phân mảnh nên chúng tôi không có cách build thống nhất, vì vậy chúng tôi không thể cung cấp các boot image không phải GKI. Nhưng bạn có thể tự build kernel với KernelSU được tích hợp vào.

Đầu tiên, bạn phải build được kernel từ nguồn có khả năng boot được , nếu kernel không có mã nguồn mở thì rất khó để chạy KernelSU cho thiết bị của bạn.

Nếu bạn có thể build kernel khởi động được, có hai cách để tích hợp KernelSU vào mã nguồn kernel:

1. Tự động với `kprobe`
2. Thủ công


## Tích hợp vào kprobe

KernelSU sử dụng kprobe để thực hiện hook kernel, nếu *kprobe* chạy tốt trong kernel của bạn thì nên sử dụng cách này.

Đầu tiên, thêm KernelSU vào mã nguồn kernel của bạn:

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

Sau đó, bạn nên kiểm tra xem *kprobe* có được bật trong config của bạn hay không, nếu không, vui lòng thêm các cấu hình sau vào:

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

Rồi build lại kernel của bạn, KernelSU sẽ hoạt động ok.

Trong trường hợp kprobe chưa được bật, bạn có thể thêm `CONFIG_MODULES=y` vào kernel config. (Nếu vẫn không có hiệu lực thì hãy sử dụng `make menuconfig` rồi tìm các thành phần khác mà kprobe phụ thuộc).

Nhưng nếu bạn gặp bootloop khi tích hợp KernelSU thì có khả năng ***kprobe bị hỏng trong kernel***, bạn nên fix lỗi kprobe trong mã nguồn hoặc dùng cách 2.

## Chỉnh sửa mã nguồn kernel thủ công

Nếu kprobe không thể hoạt động trong kernel của bạn (có thể là lỗi do upstream hoặc kernel dưới bản 4.8), thì bạn có thể thử cách này:

Đầu tiên, thêm KernelSU vào mã nguồn kernel của bạn:

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

Sau đó, thêm lệnh gọi KernelSU vào mã nguồn kernel, đây là một patch bạn có thể tham khảo:

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

Bạn sẽ tìm thấy bốn chức năng trong mã nguồn kernel:

1. do_faccessat, thường là trong `fs/open.c`
2. do_execveat_common, thường nằm trong `fs/exec.c`
3. vfs_read, thường nằm trong `fs/read_write.c`
4. vfs_statx, thường có trong `fs/stat.c`

Cuối cùng, chỉnh sửa `KernelSU/kernel/ksu.c` và bỏ `enable_sucompat()` sau đó xây dựng lại kernel của bạn, KernelSU sẽ hoạt động tốt.
