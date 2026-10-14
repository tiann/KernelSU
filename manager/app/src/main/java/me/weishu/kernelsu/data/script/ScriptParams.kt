package me.weishu.kernelsu.data.script

import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.longOrNull

/*
 * params.json (schemaVersion 1) of a one-shot script: the parameters the manager asks for
 * before running action.sh. Each value reaches the script as `KSU_PARAM_<KEY>`.
 */

class ScriptParamsException(message: String) : Exception(message)

/**
 * A plain string (same text for every locale) or locale -> string. Plain strings are stored
 * with an empty locale key.
 */
data class LocalizedText(val entries: List<Pair<String, String>>) {

    /** Exact locale, then language + region, then language only, then `en`, then the first entry. */
    fun resolve(locale: String): String {
        if (entries.size == 1) return entries[0].second
        for (candidate in localeCandidates(locale) + "en") {
            entries.firstOrNull { it.first.equals(candidate, ignoreCase = true) }?.let { return it.second }
        }
        return entries.first().second
    }

    companion object {
        fun of(text: String) = LocalizedText(listOf("" to text))

        /** "zh-Hans-CN" -> ["zh-Hans-CN", "zh-CN", "zh"]; Chinese scripts without a region imply one. */
        internal fun localeCandidates(locale: String): List<String> {
            val parts = locale.trim().replace('_', '-').split('-').filter { it.isNotEmpty() }
            if (parts.isEmpty()) return emptyList()
            val language = parts[0]
            val rest = parts.drop(1)
            val script = rest.firstOrNull()?.takeIf { it.length == 4 && it.all(Char::isLetter) }
            val region = rest.firstOrNull { (it.length == 2 && it.all(Char::isLetter)) || (it.length == 3 && it.all(Char::isDigit)) }
            val impliedRegion = when {
                region != null || !language.equals("zh", ignoreCase = true) -> null
                script.equals("Hans", ignoreCase = true) -> "CN"
                script.equals("Hant", ignoreCase = true) -> "TW"
                else -> null
            }
            return listOfNotNull(
                parts.joinToString("-"),
                (region ?: impliedRegion)?.let { "$language-$it" },
                language,
            ).distinct()
        }
    }
}

/** Why a parameter value is rejected. */
enum class ParamError {
    REQUIRED,
    TOO_LONG,
    INVALID_CHARACTERS,
    PATTERN_MISMATCH,
    NOT_AN_INTEGER,
    BELOW_MIN,
    ABOVE_MAX,
    INVALID_OPTION,
    INVALID_BOOLEAN,
}

/**
 * One declared parameter. Values are handled in their canonical string form, which is also
 * what the script receives: `true`/`false`, a decimal integer, an enum value or the raw string.
 */
sealed class ScriptParam {
    abstract val key: String
    abstract val label: LocalizedText
    abstract val description: LocalizedText?

    /** Canonical form of the default value. */
    abstract val defaultValue: String

    /** Returns null when [value] is acceptable. */
    abstract fun validate(value: String): ParamError?

    /** Canonical value passed to the script; only call with a value [validate] accepted. */
    open fun canonical(value: String): String = value

    val envName: String get() = ENV_PREFIX + key

    data class Bool(
        override val key: String,
        override val label: LocalizedText,
        override val description: LocalizedText?,
        val default: Boolean,
    ) : ScriptParam() {
        override val defaultValue: String get() = default.toString()
        override fun validate(value: String): ParamError? =
            if (value == "true" || value == "false") null else ParamError.INVALID_BOOLEAN
    }

    data class EnumOption(val value: String, val label: LocalizedText?)

    data class Choice(
        override val key: String,
        override val label: LocalizedText,
        override val description: LocalizedText?,
        val options: List<EnumOption>,
        val default: String,
    ) : ScriptParam() {
        override val defaultValue: String get() = default
        override fun validate(value: String): ParamError? =
            if (options.any { it.value == value }) null else ParamError.INVALID_OPTION
    }

