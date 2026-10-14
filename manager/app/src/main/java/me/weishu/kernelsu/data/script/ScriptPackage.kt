package me.weishu.kernelsu.data.script

import org.apache.commons.compress.archivers.zip.ZipArchiveEntry
import org.apache.commons.compress.archivers.zip.ZipFile
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.util.UUID
import java.util.zip.ZipInputStream

class ScriptPackageException(val reason: Reason, message: String, cause: Throwable? = null) : IOException(message, cause) {
    enum class Reason {
        /** Not a readable ZIP (or an unsupported compression / encryption). */
        CORRUPT,

        /** Absolute path, `..`, symlink, special file or duplicate entry. */
        UNSAFE_ENTRY,
        TOO_LARGE,
        MISSING_MODULE_PROP,
        NOT_ONESHOT,
        INVALID_ID,
        ID_MISMATCH,
        MISSING_ACTION,
        INVALID_PARAMS,
    }
}

/** module.prop of a one-shot script. */
data class ScriptManifest(
    val id: String,
    val name: String,
    val version: String,
    val versionCode: Int?,
    val author: String,
    val description: String,
)

/**
 * Validation and extraction of one-shot script ZIPs. Only the central directory is trusted;
 * entries must be plain files or directories below the root, and everything written is
 * counted against [MAX_TOTAL_BYTES] while decompressing (declared sizes are not trusted).
 */
object ScriptPackage {
    const val MODULE_PROP = "module.prop"
    const val ACTION = ScriptCommand.ENTRY
    const val PARAMS = "params.json"

    const val MAX_TOTAL_BYTES = 64L * 1024 * 1024
    const val MAX_ENTRIES = 4096
    const val MAX_PROP_BYTES = 64 * 1024
    const val MAX_PARAMS_BYTES = 64 * 1024

    private val MODULE_ID = Regex("^[a-zA-Z][a-zA-Z0-9._-]+$")
    private const val S_IFMT = 0xF000
    private const val S_IFREG = 0x8000
    private const val S_IFDIR = 0x4000

    /** Same rule as ksud's `metamodule`: trimmed value is "1" or ASCII case-insensitive "true". */
    fun isOneshot(value: String?): Boolean {
        val v = value?.trim() ?: return false
        return v == "1" || v.equals("true", ignoreCase = true)
    }

    fun isValidModuleId(id: String): Boolean = id.length in 2..64 && MODULE_ID.matches(id)

    /** `key=value` lines; `#` comments and lines without `=` are skipped, the first key wins. */
    fun parseModuleProp(text: String): Map<String, String> {
        val props = LinkedHashMap<String, String>()
        text.lineSequence().forEach { raw ->
            val line = raw.trimEnd('\r')
            if (line.trimStart().startsWith("#")) return@forEach
            val eq = line.indexOf('=')
            if (eq <= 0) return@forEach
            val key = line.substring(0, eq).trim()
            if (key.isNotEmpty() && key !in props) props[key] = line.substring(eq + 1).trim()
        }
        return props
    }

    fun manifestOf(props: Map<String, String>): ScriptManifest {
        val id = props["id"].orEmpty()
        return ScriptManifest(
            id = id,
            name = props["name"].orEmpty().ifBlank { id },
            version = props["version"].orEmpty(),
            versionCode = props["versionCode"]?.trim()?.toIntOrNull(),
            author = props["author"].orEmpty(),
            description = props["description"].orEmpty(),
        )
    }

    /**
     * Throws unless [name] is a safe relative path: no absolute path, no `.`/`..`/empty
     * segment, no backslash or NUL. A trailing `/` marks a directory.
     */
    fun checkEntryName(name: String) {
        fun unsafe(): Nothing = throw ScriptPackageException(ScriptPackageException.Reason.UNSAFE_ENTRY, "Unsafe entry: $name")
        if (name.isEmpty() || name.startsWith("/") || '\\' in name || '\u0000' in name) unsafe()
        val segments = name.removeSuffix("/").split('/')
        if (segments.any { it.isEmpty() || it == "." || it == ".." }) unsafe()
    }

