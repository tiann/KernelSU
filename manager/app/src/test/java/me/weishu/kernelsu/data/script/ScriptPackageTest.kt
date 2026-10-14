package me.weishu.kernelsu.data.script

import org.apache.commons.compress.archivers.zip.ZipArchiveEntry
import org.apache.commons.compress.archivers.zip.ZipArchiveOutputStream
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.io.FileInputStream

class ScriptPackageTest {

    @get:Rule
    val tmp = TemporaryFolder()

    private val prop = "id=my_script\nname=My Script\nversion=v1\nversionCode=3\nauthor=Alice\ndescription=Does a thing\noneshot=true\n"

    private sealed class Entry(val name: String) {
        class FileEntry(name: String, val data: ByteArray, val mode: Int? = null) : Entry(name)
        class Symlink(name: String, val target: String) : Entry(name)
        class Dir(name: String) : Entry(name)
    }

    private fun file(name: String, text: String, mode: Int? = null) = Entry.FileEntry(name, text.toByteArray(), mode)

    private fun zip(vararg entries: Entry): File {
        val f = tmp.newFile()
        ZipArchiveOutputStream(f).use { out ->
            for (e in entries) {
                val entry = ZipArchiveEntry(e.name)
                when (e) {
                    is Entry.FileEntry -> {
                        e.mode?.let { entry.unixMode = it }
                        out.putArchiveEntry(entry)
                        out.write(e.data)
                    }

                    is Entry.Symlink -> {
                        entry.unixMode = 0xA1FF
                        out.putArchiveEntry(entry)
                        out.write(e.target.toByteArray())
                    }

                    is Entry.Dir -> out.putArchiveEntry(entry)
                }
                out.closeArchiveEntry()
            }
        }
        return f
    }

    private fun validZip(vararg extra: Entry) = zip(file("module.prop", prop), file("action.sh", "echo hi\n"), *extra)

    private fun assertReason(reason: ScriptPackageException.Reason, block: () -> Unit) {
        val e = assertThrows(ScriptPackageException::class.java) { block() }
        assertEquals(reason, e.reason)
    }

    @Test
    fun oneshotFlagMatchesKsudBooleanRule() {
        assertTrue(ScriptPackage.isOneshot("true"))
        assertTrue(ScriptPackage.isOneshot(" TRUE "))
        assertTrue(ScriptPackage.isOneshot("True"))
        assertTrue(ScriptPackage.isOneshot("1"))
        assertFalse(ScriptPackage.isOneshot("yes"))
        assertFalse(ScriptPackage.isOneshot("0"))
        assertFalse(ScriptPackage.isOneshot(""))
        assertFalse(ScriptPackage.isOneshot(null))
    }

    @Test
    fun parsesModuleProp() {
        val props = ScriptPackage.parseModuleProp("# comment\r\nid = a.b\r\nname=Name = with eq\nbogus\nid=second\n")
        assertEquals("a.b", props["id"])
        assertEquals("Name = with eq", props["name"])
        assertEquals(2, props.size)
        val m = ScriptPackage.manifestOf(ScriptPackage.parseModuleProp(prop))
        assertEquals(ScriptManifest("my_script", "My Script", "v1", 3, "Alice", "Does a thing"), m)
    }

    @Test
    fun checksEntryNames() {
        for (ok in listOf("a", "a/b", "dir/", "a/b.sh", ".hidden", "a..b")) ScriptPackage.checkEntryName(ok)
        for (bad in listOf("", "/abs", "../x", "a/../b", "a/..", "./a", "a//b", "a\\b", "a\u0000", "..")) {
            assertReason(ScriptPackageException.Reason.UNSAFE_ENTRY) { ScriptPackage.checkEntryName(bad) }
        }
    }

    @Test
    fun validatesAndExtractsScript() {
        val z = validZip(
            file("params.json", """{"schemaVersion":1,"params":[{"key":"A","type":"bool","label":"a"}]}"""),
            Entry.Dir("data/"),
            file("data/nested/file.txt", "payload", mode = 0x81A4),
        )
        assertEquals("my_script", ScriptPackage.validate(z, "my_script").id)
        val dest = File(tmp.root, "oneshot/abc")
        ScriptPackage.extract(z, dest)
        assertEquals("echo hi\n", File(dest, "action.sh").readText())
        assertEquals("payload", File(dest, "data/nested/file.txt").readText())
        // No temporary directory is left next to the extraction.
        assertEquals(listOf("abc"), dest.parentFile!!.list()!!.toList())

        // Re-extraction replaces the previous contents.
        File(dest, "stale").writeText("x")
        ScriptPackage.extract(z, dest)
        assertFalse(File(dest, "stale").exists())
    }

