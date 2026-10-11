package me.weishu.kernelsu.data.registry

import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * Keeps the last good registry documents on disk. Writes go to a uniquely named temporary
 * file that is fsynced before it atomically replaces the entry, so concurrent writers never
 * share a temp file and a crash never leaves a truncated entry behind.
 */
class FileRegistryCache(private val dir: File) : RegistryCache {

    private fun fileFor(key: String): File =
        File(dir, key.replace(UNSAFE, "_") + ".cache")

    override fun read(key: String): String? {
        val file = fileFor(key)
        return if (file.isFile) file.readText() else null
    }

    override fun write(key: String, value: String) {
        if (!dir.isDirectory && !dir.mkdirs()) throw IOException("Cannot create $dir")
        val target = fileFor(key)
        val tmp = File.createTempFile(target.name + ".", ".tmp", dir)
        try {
            FileOutputStream(tmp).use { out ->
                out.write(value.toByteArray(Charsets.UTF_8))
                out.flush()
                out.fd.sync()
            }
            if (!tmp.renameTo(target)) throw IOException("Failed to write registry cache $key")
        } catch (e: Throwable) {
            tmp.delete()
            throw e
        }
    }

    private companion object {
        val UNSAFE = Regex("[^A-Za-z0-9._-]")
    }
}
