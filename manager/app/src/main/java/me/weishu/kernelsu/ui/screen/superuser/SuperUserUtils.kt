package me.weishu.kernelsu.ui.screen.superuser

import androidx.compose.runtime.Immutable
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ui.util.pickPrimary
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel

@Immutable
data class GroupedApps(
    val uid: Int,
    val apps: List<SuperUserViewModel.AppInfo>,
    val primary: SuperUserViewModel.AppInfo,
    val anyAllowSu: Boolean,
    val anyCustom: Boolean,
    val shouldUmount: Boolean,
)

private val uidShouldUmountCache = mutableMapOf<Int, Boolean>()

fun uidShouldUmountCached(uid: Int): Boolean {
    uidShouldUmountCache[uid]?.let { return it }
    val value = Natives.uidShouldUmount(uid)
    uidShouldUmountCache[uid] = value
    return value
}

fun buildGroups(apps: List<SuperUserViewModel.AppInfo>): List<GroupedApps> {
    val comparator = compareBy<SuperUserViewModel.AppInfo> {
        when {
            it.allowSu -> 0
            it.hasCustomProfile -> 1
            else -> 2
        }
    }.thenBy { it.label.lowercase() }
    val groups = apps.groupBy { it.uid }.map { (uid, list) ->
        val sorted = list.sortedWith(comparator)
        val primary = pickPrimary(sorted)
        val shouldUmount = uidShouldUmountCached(uid)
        GroupedApps(
            uid = uid,
            apps = sorted,
            primary = primary,
            anyAllowSu = sorted.any { it.allowSu },
            anyCustom = sorted.any { it.hasCustomProfile },
            shouldUmount = shouldUmount,
        )
    }
    return groups.sortedWith(Comparator { a, b ->
        fun rank(g: GroupedApps): Int = when {
            g.anyAllowSu -> 0
            g.anyCustom -> 1
            g.apps.size > 1 -> 2
            g.shouldUmount -> 4
            else -> 3
        }

        val ra = rank(a)
        val rb = rank(b)
        if (ra != rb) return@Comparator ra - rb
        return@Comparator when (ra) {
            2 -> a.uid.compareTo(b.uid)
            else -> a.primary.label.lowercase().compareTo(b.primary.label.lowercase())
        }
    })
}
