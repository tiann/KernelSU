package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException

class RegistryClientTest {

    private val base = "https://modules.kernelsu.org/"
    private val indexUrl = base + "api/v1/index.json"
    private val listUrl = base + "api/v1/modules.json"
    private fun detailUrl(id: String) = base + "api/v1/modules/$id.json"
    private fun readmeUrl(id: String) = base + "api/v1/modules/$id/readme"

    private val b1 = "1111111111111111"
    private val b2 = "2222222222222222"
    private val b3 = "3333333333333333"
    private val sha = "a".repeat(64)
    private val commit = "c".repeat(40)

    private class FakeFetcher : RegistryFetcher {
        val responses = HashMap<String, ArrayDeque<String>>()
        val calls = ArrayList<String>()
        var offline = false

        fun set(url: String, vararg bodies: String) {
            responses[url] = ArrayDeque(bodies.toList())
        }

        override fun fetch(url: String): String {
            calls += url
            if (offline) throw IOException("offline")
            val queue = responses[url] ?: throw IOException("HTTP 404 for $url")
            // Keep returning the last body once the queue is drained.
            return if (queue.size > 1) queue.removeFirst() else queue.first()
        }

        fun count(url: String) = calls.count { it == url }
    }

    private class MemoryCache : RegistryCache {
        val map = HashMap<String, String>()
        override fun read(key: String): String? = map[key]
        override fun write(key: String, value: String) {
            map[key] = value
        }
    }

    private fun index(buildId: String, schema: Int = 1) = """
        {"schemaVersion":$schema,"buildId":"$buildId","repoCommit":"$commit",
         "generatedAt":"2026-10-01T00:00:00Z","modules":"api/v1/modules.json","someFutureField":{"x":[1,2]}}
    """.trimIndent()

    private fun summary(id: String, versionCode: Long = 100, extra: String = "") = """
        {"id":"$id","name":"Name $id","summary":"Sum",
         "authors":[{"name":"Alice","url":"https://github.com/alice"}],"tags":["tools"],"metamodule":false,
         "icon":"assets/abc.png","stable":{"version":"1.0","versionCode":$versionCode,"publishedAt":"2026-10-01T00:00:00Z"},
         "detail":"api/v1/modules/$id.json","updatedAt":"2026-10-01T00:00:00Z","unknownField":true$extra}
    """.trimIndent()

    private fun modules(buildId: String, vararg items: String) =
        """{"schemaVersion":1,"buildId":"$buildId","modules":[${items.joinToString(",")}]}"""

    private fun release(
        tag: String,
        versionCode: Int,
        url: String,
        prerelease: Boolean = false,
        releaseUrl: String = "https://github.com/owner/repo/releases/tag/$tag",
        attestationUrl: String? = null,
    ) = """
        {"tag":"$tag","name":null,"version":"$tag","versionCode":$versionCode,"prerelease":$prerelease,
         "publishedAt":"2026-10-01T00:00:00Z","changelogHtml":"<p>notes</p>",
         "zip":{"url":"$url","name":"m.zip","size":123,"sha256":"$sha"},"tagCommit":"$commit",
         "releaseUrl":"$releaseUrl","attestationUrl":${attestationUrl?.let { "\"$it\"" } ?: "null"},"immutable":true}
    """.trimIndent()

    private fun detail(
        id: String,
        buildId: String,
        releases: List<String>? = null,
        sourceUrl: String = "https://github.com/owner/repo",
        readme: String? = "api/v1/modules/$id/readme",
        extra: String = "",
    ): String {
        val rel = releases ?: listOf(release("v1", 100, "https://github.com/owner/repo/releases/download/v1/m.zip"))
        return """
            {"schemaVersion":1,"buildId":"$buildId","id":"$id","defaultLocale":"en",
             "name":"Name","summary":"Sum","authors":[],"tags":[],"metamodule":true,"icon":null,
             "stable":{"version":"v1","versionCode":100,"publishedAt":"2026-10-01T00:00:00Z"},
             "detail":"api/v1/modules/$id.json","updatedAt":"2026-10-01T00:00:00Z",
             "license":"MIT","homepage":null,"support":null,"compatibility":null,
             "source":{"host":"github","fullName":"owner/repo","url":"$sourceUrl","commit":"$commit"},
             "readme":${readme?.let { "\"$it\"" } ?: "null"},
             "releases":[${rel.joinToString(",")}],
             "health":{"status":"ok","lastSuccessAt":null},"newThing":1$extra}
        """.trimIndent()
    }

