package me.weishu.kernelsu.data.registry

import java.net.URI
import java.net.URLDecoder

object RegistryUrls {

    /** Site root of KernelSU Modules. All API paths are resolved against it. */
    const val DEFAULT_BASE_URL = "https://modules.kernelsu.org/"

    const val INDEX_PATH = "api/v1/index.json"

    /** Hosts a release download may be served from (github.com redirects to these). */
    val DOWNLOAD_HOSTS: Set<String> = setOf(
        "github.com",
        "objects.githubusercontent.com",
        "release-assets.githubusercontent.com",
    )

    private val API_PATH = Regex("^[A-Za-z0-9._\\-/]+$")
    private val REPO_PART = Regex("^[A-Za-z0-9._-]+$")

    /** Ensures an https base URL ending with '/'. */
    fun normalizeBaseUrl(baseUrl: String): String {
        val trimmed = baseUrl.trim()
        val uri = runCatching { URI(trimmed) }.getOrNull()
        require(uri != null && uri.scheme.equals("https", ignoreCase = true) && !uri.host.isNullOrEmpty()) {
            "Registry base URL must be an https URL: $baseUrl"
        }
        return if (trimmed.endsWith("/")) trimmed else "$trimmed/"
    }

    /** Same rule as `ApiPath` in api.ts: relative, no "..", restricted charset. */
    fun isValidApiPath(path: String): Boolean =
        path.isNotEmpty() && API_PATH.matches(path) && !path.startsWith("/") && !path.contains("..")

    /** Resolves a document path against the site root. Throws on unsafe paths. */
    fun resolve(baseUrl: String, path: String): String {
        if (!isValidApiPath(path)) throw RegistryFormatException("Invalid path in registry document: $path")
        return normalizeBaseUrl(baseUrl) + path
    }

    fun hostOf(url: String): String? = runCatching { URI(url).host?.lowercase() }.getOrNull()

    /** An absolute https URL with an ASCII host and no user info (safe to hand to a browser). */
    fun isHttpsUrl(url: String): Boolean {
        val uri = runCatching { URI(url) }.getOrNull() ?: return false
        return uri.isAbsolute && !uri.isOpaque &&
            uri.scheme.equals("https", ignoreCase = true) &&
            !uri.host.isNullOrEmpty() && uri.rawUserInfo == null
    }

    /** An absolute http(s) URL with a host and no user info (homepage / support links). */
    fun isWebUrl(url: String): Boolean {
        val uri = runCatching { URI(url) }.getOrNull() ?: return false
        return uri.isAbsolute && !uri.isOpaque &&
            (uri.scheme.equals("https", ignoreCase = true) || uri.scheme.equals("http", ignoreCase = true)) &&
            !uri.host.isNullOrEmpty() && uri.rawUserInfo == null
    }

    /** A link to a `#fragment` of the document that was loaded with [documentUrl] as its URL. */
    fun isInPageAnchor(url: String, documentUrl: String): Boolean {
        val hash = url.indexOf('#')
        if (hash <= 0 || hash == url.length - 1) return false
        return url.substring(0, hash) == documentUrl
    }

    /** Port with the scheme default filled in, or -1 for unknown schemes. */
    fun effectivePort(uri: URI): Int = when {
        uri.port != -1 -> uri.port
        uri.scheme.equals("https", ignoreCase = true) -> 443
        uri.scheme.equals("http", ignoreCase = true) -> 80
        else -> -1
    }

    /**
     * Same origin: scheme, host and (effective) port are equal. Hosts are compared as written
     * in ASCII; a non-ASCII (IDN) host does not parse as a server authority and never matches,
     * and a punycode host only matches the identical punycode host.
     */
    fun isSameOrigin(url: String, origin: String): Boolean {
        val a = runCatching { URI(url) }.getOrNull() ?: return false
        val b = runCatching { URI(origin) }.getOrNull() ?: return false
        val hostA = a.host ?: return false
        val hostB = b.host ?: return false
        if (a.scheme == null || b.scheme == null) return false
        return a.scheme.equals(b.scheme, ignoreCase = true) &&
            hostA.equals(hostB, ignoreCase = true) &&
            effectivePort(a) != -1 &&
            effectivePort(a) == effectivePort(b)
    }

    /**
     * Sub-resource rule of the restricted (registry) renderer: https GET from the registry
     * origin only. Applied to the request and to every redirect hop, so a same-origin URL can
     * not redirect to another host.
     */
    fun isAllowedRestrictedResource(url: String, registryOrigin: String): Boolean {
        val uri = runCatching { URI(url) }.getOrNull() ?: return false
        if (!uri.scheme.equals("https", ignoreCase = true) || uri.rawUserInfo != null) return false
        return isSameOrigin(url, registryOrigin)
    }

    private fun decodeSegment(s: String): String? =
        runCatching { URLDecoder.decode(s.replace("+", "%2B"), "UTF-8") }.getOrNull()

    /** Only `https://github.com/<owner>/<repo>/releases/download/<tag>/<file>` is accepted. */
    fun isAllowedReleaseDownloadUrl(url: String): Boolean {
        val uri = runCatching { URI(url) }.getOrNull() ?: return false
        if (!uri.isAbsolute || uri.isOpaque) return false
        if (!uri.scheme.equals("https", ignoreCase = true)) return false
        if (!uri.host.equals("github.com", ignoreCase = true)) return false
        if (uri.port != -1 && uri.port != 443) return false
        if (uri.rawUserInfo != null || uri.rawQuery != null || uri.rawFragment != null) return false
        val rawPath = uri.rawPath ?: return false
        if (!rawPath.startsWith("/")) return false
        val segments = rawPath.substring(1).split('/')
        if (segments.size < 6) return false
        for (raw in segments) {
            val decoded = decodeSegment(raw) ?: return false
            if (decoded.isEmpty() || decoded == "." || decoded == ".." ||
                decoded.contains('/') || decoded.contains('\\')
            ) return false
        }
        val owner = segments[0]
        val repo = segments[1]
        if (!REPO_PART.matches(owner) || !REPO_PART.matches(repo)) return false
        if (segments[2] != "releases" || segments[3] != "download") return false
        return true
    }

    /** Checks one hop of a download (the initial request or any redirect target). */
    fun isAllowedDownloadHop(url: String): Boolean {
        val uri = runCatching { URI(url) }.getOrNull() ?: return false
        if (!uri.scheme.equals("https", ignoreCase = true)) return false
        val host = uri.host?.lowercase() ?: return false
        if (uri.port != -1 && uri.port != 443) return false
        if (uri.rawUserInfo != null) return false
        return host in DOWNLOAD_HOSTS
    }
}