    data class Text(
        override val key: String,
        override val label: LocalizedText,
        override val description: LocalizedText?,
        val default: String,
        val maxLength: Int,
        val pattern: String?,
        val required: Boolean,
    ) : ScriptParam() {
        private val regex: Regex? by lazy { pattern?.let(::Regex) }
        override val defaultValue: String get() = default

        /** Empty is valid unless [required]; [pattern] (full match) applies to non-empty values. */
        override fun validate(value: String): ParamError? = when {
            value.isEmpty() -> if (required) ParamError.REQUIRED else null
            value.any { it == '\u0000' || it == '\n' || it == '\r' } -> ParamError.INVALID_CHARACTERS
            value.length > maxLength -> ParamError.TOO_LONG
            regex?.let { matchesWithin(it, value) } == false -> ParamError.PATTERN_MISMATCH
            else -> null
        }
    }

    data class Integer(
        override val key: String,
        override val label: LocalizedText,
        override val description: LocalizedText?,
        val default: Int,
        val min: Int?,
        val max: Int?,
    ) : ScriptParam() {
        override val defaultValue: String get() = default.toString()

        override fun validate(value: String): ParamError? {
            val number = parseInteger(value) ?: return ParamError.NOT_AN_INTEGER
            return when {
                min != null && number < min -> ParamError.BELOW_MIN
                max != null && number > max -> ParamError.ABOVE_MAX
                else -> null
            }
        }

        override fun canonical(value: String): String = parseInteger(value)!!.toString()

        companion object {
            private val INTEGER = Regex("^[+-]?[0-9]{1,10}$")

            /** Values are int32 like min / max / default; anything outside is not an integer. */
            fun parseInteger(value: String): Int? = value.trim().takeIf(INTEGER::matches)?.toIntOrNull()
        }
    }

    companion object {
        /** Upper bound for one pattern match; validation runs on every edit, on the UI thread. */
        const val PATTERN_TIMEOUT_MS = 100L

        /**
         * Full match of [regex] against [value] that gives up (as a mismatch) after
         * [PATTERN_TIMEOUT_MS]: patterns come from the package, and java.util.regex can
         * backtrack exponentially, e.g. `(a+)+$`.
         */
        fun matchesWithin(regex: Regex, value: String, timeoutMs: Long = PATTERN_TIMEOUT_MS): Boolean = try {
            regex.matches(DeadlineCharSequence(value, System.nanoTime() + timeoutMs * 1_000_000))
        } catch (_: PatternTimeoutException) {
            false
        }

        const val ENV_PREFIX = "KSU_PARAM_"
    }
}

data class ScriptParams(val params: List<ScriptParam>) {
    val defaults: Map<String, String> get() = params.associate { it.key to it.defaultValue }

    /** Error per key for [values] (missing values fall back to the default). */
    fun validate(values: Map<String, String>): Map<String, ParamError> =
        params.mapNotNull { p -> p.validate(values[p.key] ?: p.defaultValue)?.let { p.key to it } }.toMap()

    companion object {
        val EMPTY = ScriptParams(emptyList())
    }
}

/** Parses and validates params.json; any violation of schemaVersion 1 throws [ScriptParamsException]. */
object ScriptParamsParser {
    const val SCHEMA_VERSION = 1
    const val MAX_PARAMS = 16
    const val MAX_OPTIONS = 32
    const val MAX_TEXT_LENGTH = 200
    const val DEFAULT_MAX_LENGTH = 256
    const val MAX_MAX_LENGTH = 1024
    const val MAX_PATTERN_LENGTH = 256

    private val KEY = Regex("^[A-Z][A-Z0-9_]{0,31}$")
    private val LOCALE = Regex("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$")
    private val OPTION_VALUE = Regex("^[A-Za-z0-9._-]{1,64}$")
    private val JSON = Json { isLenient = false }

    private fun fail(message: String): Nothing = throw ScriptParamsException(message)

