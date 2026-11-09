package me.weishu.kernelsu.data.registry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class FileRegistryCacheTest {

    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun writesReadsAndReplacesWithoutLeftovers() {
        val dir = File(tmp.root, "cache")
        val cache = FileRegistryCache(dir)
        assertNull(cache.read("catalog"))
        cache.write("catalog", "one")
        cache.write("catalog", "two ✓")
        assertEquals("two ✓", cache.read("catalog"))
        cache.write("detail/../x", "safe")
        assertEquals("safe", cache.read("detail/../x"))
        assertEquals(listOf("catalog.cache", "detail_.._x.cache"), dir.list()!!.sorted())
    }

    @Test
    fun concurrentWritersUseDistinctTempFiles() {
        val dir = File(tmp.root, "cache")
        val cache = FileRegistryCache(dir)
        val pool = Executors.newFixedThreadPool(8)
        val values = (0 until 64).map { "value-$it-" + "x".repeat(10_000) }
        values.forEach { v -> pool.submit { cache.write("same", v) } }
        pool.shutdown()
        pool.awaitTermination(30, TimeUnit.SECONDS)
        // Whatever won, it is one complete value and no temp file is left behind.
        assert(cache.read("same") in values)
        assertEquals(listOf("same.cache"), dir.list()!!.sorted())
    }
}
