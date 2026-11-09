package me.weishu.kernelsu.data.registry

/** How an `updateJson` update of an installed module is downloaded and verified. */
sealed interface UpdateVerification {
    /** Not a registry update: download as before, without registry checks. */
    data object Legacy : UpdateVerification

    /** The update is a registry release: enforce its URL allowlist, size and SHA-256. */
    data class Registry(
        val zipUrl: String,
        val expected: ExpectedFile,
        val release: ApiRelease,
    ) : UpdateVerification

    /** It claims to come from the registry but cannot be matched to a registry release. */
    data class Rejected(val reason: String) : UpdateVerification
}

/**
 * Decides whether a module update offered by `updateJson` must be verified against the
 * registry.
 *
 * - The module's id is not in the registry index -> [UpdateVerification.Legacy] (Rejected if
 *   updateJson is served by the registry). Index unavailable: treated like "in the index,
 *   detail unavailable".
 * - `zipUrl` is the ZIP of a registry release of that module with the offered versionCode ->
 *   [UpdateVerification.Registry]. If it is a registry release ZIP but no release with that
 *   URL has the offered versionCode -> [UpdateVerification.Rejected] (no downgrade through a
 *   mislabelled ZIP), wherever updateJson is hosted.
 * - `updateJson` itself is served by the registry site but nothing matches (or the detail
 *   could not be loaded) -> [UpdateVerification.Rejected]; registry metadata is never
 *   downloaded unverified.
 * - Otherwise (e.g. an author-hosted updateJson pointing elsewhere) -> Legacy, unchanged.
 */
object RegistryUpdatePolicy {

    fun decide(
        registryBaseUrl: String,
        updateJsonUrl: String,
        zipUrl: String,
        versionCode: Int,
        /** null when the index could not be loaded at all. */
        inIndex: Boolean?,
        detail: ApiModuleDetail?,
    ): UpdateVerification {
        val fromRegistry = RegistryUrls.isSameOrigin(updateJsonUrl, registryBaseUrl)
        if (inIndex == false) {
            return if (fromRegistry) UpdateVerification.Rejected("module is not in the registry index") else UpdateVerification.Legacy
        }
        // Release URLs are fixed when a release is listed and keep the old owner/repo after a
        // rename or transfer; the download itself is pinned by size + SHA-256.
        val candidates = detail?.releases.orEmpty()
            .filter { sameUrl(it.zip.url, zipUrl) && RegistryUrls.isAllowedReleaseDownloadUrl(it.zip.url) }
        if (candidates.isNotEmpty()) {
            val release = candidates.firstOrNull { it.versionCode == versionCode }
                ?: return UpdateVerification.Rejected("versionCode does not match the registry release")
            return UpdateVerification.Registry(
                zipUrl = release.zip.url,
                expected = ExpectedFile(release.zip.size, release.zip.sha256),
                release = release,
            )
        }
        return if (fromRegistry) {
            UpdateVerification.Rejected(
                if (detail == null) "registry detail unavailable" else "zipUrl does not match a registry release"
            )
        } else {
            UpdateVerification.Legacy
        }
    }

    /**
     * True when [decide] returns [UpdateVerification.Legacy] whatever the index and detail say:
     * updateJson is not served by the registry and zipUrl is not on github.com, where every
     * registry release ZIP lives (see [sameUrl]).
     */
    fun isUnrelatedToRegistry(registryBaseUrl: String, updateJsonUrl: String, zipUrl: String): Boolean =
        !RegistryUrls.isSameOrigin(updateJsonUrl, registryBaseUrl) && RegistryUrls.hostOf(zipUrl.trim()) != "github.com"

    /** URL equality with a case-insensitive scheme/host; path and query must match exactly. */
    fun sameUrl(a: String, b: String): Boolean {
        if (a == b) return true
        val ua = runCatching { java.net.URI(a.trim()) }.getOrNull() ?: return false
        val ub = runCatching { java.net.URI(b.trim()) }.getOrNull() ?: return false
        return ua.scheme.equals(ub.scheme, ignoreCase = true) &&
            ua.host != null && ua.host.equals(ub.host, ignoreCase = true) &&
            RegistryUrls.effectivePort(ua) == RegistryUrls.effectivePort(ub) &&
            ua.rawPath == ub.rawPath && ua.rawQuery == ub.rawQuery && ua.rawUserInfo == ub.rawUserInfo
    }
}
