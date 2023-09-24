# Como integrar o KernelSU para kernels não GKI?

O KernelSU pode ser integrado em kernels não GKI e foi portado para 4.14 e versões anteriores.

Devido à fragmentação de kernels não GKI, não temos uma maneira uniforme de construí-lo, portanto não podemos fornecer imagens de inicialização não GKI. Mas você mesmo pode construir o kernel com o KernelSU integrado.

Primeiro, você deve ser capaz de construir um kernel inicializável a partir do código-fonte do kernel. Se o kernel não for de código aberto, será difícil executar o KernelSU no seu dispositivo.

Se você puder construir um kernel inicializável, existem duas maneiras de integrar o KernelSU ao código-fonte do kernel:

1. Automaticamente com `kprobe`
2. Manualmente

## Integrar com kprobe

O KernelSU usa kprobe para fazer ganchos de kernel, se o *kprobe* funcionar bem em seu kernel, é recomendado usar desta forma.

Primeiro, adicione o KernelSU à árvore de origem do kernel:

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

Então, você deve verificar se *kprobe* está ativado na configuração do seu kernel, se não estiver, adicione estas configurações a ele:

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

E construa seu kernel novamente, KernelSU deve funcionar bem.

Se você descobrir que o KPROBES ainda não está ativado, você pode tentar ativar `CONFIG_MODULES`. (Se ainda assim não surtir efeito, use `make menuconfig` para procurar outras dependências do KPROBES)

Mas se você encontrar um loop de inicialização quando o KernelSU integrado, talvez *kprobe esteja quebrado em seu kernel*, você deve corrigir o bug do kprobe ou usar o segundo caminho.

:::tip COMO VERIFICAR SE O KPROBE ESTÁ QUEBRADO?

Comente `ksu_enable_sucompat()` e `ksu_enable_ksud()` em `KernelSU/kernel/ksu.c`, se o dispositivo inicializar normalmente, então o kprobe pode estar quebrado.
:::

## Modifique manualmente a fonte do kernel

Se o kprobe não funcionar no seu kernel (pode ser um bug do upstream ou do kernel abaixo de 4.8), então você pode tentar desta forma:

Primeiro, adicione o KernelSU à árvore de origem do kernel:

- Tag mais recente (estável)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- branch principal (dev)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Selecione a tag (Como v0.5.2)

- 
```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Em seguida, adicione chamadas KernelSU à fonte do kernel. Aqui está um patch para referência:

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

Você deve encontrar as quatro funções no código-fonte do kernel:

1. do_faccessat, geralmente em `fs/open.c`
2. do_execveat_common, geralmente em `fs/exec.c`
3. vfs_read, geralmente em `fs/read_write.c`
4. vfs_statx, geralmente em `fs/stat.c`

Se o seu kernel não tiver `vfs_statx`, use `vfs_fstatat`:

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

Para kernels anteriores ao 4.17, se você não conseguir encontrar `do_faccessat`, basta ir até a definição do syscall `faccessat` e fazer a chamada lá:

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

Para ativar o Modo de Segurança integrado do KernelSU, você também deve modificar `input_handle_event` em `drivers/input/input.c`:

:::tip DICA
É altamente recomendável ativar este recurso, é muito útil para evitar bootloops!
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

Finalmente, construa seu kernel novamente, e então, o KernelSU deve funcionar bem.
