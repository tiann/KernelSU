package me.weishu.kernelsu.ui.screen.modulerepo

import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoModule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.IOException
import java.util.Locale

class RepoSortTest {

    @Test
    fun storedNameWins() {
        assertEquals(RepoSort.NAME, RepoSort.fromStored("NAME", 0))
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored("UPDATED", 2))
        assertEquals(RepoSort.STARS, RepoSort.fromStored("STARS", null))
        assertEquals(RepoSort.STARS, RepoSort.fromStored("STARS", 0))
    }

    @Test
    fun legacyOrdinalsAreMigrated() {
        // Old enum: UPDATED(0), CREATED(1), NAME(2), STARS(3).
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored(null, 0))
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored(null, 1))
        assertEquals(RepoSort.NAME, RepoSort.fromStored(null, 2))
        assertEquals(RepoSort.STARS, RepoSort.fromStored(null, 3))
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored(null, 42))
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored(null, -1))
    }

    @Test
    fun unknownOrMissingValuesFallBack() {
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored(null, null))
        assertEquals(RepoSort.UPDATED, RepoSort.fromStored("CREATED", null))
        assertEquals(RepoSort.NAME, RepoSort.fromStored("bogus", 2))
        assertEquals(RepoSort.STARS, RepoSort.fromStored("bogus", 3))
    }

    private fun module(id: String, stars: Int?, epoch: Long, name: String = id) = RepoModule(
        moduleId = id,
        moduleName = name,
        authors = "",
        authorList = emptyList(),
        summary = "",
        tags = emptyList(),
        metamodule = false,
        iconUrl = null,
        latestVersion = "1.0",
        latestVersionCode = 1,
        latestReleaseTime = "",
        latestReleaseEpoch = epoch,
        stars = stars,
        starsRepository = null,
    )

    private val modules = listOf(
        module("a", stars = 5, epoch = 100, name = "Charlie"),
        module("b", stars = null, epoch = 500, name = "alpha"),
        module("c", stars = 50, epoch = 10, name = "Bravo"),
        module("d", stars = 5, epoch = 300, name = "delta"),
        module("e", stars = 0, epoch = 50, name = "Echo"),
        module("f", stars = null, epoch = 20, name = "foxtrot"),
    )

    @Test
    fun starsSortByCountThenNewestRelease() {
        // Unknown counts come after 0; ties are broken by the newest release.
        assertEquals(listOf("c", "d", "a", "e", "b", "f"), RepoSort.STARS.sort(modules).map { it.moduleId })
    }

    @Test
    fun otherSortsAreUnchanged() {
        assertEquals(listOf("b", "d", "a", "e", "f", "c"), RepoSort.UPDATED.sort(modules).map { it.moduleId })
        assertEquals(
            listOf("alpha", "Bravo", "Charlie", "delta", "Echo", "foxtrot"),
            RepoSort.NAME.sort(modules, Locale.ENGLISH).map { it.moduleName }
        )
        assertEquals(emptyList<RepoModule>(), RepoSort.STARS.sort(emptyList()))
    }

    @Test
    fun starCountIsCompact() {
        assertEquals("0", formatStarCount(0))
        assertEquals("999", formatStarCount(999))
        assertEquals("1k", formatStarCount(1_000))
        assertEquals("1k", formatStarCount(1_049))
        assertEquals("1.2k", formatStarCount(1_234))
        assertEquals("12.3k", formatStarCount(12_345))
        assertEquals("99.9k", formatStarCount(99_999))
        assertEquals("123k", formatStarCount(123_456))
        assertEquals("999k", formatStarCount(999_999))
        assertEquals("1M", formatStarCount(1_000_000))
        assertEquals("2.5M", formatStarCount(2_500_000))
        assertEquals("2147M", formatStarCount(Int.MAX_VALUE))
    }

    @Test
    fun emptyStateMessageOnlyOnceSomethingWasLoaded() {
        // Not loaded yet, or (re)loading: keep the progress indicator.
        assertNull(ModuleRepoUiState().emptyStateMessage)
        assertNull(ModuleRepoUiState(isRefreshing = true).emptyStateMessage)
        assertNull(ModuleRepoUiState(isRefreshing = true, hasLoaded = true).emptyStateMessage)
        // Loaded, but the repository lists no modules.
        assertEquals(R.string.module_repo_empty, ModuleRepoUiState(hasLoaded = true).emptyStateMessage)
        assertNull(ModuleRepoUiState(hasLoaded = true, modules = listOf(module("a", null, 0))).emptyStateMessage)
        assertEquals(R.string.module_repo_load_failed, ModuleRepoUiState(error = IOException("x")).emptyStateMessage)
    }

    @Test
    fun starsLinkPointsToTheStarsRepository() {
        assertEquals("https://github.com/owner/dist/stargazers", starsUrl("owner/dist"))
        assertNull(starsUrl(null))
    }
}
