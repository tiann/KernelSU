import{_ as s,o as n,c as a,Q as l}from"./chunks/framework.ec8f7e8e.js";const _=JSON.parse('{"title":"Как интегрировать KernelSU для не GKI ядер?","description":"","frontmatter":{},"headers":[],"relativePath":"ru_RU/guide/how-to-integrate-for-non-gki.md","filePath":"ru_RU/guide/how-to-integrate-for-non-gki.md"}'),p={name:"ru_RU/guide/how-to-integrate-for-non-gki.md"},e=l(`<h1 id="introduction" tabindex="-1">Как интегрировать KernelSU для не GKI ядер? <a class="header-anchor" href="#introduction" aria-label="Permalink to &quot;Как интегрировать KernelSU для не GKI ядер? {#introduction}&quot;">​</a></h1><p>KernelSU может быть интегрирован в ядра, отличные от GKI, и был перенесен на версии 4.14 и ниже.</p><p>В связи с фрагментацией ядер, отличных от GKI, у нас нет единого способа их сборки, поэтому мы не можем предоставить загрузочные образы, отличные от GKI. Однако вы можете собрать ядро самостоятельно с помощью интегрированной программы KernelSU.</p><p>Во-первых, вы должны уметь собирать загрузочное ядро из исходных текстов ядра. Если ядро не является открытым, то запустить KernelSU для вашего устройства будет затруднительно.</p><p>Если вы можете собрать загрузочное ядро, то существует два способа интеграции KernelSU в исходный код ядра:</p><ol><li>Автоматически с помощью <code>kprobe</code></li><li>Вручную</li></ol><h2 id="using-kprobes" tabindex="-1">Интеграция с kprobe <a class="header-anchor" href="#using-kprobes" aria-label="Permalink to &quot;Интеграция с kprobe {#using-kprobes}&quot;">​</a></h2><p>KernelSU использует kprobe для выполнения хуков ядра, если <em>kprobe</em> хорошо работает в вашем ядре, то рекомендуется использовать именно этот способ.</p><p>Сначала добавьте KernelSU в дерево исходных текстов ядра:</p><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">-</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#032F62;">-</span></span></code></pre></div><p>Затем необходимо проверить, включена ли функция <em>kprobe</em> в конфигурации ядра, если нет, то добавьте в нее эти настройки:</p><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#e1e4e8;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#e1e4e8;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#e1e4e8;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#24292e;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#24292e;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#24292e;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>И снова соберите ядро, KernelSU должен работать нормально.</p><p>Если вы обнаружите, что KPROBES по-прежнему не активирован, попробуйте включить <code>CONFIG_MODULES</code>. (Если это все равно не даст результата, используйте <code>make menuconfig</code> для поиска других зависимостей KPROBES).</p><p>Если же при интеграции KernelSU возникает зацикливание загрузки, то, возможно, в вашем ядре <em>kprobe неисправен</em>, следует исправить ошибку kprobe или воспользоваться вторым способом.</p><div class="tip custom-block"><p class="custom-block-title">Как проверить, не сломан ли kprobe？</p><p>закомментируйте <code>ksu_enable_sucompat()</code> и <code>ksu_enable_ksud()</code> в файле <code>KernelSU/kernel/ksu.c</code>, если устройство загружается нормально, то может быть нарушена работа kprobe.</p></div><h2 id="modify-kernel-source-code" tabindex="-1">Ручная модификация исходного кода ядра <a class="header-anchor" href="#modify-kernel-source-code" aria-label="Permalink to &quot;Ручная модификация исходного кода ядра {#modify-kernel-source-code}&quot;">​</a></h2><p>Если kprobe не работает в вашем ядре (возможно, это ошибка апстрима или ядра ниже 4.8), то можно попробовать следующий способ:</p><p>Сначала добавьте Kernel SU в дерево исходного кода вашего ядра:</p><ul><li>Последний тэг(стабильный)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">-</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#032F62;">-</span></span></code></pre></div><ul><li>Основная ветвь(разработка)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-s</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">main</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-s</span><span style="color:#24292E;"> </span><span style="color:#032F62;">main</span></span></code></pre></div><ul><li>Выбранный тэг(Например, версия v0.5.2)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-s</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">v0.5.2</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-s</span><span style="color:#24292E;"> </span><span style="color:#032F62;">v0.5.2</span></span></code></pre></div><p>Затем добавьте вызовы KernelSU в исходный код ядра, вот патч, на который можно сослаться:</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index ac59664eaecf..bdd585e1d2cc 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/exec.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/exec.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -1890,11 +1890,14 @@</span><span style="color:#E1E4E8;"> static int __do_execve_file(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	return retval;</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern bool ksu_execveat_hook __read_mostly;</span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,</span></span>
<span class="line"><span style="color:#85E89D;">+			void *envp, int *flags);</span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,</span></span>
<span class="line"><span style="color:#85E89D;">+				 void *argv, void *envp, int *flags);</span></span>
<span class="line"><span style="color:#E1E4E8;"> static int do_execveat_common(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 			      struct user_arg_ptr argv,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 			      struct user_arg_ptr envp,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 			      int flags)</span></span>
<span class="line"><span style="color:#E1E4E8;"> {</span></span>
<span class="line"><span style="color:#85E89D;">+	if (unlikely(ksu_execveat_hook))</span></span>
<span class="line"><span style="color:#85E89D;">+		ksu_handle_execveat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#85E89D;">+	else</span></span>
<span class="line"><span style="color:#85E89D;">+		ksu_handle_execveat_sucompat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	return __do_execve_file(fd, filename, argv, envp, flags, NULL);</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#79B8FF;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 05036d819197..965b84d486b8 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/open.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/open.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -348,6 +348,8 @@</span><span style="color:#E1E4E8;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	return ksys_fallocate(fd, mode, offset, len);</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#85E89D;">+			 int *flags);</span></span>
<span class="line"><span style="color:#E1E4E8;"> /*</span></span>
<span class="line"><span style="color:#E1E4E8;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#E1E4E8;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -355,6 +357,7 @@</span><span style="color:#E1E4E8;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#E1E4E8;">  */</span></span>
<span class="line"><span style="color:#E1E4E8;"> long do_faccessat(int dfd, const char __user *filename, int mode)</span></span>
<span class="line"><span style="color:#E1E4E8;"> {</span></span>
<span class="line"><span style="color:#85E89D;">+	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	const struct cred *old_cred;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	struct cred *override_cred;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	struct path path;</span></span>
<span class="line"><span style="color:#79B8FF;">diff --git a/fs/read_write.c b/fs/read_write.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 650fc7e0f3a6..55be193913b6 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/read_write.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/read_write.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -434,10 +434,14 @@</span><span style="color:#E1E4E8;"> ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> EXPORT_SYMBOL(kernel_read);</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern bool ksu_vfs_read_hook __read_mostly;</span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,</span></span>
<span class="line"><span style="color:#85E89D;">+			size_t *count_ptr, loff_t **pos);</span></span>
<span class="line"><span style="color:#E1E4E8;"> ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#E1E4E8;"> {</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	ssize_t ret;</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+	if (unlikely(ksu_vfs_read_hook))</span></span>
<span class="line"><span style="color:#85E89D;">+		ksu_handle_vfs_read(&amp;file, &amp;buf, &amp;count, &amp;pos);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if (!(file-&gt;f_mode &amp; FMODE_READ))</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		return -EBADF;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if (!(file-&gt;f_mode &amp; FMODE_CAN_READ))</span></span>
<span class="line"><span style="color:#79B8FF;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 376543199b5a..82adcef03ecc 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/stat.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/stat.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -148,6 +148,8 @@</span><span style="color:#E1E4E8;"> int vfs_statx_fd(unsigned int fd, struct kstat *stat,</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> EXPORT_SYMBOL(vfs_statx_fd);</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> /**</span></span>
<span class="line"><span style="color:#E1E4E8;">  * vfs_statx - Get basic and extra attributes by filename</span></span>
<span class="line"><span style="color:#E1E4E8;">  * @dfd: A file descriptor representing the base dir for a relative filename</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -170,6 +172,7 @@</span><span style="color:#E1E4E8;"> int vfs_statx(int dfd, const char __user *filename, int flags,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flags);</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if ((flags &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		       AT_EMPTY_PATH | KSTAT_QUERY_FLAGS)) != 0)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		return -EINVAL;</span></span></code></pre><pre class="shiki github-light has-diff vp-code-light"><code><span class="line"><span style="color:#005CC5;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
<span class="line"><span style="color:#24292E;">index ac59664eaecf..bdd585e1d2cc 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/exec.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/exec.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -1890,11 +1890,14 @@</span><span style="color:#24292E;"> static int __do_execve_file(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#24292E;"> 	return retval;</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern bool ksu_execveat_hook __read_mostly;</span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,</span></span>
<span class="line"><span style="color:#22863A;">+			void *envp, int *flags);</span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,</span></span>
<span class="line"><span style="color:#22863A;">+				 void *argv, void *envp, int *flags);</span></span>
<span class="line"><span style="color:#24292E;"> static int do_execveat_common(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#24292E;"> 			      struct user_arg_ptr argv,</span></span>
<span class="line"><span style="color:#24292E;"> 			      struct user_arg_ptr envp,</span></span>
<span class="line"><span style="color:#24292E;"> 			      int flags)</span></span>
<span class="line"><span style="color:#24292E;"> {</span></span>
<span class="line"><span style="color:#22863A;">+	if (unlikely(ksu_execveat_hook))</span></span>
<span class="line"><span style="color:#22863A;">+		ksu_handle_execveat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#22863A;">+	else</span></span>
<span class="line"><span style="color:#22863A;">+		ksu_handle_execveat_sucompat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#24292E;"> 	return __do_execve_file(fd, filename, argv, envp, flags, NULL);</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#005CC5;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#24292E;">index 05036d819197..965b84d486b8 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/open.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/open.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -348,6 +348,8 @@</span><span style="color:#24292E;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#24292E;"> 	return ksys_fallocate(fd, mode, offset, len);</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#22863A;">+			 int *flags);</span></span>
<span class="line"><span style="color:#24292E;"> /*</span></span>
<span class="line"><span style="color:#24292E;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#24292E;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -355,6 +357,7 @@</span><span style="color:#24292E;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#24292E;">  */</span></span>
<span class="line"><span style="color:#24292E;"> long do_faccessat(int dfd, const char __user *filename, int mode)</span></span>
<span class="line"><span style="color:#24292E;"> {</span></span>
<span class="line"><span style="color:#22863A;">+	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#24292E;"> 	const struct cred *old_cred;</span></span>
<span class="line"><span style="color:#24292E;"> 	struct cred *override_cred;</span></span>
<span class="line"><span style="color:#24292E;"> 	struct path path;</span></span>
<span class="line"><span style="color:#005CC5;">diff --git a/fs/read_write.c b/fs/read_write.c</span></span>
<span class="line"><span style="color:#24292E;">index 650fc7e0f3a6..55be193913b6 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/read_write.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/read_write.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -434,10 +434,14 @@</span><span style="color:#24292E;"> ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> EXPORT_SYMBOL(kernel_read);</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern bool ksu_vfs_read_hook __read_mostly;</span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,</span></span>
<span class="line"><span style="color:#22863A;">+			size_t *count_ptr, loff_t **pos);</span></span>
<span class="line"><span style="color:#24292E;"> ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#24292E;"> {</span></span>
<span class="line"><span style="color:#24292E;"> 	ssize_t ret;</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+	if (unlikely(ksu_vfs_read_hook))</span></span>
<span class="line"><span style="color:#22863A;">+		ksu_handle_vfs_read(&amp;file, &amp;buf, &amp;count, &amp;pos);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> 	if (!(file-&gt;f_mode &amp; FMODE_READ))</span></span>
<span class="line"><span style="color:#24292E;"> 		return -EBADF;</span></span>
<span class="line"><span style="color:#24292E;"> 	if (!(file-&gt;f_mode &amp; FMODE_CAN_READ))</span></span>
<span class="line"><span style="color:#005CC5;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#24292E;">index 376543199b5a..82adcef03ecc 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/stat.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/stat.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -148,6 +148,8 @@</span><span style="color:#24292E;"> int vfs_statx_fd(unsigned int fd, struct kstat *stat,</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> EXPORT_SYMBOL(vfs_statx_fd);</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> /**</span></span>
<span class="line"><span style="color:#24292E;">  * vfs_statx - Get basic and extra attributes by filename</span></span>
<span class="line"><span style="color:#24292E;">  * @dfd: A file descriptor representing the base dir for a relative filename</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -170,6 +172,7 @@</span><span style="color:#24292E;"> int vfs_statx(int dfd, const char __user *filename, int flags,</span></span>
<span class="line"><span style="color:#24292E;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#24292E;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flags);</span></span>
<span class="line"><span style="color:#24292E;"> 	if ((flags &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#24292E;"> 		       AT_EMPTY_PATH | KSTAT_QUERY_FLAGS)) != 0)</span></span>
<span class="line"><span style="color:#24292E;"> 		return -EINVAL;</span></span></code></pre></div><p>В исходных кодах ядра можно найти эти четыре функции:</p><ol><li>do_faccessat, обычно в <code>fs/open.c</code>.</li><li>do_execveat_common, обычно в <code>fs/exec.c</code>.</li><li>vfs_read, обычно в <code>fs/read_write.c</code>.</li><li>vfs_statx, обычно в <code>fs/stat.c</code>.</li></ol><p>Если в вашем ядре нет <code>vfs_statx</code>, используйте вместо него <code>vfs_fstatat</code>:</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 068fdbcc9e26..5348b7bb9db2 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/stat.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/stat.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -87,6 +87,8 @@</span><span style="color:#E1E4E8;"> int vfs_fstat(unsigned int fd, struct kstat *stat)</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> EXPORT_SYMBOL(vfs_fstat);</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		int flag)</span></span>
<span class="line"><span style="color:#E1E4E8;"> {</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -94,6 +96,8 @@</span><span style="color:#E1E4E8;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	unsigned int lookup_flags = 0;</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flag);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if ((flag &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		      AT_EMPTY_PATH)) != 0)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		goto out;</span></span></code></pre><pre class="shiki github-light has-diff vp-code-light"><code><span class="line"><span style="color:#005CC5;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#24292E;">index 068fdbcc9e26..5348b7bb9db2 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/stat.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/stat.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -87,6 +87,8 @@</span><span style="color:#24292E;"> int vfs_fstat(unsigned int fd, struct kstat *stat)</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> EXPORT_SYMBOL(vfs_fstat);</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#24292E;"> 		int flag)</span></span>
<span class="line"><span style="color:#24292E;"> {</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -94,6 +96,8 @@</span><span style="color:#24292E;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#24292E;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#24292E;"> 	unsigned int lookup_flags = 0;</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flag);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> 	if ((flag &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#24292E;"> 		      AT_EMPTY_PATH)) != 0)</span></span>
<span class="line"><span style="color:#24292E;"> 		goto out;</span></span></code></pre></div><p>Для ядер младше 4.17, если вы не можете найти <code>do_faccessat</code>, просто перейдите к определению системного вызова <code>faccessat</code> и поместите вызов туда:</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 2ff887661237..e758d7db7663 100644</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/fs/open.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/fs/open.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -355,6 +355,9 @@</span><span style="color:#E1E4E8;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	return error;</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#85E89D;">+			        int *flags);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> /*</span></span>
<span class="line"><span style="color:#E1E4E8;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#E1E4E8;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -370,6 +373,8 @@</span><span style="color:#E1E4E8;"> SYSCALL_DEFINE3(faccessat, int, dfd, const char __user *, filename, int, mode)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	int res;</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW;</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if (mode &amp; ~S_IRWXO)	/* where&#39;s F_OK, X_OK, W_OK, R_OK? */</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		return -EINVAL;</span></span></code></pre><pre class="shiki github-light has-diff vp-code-light"><code><span class="line"><span style="color:#005CC5;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#24292E;">index 2ff887661237..e758d7db7663 100644</span></span>
<span class="line"><span style="color:#B31D28;">--- a/fs/open.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/fs/open.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -355,6 +355,9 @@</span><span style="color:#24292E;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#24292E;"> 	return error;</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#22863A;">+			        int *flags);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> /*</span></span>
<span class="line"><span style="color:#24292E;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#24292E;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -370,6 +373,8 @@</span><span style="color:#24292E;"> SYSCALL_DEFINE3(faccessat, int, dfd, const char __user *, filename, int, mode)</span></span>
<span class="line"><span style="color:#24292E;"> 	int res;</span></span>
<span class="line"><span style="color:#24292E;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW;</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> 	if (mode &amp; ~S_IRWXO)	/* where&#39;s F_OK, X_OK, W_OK, R_OK? */</span></span>
<span class="line"><span style="color:#24292E;"> 		return -EINVAL;</span></span></code></pre></div><p>Чтобы включить встроенный в KernelSU безопасный режим, необходимо также изменить <code>input_handle_event</code> в файле <code>drivers/input/input.c</code>:</p><div class="tip custom-block"><p class="custom-block-title">TIP</p><p>Настоятельно рекомендуется включить эту функцию, она очень помогает предотвратить циклическую загрузку!</p></div><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#E1E4E8;">index 45306f9ef247..815091ebfca4 100755</span></span>
<span class="line"><span style="color:#FDAEB7;">--- a/drivers/input/input.c</span></span>
<span class="line"><span style="color:#85E89D;">+++ b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#B392F0;font-weight:bold;">@@ -367,10 +367,13 @@</span><span style="color:#E1E4E8;"> static int input_get_disposition(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 	return disposition;</span></span>
<span class="line"><span style="color:#E1E4E8;"> }</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#85E89D;">+extern bool ksu_input_hook __read_mostly;</span></span>
<span class="line"><span style="color:#85E89D;">+extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#E1E4E8;"> static void input_handle_event(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#E1E4E8;"> 			       unsigned int type, unsigned int code, int value)</span></span>
<span class="line"><span style="color:#E1E4E8;"> {</span></span>
<span class="line"><span style="color:#E1E4E8;">	int disposition = input_get_disposition(dev, type, code, &amp;value);</span></span>
<span class="line"><span style="color:#85E89D;">+</span></span>
<span class="line"><span style="color:#85E89D;">+	if (unlikely(ksu_input_hook))</span></span>
<span class="line"><span style="color:#85E89D;">+		ksu_handle_input_handle_event(&amp;type, &amp;code, &amp;value);</span></span>
<span class="line"><span style="color:#E1E4E8;"> </span></span>
<span class="line"><span style="color:#E1E4E8;"> 	if (disposition != INPUT_IGNORE_EVENT &amp;&amp; type != EV_SYN)</span></span>
<span class="line"><span style="color:#E1E4E8;"> 		add_input_randomness(type, code, value);</span></span></code></pre><pre class="shiki github-light has-diff vp-code-light"><code><span class="line"><span style="color:#005CC5;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#24292E;">index 45306f9ef247..815091ebfca4 100755</span></span>
<span class="line"><span style="color:#B31D28;">--- a/drivers/input/input.c</span></span>
<span class="line"><span style="color:#22863A;">+++ b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#6F42C1;font-weight:bold;">@@ -367,10 +367,13 @@</span><span style="color:#24292E;"> static int input_get_disposition(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#24292E;"> 	return disposition;</span></span>
<span class="line"><span style="color:#24292E;"> }</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#22863A;">+extern bool ksu_input_hook __read_mostly;</span></span>
<span class="line"><span style="color:#22863A;">+extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#24292E;"> static void input_handle_event(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#24292E;"> 			       unsigned int type, unsigned int code, int value)</span></span>
<span class="line"><span style="color:#24292E;"> {</span></span>
<span class="line"><span style="color:#24292E;">	int disposition = input_get_disposition(dev, type, code, &amp;value);</span></span>
<span class="line"><span style="color:#22863A;">+</span></span>
<span class="line"><span style="color:#22863A;">+	if (unlikely(ksu_input_hook))</span></span>
<span class="line"><span style="color:#22863A;">+		ksu_handle_input_handle_event(&amp;type, &amp;code, &amp;value);</span></span>
<span class="line"><span style="color:#24292E;"> </span></span>
<span class="line"><span style="color:#24292E;"> 	if (disposition != INPUT_IGNORE_EVENT &amp;&amp; type != EV_SYN)</span></span>
<span class="line"><span style="color:#24292E;"> 		add_input_randomness(type, code, value);</span></span></code></pre></div><p>Наконец, снова соберите ядро, KernelSU должен работать хорошо.</p>`,37),t=[e];function o(c,i,r,E,d,f){return n(),a("div",null,t)}const u=s(p,[["render",o]]);export{_ as __pageData,u as default};
