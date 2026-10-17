package me.weishu.kernelsu.ui.screen.modulerepo

import androidx.compose.runtime.Immutable
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoModule
import me.weishu.kernelsu.data.model.RepoModuleDetail
import me.weishu.kernelsu.data.registry.UnsupportedSchemaException
import me.weishu.kernelsu.data.script.ScriptRunRecord
import me.weishu.kernelsu.ui.component.SearchStatus
import java.text.Collator
import java.util.Locale

enum class RepoSort {
    UPDATED,
    NAME,
    STARS;

    /** Returns [modules] in this order; ties (and unknown star counts) fall back to newest release first. */
    fun sort(modules: List<RepoModule>, locale: Locale = Locale.getDefault()): List<RepoModule> {
        if (modules.isEmpty()) return modules
        return when (this) {
            UPDATED -> modules.sortedByDescending { it.latestReleaseEpoch }
            NAME -> {
                val collator = Collator.getInstance(locale)
                modules.sortedWith(compareBy(collator) { it.moduleName })
            }
            // Unknown counts (null) sort after every known count, including 0.
            STARS -> modules.sortedWith(
                compareByDescending<RepoModule> { it.stars ?: -1 }.thenByDescending { it.latestReleaseEpoch }
            )
        }
    }

    companion object {
        /**
         * Restores the saved order. New versions store the enum name; older ones stored the
         * ordinal of `UPDATED, CREATED, NAME, STARS`, which is migrated here (CREATED no
         * longer exists and falls back to UPDATED).
         */
        fun fromStored(name: String?, legacyOrdinal: Int?): RepoSort {
            name?.let { n -> entries.firstOrNull { it.name == n }?.let { return it } }
            return when (legacyOrdinal) {
                2 -> NAME
                3 -> STARS
                else -> UPDATED
            }
        }
    }
}

data class ModuleRepoUiState(
    val isRefreshing: Boolean = false,
    /** A module list (possibly empty) has been loaded at least once. */
    val hasLoaded: Boolean = false,
    val sortOrder: RepoSort = RepoSort.UPDATED,
    val offline: Boolean = false,
    val modules: List<RepoModule> = emptyList(),
    val searchStatus: SearchStatus = SearchStatus(""),
    val searchResults: List<RepoModule> = emptyList(),
    val error: Throwable? = null,
    /** The list shown is the last good cached copy (network or parsing failed). */
    val stale: Boolean = false,
    /** Installed module id -> versionCode, for "installed" / "update" badges. */
    val installedVersions: Map<String, Int> = emptyMap(),
) {
    /** Message to show instead of the list when it is empty or could not be loaded, or null while loading. */
    val emptyStateMessage: Int?
        get() = when {
            modules.isNotEmpty() || isRefreshing && !offline -> null
            error is UnsupportedSchemaException -> R.string.module_repo_unsupported_schema
            offline -> R.string.network_offline
            error != null -> R.string.module_repo_load_failed
            hasLoaded -> R.string.module_repo_empty
            else -> null
        }
}

/**
 * Compact star count for list rows: "999", "1.2k", "12.3k", "123k", "1.2M". Integer arithmetic
 * (rounded down) keeps it independent of the device locale, like GitHub's own counters.
 */
fun formatStarCount(count: Int): String {
    fun compact(value: Int, unit: Int, suffix: String): String {
        if (value >= unit * 100) return "${value / unit}$suffix"
        val tenths = value / (unit / 10)
        return if (tenths % 10 == 0) "${tenths / 10}$suffix" else "${tenths / 10}.${tenths % 10}$suffix"
    }
    return when {
        count < 1_000 -> count.toString()
        count < 1_000_000 -> compact(count, 1_000, "k")
        else -> compact(count, 1_000_000, "M")
    }
}

/** GitHub stargazers page of an "owner/name" repository (validated by the registry parser). */
fun starsUrl(starsRepository: String?): String? =
    starsRepository?.let { "https://github.com/$it/stargazers" }

/** Installed state of a registry module; updates are detected by versionCode only. */
enum class RepoInstallState { NONE, INSTALLED, UPDATABLE }

/** Scripts are never installed, so they have no installed / update state. */
fun repoInstallStateOf(module: RepoModule, installedVersions: Map<String, Int>): RepoInstallState =
    if (module.isScript) RepoInstallState.NONE else repoInstallState(installedVersions[module.moduleId], module.latestVersionCode)

fun repoInstallState(installedVersionCode: Int?, latestVersionCode: Int): RepoInstallState = when {
    installedVersionCode == null -> RepoInstallState.NONE
    latestVersionCode > installedVersionCode -> RepoInstallState.UPDATABLE
    else -> RepoInstallState.INSTALLED
}

@Immutable
data class ModuleRepoActions(
    val onBack: () -> Unit,
    val onRefresh: () -> Unit,
    val onSearchTextChange: (String) -> Unit,
    val onClearSearch: () -> Unit,
    val onSearchStatusChange: (SearchStatus) -> Unit,
    val onSetSortOrder: (RepoSort) -> Unit,
    val onOpenRepoDetail: (RepoModule) -> Unit,
)

@Immutable
data class ModuleRepoDetailUiState(
    val module: RepoModuleArg,
    val detail: RepoModuleDetail?,
    val loaded: Boolean,
    /** The module is not in the current index (e.g. suspended): no install entry is offered. */
    val notInIndex: Boolean,
    val loadFailed: Boolean,
    val installedVersionCode: Int?,
    /** Last local run, for scripts. */
    val lastRun: ScriptRunRecord? = null,
) {
    val webUrl: String get() = detail?.webUrl.orEmpty()

    /** One-shot script: releases are run, not installed. */
    val isScript: Boolean get() = detail?.module?.isScript ?: module.script
    val canInstall: Boolean get() = detail != null && !notInIndex
}

@Immutable
data class ModuleRepoDetailActions(
    val onBack: () -> Unit,
    val onOpenWebUrl: () -> Unit,
    val onOpenUrl: (String) -> Unit,
    /** Installs a downloaded module ZIP, or opens the run page for a script. */
    val onInstallModule: (android.net.Uri) -> Unit,
    val onRetry: () -> Unit,
)
