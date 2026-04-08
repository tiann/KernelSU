(function() {
    if (window.ksu_download_enabled) return;
    window.ksu_download_enabled = true;
    const blobMap = new Map();
    const originalCreateObjectURL = URL.createObjectURL;
    URL.createObjectURL = (obj) => {
        const url = originalCreateObjectURL(obj);
        if (obj instanceof Blob) blobMap.set(url, obj);
        return url;
    };
    const originalRevokeObjectURL = URL.revokeObjectURL;
    URL.revokeObjectURL = (url) => {
        setTimeout(() => blobMap.delete(url), 10000);
        return originalRevokeObjectURL(url);
    };
    const handleDownload = async (anchor) => {
        const url = new URL(anchor.href, location.href);
        const fileName = anchor.download || url.pathname.split("/").pop().split("?")[0] || "download.bin";
        const isInternal = url.hostname === 'mui.kernelsu.org';
        if (url.protocol === 'blob:' || url.protocol === 'data:' || isInternal) {
            const blob = (url.protocol === 'blob:' && blobMap.has(url.href)) ? blobMap.get(url.href) : await (await fetch(url.href, { credentials: 'include' })).blob();
            const base64 = await new Promise((resolve, reject) => {
                const reader = new FileReader();
                reader.onload = () => resolve(reader.result.split(',')[1] || '');
                reader.onerror = () => reject(reader.error || new Error('Failed to read blob'));
                reader.readAsDataURL(blob);
            });
            ksu_download.save(base64, fileName);
            return;
        }
        ksu_download.download(url.href, fileName, anchor.type || null);
    };
    document.addEventListener('click', (event) => {
        const anchor = event.target.closest('a[download]');
        if (!anchor || !anchor.href) return;
        event.preventDefault();
        handleDownload(anchor).catch((error) => console.error('KernelSU download failed', error));
    }, true);
    const originalClick = HTMLAnchorElement.prototype.click;
    HTMLAnchorElement.prototype.click = function() {
        if (this.hasAttribute('download') && this.href) {
            handleDownload(this).catch((error) => console.error('KernelSU download failed', error));
            return;
        }
        return originalClick.apply(this, arguments);
    };
})();
