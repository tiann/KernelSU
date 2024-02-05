# Como integrar o KernelSU para kernels não GKI?

O KernelSU pode ser integrado em kernels não GKI e foi portado para 4.14 e versões anteriores.

Devido à fragmentação de kernels não GKI, não temos uma maneira uniforme de construí-lo, portanto não podemos fornecer boot.img não GKI. Mas você mesmo pode construir o kernel com o KernelSU integrado.

Primeiro, você deve ser capaz de construir um kernel inicializável a partir do código-fonte do kernel. Se o kernel não for de código aberto, será difícil executar o KernelSU no seu dispositivo.

Se você puder construir um kernel inicializável, existem duas maneiras de integrar o KernelSU ao código-fonte do kernel:

1. Automaticamente com `kprobe`
2. Manualmente

## Integrar com kprobe

O KernelSU usa kprobe para fazer ganchos de kernel, se o kprobe funcionar bem em seu kernel, é recomendado usar desta forma.

Primeiro, adicione o KernelSU à árvore de origem do kernel:

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

Então, você deve verificar se o kprobe está ativado na configuração do seu kernel, se não estiver, adicione estas configurações a ele:

```
CONFIG_KPROBES=y
CONFIG_HAVE_KPROBES=y
CONFIG_KPROBE_EVENTS=y
```

E construa seu kernel novamente, KernelSU deve funcionar bem.

Se você descobrir que o KPROBES ainda não está ativado, você pode tentar ativar `CONFIG_MODULES`. (Se ainda assim não surtir efeito, use `make menuconfig` para procurar outras dependências do KPROBES)

Mas se você entrar em um bootloop quando o KernelSU for integrado, talvez o **kprobe esteja quebrado em seu kernel**. Você deve corrigir o bug do kprobe ou usar o segundo caminho.

:::tip COMO VERIFICAR SE O KPROBE ESTÁ QUEBRADO?

Comente `ksu_enable_sucompat()` e `ksu_enable_ksud()` em `KernelSU/kernel/ksu.c`, se o dispositivo inicializar normalmente, então o kprobe pode estar quebrado.
:::

## Modifique manualmente a fonte do kernel

Se o kprobe não funcionar no seu kernel (pode ser um bug do upstream ou do kernel abaixo de 4.8), então você pode tentar desta forma:

Primeiro, adicione o KernelSU à árvore de origem do kernel:

::: code-group

```sh[Tag mais recente (estável)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

```sh[Branch principal (dev)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

```sh[Selecionar tag (como v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

Tenha em mente que em alguns dispositivos, seu defconfig pode estar em `arch/arm64/configs` ou em outros casos `arch/arm64/configs/vendor/your_defconfig`. Por exemplo, em seu defconfig, habilite `CONFIG_KSU` com y para habilitar ou n para desabilitar. Seu caminho será algo como:
`arch/arm64/configs/...` 
```
# KernelSU
CONFIG_KSU=y
```

Em seguida, adicione chamadas KernelSU à fonte do kernel. Aqui estão alguns patches para referência:

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
  * access() precisa usar o uid/gid real, não o uid/gid efetivo.
  * Fazemos isso limpando temporariamente todos os recursos relacionados ao FS e
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
  * vfs_statx - Obtenha atributos básicos e extras por filename
  * @dfd: Um descritor de arquivo que representa o diretório base para um filename relativo
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

Para kernels anteriores ao 4.17, se você não conseguir encontrar `do_faccessat`, basta ir até a definição do syscall `faccessat` e fazer a chamada lá:

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
  * access() precisa usar o uid/gid real, não o uid/gid efetivo.
  * Fazemos isso limpando temporariamente todos os recursos relacionados ao FS e
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

Finalmente, construa seu kernel novamente, e então, o KernelSU deve funcionar bem.

:::info ENTRANDO NO MODO DE SEGURANÇA ACIDENTALMENTE?
Se você estiver usando a integração manual e não desabilitar `CONFIG_KPROBES`, o usuário poderá acionar o Modo de Segurança pressionando o botão de diminuir volume após a inicialização! Portanto, se estiver usando a integração manual, você precisa desabilitar `CONFIG_KPROBES`!
:::