    /**
     * Reads module.prop from a ZIP stream to tell whether it is a one-shot script; null if it
     * is not, or cannot be read. Only a hint: [validate] is what decides before running.
     */
    fun readOneshotManifest(input: InputStream): ScriptManifest? = try {
        ZipInputStream(input).use { zip ->
            var manifest: ScriptManifest? = null
            for (i in 0 until MAX_ENTRIES) {
                val entry = zip.nextEntry ?: break
                if (entry.name == MODULE_PROP) {
                    val props = parseModuleProp(readLimited(zip, MAX_PROP_BYTES).toString(Charsets.UTF_8))
                    if (isOneshot(props["oneshot"])) manifest = manifestOf(props)
                    break
                }
            }
            manifest
        }
    } catch (e: IOException) {
        null
    } catch (e: IllegalArgumentException) {
        null
    }

    private fun readLimited(input: InputStream, limit: Int): ByteArray {
        val out = ByteArrayOutputStream()
        val buf = ByteArray(8192)
        while (true) {
            val n = input.read(buf)
            if (n == -1) break
            out.write(buf, 0, n)
            if (out.size() > limit) throw ScriptPackageException(ScriptPackageException.Reason.TOO_LARGE, "Entry exceeds $limit bytes")
        }
        return out.toByteArray()
    }

    private fun open(zip: File): ZipFile = try {
        ZipFile.builder().setFile(zip).get()
    } catch (e: IOException) {
        throw ScriptPackageException(ScriptPackageException.Reason.CORRUPT, "Not a valid ZIP: ${e.message}", e)
    }

    private fun isDirectory(entry: ZipArchiveEntry): Boolean = entry.name.endsWith("/")

    /** Checks every entry of the central directory; returns the entries by name. */
    private fun checkEntries(file: ZipFile): Map<String, ZipArchiveEntry> {
        val byName = LinkedHashMap<String, ZipArchiveEntry>()
        var declared = 0L
        for (entry in file.entries) {
            if (byName.size >= MAX_ENTRIES) {
                throw ScriptPackageException(ScriptPackageException.Reason.TOO_LARGE, "More than $MAX_ENTRIES entries")
            }
            checkEntryName(entry.name)
            if (entry.isUnixSymlink) {
                throw ScriptPackageException(ScriptPackageException.Reason.UNSAFE_ENTRY, "Symlink entry: ${entry.name}")
            }
            if (entry.platform == ZipArchiveEntry.PLATFORM_UNIX) {
                val type = entry.unixMode and S_IFMT
                val expected = if (isDirectory(entry)) S_IFDIR else S_IFREG
                if (type != 0 && type != expected) {
                    throw ScriptPackageException(ScriptPackageException.Reason.UNSAFE_ENTRY, "Special file entry: ${entry.name}")
                }
            }
            val key = entry.name.removeSuffix("/")
            if (byName.put(key, entry) != null) {
                throw ScriptPackageException(ScriptPackageException.Reason.UNSAFE_ENTRY, "Duplicate entry: ${entry.name}")
            }
            if (!isDirectory(entry)) {
                if (!file.canReadEntryData(entry)) {
                    throw ScriptPackageException(ScriptPackageException.Reason.CORRUPT, "Unsupported entry: ${entry.name}")
                }
                if (entry.size > 0) declared += entry.size
                if (declared > MAX_TOTAL_BYTES) {
                    throw ScriptPackageException(ScriptPackageException.Reason.TOO_LARGE, "Contents exceed $MAX_TOTAL_BYTES bytes")
                }
            }
        }
        return byName
    }

    private fun readEntry(file: ZipFile, entry: ZipArchiveEntry, limit: Int): String =
        file.getInputStream(entry).use { readLimited(it, limit) }.toString(Charsets.UTF_8)

