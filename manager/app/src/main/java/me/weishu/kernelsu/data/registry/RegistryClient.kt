package me.weishu.kernelsu.data.registry

import kotlinx.serialization.Serializable
import java.io.IOException

/** Fetches a document; throws [IOException] on network / HTTP errors. */
fun interface RegistryFetcher {
    @Throws(IOException::class)
    fun fetch(url: String): String
}

/** Small key/value store for the last complete good copies. Keys are file-name safe. */
interface RegistryCache {
    fun read(key: String): String?
    fun write(key: String, value: String)
}

/**
 * Registry protocol client: index -> module list -> detail on demand.
 *
 * - Documents from different builds are never combined: when a detail's buildId differs from
 *   the list's, the index and list are re-read once; if they still disagree the load fails.
 * - The last complete, consistent copy is kept in [cache] and used when the network or
 *   parsing fails.
 *
 * Blocking; call from a background thread.
 */
class RegistryClient(
    baseUrl: String = RegistryUrls.DEFAULT_BASE_URL,
    private val fetcher: RegistryFetcher,
    private val cache: RegistryCache,
    private val parser: RegistryParser = RegistryParser(),
) {
    val baseUrl: String = RegistryUrls.normalizeBaseUrl(baseUrl)

    @Volatile
    private var current: RegistryCatalog? = null
    private val lock = Any()

    data class CatalogLoad(
        val catalog: RegistryCatalog,
        /** Non-null when the network copy could not be used and a cached copy was returned. */
        val error: Throwable? = null,
    )

    data class DetailLoad(
        /** Catalog the detail belongs to; may be newer than the one the caller held. */
        val catalog: RegistryCatalog,
        val detail: ApiModuleDetail,
        val fromCache: Boolean = false,
        val error: Throwable? = null,
    )

    data class Readme(val html: String)

    @Serializable
    private data class CachedCatalog(val index: String, val modules: String)

    fun resolve(path: String): String = RegistryUrls.resolve(baseUrl, path)

    fun currentCatalog(): RegistryCatalog? = current

    /** Reads index + list from the network, falling back to the last complete good copy. */
    fun loadCatalog(): CatalogLoad = synchronized(lock) {
        try {
            CatalogLoad(fetchCatalog())
        } catch (e: Exception) {
            val fallback = current ?: readCachedCatalog()
            if (fallback != null) {
                current = fallback
                CatalogLoad(fallback, e)
            } else {
                throw e
            }
        }
    }

    /** Returns the catalog in memory, or loads one (network, then cache). */
    fun catalog(): RegistryCatalog = current ?: loadCatalog().catalog

    private fun fetchCatalog(): RegistryCatalog {
        var lastError: Exception? = null
        repeat(2) {
            val indexText = fetcher.fetch(resolve(RegistryUrls.INDEX_PATH))
            val index = parser.parseIndex(indexText)
            val modulesText = fetcher.fetch(resolve(index.modules))
            val modules = parser.parseModules(modulesText)
            if (modules.buildId == index.buildId) {
                val catalog = RegistryCatalog(index, modules.modules, fromCache = false)
                current = catalog
                runCatching {
                    cache.write(
                        CATALOG_KEY,
                        RegistryParser.DEFAULT_JSON.encodeToString(CachedCatalog.serializer(), CachedCatalog(indexText, modulesText))
                    )
                }
                return catalog
            }
            lastError = BuildMismatchException("index ${index.buildId} != modules ${modules.buildId}")
        }
        throw lastError!!
    }

    private fun readCachedCatalog(): RegistryCatalog? = runCatching {
        val text = cache.read(CATALOG_KEY) ?: return null
        val cached = RegistryParser.DEFAULT_JSON.decodeFromString(CachedCatalog.serializer(), text)
        val index = parser.parseIndex(cached.index)
        val modules = parser.parseModules(cached.modules)
        if (modules.buildId != index.buildId) return null
        RegistryCatalog(index, modules.modules, fromCache = true)
    }.getOrNull()

    private fun fetchDetail(summary: ApiModuleSummary): Pair<String, ApiModuleDetail> {
        val text = fetcher.fetch(resolve(summary.detail))
        val detail = parser.parseDetail(text)
        if (detail.id != summary.id) throw RegistryFormatException("Detail id ${detail.id} != ${summary.id}")
        return text to detail
    }

    /**
     * Loads a module detail consistent with the catalog. Throws [ModuleNotInIndexException]
     * when the module is not part of the current index.
     */
    fun loadDetail(id: String): DetailLoad {
        // A catalog loaded right here is as fresh as a re-read would be.
        var refreshed = current == null
        val catalog = catalog()
        val summary = catalog.find(id) ?: throw ModuleNotInIndexException(id)
        try {
            val (text, detail) = fetchDetail(summary)
            if (detail.buildId == catalog.buildId) {
                writeDetail(id, text)
                return DetailLoad(catalog, detail)
            }
            // The site was redeployed in between: re-read index + list once, never mix builds.
            refreshed = true
            val fresh = synchronized(lock) { fetchCatalog() }
            val freshSummary = fresh.find(id) ?: throw ModuleNotInIndexException(id)
            val (freshText, freshDetail) =
                if (detail.buildId == fresh.buildId) text to detail else fetchDetail(freshSummary)
            if (freshDetail.buildId != fresh.buildId) {
                throw BuildMismatchException("detail ${freshDetail.buildId} != modules ${fresh.buildId}")
            }
            writeDetail(id, freshText)
            return DetailLoad(fresh, freshDetail)
        } catch (e: ModuleNotInIndexException) {
            throw e
        } catch (e: Exception) {
            // The detail may be gone because the module was removed in a newer build: re-read
            // index + list once. Offline, the cached fallback below still applies.
            if (!refreshed) {
                val fresh = runCatching { synchronized(lock) { fetchCatalog() } }.getOrNull()
                if (fresh != null) {
                    val freshSummary = fresh.find(id) ?: throw ModuleNotInIndexException(id)
                    if (fresh.buildId != catalog.buildId) {
                        runCatching { fetchDetail(freshSummary) }.getOrNull()
                            ?.takeIf { (_, detail) -> detail.buildId == fresh.buildId }
                            ?.let { (text, detail) ->
                                writeDetail(id, text)
                                return DetailLoad(fresh, detail)
                            }
                    }
                }
            }
            // A refresh above may already have advanced the catalog: only fall back to a cached
            // detail of the build the client is on now, for a module that is still listed there.
            val latest = current ?: catalog
            if (latest.find(id) == null) throw e
            val cached = readCachedDetail(id)?.takeIf { it.buildId == latest.buildId && it.id == id }
                ?: throw e
            return DetailLoad(latest, cached, fromCache = true, error = e)
        }
    }

    /** Loads the README HTML, or null if the module has none. Falls back to a cached copy of the same build. */
    fun loadReadme(detail: ApiModuleDetail): Readme? {
        val path = detail.readme ?: return null
        val key = "readme-${detail.id}"
        return try {
            val html = fetcher.fetch(resolve(path))
            runCatching { cache.write(key, detail.buildId + "\n" + html) }
            Readme(html)
        } catch (e: Exception) {
            val cached = runCatching { cache.read(key) }.getOrNull() ?: throw e
            val buildId = cached.substringBefore('\n')
            if (buildId != detail.buildId) throw e
            Readme(cached.substringAfter('\n'))
        }
    }

    private fun writeDetail(id: String, text: String) {
        runCatching { cache.write("detail-$id", text) }
    }

    private fun readCachedDetail(id: String): ApiModuleDetail? = runCatching {
        cache.read("detail-$id")?.let(parser::parseDetail)
    }.getOrNull()

    private companion object {
        const val CATALOG_KEY = "catalog"
    }
}
