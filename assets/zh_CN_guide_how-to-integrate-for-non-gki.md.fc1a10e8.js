import{_ as s,o as n,c as a,Q as l}from"./chunks/framework.ec8f7e8e.js";const _=JSON.parse('{"title":"如何为非 GKI 内核集成 KernelSU","description":"","frontmatter":{},"headers":[],"relativePath":"zh_CN/guide/how-to-integrate-for-non-gki.md","filePath":"zh_CN/guide/how-to-integrate-for-non-gki.md"}'),p={name:"zh_CN/guide/how-to-integrate-for-non-gki.md"},e=l(`<h1 id="introduction" tabindex="-1">如何为非 GKI 内核集成 KernelSU <a class="header-anchor" href="#introduction" aria-label="Permalink to &quot;如何为非 GKI 内核集成 KernelSU {#introduction}&quot;">​</a></h1><p>KernelSU 可以被集成到非 GKI 内核中，现在它最低支持到内核 4.14 版本；理论上也可以支持更低的版本。</p><p>由于非 GKI 内核的碎片化极其严重，因此通常没有统一的方法来编译它，所以我们也无法为非 GKI 设备提供 boot 镜像。但你完全可以自己集成 KernelSU 然后编译内核使用。</p><p>首先，你必须有能力从你设备的内核源码编译出一个可以开机并且能正常使用的内核，如果内核不开源，这通常难以做到。</p><p>如果你已经做好了上述准备，那有两个方法来集成 KernelSU 到你的内核之中。</p><ol><li>借助 <code>kprobe</code> 自动集成</li><li>手动修改内核源码</li></ol><h2 id="using-kprobes" tabindex="-1">使用 kprobe 集成 <a class="header-anchor" href="#using-kprobes" aria-label="Permalink to &quot;使用 kprobe 集成 {#using-kprobes}&quot;">​</a></h2><p>KernelSU 使用 kprobe 机制来做内核的相关 hook，如果 <em>kprobe</em> 可以在你编译的内核中正常运行，那么推荐用这个方法来集成。</p><p>首先，把 KernelSU 添加到你的内核源码树，在内核的根目录执行以下命令：</p><ul><li>最新tag(稳定版本)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">-</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#032F62;">-</span></span></code></pre></div><ul><li>main分支(开发版本)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-s</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">main</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-s</span><span style="color:#24292E;"> </span><span style="color:#032F62;">main</span></span></code></pre></div><ul><li>指定tag(比如v0.5.2)</li></ul><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-s</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">v0.5.2</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-s</span><span style="color:#24292E;"> </span><span style="color:#032F62;">v0.5.2</span></span></code></pre></div><p>然后，你需要检查你的内核是否开启了 <em>kprobe</em> 相关的配置，如果没有开启，需要添加以下配置：</p><div class="language- vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#e1e4e8;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#e1e4e8;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#e1e4e8;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#24292e;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#24292e;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#24292e;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>最后，重新编译你的内核即可。</p><p>如果你发现KPROBES仍未生效，很有可能是因为它的依赖项<code>CONFIG_MODULES</code>没有被启用（如果还是未生效请键入<code>make menuconfig</code>搜索KPROBES 的其它依赖并启用 ）</p><p>如果你在集成 KernelSU 之后手机无法启动，那么很可能你的内核中 <strong>kprobe 工作不正常</strong>，你需要修复这个 bug 或者用第二种方法。</p><div class="tip custom-block"><p class="custom-block-title">如何验证是否是 kprobe 的问题？</p><p>注释掉 <code>KernelSU/kernel/ksu.c</code> 中 <code>ksu_enable_sucompat()</code> 和 <code>ksu_enable_ksud()</code>，如果正常开机，那么就是 kprobe 的问题；或者你可以手动尝试使用 kprobe 功能，如果不正常，手机会直接重启。</p></div><h2 id="modify-kernel-source-code" tabindex="-1">手动修改内核源码 <a class="header-anchor" href="#modify-kernel-source-code" aria-label="Permalink to &quot;手动修改内核源码 {#modify-kernel-source-code}&quot;">​</a></h2><p>如果 kprobe 工作不正常（通常是上游的 bug 或者内核版本过低），那你可以尝试这种方法：</p><p>首先，把 KernelSU 添加到你的内核源码树，在内核的根目录执行以下命令：</p><div class="language-sh vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki github-dark vp-code-dark"><code><span class="line"><span style="color:#B392F0;">curl</span><span style="color:#E1E4E8;"> </span><span style="color:#79B8FF;">-LSs</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#E1E4E8;"> </span><span style="color:#F97583;">|</span><span style="color:#E1E4E8;"> </span><span style="color:#B392F0;">bash</span><span style="color:#E1E4E8;"> </span><span style="color:#9ECBFF;">-</span></span></code></pre><pre class="shiki github-light vp-code-light"><code><span class="line"><span style="color:#6F42C1;">curl</span><span style="color:#24292E;"> </span><span style="color:#005CC5;">-LSs</span><span style="color:#24292E;"> </span><span style="color:#032F62;">&quot;https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh&quot;</span><span style="color:#24292E;"> </span><span style="color:#D73A49;">|</span><span style="color:#24292E;"> </span><span style="color:#6F42C1;">bash</span><span style="color:#24292E;"> </span><span style="color:#032F62;">-</span></span></code></pre></div><p>然后，手动修改内核源码，你可以参考下面这个 patch:</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
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
<span class="line"><span style="color:#24292E;"> 		return -EINVAL;</span></span></code></pre></div><p>主要是要改四个地方：</p><ol><li>do_faccessat，通常位于 <code>fs/open.c</code></li><li>do_execveat_common，通常位于 <code>fs/exec.c</code></li><li>vfs_read，通常位于 <code>fs/read_write.c</code></li><li>vfs_statx，通常位于 <code>fs/stat.c</code></li></ol><p>如果你的内核没有 <code>vfs_statx</code>, 使用 <code>vfs_fstatat</code> 来代替它：</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
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
<span class="line"><span style="color:#24292E;"> 		goto out;</span></span></code></pre></div><p>对于早于 4.17 的内核，如果没有 <code>do_faccessat</code>，可以直接找到 <code>faccessat</code> 系统调用的定义然后修改：</p><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/fs/open.c b/fs/open.c</span></span>
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
<span class="line"><span style="color:#24292E;"> 		return -EINVAL;</span></span></code></pre></div><p>要使用 KernelSU 内置的安全模式，你还需要修改 <code>drivers/input/input.c</code> 中的 <code>input_handle_event</code> 方法：</p><div class="tip custom-block"><p class="custom-block-title">TIP</p><p>强烈建议开启此功能，对用户救砖会非常有帮助！</p></div><div class="language-diff vp-adaptive-theme"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki github-dark has-diff vp-code-dark"><code><span class="line"><span style="color:#79B8FF;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
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
<span class="line"><span style="color:#24292E;"> 		add_input_randomness(type, code, value);</span></span></code></pre></div><p>改完之后重新编译内核即可。</p>`,37),t=[e];function o(c,i,r,E,d,f){return n(),a("div",null,t)}const u=s(p,[["render",o]]);export{_ as __pageData,u as default};
