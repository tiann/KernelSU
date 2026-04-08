(function() {
    if (window.ksu_download_enabled) return;
    window.ksu_download_enabled = true;

    const CHUNK_SIZE = 512 * 1024;
    const SIZE_THRESHOLD = 10 * 1024 * 1024;

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

    const readChunk = (blob, start, end) => {
        return new Promise((resolve, reject) => {
            const chunk = blob.slice(start, end);
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result.split(',')[1] || '');
            reader.onerror = () => reject(reader.error || new Error('Failed to read chunk'));
            reader.readAsDataURL(chunk);
        });
    };

    const downloadChunked = async (blob, fileName) => {
        const mimeType = blob.type || 'application/octet-stream';
        const downloadId = ksu_download.startChunkedDownload(fileName, mimeType);

        if (!downloadId) {
            throw new Error('Failed to start chunked download');
        }

        try {
            let offset = 0;
            const size = blob.size;

            while (offset < size) {
                const end = Math.min(offset + CHUNK_SIZE, size);
                const base64Chunk = await readChunk(blob, offset, end);

                if (!ksu_download.writeDownloadChunk(downloadId, base64Chunk)) {
                    throw new Error('Failed to write chunk');
                }

                offset = end;
            }

            if (!ksu_download.completeChunkedDownload(downloadId)) {
                throw new Error('Failed to complete download');
            }

            const a = document.createElement('a');
            a.href = downloadId;
            a.download = fileName;
            a.style.display = 'none';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
        } catch (error) {
            ksu_download.cancelChunkedDownload(downloadId);
            throw error;
        }
    };

    const downloadLegacy = async (blob, fileName) => {
        const base64 = await new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result.split(',')[1] || '');
            reader.onerror = () => reject(reader.error || new Error('Failed to read blob'));
            reader.readAsDataURL(blob);
        });
        ksu_download.save(base64, fileName);
    };

    const handleDownload = async (anchor) => {
        const url = new URL(anchor.href, location.href);
        const fileName = anchor.download || url.pathname.split("/").pop().split("?")[0] || "download.bin";
        const isInternal = url.hostname === 'mui.kernelsu.org';

        if (url.protocol === 'blob:' || url.protocol === 'data:' || isInternal) {
            const blob = (url.protocol === 'blob:' && blobMap.has(url.href))
                ? blobMap.get(url.href)
                : await (await fetch(url.href, { credentials: 'include' })).blob();

            if (blob.size > SIZE_THRESHOLD) {
                await downloadChunked(blob, fileName);
            } else {
                await downloadLegacy(blob, fileName);
            }
            return;
        }

        ksu_download.openExternal(url.href);
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
