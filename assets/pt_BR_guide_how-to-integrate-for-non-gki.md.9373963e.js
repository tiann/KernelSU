import{_ as s,o as n,c as a,O as e}from"./chunks/framework.43781440.js";const y=JSON.parse('{"title":"Como integrar o KernelSU para kernels não GKI?","description":"","frontmatter":{},"headers":[],"relativePath":"pt_BR/guide/how-to-integrate-for-non-gki.md","filePath":"pt_BR/guide/how-to-integrate-for-non-gki.md"}'),l={name:"pt_BR/guide/how-to-integrate-for-non-gki.md"},p=e(`<h1 id="como-integrar-o-kernelsu-para-kernels-nao-gki" tabindex="-1">Como integrar o KernelSU para kernels não GKI? <a class="header-anchor" href="#como-integrar-o-kernelsu-para-kernels-nao-gki" aria-label="Permalink to &quot;Como integrar o KernelSU para kernels não GKI?&quot;">​</a></h1><p>O KernelSU pode ser integrado em kernels não GKI e foi portado para 4.14 e versões anteriores.</p><p>Devido à fragmentação de kernels não GKI, não temos uma maneira uniforme de construí-lo, portanto não podemos fornecer imagens boot não GKI. Mas você mesmo pode construir o kernel com o KernelSU integrado.</p><p>Primeiro, você deve ser capaz de construir um kernel inicializável a partir do código-fonte do kernel. Se o kernel não for de código aberto, será difícil executar o KernelSU no seu dispositivo.</p><p>Se você puder construir um kernel inicializável, existem duas maneiras de integrar o KernelSU ao código-fonte do kernel:</p><ol><li>Automaticamente com <code>kprobe</code></li><li>Manualmente</li></ol><h2 id="integrar-com-kprobe" tabindex="-1">Integrar com kprobe <a class="header-anchor" href="#integrar-com-kprobe" aria-label="Permalink to &quot;Integrar com kprobe&quot;">​</a></h2><p>O KernelSU usa kprobe para fazer ganchos de kernel, se o kprobe funcionar bem em seu kernel, é recomendado usar desta forma.</p><p>Primeiro, adicione o KernelSU à árvore de origem do kernel:</p><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><p>Então, você deve verificar se o kprobe está ativado na configuração do seu kernel, se não estiver, adicione estas configurações a ele:</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_HAVE_KPROBES=y</span></span>
<span class="line"><span style="color:#A6ACCD;">CONFIG_KPROBE_EVENTS=y</span></span></code></pre></div><p>E construa seu kernel novamente, KernelSU deve funcionar bem.</p><p>Se você descobrir que o KPROBES ainda não está ativado, você pode tentar ativar <code>CONFIG_MODULES</code>. (Se ainda assim não surtir efeito, use <code>make menuconfig</code> para procurar outras dependências do KPROBES)</p><p>Mas se você entrar em um bootloop quando o KernelSU for integrado, talvez o <strong>kprobe esteja quebrado em seu kernel</strong>, você deve corrigir o bug do kprobe ou usar o segundo caminho.</p><div class="tip custom-block"><p class="custom-block-title">COMO VERIFICAR SE O KPROBE ESTÁ QUEBRADO?</p><p>Comente <code>ksu_enable_sucompat()</code> e <code>ksu_enable_ksud()</code> em <code>KernelSU/kernel/ksu.c</code>, se o dispositivo inicializar normalmente, então o kprobe pode estar quebrado.</p></div><h2 id="modifique-manualmente-a-fonte-do-kernel" tabindex="-1">Modifique manualmente a fonte do kernel <a class="header-anchor" href="#modifique-manualmente-a-fonte-do-kernel" aria-label="Permalink to &quot;Modifique manualmente a fonte do kernel&quot;">​</a></h2><p>Se o kprobe não funcionar no seu kernel (pode ser um bug do upstream ou do kernel abaixo de 4.8), então você pode tentar desta forma:</p><p>Primeiro, adicione o KernelSU à árvore de origem do kernel:</p><ul><li>Tag mais recente (estável)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-</span></span></code></pre></div><ul><li>branch principal (dev)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">main</span></span></code></pre></div><ul><li>Selecione a tag (Como v0.5.2)</li></ul><div class="language-sh"><button title="Copy Code" class="copy"></button><span class="lang">sh</span><pre class="shiki material-theme-palenight"><code><span class="line"><span style="color:#FFCB6B;">curl</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-LSs</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">&quot;</span><span style="color:#C3E88D;">https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh</span><span style="color:#89DDFF;">&quot;</span><span style="color:#A6ACCD;"> </span><span style="color:#89DDFF;">|</span><span style="color:#A6ACCD;"> </span><span style="color:#FFCB6B;">bash</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">-s</span><span style="color:#A6ACCD;"> </span><span style="color:#C3E88D;">v0.5.2</span></span></code></pre></div><p>Em seguida, adicione chamadas KernelSU à fonte do kernel. Aqui está um patch para referência:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/exec.c b/fs/exec.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>Você deve encontrar as quatro funções no código-fonte do kernel:</p><ol><li>do_faccessat, geralmente em <code>fs/open.c</code></li><li>do_execveat_common, geralmente em <code>fs/exec.c</code></li><li>vfs_read, geralmente em <code>fs/read_write.c</code></li><li>vfs_statx, geralmente em <code>fs/stat.c</code></li></ol><p>Se o seu kernel não tiver <code>vfs_statx</code>, use <code>vfs_fstatat</code>:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/stat.c b/fs/stat.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		goto out;</span></span></code></pre></div><p>Para kernels anteriores ao 4.17, se você não conseguir encontrar <code>do_faccessat</code>, basta ir até a definição do syscall <code>faccessat</code> e fazer a chamada lá:</p><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/fs/open.c b/fs/open.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		return -EINVAL;</span></span></code></pre></div><p>Para ativar o Modo de Segurança integrado do KernelSU, você também deve modificar <code>input_handle_event</code> em <code>drivers/input/input.c</code>:</p><div class="tip custom-block"><p class="custom-block-title">DICA</p><p>É altamente recomendável ativar este recurso, é muito útil para evitar bootloops!</p></div><div class="language-diff"><button title="Copy Code" class="copy"></button><span class="lang">diff</span><pre class="shiki material-theme-palenight has-diff"><code><span class="line"><span style="color:#A6ACCD;">diff --git a/drivers/input/input.c b/drivers/input/input.c</span></span>
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
<span class="line"><span style="color:#A6ACCD;"> 		add_input_randomness(type, code, value);</span></span></code></pre></div><p>Finalmente, construa seu kernel novamente, e então, o KernelSU deve funcionar bem.</p>`,37),o=[p];function t(c,r,i,d,C,D){return n(),a("div",null,o)}const f=s(l,[["render",t]]);export{y as __pageData,f as default};
