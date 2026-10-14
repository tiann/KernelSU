package me.weishu.kernelsu.data.script

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class ScriptParamsTest {

    private val sample = """
        {
          "schemaVersion": 1,
          "params": [
            {
              "key": "ENABLE_FASTBOOT",
              "type": "bool",
              "label": "Also enable fastboot",
              "description": "Optional longer help text",
              "default": true,
              "futureField": 1
            },
            { "key": "MODE", "type": "enum", "label": "Mode", "default": "a",
              "options": [ { "value": "a", "label": "A" }, { "value": "b" } ] },
            { "key": "NAME", "type": "string", "label": "Name", "default": "", "maxLength": 64,
              "required": false },
            { "key": "COUNT", "type": "int", "label": "Count", "default": 1, "min": 0, "max": 10 }
          ]
        }
    """.trimIndent()

    private fun parse(params: String) = ScriptParamsParser.parse("""{"schemaVersion":1,"params":[$params]}""")

    private fun assertInvalid(params: String) {
        assertThrows(ScriptParamsException::class.java) { parse(params) }
    }

    @Test
    fun parsesContractSample() {
        val p = ScriptParamsParser.parse(sample).params
        assertEquals(listOf("ENABLE_FASTBOOT", "MODE", "NAME", "COUNT"), p.map { it.key })

        val bool = p[0] as ScriptParam.Bool
        assertEquals(true, bool.default)
        assertEquals("Also enable fastboot", bool.label)
        assertEquals("Optional longer help text", bool.description)

        val choice = p[1] as ScriptParam.Choice
        assertEquals(listOf("a", "b"), choice.options.map { it.value })
        assertEquals("A", choice.options[0].label)
        assertNull(choice.options[1].label)

        val text = p[2] as ScriptParam.Text
        assertEquals(64, text.maxLength)

        val int = p[3] as ScriptParam.Integer
        assertEquals(1, int.default)
        assertEquals(0, int.min)
        assertEquals(10, int.max)

        assertEquals(
            mapOf("ENABLE_FASTBOOT" to "true", "MODE" to "a", "NAME" to "", "COUNT" to "1"),
            ScriptParamsParser.parse(sample).defaults,
        )
    }

    @Test
    fun appliesDefaults() {
        val p = parse(
            """
            {"key":"B","type":"bool","label":"b"},
            {"key":"E","type":"enum","label":"e","options":[{"value":"x"},{"value":"y"}]},
            {"key":"S","type":"string","label":"s"},
            {"key":"I","type":"int","label":"i"},
            {"key":"J","type":"int","label":"j","min":5}
            """
        )
        assertEquals(mapOf("B" to "false", "E" to "x", "S" to "", "I" to "0", "J" to "5"), p.defaults)
        assertEquals(ScriptParamsParser.DEFAULT_MAX_LENGTH, (p.params[2] as ScriptParam.Text).maxLength)
        assertEquals(false, (p.params[2] as ScriptParam.Text).required)
    }

    @Test
    fun rejectsSchemaViolations() {
        assertThrows(ScriptParamsException::class.java) { ScriptParamsParser.parse("""{"schemaVersion":2,"params":[]}""") }
        assertThrows(ScriptParamsException::class.java) { ScriptParamsParser.parse("""{"schemaVersion":"1","params":[]}""") }
        assertThrows(ScriptParamsException::class.java) { ScriptParamsParser.parse("""{"schemaVersion":1}""") }
        assertThrows(ScriptParamsException::class.java) { ScriptParamsParser.parse("not json") }
        assertThrows(ScriptParamsException::class.java) { ScriptParamsParser.parse("[]") }

        val tooMany = (1..17).joinToString(",") { """{"key":"K$it","type":"bool","label":"x"}""" }
        assertInvalid(tooMany)
        assertEquals(16, parse((1..16).joinToString(",") { """{"key":"K$it","type":"bool","label":"x"}""" }).params.size)

        assertInvalid("""{"key":"lower","type":"bool","label":"x"}""")
        assertInvalid("""{"key":"1ABC","type":"bool","label":"x"}""")
        assertInvalid("""{"key":"A${"B".repeat(32)}","type":"bool","label":"x"}""")
        assertInvalid("""{"key":"A","type":"bool","label":"x"},{"key":"A","type":"bool","label":"y"}""")
        assertInvalid("""{"key":"A","type":"float","label":"x"}""")
        assertInvalid("""{"key":"A","type":"bool"}""")
    }

    @Test
    fun validatesText() {
        assertInvalid("""{"key":"A","type":"bool","label":""}""")
        assertInvalid("""{"key":"A","type":"bool","label":"${"x".repeat(201)}"}""")
        assertInvalid("""{"key":"A","type":"bool","label":{"en":"x"}}""")
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[{"value":"a","label":{"en":"A"}}]}""")
        assertInvalid("""{"key":"A","type":"bool","label":1}""")
        assertInvalid("""{"key":"A","type":"bool","label":"x","description":""}""")
        // Trimmed before the length check, like the validator's zod schema.
        assertInvalid("""{"key":"A","type":"bool","label":"   "}""")
        assertEquals("x".repeat(200), parse("""{"key":"A","type":"bool","label":" ${"x".repeat(200)}  "}""").params[0].label)
    }

    @Test
    fun matchesValidatorJsonReading() {
        // A UTF-8 BOM is dropped, and integers may be written like any JSON number with an integral value.
        assertEquals(0, ScriptParamsParser.parse("\uFEFF{\"schemaVersion\":1,\"params\":[]}").params.size)
        assertEquals(0, ScriptParamsParser.parse("""{"schemaVersion":1.0,"params":[]}""").params.size)
        val text = parse("""{"key":"A","type":"string","label":"x","maxLength":5.0}""").params[0] as ScriptParam.Text
        assertEquals(5, text.maxLength)
        val int = parse("""{"key":"A","type":"int","label":"x","min":1e1,"max":2E+1}""").params[0] as ScriptParam.Integer
        assertEquals(10, int.min)
        assertEquals(20, int.max)
        assertInvalid("""{"key":"A","type":"int","label":"x","min":1.5}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","min":1e10}""")
    }

    @Test
    fun validatesTypeSpecificFields() {
        assertInvalid("""{"key":"A","type":"bool","label":"x","default":"true"}""")
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[]}""")
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[{"value":"a b"}]}""")
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[{"value":"a"},{"value":"a"}]}""")
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[{"value":"a"}],"default":"b"}""")
        val options = (1..33).joinToString(",") { """{"value":"v$it"}""" }
        assertInvalid("""{"key":"A","type":"enum","label":"x","options":[$options]}""")

        assertInvalid("""{"key":"A","type":"string","label":"x","maxLength":0}""")
        assertInvalid("""{"key":"A","type":"string","label":"x","maxLength":1025}""")
        assertInvalid("""{"key":"A","type":"string","label":"x","maxLength":2,"default":"abc"}""")
        assertInvalid("""{"key":"A","type":"string","label":"x","default":"a\nb"}""")
        assertInvalid("""{"key":"A","type":"string","label":"x","required":"yes"}""")
        // A required field may start out empty: the user has to fill it in.
        parse("""{"key":"A","type":"string","label":"x","required":true}""")

        assertInvalid("""{"key":"A","type":"int","label":"x","default":1.5}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","default":"1"}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","min":5,"max":1}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","min":1,"max":3,"default":4}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","max":-1}""")
        // int32 only.
        assertInvalid("""{"key":"A","type":"int","label":"x","max":2147483648}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","min":-2147483649}""")
        assertInvalid("""{"key":"A","type":"int","label":"x","default":2147483648}""")
        parse("""{"key":"A","type":"int","label":"x","min":-2147483648,"max":2147483647}""")

        // `pattern` is not part of the format: like any unknown field it is ignored, never enforced.
        val text = parse("""{"key":"A","type":"string","label":"x","pattern":"(a+)+$"}""").params[0]
        assertNull(text.validate("a".repeat(64) + "!"))
    }

    @Test
    fun rejectsDuplicateJsonKeys() {
        val e = assertThrows(ScriptParamsException::class.java) {
            ScriptParamsParser.parse("""{"schemaVersion":1,"params":[{"key":"A","type":"bool","label":"x","label":"y"}]}""")
        }
        assertTrue(e.message!!.contains("label"))
        assertThrows(ScriptParamsException::class.java) {
            ScriptParamsParser.parse("""{"schemaVersion":1,"params":[],"params":[]}""")
        }
        // Escaped spellings of the same key are the same key.
        assertEquals("ab", ScriptParamsParser.findDuplicateKey("""{"ab":1,"a\u0062":2}"""))
        // Same key in different objects, or as a string value, is fine.
        assertNull(ScriptParamsParser.findDuplicateKey("""{"a":{"a":1},"b":["a","a"],"c":"a","d":{"x":"\"a\""}}"""))
        assertNull(ScriptParamsParser.findDuplicateKey(sample))
    }

    @Test
    fun validatesValues() {
        val p = ScriptParamsParser.parse(sample)
        val bool = p.params[0]
        assertNull(bool.validate("false"))
        assertEquals(ParamError.INVALID_BOOLEAN, bool.validate("yes"))

        val choice = p.params[1]
        assertNull(choice.validate("b"))
        assertEquals(ParamError.INVALID_OPTION, choice.validate("c"))

        val text = p.params[2]
        assertNull(text.validate(""))
        assertNull(text.validate("abc"))
        assertEquals(ParamError.TOO_LONG, text.validate("a".repeat(65)))
        assertEquals(ParamError.INVALID_CHARACTERS, text.validate("a\nb"))
        assertEquals(ParamError.INVALID_CHARACTERS, text.validate("a\u0000"))

        val required = parse("""{"key":"R","type":"string","label":"x","required":true}""").params[0]
        assertEquals(ParamError.REQUIRED, required.validate(""))
        assertNull(required.validate("it's \$fine `really`"))

        val count = p.params[3]
        assertNull(count.validate("10"))
        assertNull(count.validate(" 7 "))
        assertEquals("7", count.canonical("+7"))
        assertEquals(ParamError.ABOVE_MAX, count.validate("11"))
        assertEquals(ParamError.BELOW_MIN, count.validate("-1"))
        assertEquals(ParamError.NOT_AN_INTEGER, count.validate("1.0"))
        assertEquals(ParamError.NOT_AN_INTEGER, count.validate(""))
        assertEquals(ParamError.NOT_AN_INTEGER, count.validate("99999999999999999999"))
        assertEquals(ParamError.NOT_AN_INTEGER, count.validate("2147483648"))
        assertEquals(ParamError.INVALID_CHARACTERS, text.validate("a\rb"))

        assertTrue(p.validate(p.defaults).isEmpty())
        assertEquals(mapOf("COUNT" to ParamError.ABOVE_MAX), p.validate(mapOf("COUNT" to "42")))
    }
}
