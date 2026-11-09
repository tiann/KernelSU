package me.weishu.kernelsu.data.registry

import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.intOrNull

open class RegistryException(message: String, cause: Throwable? = null) : Exception(message, cause)

/** The document is not valid JSON or violates the protocol. */
class RegistryFormatException(message: String, cause: Throwable? = null) : RegistryException(message, cause)

/** The document declares a schemaVersion this client does not understand. */
class UnsupportedSchemaException(val schemaVersion: Int?) :
    RegistryException("Unsupported registry schemaVersion: ${schemaVersion ?: "missing"}")

/** Documents of two different builds were observed even after re-reading the index. */
class BuildMismatchException(message: String) : RegistryException(message)

/** The module is not part of the current index (e.g. suspended or removed). */
class ModuleNotInIndexException(val moduleId: String) : RegistryException("Module $moduleId is not in the registry index")

/**
 * Parses and validates registry documents. Unknown fields are ignored, unknown schema
 * versions are rejected, and entries that fail validation are dropped instead of failing the
 * whole document where that is safe (single modules in the list, single releases).
 */
class RegistryParser(private val json: Json = DEFAULT_JSON) {

    companion object {
        val DEFAULT_JSON = Json {
            ignoreUnknownKeys = true
            isLenient = false
            explicitNulls = false
        }

        private val BUILD_ID = Regex("^[0-9a-f]{16,64}$")
        private val MODULE_ID = Regex("^[a-zA-Z][a-zA-Z0-9._-]+$")
        private val SHA256 = Regex("^[0-9a-f]{64}$")
        private val GIT_SHA = Regex("^[0-9a-f]{40}$")
        private val FULL_NAME = Regex("^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$")
        /** GitHub "owner/name"; it becomes a link, so "." / ".." names are refused. */
        private val STARS_REPOSITORY = Regex("^[A-Za-z0-9-]+/(?!\\.\\.?$)[A-Za-z0-9._-]+$")

        fun isValidModuleId(id: String): Boolean = id.length in 2..64 && MODULE_ID.matches(id)
        fun isValidBuildId(id: String): Boolean = BUILD_ID.matches(id)

        /** Unknown kinds are hidden; a script can never be a metamodule (a manifest error upstream). */
        fun isSupportedKind(kind: String, metamodule: Boolean): Boolean =
            RegistryKind.isKnown(kind) && !(kind == RegistryKind.SCRIPT && metamodule)
    }

    private fun parseObject(text: String): JsonObject {
        val element = try {
            json.parseToJsonElement(text)
        } catch (e: SerializationException) {
            throw RegistryFormatException("Malformed JSON", e)
        } catch (e: IllegalArgumentException) {
            throw RegistryFormatException("Malformed JSON", e)
        }
        val obj = element as? JsonObject ?: throw RegistryFormatException("Expected a JSON object")
        val version = (obj["schemaVersion"] as? JsonPrimitive)?.takeIf { !it.isString }?.intOrNull
        if (version != REGISTRY_API_SCHEMA_VERSION) throw UnsupportedSchemaException(version)
        return obj
    }

    private fun <T> decode(deserializer: kotlinx.serialization.DeserializationStrategy<T>, element: JsonElement): T =
        try {
            json.decodeFromJsonElement(deserializer, element)
        } catch (e: SerializationException) {
            throw RegistryFormatException("Invalid registry document: ${e.message}", e)
        } catch (e: IllegalArgumentException) {
            throw RegistryFormatException("Invalid registry document: ${e.message}", e)
        }

    private fun requireBuildId(buildId: String) {
        if (!isValidBuildId(buildId)) throw RegistryFormatException("Invalid buildId: $buildId")
    }

    fun parseIndex(text: String): ApiIndex {
        val index = decode(ApiIndex.serializer(), parseObject(text))
        requireBuildId(index.buildId)
        if (!RegistryUrls.isValidApiPath(index.modules)) throw RegistryFormatException("Invalid modules path")
        return index
    }

    fun parseModules(text: String): ApiModules {
        val obj = parseObject(text)
        val buildId = (obj["buildId"] as? JsonPrimitive)?.takeIf { it.isString }?.content
            ?: throw RegistryFormatException("Missing buildId")
        requireBuildId(buildId)
        val array = obj["modules"] as? JsonArray ?: throw RegistryFormatException("Missing modules array")
        val seen = HashSet<String>()
        val modules = array.mapNotNull { element ->
            val summary = runCatching { decode(ApiModuleSummary.serializer(), sanitizeStars(element)) }.getOrNull()
                ?.let(::validateSummary)
                ?: return@mapNotNull null
            if (!seen.add(summary.id.lowercase())) return@mapNotNull null
            summary
        }
        return ApiModules(schemaVersion = REGISTRY_API_SCHEMA_VERSION, buildId = buildId, modules = modules)
    }