    fun parse(text: String): ScriptParams {
        val root = try {
            JSON.parseToJsonElement(text)
        } catch (e: SerializationException) {
            fail("Malformed JSON: ${e.message}")
        } catch (e: IllegalArgumentException) {
            fail("Malformed JSON: ${e.message}")
        } as? JsonObject ?: fail("params.json must be an object")
        // kotlinx keeps the last of duplicate keys; the registry rejects them, so do we.
        findDuplicateKey(text)?.let { fail("Duplicate JSON key: $it") }

        val version = (root["schemaVersion"] as? JsonPrimitive)?.takeIf { !it.isString }?.longOrNull
        if (version != SCHEMA_VERSION.toLong()) fail("Unsupported schemaVersion: ${root["schemaVersion"]}")
        val array = root["params"] as? JsonArray ?: fail("params must be an array")
        if (array.size > MAX_PARAMS) fail("At most $MAX_PARAMS params are allowed")

        val seen = HashSet<String>()
        val params = array.mapIndexed { index, element ->
            val obj = element as? JsonObject ?: fail("params[$index] must be an object")
            parseParam(obj, index).also { if (!seen.add(it.key)) fail("Duplicate key: ${it.key}") }
        }
        return ScriptParams(params)
    }

    private fun parseParam(obj: JsonObject, index: Int): ScriptParam {
        val key = obj.string("key") ?: fail("params[$index].key is required")
        if (!KEY.matches(key)) fail("Invalid key: $key")
        val label = localized(obj["label"], "$key.label") ?: fail("$key.label is required")
        val description = localized(obj["description"], "$key.description")
        val default = obj["default"]?.takeIf { it !is JsonNull }

        val param = when (obj.string("type")) {
            "bool" -> ScriptParam.Bool(
                key, label, description,
                default = default?.let { (it as? JsonPrimitive)?.takeIf { p -> !p.isString }?.booleanOrNull ?: fail("$key.default must be a boolean") }
                    ?: false,
            )

            "enum" -> {
                val array = obj["options"] as? JsonArray ?: fail("$key.options must be an array")
                if (array.size !in 1..MAX_OPTIONS) fail("$key.options must have 1..$MAX_OPTIONS entries")
                val values = HashSet<String>()
                val options = array.map { e ->
                    val o = e as? JsonObject ?: fail("$key.options entries must be objects")
                    val value = o.string("value")?.takeIf(OPTION_VALUE::matches) ?: fail("$key: invalid option value")
                    if (!values.add(value)) fail("$key: duplicate option $value")
                    ScriptParam.EnumOption(value, localized(o["label"], "$key.$value.label"))
                }
                val defaultValue = default?.let { (it as? JsonPrimitive)?.takeIf { p -> p.isString }?.content ?: fail("$key.default must be a string") }
                    ?: options.first().value
                ScriptParam.Choice(key, label, description, options, defaultValue)
            }

            "string" -> {
                val maxLength = obj.integer("maxLength", key)?.also {
                    if (it !in 1..MAX_MAX_LENGTH) fail("$key.maxLength must be 1..$MAX_MAX_LENGTH")
                } ?: DEFAULT_MAX_LENGTH
                val pattern = obj["pattern"]?.takeIf { it !is JsonNull }?.let {
                    (it as? JsonPrimitive)?.takeIf { p -> p.isString }?.content ?: fail("$key.pattern must be a string")
                }
                if (pattern != null) {
                    if (pattern.length > MAX_PATTERN_LENGTH) fail("$key.pattern is longer than $MAX_PATTERN_LENGTH")
                    runCatching { Regex(pattern) }.onFailure { fail("$key.pattern does not compile: ${it.message}") }
                }
                val required = obj["required"]?.takeIf { it !is JsonNull }?.let {
                    (it as? JsonPrimitive)?.takeIf { p -> !p.isString }?.booleanOrNull ?: fail("$key.required must be a boolean")
                } ?: false
                val defaultValue = default?.let { (it as? JsonPrimitive)?.takeIf { p -> p.isString }?.content ?: fail("$key.default must be a string") }
                    ?: ""
                // An empty default is always fine, even when required: the user has to fill it in.
                ScriptParam.Text(key, label, description, defaultValue, maxLength, pattern, required)
            }

            "int" -> {
                val min = obj.integer("min", key)
                val max = obj.integer("max", key)
                if (min != null && max != null && min > max) fail("$key: min > max")
                val defaultValue = default?.let { integerOf(it) ?: fail("$key.default must be an integer") } ?: min ?: 0
                ScriptParam.Integer(key, label, description, defaultValue, min, max)
            }

            else -> fail("$key: unsupported type ${obj["type"]}")
        }
        param.validate(param.defaultValue)?.let { if (it != ParamError.REQUIRED) fail("$key.default is invalid: $it") }
        return param
    }

