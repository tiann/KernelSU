package me.weishu.kernelsu.data.model

import androidx.compose.runtime.Immutable

@Immutable
data class Author(
    val name: String,
    val link: String,
)

/** A module of the registry index, already localized for the current UI locale. */
@Immutable
data class RepoModule(
    val moduleId: String,
    val moduleName: String,
    val authors: String,
    val authorList: List<Author>,
    val summary: String,
    val tags: List<String>,
    val metamodule: Boolean,
    val iconUrl: String?,
    val latestVersion: String,
    val latestVersionCode: Int,
    /** Formatted publish date of the stable release. */
    val latestReleaseTime: String,
    /** Epoch millis of the stable release, used for sorting. */
    val latestReleaseEpoch: Long,
    /** GitHub stargazer count, or null when the registry doesn't know it. */
    val stars: Int? = null,
    /** "owner/name" of the repository [stars] belongs to. */
    val starsRepository: String? = null,
)

@Immutable
data class RepoModuleList(
    val modules: List<RepoModule>,
    val buildId: String,
    /** True when the list is the last good cached copy because the network copy failed. */
    val stale: Boolean,
    val error: Throwable?,
)

/** A stable (non-prerelease), immutable release; its ZIP is verified after download. */
@Immutable
data class RepoRelease(
    val tag: String,
    val name: String,
    val version: String,
    val versionCode: Int,
    val publishedAt: String,
    val changelogHtml: String?,
    val zipUrl: String,
    val zipName: String,
    val zipSize: Long,
    val sha256: String,
    val tagCommit: String,
    /** https only (validated by the parser). */
    val releaseUrl: String,
    /** https only, or null. */
    val attestationUrl: String?,
    val immutable: Boolean,
)

@Immutable
data class RepoModuleDetail(
    val module: RepoModule,
    val license: String?,
    val homepage: String?,
    val support: String?,
    val sourceFullName: String,
    val sourceUrl: String,
    val sourceCommit: String,
    val readmeHtml: String?,
    /** The module has a README but it could not be loaded (show a retry, not "no README"). */
    val readmeFailed: Boolean,
    /** Base URL relative resources in the readme are resolved against (the site root). */
    val readmeBaseUrl: String,
    val releases: List<RepoRelease>,
    val healthStale: Boolean,
    val webUrl: String,
    val fromCache: Boolean,
)