    fun parseDetail(text: String): ApiModuleDetail {
        val obj = parseObject(text)
        // Releases are decoded one by one so a single bad entry is dropped, not the whole detail.
        val releaseElements = when (val r = obj["releases"]) {
            null -> emptyList()
            is JsonArray -> r.toList()
            else -> throw RegistryFormatException("releases must be an array")
        }
        val detail = decode(
            ApiModuleDetail.serializer(),
            JsonObject(sanitizeStars(obj) + ("releases" to JsonArray(emptyList()))),
        )
        requireBuildId(detail.buildId)
        if (!isValidModuleId(detail.id)) throw RegistryFormatException("Invalid module id: ${detail.id}")
        if (!isSupportedKind(detail.kind, detail.metamodule)) throw RegistryFormatException("Unsupported kind: ${detail.kind}")
        if (detail.stable.versionCode < 1) throw RegistryFormatException("Invalid stable versionCode")
        if (detail.source.host != "github") throw RegistryFormatException("Unsupported source host: ${detail.source.host}")
        val fullName = detail.source.fullName
        if (!FULL_NAME.matches(fullName)) throw RegistryFormatException("Invalid source fullName: $fullName")
        val seenTags = HashSet<String>()
        val releases = releaseElements.mapNotNull { element ->
            val release = runCatching { decode(ApiRelease.serializer(), element) }.getOrNull()
                ?.let { sanitizeRelease(it) }
                ?: return@mapNotNull null
            // LazyColumn keys are tags: the first (newest) entry of a tag wins.
            if (!seenTags.add(release.tag)) return@mapNotNull null
            release
        }
        return detail.copy(
            icon = detail.icon?.takeIf(RegistryUrls::isValidApiPath),
            readme = detail.readme?.takeIf(RegistryUrls::isValidApiPath),
            releases = releases,
            homepage = detail.homepage?.takeIf(RegistryUrls::isWebUrl),
            support = detail.support?.takeIf(RegistryUrls::isWebUrl),
            authors = detail.authors.map { a -> a.copy(url = a.url?.takeIf(RegistryUrls::isHttpsUrl)) },
            // Never open a non-https source link; the GitHub URL is derivable from fullName.
            source = detail.source.copy(
                url = detail.source.url.takeIf(RegistryUrls::isHttpsUrl) ?: "https://github.com/$fullName"
            ),
        )
    }

    /**
     * `stars` / `starsRepository` are optional display data: a malformed value is dropped
     * instead of rejecting the module (decoding a wrongly typed field would fail the entry).
     */
    private fun sanitizeStars(element: JsonElement): JsonObject {
        val obj = element as? JsonObject ?: throw RegistryFormatException("Expected a JSON object")
        if ("stars" !in obj && "starsRepository" !in obj) return obj
        val stars = (obj["stars"] as? JsonPrimitive)?.takeIf { !it.isString }?.intOrNull?.takeIf { it >= 0 }
        val repo = (obj["starsRepository"] as? JsonPrimitive)?.takeIf { it.isString }?.content
            ?.takeIf { STARS_REPOSITORY.matches(it) }
        return JsonObject(
            obj - "stars" - "starsRepository" +
                listOfNotNull(stars?.let { "stars" to JsonPrimitive(it) }, repo?.let { "starsRepository" to JsonPrimitive(it) })
        )
    }

    private fun validateSummary(s: ApiModuleSummary): ApiModuleSummary? {
        if (!isValidModuleId(s.id)) return null
        if (!RegistryUrls.isValidApiPath(s.detail)) return null
        if (s.stable.versionCode < 1) return null
        if (!isSupportedKind(s.kind, s.metamodule)) return null
        return s.copy(
            icon = s.icon?.takeIf(RegistryUrls::isValidApiPath),
            authors = s.authors.map { a -> a.copy(url = a.url?.takeIf(RegistryUrls::isHttpsUrl)) },
        )
    }

    // The ZIP URL is not tied to source.fullName: it is fixed when the release is listed and
    // keeps the old owner/repo after a rename or transfer. The download is pinned by SHA-256.
    private fun sanitizeRelease(r: ApiRelease): ApiRelease? {
        val valid = r.tag.isNotBlank() &&
            r.versionCode >= 1 &&
            r.immutable &&
            r.zip.size > 0 &&
            SHA256.matches(r.zip.sha256) &&
            GIT_SHA.matches(r.tagCommit) &&
            RegistryUrls.isHttpsUrl(r.releaseUrl) &&
            RegistryUrls.isAllowedReleaseDownloadUrl(r.zip.url)
        if (!valid) return null
        return r.copy(attestationUrl = r.attestationUrl?.takeIf(RegistryUrls::isHttpsUrl))
    }
}