    private fun JsonObject.string(name: String): String? =
        (this[name] as? JsonPrimitive)?.takeIf { it.isString }?.content

    /** A plain JSON integer (no fraction or exponent) within int32. */
    private fun integerOf(element: JsonElement): Int? {
        val p = element as? JsonPrimitive ?: return null
        if (p.isString) return null
        if (!Regex("^-?[0-9]+$").matches(p.content)) return null
        return p.content.toIntOrNull()
    }

    private fun JsonObject.integer(name: String, key: String): Int? {
        val e = this[name]?.takeIf { it !is JsonNull } ?: return null
        return integerOf(e) ?: fail("$key.$name must be a 32-bit integer")
    }

    /**
     * First key that occurs twice in one object of the (already parsed, so valid) JSON
     * [text], or null.
     */
    internal fun findDuplicateKey(text: String): String? {
        class Frame(val keys: HashSet<String>?, var expectKey: Boolean)

        val stack = ArrayDeque<Frame>()
        var i = 0
        while (i < text.length) {
            when (text[i]) {
                '{' -> stack.addLast(Frame(HashSet(), expectKey = true))
                '[' -> stack.addLast(Frame(null, expectKey = false))
                '}', ']' -> stack.removeLastOrNull()
                ',' -> stack.lastOrNull()?.let { if (it.keys != null) it.expectKey = true }
                '"' -> {
                    val sb = StringBuilder()
                    i++
                    while (i < text.length && text[i] != '"') {
                        if (text[i] == '\\' && i + 1 < text.length) {
                            i++
                            when (val e = text[i]) {
                                'u' -> {
                                    text.substring(i + 1, minOf(i + 5, text.length)).toIntOrNull(16)?.let { sb.append(it.toChar()) }
                                    i += 4
                                }

                                'b' -> sb.append('\b')
                                'f' -> sb.append('\u000C')
                                'n' -> sb.append('\n')
                                'r' -> sb.append('\r')
                                't' -> sb.append('\t')
                                else -> sb.append(e)
                            }
                        } else {
                            sb.append(text[i])
                        }
                        i++
                    }
                    val top = stack.lastOrNull()
                    if (top?.keys != null && top.expectKey) {
                        val key = sb.toString()
                        if (!top.keys.add(key)) return key
                        top.expectKey = false
                    }
                }
            }
            i++
        }
        return null
    }

    private fun checkText(text: String, what: String): String {
        if (text.isEmpty() || text.length > MAX_TEXT_LENGTH) fail("$what must be 1..$MAX_TEXT_LENGTH characters")
        return text
    }

    private fun localized(element: JsonElement?, what: String): LocalizedText? = when (element) {
        null, is JsonNull -> null
        is JsonPrimitive -> {
            if (!element.isString) fail("$what must be a string or an object")
            LocalizedText.of(checkText(element.content, what))
        }

        is JsonObject -> {
            if (element.isEmpty()) fail("$what must have at least one locale")
            LocalizedText(element.entries.map { (locale, value) ->
                if (!LOCALE.matches(locale)) fail("$what: invalid locale $locale")
                val text = (value as? JsonPrimitive)?.takeIf { it.isString }?.content ?: fail("$what.$locale must be a string")
                locale to checkText(text, "$what.$locale")
            })
        }

        else -> fail("$what must be a string or an object")
    }
}

private class PatternTimeoutException : RuntimeException()

/** Lets a regex match be aborted: every character access checks the deadline. */
private class DeadlineCharSequence(
    private val inner: CharSequence,
    private val deadlineNanos: Long,
) : CharSequence {
    override val length: Int get() = inner.length

    override fun get(index: Int): Char {
        if (System.nanoTime() > deadlineNanos) throw PatternTimeoutException()
        return inner[index]
    }

    override fun subSequence(startIndex: Int, endIndex: Int): CharSequence =
        DeadlineCharSequence(inner.subSequence(startIndex, endIndex), deadlineNanos)

    override fun toString(): String = inner.toString()
}