    @Test
    fun rejectsInvalidScripts() {
        assertReason(ScriptPackageException.Reason.ID_MISMATCH) { ScriptPackage.validate(validZip(), "other") }
        assertReason(ScriptPackageException.Reason.MISSING_MODULE_PROP) {
            ScriptPackage.validate(zip(file("action.sh", "x")))
        }
        assertReason(ScriptPackageException.Reason.NOT_ONESHOT) {
            ScriptPackage.validate(zip(file("module.prop", "id=my_script\n"), file("action.sh", "x")))
        }
        assertReason(ScriptPackageException.Reason.NOT_ONESHOT) {
            ScriptPackage.validate(zip(file("module.prop", "id=my_script\noneshot=yes\n"), file("action.sh", "x")))
        }
        assertReason(ScriptPackageException.Reason.INVALID_ID) {
            ScriptPackage.validate(zip(file("module.prop", "id=1bad\noneshot=1\n"), file("action.sh", "x")))
        }
        assertReason(ScriptPackageException.Reason.MISSING_ACTION) {
            ScriptPackage.validate(zip(file("module.prop", prop), file("sub/action.sh", "x")))
        }
        assertReason(ScriptPackageException.Reason.MISSING_ACTION) {
            ScriptPackage.validate(zip(file("module.prop", prop), Entry.Dir("action.sh/")))
        }
        assertReason(ScriptPackageException.Reason.INVALID_PARAMS) {
            ScriptPackage.validate(validZip(file("params.json", """{"schemaVersion":2,"params":[]}""")))
        }
        val notZip = tmp.newFile().apply { writeText("not a zip") }
        assertReason(ScriptPackageException.Reason.CORRUPT) { ScriptPackage.validate(notZip) }
    }

    @Test
    fun rejectsUnsafeEntries() {
        val dest = File(tmp.root, "out")
        for (z in listOf(
            validZip(file("../evil", "x")),
            validZip(file("/abs", "x")),
            validZip(file("a/../../evil", "x")),
            validZip(Entry.Symlink("link", "/data/adb")),
            validZip(file("fifo", "", mode = 0x11A4)),
            validZip(file("action.sh", "dup")),
        )) {
            assertReason(ScriptPackageException.Reason.UNSAFE_ENTRY) { ScriptPackage.validate(z) }
            assertReason(ScriptPackageException.Reason.UNSAFE_ENTRY) { ScriptPackage.extract(z, dest) }
            assertFalse(dest.exists())
            assertFalse(File(tmp.root, "evil").exists())
        }
        // Nothing but the test ZIPs is left in the parent directory.
        assertTrue(tmp.root.list()!!.none { it.startsWith(".tmp-") })
    }

    @Test
    fun enforcesSizeLimit() {
        // Highly compressible: tiny on disk, larger than the limit once extracted.
        val big = ByteArray((ScriptPackage.MAX_TOTAL_BYTES + 1).toInt())
        val z = validZip(Entry.FileEntry("big.bin", big))
        assertReason(ScriptPackageException.Reason.TOO_LARGE) { ScriptPackage.validate(z) }
        val dest = File(tmp.root, "out")
        assertReason(ScriptPackageException.Reason.TOO_LARGE) { ScriptPackage.extract(z, dest) }
        assertFalse(dest.exists())
    }

    @Test
    fun peeksOneshotFromStream() {
        val m = FileInputStream(validZip()).use { ScriptPackage.readOneshotManifest(it) }
        assertEquals("my_script", m?.id)
        val module = zip(file("module.prop", "id=m\nname=M\n"), file("customize.sh", ""))
        assertNull(FileInputStream(module).use { ScriptPackage.readOneshotManifest(it) })
        assertNull("garbage".byteInputStream().use { ScriptPackage.readOneshotManifest(it) })
    }
}
