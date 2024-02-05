# Как интегрировать KernelSU для не GKI ядер? {#introduction}

KernelSU может быть интегрирован в ядра, отличные от GKI, и был перенесен на версии 4.14 и ниже.

В связи с фрагментацией ядер, отличных от GKI, у нас нет единого способа их сборки, поэтому мы не можем предоставить загрузочные образы, отличные от GKI. Однако вы можете собрать ядро самостоятельно с помощью интегрированной программы KernelSU.

Во-первых, вы должны уметь собирать загрузочное ядро из исходных текстов ядра. Если ядро не является открытым, то запустить KernelSU для вашего устройства будет затруднительно.

Если вы можете собрать загрузочное ядро, то существует два способа интеграции KernelSU в исходный код ядра:

1. Автоматически с помощью `kprobe`
2. Вручную

## Интеграция с kprobe {#using-kprobes}

KernelSU использует kprobe для выполнения хуков ядра, если *kprobe* хорошо работает в вашем ядре, то рекомендуется использовать именно этот способ.

Сначала добавьте KernelSU в дерево исходных текстов ядра:

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

Затем необходимо проверить, включена ли функция *kprobe* в конфигурации ядра, если нет, то добавьте в нее эти настройки:

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

И снова соберите ядро, KernelSU должен работать нормально.

Если вы обнаружите, что KPROBES по-прежнему не активирован, попробуйте включить `CONFIG_MODULES`. (Если это все равно не даст результата, используйте `make menuconfig` для поиска других зависимостей KPROBES).

Если же при интеграции KernelSU возникает зацикливание загрузки, то, возможно, в вашем ядре *kprobe неисправен*, следует исправить ошибку kprobe или воспользоваться вторым способом.

:::tip Как проверить, не сломан ли kprobe？

закомментируйте `ksu_enable_sucompat()` и `ksu_enable_ksud()` в файле `KernelSU/kernel/ksu.c`, если устройство загружается нормально, то может быть нарушена работа kprobe.
:::

## Ручная модификация исходного кода ядра {#modify-kernel-source-code}

Если kprobe не работает в вашем ядре (возможно, это ошибка апстрима или ядра ниже 4.8), то можно попробовать следующий способ:

Сначала добавьте Kernel SU в дерево исходного кода вашего ядра:

- Последний тэг(стабильный)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- Основная ветвь(разработка)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Выбранный тэг(Например, версия v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Затем добавьте вызовы KernelSU в исходный код ядра, вот патч, на который можно сослаться:

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
```
```diff
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
 	const struct cred *old_cred;
 	struct cred *override_cred;
 	struct path path;
 	struct inode *inode;
 	struct vfsmount *mnt;
 	int res;
 	unsigned int lookup_flags = LOOKUP_FOLLOW;
 
+	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
 
 	if (mode & ~S_IRWXO)	/* where's F_OK, X_OK, W_OK, R_OK? */
 		return -EINVAL;
```
```diff
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
```
```diff
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

В исходных кодах ядра можно найти эти четыре функции:

1. do_faccessat, обычно в `fs/open.c`.
2. do_execveat_common, обычно в `fs/exec.c`.
3. vfs_read, обычно в `fs/read_write.c`.
4. vfs_statx, обычно в `fs/stat.c`.

Если в вашем ядре нет `vfs_statx`, используйте вместо него `vfs_fstatat`:

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

Для ядер младше 4.17, если вы не можете найти `do_faccessat`, просто перейдите к определению системного вызова `faccessat` и поместите вызов туда:

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

Чтобы включить встроенный в KernelSU безопасный режим, необходимо также изменить `input_handle_event` в файле `drivers/input/input.c`:

:::tip
Настоятельно рекомендуется включить эту функцию, она очень помогает предотвратить циклическую загрузку!
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

Наконец, снова соберите ядро, KernelSU должен работать хорошо.
