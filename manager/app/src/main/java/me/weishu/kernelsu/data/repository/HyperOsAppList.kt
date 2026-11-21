package me.weishu.kernelsu.data.repository

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import me.weishu.kernelsu.data.model.HYPEROS_PACKAGE

internal class HyperOsAppList {
    // The root service is unbound after each scan. Keep marker decisions in the Manager process.
    @Volatile
    private var markers: Map<Revision, Boolean>? = null

    private data class Revision(
        val packageName: String,
        val uid: Int,
        val firstInstallTime: Long,
        val lastUpdateTime: Long,
        val version: Long,
        val sourceDir: String?,
        val splitSourceDirs: List<String>?,
    )

    private fun PackageInfo.codeLessRevision(): Revision? {
        val app = applicationInfo ?: return null
        if (app.flags and ApplicationInfo.FLAG_HAS_CODE != 0) return null
        return Revision(
            packageName, app.uid, firstInstallTime, lastUpdateTime, longVersionCode,
            app.sourceDir, app.splitSourceDirs?.toList(),
        )
    }

    suspend fun load(fetch: suspend (readMetadata: Boolean) -> List<PackageInfo>): List<PackageInfo> {
        var previous = markers
        var packages = fetch(previous == null)
        if (previous != null && packages.any { pkg ->
                val revision = pkg.codeLessRevision()
                revision != null && revision !in previous
            }
        ) {
            // A new or updated code-less package needs a fresh manifest decision.
            previous = null
            packages = fetch(true)
        }

        val next = mutableMapOf<Revision, Boolean>()
        val selected = packages.filter { pkg ->
            val revision = pkg.codeLessRevision() ?: return@filter pkg.applicationInfo != null
            val app = pkg.applicationInfo!!
            val marked = previous?.get(revision) ?: (app.metaData?.getBoolean(HYPEROS_PACKAGE) == true)
            next[revision] = marked
            app.metaData = null
            marked
        }
        markers = next
        return selected
    }
}
