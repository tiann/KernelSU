package me.weishu.kernelsu.data.script

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.data.registry.DownloadVerificationException
import me.weishu.kernelsu.data.registry.RegistryDownloadStore
import me.weishu.kernelsu.data.registry.copyForInstall
import me.weishu.kernelsu.ksuApp
import java.io.File
import java.io.IOException
import java.security.MessageDigest
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

/** A validated script, extracted and ready to run. */
data class PreparedScript(
    val manifest: ScriptManifest,
    /** Extraction directory: the script's working directory and `$MODPATH`. */
    val dir: File,
    val sha256: String,
    val params: ScriptParams,
    val actionSource: String,
    val actionSourceTruncated: Boolean,
)

/** The registry ZIP is not (or no longer) a verified download of the store. */
class UnverifiedScriptException : IOException("Registry download is not verified")

object ScriptRepository {
    private const val TAG = "ScriptRepository"
    const val DIR_NAME = "oneshot"
    private const val PREFS = "oneshot_scripts"
    private const val MAX_SOURCE_CHARS = 256 * 1024

    private fun root(): File = File(ksuApp.filesDir, DIR_NAME)

    /** Extraction held by each owner (one run page); everything else under [root] is stale. */
    private val extractions = ConcurrentHashMap<String, File>()

    /** Deletes released extractions off the caller's (often the main) thread. */
    private val ioScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    /** Serialises preparations, so cleanup never sees another preparation's half-done files. */
    private val prepareLock = Mutex()

    val history: ScriptRunHistory by lazy {
        val prefs = ksuApp.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        ScriptRunHistory(object : ScriptRunStorage {
            override fun read(key: String): String? = prefs.getString(key, null)
            override fun write(key: String, value: String) {
                prefs.edit().putString(key, value).apply()
            }
        })
    }

    fun hostInfo(): ScriptHostInfo = ScriptHostInfo(
        // Same value as ksud's VERSION_NAME (git describe without the leading "v").
        ksuVersion = BuildConfig.VERSION_NAME.removePrefix("v"),
        ksuVersionCode = BuildConfig.VERSION_CODE,
        kernelVersionCode = Natives.version,
        lateLoad = Natives.isLateLoadMode,
    )

    /** module.prop of [uri] if it is a one-shot script ZIP, otherwise (or when unreadable) null. */
    suspend fun peekOneshot(uri: Uri): ScriptManifest? = withContext(Dispatchers.IO) {
        runCatching {
            ksuApp.contentResolver.openInputStream(uri)?.use { ScriptPackage.readOneshotManifest(it) }
        }.getOrNull()
    }

    /**
     * Copies, verifies, validates and extracts the ZIP at [uri]. With [registryId] the ZIP must
     * be a verified registry download (re-hashed here like a module install) whose module.prop
     * id equals [registryId]. The extraction belongs to [owner] until [release]; preparing again
     * for the same owner releases its previous one.
     */
    suspend fun prepare(uri: Uri, registryId: String?, owner: String): PreparedScript = withContext(Dispatchers.IO) {
        prepareLock.withLock { prepareLocked(uri, registryId, owner) }
    }

    private fun prepareLocked(uri: Uri, registryId: String?, owner: String): PreparedScript {
        val storeRoot = File(ksuApp.filesDir, RegistryDownloadStore.DIR_NAME)
        val expected = if (uri.scheme == ContentResolver.SCHEME_FILE) {
            uri.path?.let { RegistryDownloadStore.expectedFor(storeRoot, File(it)) }
        } else {
            null
        }
        if (registryId != null && expected == null) throw UnverifiedScriptException()

        val zip = File.createTempFile("oneshot-", ".zip", ksuApp.cacheDir)
        try {
            try {
                val input = ksuApp.contentResolver.openInputStream(uri) ?: throw IOException("Cannot open $uri")
                input.use { copyForInstall(it, zip, expected) }
            } catch (e: DownloadVerificationException) {
                // Corrupted or tampered after download: never keep it around for a retry.
                uri.path?.let { RegistryDownloadStore.delete(storeRoot, File(it)) }
                throw e
            }
            val manifest = ScriptPackage.validate(zip, registryId)
            val sha256 = sha256(zip)
            // One directory per preparation: the same ZIP may be open on two run pages at once.
            val dir = File(root(), "$sha256-${UUID.randomUUID()}")
            extractions.put(owner, dir)?.let(::delete)
            cleanup()
            try {
                ScriptPackage.extract(zip, dir)
                return readPrepared(manifest, dir, sha256)
            } catch (e: Throwable) {
                release(owner)
                throw e
            }
        } finally {
            zip.delete()
        }
    }

    private fun readPrepared(manifest: ScriptManifest, dir: File, sha256: String): PreparedScript {
        val paramsFile = File(dir, ScriptPackage.PARAMS)
        val params = if (paramsFile.isFile) {
            ScriptPackage.parseParams(paramsFile.readText())
        } else {
            ScriptParams.EMPTY
        }
        // Read at most one char past the limit: action.sh itself may be up to the ZIP size cap.
        val source = File(dir, ScriptPackage.ACTION).bufferedReader().use { reader ->
            val buf = CharArray(MAX_SOURCE_CHARS + 1)
            var n = 0
            while (n < buf.size) {
                val read = reader.read(buf, n, buf.size - n)
                if (read == -1) break
                n += read
            }
            String(buf, 0, n)
        }
        return PreparedScript(
            manifest = manifest,
            dir = dir,
            sha256 = sha256,
            params = params,
            actionSource = source.take(MAX_SOURCE_CHARS),
            actionSourceTruncated = source.length > MAX_SOURCE_CHARS,
        )
    }

    /**
     * Drops the extraction of [owner] (page left, or the preparation failed). Not for a running
     * script: the run removes its directory as root, since the script may create root-owned files.
     */
    fun release(owner: String) {
        val dir = extractions.remove(owner) ?: return
        ioScope.launch { delete(dir) }
    }

    private val _runCount = MutableStateFlow(0)

    /** Bumped after every recorded run, so screens showing the last run can refresh. */
    val runCount: StateFlow<Int> = _runCount.asStateFlow()

    fun recordRun(prepared: PreparedScript, exitCode: Int) {
        history.record(
            ScriptRunRecord(
                id = prepared.manifest.id,
                versionCode = prepared.manifest.versionCode,
                time = System.currentTimeMillis(),
                exitCode = exitCode,
            )
        )
        _runCount.update { it + 1 }
    }

    /** Drops extractions no page holds, e.g. left behind by a killed process. */
    private fun cleanup() {
        val held = extractions.values.mapTo(HashSet()) { it.name }
        root().listFiles()?.forEach { f -> if (f.name !in held) delete(f) }
    }

    private fun delete(dir: File) {
        if (dir.exists() && !dir.deleteRecursively()) Log.w(TAG, "cannot delete $dir")
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buf = ByteArray(64 * 1024)
            while (true) {
                val n = input.read(buf)
                if (n == -1) break
                digest.update(buf, 0, n)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}
