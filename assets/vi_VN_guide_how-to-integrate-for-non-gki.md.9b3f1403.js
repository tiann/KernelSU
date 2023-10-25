import{_ as s,o as n,c as a,O as l}from"./chunks/framework.43781440.js";const A=JSON.parse('{"title":"Làm thế nào để tích hợp KernelSU vào thiết bị không sử dụng GKI ?","description":"","frontmatter":{},"headers":[],"relativePath":"vi_VN/guide/how-to-integrate-for-non-gki.md","filePath":"vi_VN/guide/how-to-integrate-for-non-gki.md"}'),e={name:"vi_VN/guide/how-to-integrate-for-non-gki.md"},p=l(`<h1 id="lam-the-nao-đe-tich-hop-kernelsu-vao-thiet-bi-khong-su-dung-gki" tabindex="-1">Làm thế nào để tích hợp KernelSU vào thiết bị không sử dụng GKI ? <a class="header-anchor" href="#lam-the-nao-đe-tich-hop-kernelsu-vao-thiet-bi-khong-su-dung-gki" aria-label="Permalink to &quot;Làm thế nào để tích hợp KernelSU vào thiết bị không sử dụng GKI ?&quot;">​</a></h1><p>KernelSU có thể được tích hợp vào kernel không phải GKI và hiện tại nó đã được backport xuống 4.14, thậm chí cũng có thể chạy trên kernel thấp hơn 4.14.</p><p>Do các kernel không phải GKI bị phân mảnh nên chúng tôi không có cách build thống nhất, vì vậy chúng tôi không thể cung cấp các boot image không phải GKI. Nhưng bạn có thể tự build kernel với KernelSU được tích hợp vào.</p><p>Đầu tiên, bạn phải build được kernel từ nguồn có khả năng boot được , nếu kernel không có mã nguồn mở thì rất khó để chạy KernelSU cho thiết bị của bạn.</p><p>Nếu bạn có thể build kernel khởi động được, có hai cách để tích hợp KernelSU vào mã nguồn kernel:</p><ol><li>Tự động với <code>kprobe</code></li><li>Thủ công</li></ol><h2 id="tich-hop-vao-kprobe" tabindex="-1">Tích hợp vào kprobe <a class="header-anchor" href="#tich-hop-vao-kprobe" aria-label="Permalink to &quot;Tích hợp vào kprobe&quot;">​</a></h2><p>KernelSU sử dụng kprobe để thực hiện hook kernel, nếu <em>kprobe</em> chạy tốt trong kernel của bạn thì nên sử dụng cách này.</p><p>Đầu tiên, thêm KernelSU vào mã nguồn kernel của bạn:</p><ul><li>Thẻ mới nhất (ổn định)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><ul><li>Nhánh chính (dev)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">main</span></span></code></pre></div><ul><li>Chọn thẻ (chẳng hạn như v0.5.2)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">v0.5.2</span></span></code></pre></div><p>Sau đó, bạn nên kiểm tra xem <em>kprobe</em> có được bật trong config của bạn hay không, nếu không, vui lòng thêm các cấu hình sau vào:</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>Rồi build lại kernel của bạn, KernelSU sẽ hoạt động ok.</p><p>Trong trường hợp kprobe chưa được bật, bạn có thể thêm <code>CONFIG_MODULES=y</code> vào kernel config. (Nếu vẫn không có hiệu lực thì hãy sử dụng <code>make menuconfig</code> rồi tìm các thành phần khác mà kprobe phụ thuộc).</p><p>Nhưng nếu bạn gặp bootloop khi tích hợp KernelSU thì có khả năng <em><strong>kprobe bị hỏng trong kernel</strong></em>, bạn nên fix lỗi kprobe trong mã nguồn hoặc dùng cách 2.</p><h2 id="chinh-sua-ma-nguon-kernel-thu-cong" tabindex="-1">Chỉnh sửa mã nguồn kernel thủ công <a class="header-anchor" href="#chinh-sua-ma-nguon-kernel-thu-cong" aria-label="Permalink to &quot;Chỉnh sửa mã nguồn kernel thủ công&quot;">​</a></h2><p>Nếu kprobe không thể hoạt động trong kernel của bạn (có thể là lỗi do upstream hoặc kernel dưới bản 4.8), thì bạn có thể thử cách này:</p><p>Đầu tiên, thêm KernelSU vào mã nguồn kernel của bạn:</p><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><p>Sau đó, thêm lệnh gọi KernelSU vào mã nguồn kernel, đây là một patch bạn có thể tham khảo:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>Bạn sẽ tìm thấy bốn chức năng trong mã nguồn kernel:</p><ol><li>do_faccessat, thường là trong <code>fs/open.c</code></li><li>do_execveat_common, thường nằm trong <code>fs/exec.c</code></li><li>vfs_read, thường nằm trong <code>fs/read_write.c</code></li><li>vfs_statx, thường có trong <code>fs/stat.c</code></li></ol><p>Cuối cùng, chỉnh sửa <code>KernelSU/kernel/ksu.c</code> và bỏ <code>enable_sucompat()</code> sau đó xây dựng lại kernel của bạn, KernelSU sẽ hoạt động tốt.</p>`,29),t=[p];function o(c,r,i,C,h,D){return n(),a("div",null,t)}const d=s(e,[["render",o]]);export{A as __pageData,d as default};
