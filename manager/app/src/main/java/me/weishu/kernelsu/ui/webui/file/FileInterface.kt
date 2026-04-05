package me.weishu.kernelsu.ui.webui.file

import android.util.Log
import android.webkit.JavascriptInterface
import com.topjohnwu.superuser.io.SuFile
import java.io.BufferedOutputStream

class FileInterface(private val path: String) {
    private val suFile: SuFile = SuFile(path)

    @JavascriptInterface
    fun exists(): Boolean = runCatching { suFile.exists() }.getOrDefault(false)

    @JavascriptInterface
    fun isFile(): Boolean = runCatching { suFile.isFile }.getOrDefault(false)

    @JavascriptInterface
    fun isDirectory(): Boolean = runCatching { suFile.isDirectory }.getOrDefault(false)

    @JavascriptInterface
    fun canRead(): Boolean = runCatching { suFile.canRead() }.getOrDefault(false)

    @JavascriptInterface
    fun canWrite(): Boolean = runCatching { suFile.canWrite() }.getOrDefault(false)

    @JavascriptInterface
    fun canExecute(): Boolean = runCatching { suFile.canExecute() }.getOrDefault(false)

    @JavascriptInterface
    fun createNewFile(): Boolean = runCatching { suFile.createNewFile() }.getOrDefault(false)

    @JavascriptInterface
    fun delete(): Boolean = runCatching { suFile.delete() }.getOrDefault(false)

    @JavascriptInterface
    fun deleteRecursive(): Boolean = runCatching { suFile.deleteRecursive() }.getOrDefault(false)

    @JavascriptInterface
    fun mkdir(): Boolean = runCatching { suFile.mkdir() }.getOrDefault(false)

    @JavascriptInterface
    fun mkdirs(): Boolean = runCatching { suFile.mkdirs() }.getOrDefault(false)

    @JavascriptInterface
    fun renameTo(destPath: String): Boolean = runCatching {
        suFile.renameTo(SuFile(destPath))
    }.getOrDefault(false)

    @JavascriptInterface
    fun list(): Array<String> = runCatching { suFile.list() ?: emptyArray() }.getOrDefault(emptyArray())

    @JavascriptInterface
    fun listFiles(): Array<String> = runCatching {
        suFile.listFiles()?.map { it.absolutePath }?.toTypedArray() ?: emptyArray()
    }.getOrDefault(emptyArray())

    @JavascriptInterface
    fun length(): Long = runCatching { suFile.length() }.getOrDefault(-1L)

    @JavascriptInterface
    fun lastModified(): Long = runCatching { suFile.lastModified() }.getOrDefault(-1L)

    @JavascriptInterface
    fun setLastModified(time: Long): Boolean = runCatching { suFile.setLastModified(time) }.getOrDefault(false)

    @JavascriptInterface
    fun getAbsolutePath(): String = runCatching { suFile.absolutePath }.getOrDefault(path)

    @JavascriptInterface
    fun getCanonicalPath(): String = runCatching { suFile.canonicalPath }.getOrDefault(path)

    @JavascriptInterface
    fun getParent(): String? = runCatching { suFile.parent }.getOrNull()

    @JavascriptInterface
    fun getPath(): String = path

    @JavascriptInterface
    fun getName(): String = runCatching { suFile.name }.getOrDefault("")

    @JavascriptInterface
    fun isHidden(): Boolean = runCatching { suFile.isHidden }.getOrDefault(false)

    @JavascriptInterface
    fun isBlock(): Boolean = runCatching { suFile.isBlock }.getOrDefault(false)

    @JavascriptInterface
    fun isCharacter(): Boolean = runCatching { suFile.isCharacter }.getOrDefault(false)

    @JavascriptInterface
    fun isSymlink(): Boolean = runCatching { suFile.isSymlink }.getOrDefault(false)

    @JavascriptInterface
    fun createNewSymlink(target: String): Boolean = runCatching {
        suFile.createNewSymlink(target)
    }.getOrDefault(false)

    @JavascriptInterface
    fun createNewLink(existing: String): Boolean = runCatching {
        suFile.createNewLink(existing)
    }.getOrDefault(false)

    @JavascriptInterface
    fun clear(): Boolean = runCatching { suFile.clear() }.getOrDefault(false)

    @JavascriptInterface
    fun setReadOnly(): Boolean = runCatching { suFile.setReadOnly() }.getOrDefault(false)

    @JavascriptInterface
    fun setReadable(readable: Boolean, ownerOnly: Boolean): Boolean =
        runCatching { suFile.setReadable(readable, ownerOnly) }.getOrDefault(false)

    @JavascriptInterface
    fun setWritable(writable: Boolean, ownerOnly: Boolean): Boolean =
        runCatching { suFile.setWritable(writable, ownerOnly) }.getOrDefault(false)

    @JavascriptInterface
    fun setExecutable(executable: Boolean, ownerOnly: Boolean): Boolean =
        runCatching { suFile.setExecutable(executable, ownerOnly) }.getOrDefault(false)

    @JavascriptInterface
    fun getFreeSpace(): Long = runCatching { suFile.freeSpace }.getOrDefault(-1L)

    @JavascriptInterface
    fun getTotalSpace(): Long = runCatching { suFile.totalSpace }.getOrDefault(-1L)

    @JavascriptInterface
    fun getUsableSpace(): Long = runCatching { suFile.usableSpace }.getOrDefault(-1L)

    @JavascriptInterface
    fun newInputStream(): String {
        return runCatching {
            val stream = ManagedInputStream(suFile.newInputStream())
            val id = java.util.UUID.randomUUID().toString()
            KsuIO.openInputStreams[id] = stream
            id
        }.onFailure { Log.e(TAG, "newInputStream failed", it) }.getOrElse { "" }
    }

    @JavascriptInterface
    fun newOutputStream(append: Boolean): String {
        return runCatching {
            val stream = suFile.newOutputStream(append)
            val id = java.util.UUID.randomUUID().toString()
            KsuIO.openOutputStreams[id] = BufferedOutputStream(stream, ManagedInputStream.MAX_CHUNK_SIZE)
            id
        }.onFailure { Log.e(TAG, "newOutputStream failed", it) }.getOrElse { "" }
    }

    @JavascriptInterface
    fun newOutputStream(): String = newOutputStream(false)

    override fun toString(): String = path
}