    private fun client(fetcher: FakeFetcher, cache: RegistryCache = MemoryCache()) =
        RegistryClient(base, fetcher, cache)

    @Test
    fun loadsCatalogIgnoringUnknownFields() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo"), summary("bar")))
        val load = client(f).loadCatalog()
        assertNull(load.error)
        assertFalse(load.catalog.fromCache)
        assertEquals(b1, load.catalog.buildId)
        assertEquals(listOf("foo", "bar"), load.catalog.modules.map { it.id })
    }

    @Test
    fun rejectsUnknownSchemaVersion() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1, schema = 2))
        f.set(listUrl, modules(b1, summary("foo")))
        val e = assertThrows(UnsupportedSchemaException::class.java) { client(f).loadCatalog() }
        assertEquals(2, e.schemaVersion)

        f.set(indexUrl, index(b1))
        f.set(listUrl, """{"schemaVersion":"1","buildId":"$b1","modules":[]}""")
        assertThrows(UnsupportedSchemaException::class.java) { client(f).loadCatalog() }
    }

    @Test
    fun dropsInvalidEntries() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(
            listUrl,
            modules(
                b1,
                summary("ok"),
                summary("overflow", versionCode = 3_000_000_000L), // not an int32
                summary("zero", versionCode = 0),
                summary("1bad-id"),
                summary("evilpath", extra = ""),
            ).replace("api/v1/modules/evilpath.json", "../../etc/passwd")
        )
        val ids = client(f).loadCatalog().catalog.modules.map { it.id }
        assertEquals(listOf("ok"), ids)
    }

    @Test
    fun listFromAnotherBuildIsRereadOnce() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b2))
        f.set(listUrl, modules(b2, summary("foo")))
        val catalog = client(f).loadCatalog().catalog
        assertEquals(b2, catalog.buildId)
        assertEquals(2, f.count(indexUrl))
    }

    @Test
    fun persistentListMismatchFails() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b2, summary("foo")))
        assertThrows(BuildMismatchException::class.java) { client(f).loadCatalog() }
        assertEquals(2, f.count(indexUrl))
    }

    @Test
    fun detailFromNewerBuildRefetchesIndexAndListOnce() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b2))
        f.set(listUrl, modules(b1, summary("foo")), modules(b2, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b2))
        val c = client(f)
        assertEquals(b1, c.loadCatalog().catalog.buildId)

        val load = c.loadDetail("foo")
        assertEquals(b2, load.catalog.buildId)
        assertEquals(b2, load.detail.buildId)
        assertEquals(b2, c.currentCatalog()?.buildId)
        assertEquals(2, f.count(indexUrl))
        // The detail already matched the refreshed list, no second detail request is needed.
        assertEquals(1, f.count(detailUrl("foo")))
    }

    @Test
    fun detailIsRefetchedForTheRefreshedBuild() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b3))
        f.set(listUrl, modules(b1, summary("foo")), modules(b3, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b2), detail("foo", b3))
        val c = client(f)
        c.loadCatalog()
        val load = c.loadDetail("foo")
        assertEquals(b3, load.catalog.buildId)
        assertEquals(b3, load.detail.buildId)
        assertEquals(2, f.count(detailUrl("foo")))
    }

    @Test
    fun neverMixesBuilds() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b3))
        f.set(listUrl, modules(b1, summary("foo")), modules(b3, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b2))
        val c = client(f)
        c.loadCatalog()
        assertThrows(BuildMismatchException::class.java) { c.loadDetail("foo") }
        // Index + list were re-read exactly once.
        assertEquals(2, f.count(indexUrl))
    }

    @Test
    fun cachedDetailOfTheCatalogBeforeARefreshIsNotUsed() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b3))
        f.set(listUrl, modules(b1, summary("foo")), modules(b3, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b1), detail("foo", b2))
        val c = client(f)
        c.loadCatalog()
        assertEquals(b1, c.loadDetail("foo").detail.buildId) // cached as b1

        // Redeployed: the detail is b2, the refreshed catalog b3. The cached b1 detail no longer
        // matches the catalog the client is on, so it must not be offered.
        assertThrows(BuildMismatchException::class.java) { c.loadDetail("foo") }
        assertEquals(b3, c.currentCatalog()?.buildId)
    }

    @Test
    fun moduleMissingFromIndexIsNotLoaded() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("gone"), detail("gone", b1))
        val c = client(f)
        assertThrows(ModuleNotInIndexException::class.java) { c.loadDetail("gone") }
        assertEquals(0, f.count(detailUrl("gone")))
    }

    @Test
    fun removedModuleIsReportedAsNotInIndex() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo"), summary("bar")))
        f.set(detailUrl("foo"), detail("foo", b1))
        val c = client(f)
        c.loadCatalog()
        assertEquals(b1, c.loadDetail("foo").detail.buildId) // cached as b1

        // Removed in b2: its detail is gone (404) and the client still holds b1.
        f.set(indexUrl, index(b2))
        f.set(listUrl, modules(b2, summary("bar")))
        f.responses.remove(detailUrl("foo"))
        assertThrows(ModuleNotInIndexException::class.java) { c.loadDetail("foo") }
        assertEquals(b2, c.currentCatalog()?.buildId)
    }

    @Test
    fun failedDetailIsReloadedForANewerBuild() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1), index(b2))
        f.set(listUrl, modules(b1, summary("foo")), modules(b2, summary("foo")))
        f.set(detailUrl("foo"), "{broken", detail("foo", b2))
        val c = client(f)
        c.loadCatalog()
        val load = c.loadDetail("foo")
        assertFalse(load.fromCache)
        assertEquals(b2, load.catalog.buildId)
        assertEquals(b2, load.detail.buildId)
        assertEquals(2, f.count(detailUrl("foo")))
    }

    @Test
    fun offlineDetailFallsBackToCacheAfterAFailedRefresh() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b1))
        val c = client(f)
        c.loadCatalog()
        c.loadDetail("foo")

        f.offline = true
        val load = c.loadDetail("foo")
        assertTrue(load.fromCache)
        assertNotNull(load.error)
        assertEquals(b1, load.detail.buildId)
        // One refresh of index + list was attempted.
        assertEquals(2, f.count(indexUrl))
    }

    @Test
    fun detailIdMustMatch() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("foo"), detail("bar", b1))
        assertThrows(RegistryFormatException::class.java) { client(f).loadDetail("foo") }
    }

    @Test
    fun releasesWithDisallowedUrlsAreDropped() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(
            detailUrl("foo"),
            detail(
                "foo", b1, listOf(
                    release("v3", 300, "https://evil.example/owner/repo/releases/download/v3/m.zip"),
                    release("v2", 200, "https://github.com/other/repo/releases/download/v2/m.zip"),
                    release("v1", 100, "https://github.com/owner/repo/releases/download/v1/m.zip"),
                    release("v0", 50, "https://github.com/owner/repo/releases/download/v0/m.zip", prerelease = true),
                )
            )
        )
        val releases = client(f).loadDetail("foo").detail.releases
        // A release of another owner/repo is kept: its URL predates a rename or transfer.
        assertEquals(listOf("v2", "v1", "v0"), releases.map { it.tag })
        assertTrue(releases.last().prerelease)
    }

    @Test
    fun fallsBackToLastCompleteCopy() {
        val cache = MemoryCache()
        val online = FakeFetcher()
        online.set(indexUrl, index(b1))
        online.set(listUrl, modules(b1, summary("foo")))
        online.set(detailUrl("foo"), detail("foo", b1))
        online.set(readmeUrl("foo"), "<p>hello</p>")
        client(online, cache).also {
            it.loadCatalog()
            val d = it.loadDetail("foo").detail
            assertEquals("<p>hello</p>", it.loadReadme(d)?.html)
        }

        val offline = FakeFetcher().apply { this.offline = true }
        val c = client(offline, cache)
        val load = c.loadCatalog()
        assertTrue(load.catalog.fromCache)
        assertNotNull(load.error)
        assertEquals(b1, load.catalog.buildId)
        val detail = c.loadDetail("foo")
        assertTrue(detail.fromCache)
        assertEquals(b1, detail.detail.buildId)
        assertEquals("<p>hello</p>", c.loadReadme(detail.detail)?.html)
    }

    @Test
    fun brokenNetworkCopyFallsBackToCache() {
        val cache = MemoryCache()
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        client(f, cache).loadCatalog()

        val broken = FakeFetcher()
        broken.set(indexUrl, index(b2))
        broken.set(listUrl, """{"schemaVersion":1,"buildId":"$b2","modules":[""")
        val load = client(broken, cache).loadCatalog()
        assertEquals(b1, load.catalog.buildId)
        assertTrue(load.error is RegistryFormatException)
        // The broken copy did not replace the good one.
        assertEquals(b1, client(FakeFetcher().apply { offline = true }, cache).loadCatalog().catalog.buildId)
    }

    @Test
    fun cachedDetailOfAnotherBuildIsNotUsed() {
        val cache = MemoryCache()
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b1))
        client(f, cache).also { it.loadCatalog(); it.loadDetail("foo") }

        val f2 = FakeFetcher()
        f2.set(indexUrl, index(b2))
        f2.set(listUrl, modules(b2, summary("foo")))
        val c = client(f2, cache)
        c.loadCatalog()
        f2.offline = true
        assertThrows(IOException::class.java) { c.loadDetail("foo") }
    }

    @Test
    fun noNetworkAndNoCacheFails() {
        val f = FakeFetcher().apply { offline = true }
        assertThrows(IOException::class.java) { client(f).loadCatalog() }
    }

    @Test
    fun singleLanguageFieldsAreParsed() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b1))
        val c = client(f)
        val m = c.loadCatalog().catalog.find("foo")!!
        assertEquals("Name foo", m.name)
        assertEquals("Sum", m.summary)
        val d = c.loadDetail("foo").detail
        assertEquals("api/v1/modules/foo/readme", d.readme)
    }

    @Test
    fun missingReadmeIsNullAndUnsafeReadmePathIsDropped() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo"), summary("bar")))
        f.set(detailUrl("foo"), detail("foo", b1, readme = null))
        f.set(detailUrl("bar"), detail("bar", b1, readme = "../secret.html"))
        val c = client(f)
        c.loadCatalog()
        val foo = c.loadDetail("foo").detail
        assertNull(foo.readme)
        assertNull(c.loadReadme(foo))
        assertNull(c.loadDetail("bar").detail.readme)
    }

    @Test
    fun readmeFailureIsAnErrorNotAMissingReadme() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(detailUrl("foo"), detail("foo", b1))
        val c = client(f)
        c.loadCatalog()
        val d = c.loadDetail("foo").detail
        // No body registered for the readme URL -> the fetch fails and no cached copy exists.
        assertThrows(IOException::class.java) { c.loadReadme(d) }
    }

    @Test
    fun badReleasesAreDroppedIndividually() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        val ok = "https://github.com/owner/repo/releases/download/v1/m.zip"
        f.set(
            detailUrl("foo"),
            detail(
                "foo", b1, listOf(
                    // Wrong type for versionCode: undecodable on its own.
                    release("v5", 500, ok).replace("\"versionCode\":500", "\"versionCode\":\"x\""),
                    // Missing required field.
                    release("v4", 400, ok).replace("\"tagCommit\":\"$commit\",", ""),
                    // Non-https release page.
                    release("v3", 300, ok, releaseUrl = "http://github.com/owner/repo/releases/tag/v3"),
                    release("v2", 200, ok, attestationUrl = "javascript:alert(1)"),
                    "42",
                    release("v1", 100, ok, attestationUrl = "https://github.com/owner/repo/attestations/1"),
                )
            )
        )
        val releases = client(f).loadDetail("foo").detail.releases
        assertEquals(listOf("v2", "v1"), releases.map { it.tag })
        assertNull(releases[0].attestationUrl)
        assertEquals("https://github.com/owner/repo/attestations/1", releases[1].attestationUrl)
    }

    @Test
    fun releasesAreDeduplicatedByTag() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo")))
        f.set(
            detailUrl("foo"),
            detail(
                "foo", b1, listOf(
                    release("v2", 200, "https://github.com/owner/repo/releases/download/v2/m.zip"),
                    release("v2", 150, "https://github.com/owner/repo/releases/download/v2/other.zip"),
                    release("v1", 100, "https://github.com/owner/repo/releases/download/v1/m.zip"),
                )
            )
        )
        val releases = client(f).loadDetail("foo").detail.releases
        assertEquals(listOf("v2", "v1"), releases.map { it.tag })
        assertEquals(200, releases[0].versionCode)
    }

    @Test
    fun nonHttpsSourceUrlIsReplacedWithGithubUrl() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo"), summary("bar")))
        f.set(detailUrl("foo"), detail("foo", b1, sourceUrl = "javascript:alert(1)"))
        f.set(detailUrl("bar"), detail("bar", b1, sourceUrl = "http://github.com/owner/repo"))
        val c = client(f)
        c.loadCatalog()
        assertEquals("https://github.com/owner/repo", c.loadDetail("foo").detail.source.url)
        assertEquals("https://github.com/owner/repo", c.loadDetail("bar").detail.source.url)
    }

    @Test
    fun starsAreParsedWhenPresentNullOrMissing() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(
            listUrl,
            modules(
                b1,
                summary("present", extra = ""","stars":1234,"starsRepository":"owner/module-dist""""),
                summary("zero", extra = ""","stars":0,"starsRepository":"owner/repo""""),
                summary("nulls", extra = ""","stars":null,"starsRepository":null"""),
                summary("missing"),
            )
        )
        val catalog = client(f).loadCatalog().catalog
        assertEquals(listOf("present", "zero", "nulls", "missing"), catalog.modules.map { it.id })
        catalog.find("present")!!.let {
            assertEquals(1234, it.stars)
            assertEquals("owner/module-dist", it.starsRepository)
        }
        catalog.find("zero")!!.let {
            assertEquals(0, it.stars)
            assertEquals("owner/repo", it.starsRepository)
        }
        for (id in listOf("nulls", "missing")) {
            assertNull(catalog.find(id)!!.stars)
            assertNull(catalog.find(id)!!.starsRepository)
        }
    }

    @Test
    fun malformedStarsAreDroppedButTheModuleIsKept() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(
            listUrl,
            modules(
                b1,
                summary("str", extra = ""","stars":"12","starsRepository":"owner/repo""""),
                summary("neg", extra = ""","stars":-1,"starsRepository":"not a repo""""),
                summary("frac", extra = ""","stars":1.5,"starsRepository":"owner/..""""),
                summary("huge", extra = ""","stars":3000000000,"starsRepository":{"x":1}"""),
                summary("obj", extra = ""","stars":{"n":1},"starsRepository":"https://github.com/owner/repo""""),
            )
        )
        val modules = client(f).loadCatalog().catalog.modules
        assertEquals(listOf("str", "neg", "frac", "huge", "obj"), modules.map { it.id })
        assertTrue(modules.all { it.stars == null })
        assertEquals(listOf("owner/repo", null, null, null, null), modules.map { it.starsRepository })
    }

    @Test
    fun detailStarsAreParsed() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(listUrl, modules(b1, summary("foo"), summary("bar"), summary("baz")))
        f.set(detailUrl("foo"), detail("foo", b1, extra = ""","stars":7,"starsRepository":"owner/dist""""))
        f.set(detailUrl("bar"), detail("bar", b1, extra = ""","stars":null,"starsRepository":null"""))
        f.set(detailUrl("baz"), detail("baz", b1, extra = ""","stars":"7","starsRepository":"../x""""))
        val c = client(f)
        c.loadCatalog()
        c.loadDetail("foo").detail.let {
            assertEquals(7, it.stars)
            assertEquals("owner/dist", it.starsRepository)
            // The source repository is unrelated to the repository the stars belong to.
            assertEquals("owner/repo", it.source.fullName)
        }
        c.loadDetail("bar").detail.let {
            assertNull(it.stars)
            assertNull(it.starsRepository)
        }
        c.loadDetail("baz").detail.let {
            assertNull(it.stars)
            assertNull(it.starsRepository)
        }
    }

    @Test
    fun liveSampleShapeIsParsed() {
        val text = """
            {"schemaVersion":1,"buildId":"9445ddaac7e3757d","modules":[{"id":"example_module_demo",
             "name":"Example Module Demo","summary":"A minimal module","authors":[{"name":"KernelSU-Modules contributors",
             "url":"https://github.com/KernelSU-Modules"}],"tags":["example","tools"],"metamodule":false,
             "icon":"assets/2ebaa6f72c2ac3a7.png","stable":{"version":"v1.0.0","versionCode":100,
             "publishedAt":"2026-10-11T01:26:58Z"},"stars":0,"starsRepository":"KernelSU-Modules/example-module",
             "detail":"api/v1/modules/example_module_demo.json","updatedAt":"2026-10-11T01:38:10.702Z"}]}
        """.trimIndent()
        val m = RegistryParser().parseModules(text).modules.single()
        assertEquals(0, m.stars)
        assertEquals("KernelSU-Modules/example-module", m.starsRepository)
    }

    @Test
    fun kindDefaultsToModuleAndUnknownKindsAreHidden() {
        val f = FakeFetcher()
        f.set(indexUrl, index(b1))
        f.set(
            listUrl,
            modules(
                b1,
                summary("plain"),
                summary("mod", extra = ",\"kind\":\"module\""),
                summary("tool", extra = ",\"kind\":\"script\""),
                summary("future", extra = ",\"kind\":\"theme\""),
                summary("numeric", extra = ",\"kind\":1"),
                summary("metascript", extra = ",\"kind\":\"script\"").replace("\"metamodule\":false", "\"metamodule\":true"),
            )
        )
        val modules = client(f).loadCatalog().catalog.modules
        assertEquals(listOf("plain", "mod", "tool"), modules.map { it.id })
        assertEquals(listOf("module", "module", "script"), modules.map { it.kind })
    }

    @Test
    fun detailKindIsParsedAndUnknownKindRejected() {
        val parser = RegistryParser()
        assertEquals(RegistryKind.MODULE, parser.parseDetail(detail("foo", b1)).kind)
        val script = detail("foo", b1, extra = ",\"kind\":\"script\"").replace("\"metamodule\":true", "\"metamodule\":false")
        assertEquals(RegistryKind.SCRIPT, parser.parseDetail(script).kind)
        assertThrows(RegistryFormatException::class.java) {
            parser.parseDetail(detail("foo", b1, extra = ",\"kind\":\"theme\""))
        }
        // detail() is a metamodule: a metamodule script is a manifest error.
        assertThrows(RegistryFormatException::class.java) {
            parser.parseDetail(detail("foo", b1, extra = ",\"kind\":\"script\""))
        }
    }
}
