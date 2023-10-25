import{_ as s,o as n,c as a,O as l}from"./chunks/framework.43781440.js";const d=JSON.parse('{"title":"如何為非 GKI 核心整合 KernelSU","description":"","frontmatter":{},"headers":[],"relativePath":"zh_TW/guide/how-to-integrate-for-non-gki.md","filePath":"zh_TW/guide/how-to-integrate-for-non-gki.md"}'),p={name:"zh_TW/guide/how-to-integrate-for-non-gki.md"},e=l(`<h1 id="introduction" tabindex="-1">如何為非 GKI 核心整合 KernelSU <a class="header-anchor" href="#introduction" aria-label="Permalink to &quot;如何為非 GKI 核心整合 KernelSU {#introduction}&quot;">​</a></h1><p>KernelSU 可以被整合到非 GKI 核心中，現在它最低支援到核心 4.14 版本；理論上也可以支援更低的版本。</p><p>由於非 GKI 核心的片段化極其嚴重，因此通常沒有統一的方法來建置它，所以我們也無法為非 GKI 裝置提供 Boot 映像。但您完全可以自行整合 KernelSU 並建置核心以繼續使用。</p><p>首先，您必須有能力從您裝置的核心原始碼建置出一個可以開機並且能夠正常使用的核心，如果核心並非開放原始碼，這通常難以做到。</p><p>如果您已經做好了上述準備，那有兩個方法來將 KernelSU 整合至您的核心之中。</p><ol><li>藉助 <code>kprobe</code> 自動整合</li><li>手動修改核心原始碼</li></ol><h2 id="using-kprobes" tabindex="-1">使用 kprobe 整合 <a class="header-anchor" href="#using-kprobes" aria-label="Permalink to &quot;使用 kprobe 整合 {#using-kprobes}&quot;">​</a></h2><p>KernelSU 使用 kprobe 機制來處理核心的相關 hook，如果 <em>kprobe</em> 可以在您建置的核心中正常運作，那麼建議使用這個方法進行整合。</p><p>首先，把 KernelSU 新增至您的核心來源樹狀結構，再核心的根目錄執行以下命令：</p><ul><li>最新 tag (稳定版本)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><ul><li>main 分支(開發版本)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">main</span></span></code></pre></div><ul><li>選取 tag (例如 v0.5.2)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">v0.5.2</span></span></code></pre></div><p>然後，您需要檢查您的核心是否啟用 <em>kprobe</em> 相關組態，如果未啟用，則需要新增以下組態：</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>最後，重新建置您的核心即可。</p><p>如果您發現 KPROBES 仍未生效，很有可能是因為它的相依性 <code>CONFIG_MODULES</code> 並未被啟用 (如果還是未生效請輸入 <code>make menuconfig</code> 搜尋 KPROBES 的其他相依性並啟用)</p><p>如果您在整合 KernelSU 之後手機無法啟動，那麼很可能您的核心中 <strong>kprobe 無法正常運作</strong>，您需要修正這個錯誤，或者使用第二種方法。</p><div class="tip custom-block"><p class="custom-block-title">如何檢查 kprobe 是否損毀？</p><p>將 <code>KernelSU/kernel/ksu.c</code> 中的 <code>ksu_enable_sucompat()</code> 和 <code>ksu_enable_ksud()</code> 取消註解，如果正常開機，即 kprobe 已損毀；或者您可以手動嘗試使用 kprobe 功能，如果不正常，手機會直接重新啟動。</p></div><h2 id="modify-kernel-source-code" tabindex="-1">手動修改核心原始碼 <a class="header-anchor" href="#modify-kernel-source-code" aria-label="Permalink to &quot;手動修改核心原始碼 {#modify-kernel-source-code}&quot;">​</a></h2><p>如果 kprobe 無法正常運作 (可能是上游的錯誤或核心版本過低)，那您可以嘗試這種方法：</p><p>首先，將 KernelSU 新增至您的原始碼樹狀結構，再核心的根目錄執行以下命令：</p><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><p>然後，手動修改核心原始碼，您可以參閱下方的 patch：</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>主要修改四個項目：</p><ol><li>do_faccessat，通常位於 <code>fs/open.c</code></li><li>do_execveat_common，通常位於 <code>fs/exec.c</code></li><li>vfs_read，通常位於 <code>fs/read_write.c</code></li><li>vfs_statx，通常位於 <code>fs/stat.c</code></li></ol><p>如果您的核心沒有 <code>vfs_statx</code>，使用 <code>vfs_fstatat</code> 將其取代：</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		goto out;</span></span></code></pre></div><p>對於早於 4.17 的核心，如果沒有 <code>do_faccessat</code>，可以直接找到 <code>faccessat</code> 系統呼叫的定義並進行修改：</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/open.c b/fs/open.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>若要啟用 KernelSU 內建的安全模式，您還需要修改 <code>drivers/input/input.c</code> 中的 <code>input_handle_event</code> 方法：</p><div class="tip custom-block"><p class="custom-block-title">TIP</p><p>強烈建議啟用此功能，如果遇到開機迴圈，這將會非常有用！</p></div><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		add_input_randomness(type, code, value);</span></span></code></pre></div><p>最後，再次建置您的核心，KernelSU 將會如期運作。</p>`,37),o=[e];function t(c,r,i,C,D,A){return n(),a("div",null,o)}const f=s(p,[["render",t]]);export{d as __pageData,f as default};