    /**
     * Validates a script ZIP: safe entries, module.prop with `oneshot`, a valid `id` (equal to
     * [expectedId] when given), a root action.sh and, if present, a valid params.json.
     */
    fun validate(zip: File, expectedId: String? = null): ScriptManifest = open(zip).use { file ->
        val entries = checkEntries(file)
        val propEntry = entries[MODULE_PROP]?.takeIf { !isDirectory(it) }
            ?: throw ScriptPackageException(ScriptPackageException.Reason.MISSING_MODULE_PROP, "module.prop not found")
        val props = parseModuleProp(readEntry(file, propEntry, MAX_PROP_BYTES))
        if (!isOneshot(props["oneshot"])) {
            throw ScriptPackageException(ScriptPackageException.Reason.NOT_ONESHOT, "module.prop does not declare oneshot=true")
        }
        val manifest = manifestOf(props)
        if (!isValidModuleId(manifest.id)) {
            throw ScriptPackageException(ScriptPackageException.Reason.INVALID_ID, "Invalid id: ${manifest.id}")
        }
        if (expectedId != null && manifest.id != expectedId) {
            throw ScriptPackageException(ScriptPackageException.Reason.ID_MISMATCH, "id ${manifest.id} does not match $expectedId")
        }
        if (entries[ACTION]?.takeIf { !isDirectory(it) } == null) {
            throw ScriptPackageException(ScriptPackageException.Reason.MISSING_ACTION, "action.sh not found")
        }
        entries[PARAMS]?.takeIf { !isDirectory(it) }?.let { parseParams(readEntry(file, it, MAX_PARAMS_BYTES)) }
        manifest
    }

    fun parseParams(text: String): ScriptParams = try {
        ScriptParamsParser.parse(text)
    } catch (e: ScriptParamsException) {
        throw ScriptPackageException(ScriptPackageException.Reason.INVALID_PARAMS, "Invalid params.json: ${e.message}", e)
    }

    /**
     * Extracts [zip] to [dest] (replacing it). Files are written to a sibling temporary
     * directory first, so [dest] only ever holds a complete extraction.
     */
    fun extract(zip: File, dest: File) {
        val parent = dest.absoluteFile.parentFile ?: throw IOException("No parent for $dest")
        if (!parent.isDirectory && !parent.mkdirs()) throw IOException("Cannot create $parent")
        val tmp = File(parent, ".tmp-${UUID.randomUUID()}")
        try {
            open(zip).use { file ->
                val entries = checkEntries(file)
                if (!tmp.mkdir()) throw IOException("Cannot create $tmp")
                val root = tmp.canonicalFile
                var total = 0L
                val buf = ByteArray(64 * 1024)
                for (entry in entries.values) {
                    val target = File(root, entry.name.removeSuffix("/"))
                    // Defence in depth on top of checkEntryName.
                    if (!target.canonicalPath.startsWith(root.path + File.separator)) {
                        throw ScriptPackageException(ScriptPackageException.Reason.UNSAFE_ENTRY, "Unsafe entry: ${entry.name}")
                    }
                    if (isDirectory(entry)) {
                        if (!target.isDirectory && !target.mkdirs()) throw IOException("Cannot create ${entry.name}")
                        continue
                    }
                    target.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw IOException("Cannot create ${entry.name}") }
                    file.getInputStream(entry).use { input ->
                        FileOutputStream(target).use { out ->
                            while (true) {
                                val n = input.read(buf)
                                if (n == -1) break
                                total += n
                                if (total > MAX_TOTAL_BYTES) {
                                    throw ScriptPackageException(
                                        ScriptPackageException.Reason.TOO_LARGE,
                                        "Contents exceed $MAX_TOTAL_BYTES bytes"
                                    )
                                }
                                out.write(buf, 0, n)
                            }
                        }
                    }
                }
            }
            if (dest.exists() && !dest.deleteRecursively()) throw IOException("Cannot replace $dest")
            if (!tmp.renameTo(dest)) throw IOException("Cannot move extraction to $dest")
        } finally {
            if (tmp.exists()) tmp.deleteRecursively()
        }
    }
}
