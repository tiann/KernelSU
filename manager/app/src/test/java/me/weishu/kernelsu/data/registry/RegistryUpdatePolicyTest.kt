package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RegistryUpdatePolicyTest {

    private val base = RegistryUrls.DEFAULT_BASE_URL
    private val registryUpdateJson = "https://modules.kernelsu.org/api/v1/modules/foo/update.json"
    private val authorUpdateJson = "https://raw.githubusercontent.com/owner/repo/main/update.json"
    private val sha1 = "1".repeat(64)
    private val sha2 = "2".repeat(64)
    private fun zip(tag: String) = "https://github.com/owner/repo/releases/download/$tag/foo.zip"

    private fun release(tag: String, versionCode: Int, sha: String, url: String = zip(tag)) = ApiRelease(
        tag = tag, version = tag, versionCode = versionCode, prerelease = false,
        publishedAt = "2026-10-01T00:00:00Z", changelogHtml = "<p>$tag</p>",
        zip = ApiZip(url = url, name = "foo.zip", size = 100L + versionCode, sha256 = sha),
        tagCommit = "c".repeat(40), releaseUrl = "https://github.com/owner/repo/releases/tag/$tag",
        immutable = true,
    )

    private val detail = ApiModuleDetail(
        schemaVersion = 1, buildId = "1".repeat(16), id = "foo", name = "Foo",
        stable = ApiVersionRef("v2", 200, "2026-10-01T00:00:00Z"), detail = "api/v1/modules/foo.json",
        updatedAt = "2026-10-01T00:00:00Z",
        source = ApiSource("github", "owner/repo", "https://github.com/owner/repo", "c".repeat(40)),
        releases = listOf(release("v2", 200, sha2), release("v1", 100, sha1)),
    )

    private fun decide(
        updateJson: String = registryUpdateJson,
        zipUrl: String = zip("v2"),
        versionCode: Int = 200,
        inIndex: Boolean? = true,
        d: ApiModuleDetail? = detail,
    ) = RegistryUpdatePolicy.decide(base, updateJson, zipUrl, versionCode, inIndex, d)

    @Test
    fun registryReleaseIsVerified() {
        val v = decide() as UpdateVerification.Registry
        assertEquals(ExpectedFile(300, sha2), v.expected)
        assertEquals(zip("v2"), v.zipUrl)
        assertEquals("v2", v.release.tag)
        // An author-hosted updateJson that points at a registry release is verified as well.
        val a = decide(updateJson = authorUpdateJson, zipUrl = zip("v1"), versionCode = 100) as UpdateVerification.Registry
        assertEquals(ExpectedFile(200, sha1), a.expected)
        // Case differences in scheme / host do not defeat the match.
        assertTrue(decide(zipUrl = "HTTPS://GitHub.com/owner/repo/releases/download/v2/foo.zip") is UpdateVerification.Registry)
    }

    @Test
    fun versionCodePicksBetweenReleasesSharingAUrl() {
        val shared = "https://github.com/owner/repo/releases/download/latest/foo.zip"
        val d = detail.copy(releases = listOf(release("v2", 200, sha2, shared), release("v1", 100, sha1, shared)))
        assertEquals(ExpectedFile(200, sha1), (decide(zipUrl = shared, versionCode = 100, d = d) as UpdateVerification.Registry).expected)
        assertEquals(ExpectedFile(300, sha2), (decide(zipUrl = shared, versionCode = 200, d = d) as UpdateVerification.Registry).expected)
        assertTrue(decide(zipUrl = shared, versionCode = 999, d = d) is UpdateVerification.Rejected)
    }

    @Test
    fun registryZipWithAnotherVersionCodeIsRejected() {
        // The v1 ZIP offered as versionCode 200 would be a silent downgrade.
        assertEquals(
            UpdateVerification.Rejected("versionCode does not match the registry release"),
            decide(zipUrl = zip("v1"), versionCode = 200),
        )
        // Also when updateJson is author-hosted.
        assertEquals(
            UpdateVerification.Rejected("versionCode does not match the registry release"),
            decide(updateJson = authorUpdateJson, zipUrl = zip("v1"), versionCode = 200),
        )
    }

    @Test
    fun releaseOfARenamedRepositoryIsStillVerified() {
        // Release URLs keep the owner/repo they were listed under.
        val old = "https://github.com/old-owner/old-repo/releases/download/v2/foo.zip"
        val renamed = detail.copy(releases = listOf(release("v2", 200, sha2, old)))
        val v = decide(zipUrl = old, d = renamed) as UpdateVerification.Registry
        assertEquals(ExpectedFile(300, sha2), v.expected)
    }

    @Test
    fun unrelatedUpdatesAreLegacyWithoutRegistryData() {
        val zips = listOf("https://example.com/foo.zip", "https://raw.githubusercontent.com/owner/repo/main/foo.zip", "not a url")
        for (zipUrl in zips) {
            assertTrue(zipUrl, RegistryUpdatePolicy.isUnrelatedToRegistry(base, authorUpdateJson, zipUrl))
            for (inIndex in listOf(true, false, null)) for (d in listOf(detail, null)) {
                assertEquals(zipUrl, UpdateVerification.Legacy, decide(authorUpdateJson, zipUrl, 200, inIndex, d))
            }
        }
        // Possibly a registry release, or a registry-hosted updateJson: registry data is needed.
        assertFalse(RegistryUpdatePolicy.isUnrelatedToRegistry(base, authorUpdateJson, zip("v2")))
        assertFalse(RegistryUpdatePolicy.isUnrelatedToRegistry(base, authorUpdateJson, " HTTPS://GitHub.com/owner/repo/releases/download/v2/foo.zip#x"))
        assertFalse(RegistryUpdatePolicy.isUnrelatedToRegistry(base, registryUpdateJson, "https://example.com/foo.zip"))
    }

    @Test
    fun nonRegistryModulesStayLegacy() {
        assertEquals(UpdateVerification.Legacy, decide(updateJson = authorUpdateJson, inIndex = false, d = null))
        assertEquals(UpdateVerification.Legacy, decide(updateJson = authorUpdateJson, zipUrl = "https://example.com/foo.zip"))
        // Index unavailable and an author-hosted updateJson: unchanged legacy behaviour.
        assertEquals(UpdateVerification.Legacy, decide(updateJson = authorUpdateJson, inIndex = null, d = null))
        // Path / query differences are a different file.
        assertEquals(UpdateVerification.Legacy, decide(updateJson = authorUpdateJson, zipUrl = zip("v2") + "?x=1"))
    }

    @Test
    fun unmatchedRegistryHostedUpdatesAreRejected() {
        assertTrue(decide(zipUrl = "https://example.com/foo.zip") is UpdateVerification.Rejected)
        assertTrue(decide(d = null) is UpdateVerification.Rejected)
        assertTrue(decide(inIndex = false, d = null) is UpdateVerification.Rejected)
        assertTrue(decide(inIndex = null, d = null) is UpdateVerification.Rejected)
        // Only URLs of the module's own registry releases match.
        assertTrue(decide(zipUrl = "https://github.com/evil/repo/releases/download/v2/foo.zip") is UpdateVerification.Rejected)
    }
}
