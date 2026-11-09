package me.weishu.kernelsu.data.script

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class ScriptCommandTest {

    private val host = ScriptHostInfo(ksuVersion = "v3.0.0-1-gabc", ksuVersionCode = 31234, kernelVersionCode = 32500, lateLoad = false)
    private val dir = "/data/user/0/me.weishu.kernelsu/files/oneshot/" + "a".repeat(64)

    private val params = ScriptParamsParser.parse(
        """
        {"schemaVersion":1,"params":[
          {"key":"FLAG","type":"bool","label":"f","default":true},
          {"key":"MODE","type":"enum","label":"m","options":[{"value":"a"},{"value":"b"}]},
          {"key":"NAME","type":"string","label":"n"},
          {"key":"COUNT","type":"int","label":"c","min":0,"max":10}
        ]}
        """.trimIndent()
    )

    @Test
    fun quotesForPosixShell() {
        assertEquals("''", ScriptCommand.quote(""))
        assertEquals("'abc'", ScriptCommand.quote("abc"))
        assertEquals("'it'\\''s'", ScriptCommand.quote("it's"))
        assertEquals("'\$(id) `id` \"x\" \\ ;&|'", ScriptCommand.quote("\$(id) `id` \"x\" \\ ;&|"))
        assertEquals("''\\'''\\'''", ScriptCommand.quote("''"))
        assertThrows(IllegalArgumentException::class.java) { ScriptCommand.quote("a\u0000b") }
    }

    @Test
    fun buildsEnvironmentInContractOrder() {
        val env = ScriptCommand.environment(host, "my_script", dir, params, mapOf("NAME" to "x'y", "COUNT" to "+3"))
        assertEquals(
            listOf(
                "ASH_STANDALONE" to "1",
                "KSU" to "true",
                "KSU_VER" to "v3.0.0-1-gabc",
                "KSU_VER_CODE" to "31234",
                "KSU_KERNEL_VER_CODE" to "32500",
                "KSU_MODULE" to "my_script",
                "KSU_ONESHOT" to "1",
                "MODPATH" to dir,
                "KSU_PARAM_FLAG" to "true",
                "KSU_PARAM_MODE" to "a",
                "KSU_PARAM_NAME" to "x'y",
                "KSU_PARAM_COUNT" to "3",
            ),
            env,
        )
        val late = ScriptCommand.environment(host.copy(lateLoad = true), "my_script", dir, ScriptParams.EMPTY, emptyMap())
        assertEquals("KSU_LATE_LOAD" to "1", late[5])
    }

    @Test
    fun rejectsInvalidValues() {
        assertThrows(IllegalArgumentException::class.java) {
            ScriptCommand.environment(host, "s", dir, params, mapOf("COUNT" to "11"))
        }
        assertThrows(IllegalArgumentException::class.java) {
            ScriptCommand.environment(host, "s", dir, params, mapOf("MODE" to "c"))
        }
        assertThrows(IllegalArgumentException::class.java) {
            ScriptCommand.environment(host, "s", dir, params, mapOf("NAME" to "a\nb"))
        }
        assertThrows(IllegalArgumentException::class.java) {
            ScriptCommand.build(dir, listOf("BAD-NAME" to "x"))
        }
        assertThrows(IllegalArgumentException::class.java) {
            ScriptCommand.build(dir, listOf("PATH" to "/tmp"))
        }
    }

    @Test
    fun buildsCommandWithEveryValueQuoted() {
        val env = ScriptCommand.environment(
            host, "my_script", dir, params,
            mapOf("NAME" to "'; rm -rf / #", "MODE" to "b", "FLAG" to "false"),
        )
        val cmd = ScriptCommand.build(dir, env)
        assertEquals(
            "cd '$dir' && ASH_STANDALONE='1' KSU='true' KSU_VER='v3.0.0-1-gabc' KSU_VER_CODE='31234' " +
                "KSU_KERNEL_VER_CODE='32500' KSU_MODULE='my_script' KSU_ONESHOT='1' MODPATH='$dir' " +
                "KSU_PARAM_FLAG='false' KSU_PARAM_MODE='b' KSU_PARAM_NAME=''\\''; rm -rf / #' KSU_PARAM_COUNT='0' " +
                "PATH=\"\$PATH\":'/data/adb/ksu/bin' '/data/adb/ksu/bin/busybox' sh ./action.sh </dev/null",
            cmd,
        )
    }

    @Test
    fun classifiesExitCodes() {
        assertEquals(ScriptOutcome.SUCCESS, ScriptOutcome.of(0))
        assertEquals(ScriptOutcome.NOT_APPLICABLE, ScriptOutcome.of(2))
        assertEquals(ScriptOutcome.FAILURE, ScriptOutcome.of(1))
        assertEquals(ScriptOutcome.FAILURE, ScriptOutcome.of(127))
        assertEquals(ScriptOutcome.FAILURE, ScriptOutcome.of(-1))
    }

    @Test
    fun runRecordsRoundTrip() {
        val storage = object : ScriptRunStorage {
            val map = HashMap<String, String>()
            override fun read(key: String) = map[key]
            override fun write(key: String, value: String) {
                map[key] = value
            }
        }
        val history = ScriptRunHistory(storage)
        assertEquals(null, history.last("a"))
        val record = ScriptRunRecord(id = "a", versionCode = 3, time = 1234L, exitCode = 2)
        history.record(record)
        assertEquals(record, history.last("a"))
        assertEquals(ScriptOutcome.NOT_APPLICABLE, history.last("a")!!.outcome)
        assertEquals(null, history.last("b"))
        assertEquals(null, ScriptRunRecord.decode("garbage", "a"))
        // A record stored under another id is ignored.
        assertEquals(null, ScriptRunRecord.decode(record.encode(), "b"))
    }
}
