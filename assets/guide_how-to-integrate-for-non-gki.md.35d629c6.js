import{_ as s,o as n,c as a,O as e}from"./chunks/framework.43781440.js";const A=JSON.parse('{"title":"How to integrate KernelSU for non GKI kernels?","description":"","frontmatter":{},"headers":[],"relativePath":"guide/how-to-integrate-for-non-gki.md","filePath":"guide/how-to-integrate-for-non-gki.md"}'),l={name:"guide/how-to-integrate-for-non-gki.md"},p=e(`<h1 id="how-to-integrate-kernelsu-for-non-gki-kernels" tabindex="-1">How to integrate KernelSU for non GKI kernels? <a class="header-anchor" href="#how-to-integrate-kernelsu-for-non-gki-kernels" aria-label="Permalink to &quot;How to integrate KernelSU for non GKI kernels?&quot;">​</a></h1><p>KernelSU can be integrated into non GKI kernels, and was backported to 4.14 and below.</p><p>Due to the fragmentization of non GKI kernels, we do not have a uniform way to build it, so we can not provide non GKI boot images. But you can build the kernel yourself with KernelSU integrated.</p><p>First, you should be able to build a bootable kernel from kernel source code. If the kernel is not open source, then it is difficult to run KernelSU for your device.</p><p>If you can build a bootable kernel, there are two ways to integrate KernelSU to the kernel source code:</p><ol><li>Automatically with <code>kprobe</code></li><li>Manually</li></ol><h2 id="integrate-with-kprobe" tabindex="-1">Integrate with kprobe <a class="header-anchor" href="#integrate-with-kprobe" aria-label="Permalink to &quot;Integrate with kprobe&quot;">​</a></h2><p>KernelSU uses kprobe to do kernel hooks, if the <em>kprobe</em> runs well in your kernel, it is recommended to use this way.</p><p>First, add KernelSU to your kernel source tree:</p><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><p>Then, you should check if <em>kprobe</em> is enabled in your kernel config, if it is not, please add these configs to it:</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>And build your kernel again, KernelSU should works well.</p><p>If you find that KPROBES is still not activated, you can try enabling <code>CONFIG_MODULES</code>. (If it still doesn&#39;t take effect, use <code>make menuconfig</code> to search for other dependencies of KPROBES)</p><p>But if you encounter a boot loop when integrated KernelSU, it is maybe <em>kprobe is broken in your kernel</em>, you should fix the kprobe bug or use the second way.</p><div class="tip custom-block"><p class="custom-block-title">How to check if kprobe is broken？</p><p>comment out <code>ksu_enable_sucompat()</code> and <code>ksu_enable_ksud()</code> in <code>KernelSU/kernel/ksu.c</code>, if the device boots normally, then kprobe may be broken.</p></div><h2 id="manually-modify-the-kernel-source" tabindex="-1">Manually modify the kernel source <a class="header-anchor" href="#manually-modify-the-kernel-source" aria-label="Permalink to &quot;Manually modify the kernel source&quot;">​</a></h2><p>If kprobe does not work in your kernel (may be an upstream or kernel bug below 4.8), then you can try this way:</p><p>First, add KernelSU to your kernel source tree:</p><ul><li>Latest tag(stable)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><ul><li>main branch(dev)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">main</span></span></code></pre></div><ul><li>Select tag(Such as v0.5.2)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">v0.5.2</span></span></code></pre></div><p>Then, add KernelSU calls to the kernel source, here is a patch to refer:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index ac59664eaecf..bdd585e1d2cc 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/exec.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/exec.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -1890,11 +1890,14 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> static int __do_execve_file(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	return retval;</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern bool ksu_execveat_hook __read_mostly;</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">			void *envp, int *flags);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">				 void *argv, void *envp, int *flags);</span></span>
<span class="line"><span style="color:#A6ACCD;"> static int do_execveat_common(int fd, struct filename *filename,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 			      struct user_arg_ptr argv,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 			      struct user_arg_ptr envp,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 			      int flags)</span></span>
<span class="line"><span style="color:#A6ACCD;"> {</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	if (unlikely(ksu_execveat_hook))</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">		ksu_handle_execveat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	else</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">		ksu_handle_execveat_sucompat(&amp;fd, &amp;filename, &amp;argv, &amp;envp, &amp;flags);</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	return __do_execve_file(fd, filename, argv, envp, flags, NULL);</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#A6ACCD;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 05036d819197..965b84d486b8 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/open.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/open.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -348,6 +348,8 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	return ksys_fallocate(fd, mode, offset, len);</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">			 int *flags);</span></span>
<span class="line"><span style="color:#A6ACCD;"> /*</span></span>
<span class="line"><span style="color:#A6ACCD;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#A6ACCD;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -355,6 +357,7 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#A6ACCD;">  */</span></span>
<span class="line"><span style="color:#A6ACCD;"> long do_faccessat(int dfd, const char __user *filename, int mode)</span></span>
<span class="line"><span style="color:#A6ACCD;"> {</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	const struct cred *old_cred;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	struct cred *override_cred;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	struct path path;</span></span>
<span class="line"><span style="color:#A6ACCD;">diff --git a/fs/read_write.c b/fs/read_write.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 650fc7e0f3a6..55be193913b6 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/read_write.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/read_write.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -434,10 +434,14 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> EXPORT_SYMBOL(kernel_read);</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern bool ksu_vfs_read_hook __read_mostly;</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">			size_t *count_ptr, loff_t **pos);</span></span>
<span class="line"><span style="color:#A6ACCD;"> ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)</span></span>
<span class="line"><span style="color:#A6ACCD;"> {</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	ssize_t ret;</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	if (unlikely(ksu_vfs_read_hook))</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">		ksu_handle_vfs_read(&amp;file, &amp;buf, &amp;count, &amp;pos);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if (!(file-&gt;f_mode &amp; FMODE_READ))</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		return -EBADF;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if (!(file-&gt;f_mode &amp; FMODE_CAN_READ))</span></span>
<span class="line"><span style="color:#A6ACCD;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 376543199b5a..82adcef03ecc 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/stat.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/stat.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -148,6 +148,8 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> int vfs_statx_fd(unsigned int fd, struct kstat *stat,</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> EXPORT_SYMBOL(vfs_statx_fd);</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> /**</span></span>
<span class="line"><span style="color:#A6ACCD;">  * vfs_statx - Get basic and extra attributes by filename</span></span>
<span class="line"><span style="color:#A6ACCD;">  * @dfd: A file descriptor representing the base dir for a relative filename</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -170,6 +172,7 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> int vfs_statx(int dfd, const char __user *filename, int flags,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flags);</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if ((flags &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		       AT_EMPTY_PATH | KSTAT_QUERY_FLAGS)) != 0)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>You should find the four functions in kernel source:</p><ol><li>do_faccessat, usually in <code>fs/open.c</code></li><li>do_execveat_common, usually in <code>fs/exec.c</code></li><li>vfs_read, usually in <code>fs/read_write.c</code></li><li>vfs_statx, usually in <code>fs/stat.c</code></li></ol><p>If your kernel does not have the <code>vfs_statx</code>, use <code>vfs_fstatat</code> instead:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 068fdbcc9e26..5348b7bb9db2 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/stat.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/stat.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -87,6 +87,8 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> int vfs_fstat(unsigned int fd, struct kstat *stat)</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> EXPORT_SYMBOL(vfs_fstat);</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		int flag)</span></span>
<span class="line"><span style="color:#A6ACCD;"> {</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -94,6 +96,8 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	int error = -EINVAL;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	unsigned int lookup_flags = 0;</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	ksu_handle_stat(&amp;dfd, &amp;filename, &amp;flag);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if ((flag &amp; ~(AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT |</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		      AT_EMPTY_PATH)) != 0)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		goto out;</span></span></code></pre></div><p>For kernels eariler than 4.17, if you cannot find <code>do_faccessat</code>, just go to the definition of the <code>faccessat</code> syscall and place the call there:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/open.c b/fs/open.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 2ff887661237..e758d7db7663 100644</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/fs/open.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/fs/open.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -355,6 +355,9 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> SYSCALL_DEFINE4(fallocate, int, fd, int, mode, loff_t, offset, loff_t, len)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	return error;</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">			        int *flags);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> /*</span></span>
<span class="line"><span style="color:#A6ACCD;">  * access() needs to use the real uid/gid, not the effective uid/gid.</span></span>
<span class="line"><span style="color:#A6ACCD;">  * We do this by temporarily clearing all FS-related capabilities and</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -370,6 +373,8 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> SYSCALL_DEFINE3(faccessat, int, dfd, const char __user *, filename, int, mode)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	int res;</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	unsigned int lookup_flags = LOOKUP_FOLLOW;</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	ksu_handle_faccessat(&amp;dfd, &amp;filename, &amp;mode, NULL);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if (mode &amp; ~S_IRWXO)	/* where&#39;s F_OK, X_OK, W_OK, R_OK? */</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>To enable KernelSU&#39;s builtin SafeMode, You should also modify <code>input_handle_event</code> in <code>drivers/input/input.c</code>:</p><div class="tip custom-block"><p class="custom-block-title">TIP</p><p>It is strongly recommended to enable this feature, it is very helpful to prevent bootloops!</p></div><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#A6ACCD;">index 45306f9ef247..815091ebfca4 100755</span></span>
<span class="line"><span style="color:#89DDFF;">---</span><span style="color:#A6ACCD;"> a/drivers/input/input.c</span></span>
<span class="line"><span style="color:#89DDFF;">+++</span><span style="color:#A6ACCD;"> b/drivers/input/input.c</span></span>
<span class="line"><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> -367,10 +367,13 </span><span style="color:#89DDFF;">@@</span><span style="color:#A6ACCD;"> static int input_get_disposition(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 	return disposition;</span></span>
<span class="line"><span style="color:#A6ACCD;"> }</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern bool ksu_input_hook __read_mostly;</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">extern int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#A6ACCD;"> static void input_handle_event(struct input_dev *dev,</span></span>
<span class="line"><span style="color:#A6ACCD;"> 			       unsigned int type, unsigned int code, int value)</span></span>
<span class="line"><span style="color:#A6ACCD;"> {</span></span>
<span class="line"><span style="color:#A6ACCD;">	int disposition = input_get_disposition(dev, type, code, &amp;value);</span></span>
<span class="line"><span style="color:#89DDFF;">+</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">	if (unlikely(ksu_input_hook))</span></span>
<span class="line"><span style="color:#89DDFF;">+</span><span style="color:#C3E88D;">		ksu_handle_input_handle_event(&amp;type, &amp;code, &amp;value);</span></span>
<span class="line"><span style="color:#A6ACCD;"> </span></span>
<span class="line"><span style="color:#A6ACCD;"> 	if (disposition != INPUT_IGNORE_EVENT &amp;&amp; type != EV_SYN)</span></span>
<span class="line"><span style="color:#A6ACCD;"> 		add_input_randomness(type, code, value);</span></span></code></pre></div><p>Finally, build your kernel again, KernelSU should work well.</p>`,37),o=[p];function t(c,r,i,C,y,D){return n(),a("div",null,o)}const f=s(l,[["render",t]]);export{A as __pageData,f as default};
