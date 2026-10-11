package me.weishu.kernelsu.data.registry

import kotlinx.serialization.Serializable

/*
 * Client protocol of KernelSU Modules ({baseUrl}/api/v1/...).
 *
 * Mirrors packages/schema/src/api.ts of KernelSU-Modules/tools. Every path inside these
 * documents is relative to the site root (baseUrl). Unknown fields are ignored; an unknown
 * schemaVersion is rejected (see [RegistryParser]).
 */

const val REGISTRY_API_SCHEMA_VERSION = 1

@Serializable
data class ApiIndex(
    val schemaVersion: Int,
    val buildId: String,
    val repoCommit: String,
    val generatedAt: String,
    /** "api/v1/modules.json" */
    val modules: String,
)

@Serializable
data class ApiVersionRef(
    val version: String,
    val versionCode: Int,
    val publishedAt: String,
)

@Serializable
data class ApiAuthor(
    val name: String,
    val url: String? = null,
)

@Serializable
data class ApiModuleSummary(
    val id: String,
    /** Module content is single-language; no locale fallback is applied. */
    val name: String = "",
    val summary: String = "",
    val authors: List<ApiAuthor> = emptyList(),
    val tags: List<String> = emptyList(),
    val metamodule: Boolean = false,
    /** Content-hashed icon, "assets/{hash}.{ext}". */
    val icon: String? = null,
    val stable: ApiVersionRef,
    /** GitHub stargazer count of [starsRepository], or null when unknown. */
    val stars: Int? = null,
    /** "owner/name" of the repository [stars] belongs to (may differ from the source repository). */
    val starsRepository: String? = null,
    /** "api/v1/modules/{id}.json" */
    val detail: String,
    val updatedAt: String,
)

@Serializable
data class ApiModules(
    val schemaVersion: Int,
    val buildId: String,
    val modules: List<ApiModuleSummary>,
)

@Serializable
data class ApiZip(
    val url: String,
    val name: String,
    val size: Long,
    val sha256: String,
)

@Serializable
data class ApiRelease(
    val tag: String,
    val name: String? = null,
    val version: String,
    val versionCode: Int,
    val prerelease: Boolean,
    val publishedAt: String,
    /** Sanitized HTML of the release notes, or null. */
    val changelogHtml: String? = null,
    val zip: ApiZip,
    val tagCommit: String,
    val releaseUrl: String,
    val attestationUrl: String? = null,
    val immutable: Boolean,
)

@Serializable
data class ApiCompatibility(
    val androidMinApi: Int? = null,
    val androidMaxApi: Int? = null,
    val architectures: List<String>? = null,
)

@Serializable
data class ApiSource(
    val host: String,
    val fullName: String,
    val url: String,
    val commit: String,
)

@Serializable
data class ApiHealth(
    val status: String,
    val lastSuccessAt: String? = null,
)

@Serializable
data class ApiModuleDetail(
    val schemaVersion: Int,
    val buildId: String,
    val id: String,
    val name: String = "",
    val summary: String = "",
    val authors: List<ApiAuthor> = emptyList(),
    val tags: List<String> = emptyList(),
    val metamodule: Boolean = false,
    val icon: String? = null,
    val stable: ApiVersionRef,
    val stars: Int? = null,
    val starsRepository: String? = null,
    val detail: String,
    val updatedAt: String,
    val license: String? = null,
    val homepage: String? = null,
    val support: String? = null,
    val compatibility: ApiCompatibility? = null,
    val source: ApiSource,
    /** "api/v1/modules/{id}/readme" (an HTML fragment), or null when the module has no README. */
    val readme: String? = null,
    /** Newest versionCode first; the first non-prerelease entry equals `stable`. */
    val releases: List<ApiRelease> = emptyList(),
    val health: ApiHealth? = null,
)

/** A consistent pair of index + module list taken from the same build. */
data class RegistryCatalog(
    val index: ApiIndex,
    val modules: List<ApiModuleSummary>,
    /** True when this copy came from the on-disk cache instead of the network. */
    val fromCache: Boolean,
) {
    val buildId: String get() = index.buildId
    private val byId: Map<String, ApiModuleSummary> by lazy { modules.associateBy { it.id } }
    fun find(id: String): ApiModuleSummary? = byId[id]
}
