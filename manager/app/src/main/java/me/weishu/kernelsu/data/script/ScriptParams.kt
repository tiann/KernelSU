package me.weishu.kernelsu.data.script

import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.intOrNull

/*
 * params.json (schemaVersion 1) of a one-shot script: the parameters the manager asks for
 * before running action.sh. Each value reaches the script as `KSU_PARAM_<KEY>`.
 */

class ScriptParamsException(message: String) : Exception(message)

/** Why a parameter value is rejected. */
enum class ParamError {
    REQUIRED,
    TOO_LONG,
    INVALID_CHARACTERS,
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
    abstract val label: String
    abstract val description: String?

    /** Canonical form of the default value. */
    abstract val defaultValue: String

    /** Returns null when [value] is acceptable. */
    abstract fun validate(value: String): ParamError?

    /** Canonical value passed to the script; only call with a value [validate] accepted. */
    open fun canonical(value: String): String = value

    val envName: String get() = ENV_PREFIX + key

    data class Bool(
        override val key: String,
        override val label: String,
        override val description: String?,
        val default: Boolean,
    ) : ScriptParam() {
        override val defaultValue: String get() = default.toString()
        override fun validate(value: String): ParamError? =
            if (value == "true" || value == "false") null else ParamError.INVALID_BOOLEAN
    }

    data class EnumOption(val value: String, val label: String?)

    data class Choice(
        override val key: String,
        override val label: String,
        override val description: String?,
        val options: List<EnumOption>,
        val default: String,
    ) : ScriptParam() {
        override val defaultValue: String get() = default
        override fun validate(value: String): ParamError? =
            if (options.any { it.value == value }) null else ParamError.INVALID_OPTION
    }

    data class Text(
        override val key: String,
        override val label: String,
        override val description: String?,
        val default: String,
        val maxLength: Int,
        val required: Boolean,
    ) : ScriptParam() {
        override val defaultValue: String get() = default

        /** Empty is valid unless [required]. */
        override fun validate(value: String): ParamError? = when {
            value.isEmpty() -> if (required) ParamError.REQUIRED else null
            value.any { it == '\u0000' || it == '\n' || it == '\r' } -> ParamError.INVALID_CHARACTERS
            value.length > maxLength -> ParamError.TOO_LONG
            else -> null
        }
    }

    data class Integer(
        override val key: String,
        override val label: String,
        override val description: String?,
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

    private val KEY = Regex("^[A-Z][A-Z0-9_]{0,31}$")
    private val OPTION_VALUE = Regex("^[A-Za-z0-9._-]{1,64}$")
    private val JSON = Json { ignoreUnknownKeys = true }

    @Serializable
    private class RawParams(val schemaVersion: Int, val params: List<RawParam>)

    @Serializable
    private class RawParam(
        val key: String,
        val type: String,
        val label: String,
        val description: String? = null,
        // Its type depends on [type].
        val default: JsonElement? = null,
        val options: List<RawOption>? = null,
        val maxLength: Int? = null,
        val required: Boolean = false,
        val min: Int? = null,
        val max: Int? = null,
    )

    @Serializable
    private class RawOption(val value: String, val label: String? = null)

    private fun fail(message: String): Nothing = throw ScriptParamsException(message)

    fun parse(source: String): ScriptParams {
        val raw = try {
            JSON.decodeFromString(RawParams.serializer(), source.removePrefix("\uFEFF"))
        } catch (e: SerializationException) {
            fail("Malformed params.json: ${e.message}")
        } catch (e: IllegalArgumentException) {
            fail("Malformed params.json: ${e.message}")
        }
        if (raw.schemaVersion != SCHEMA_VERSION) fail("Unsupported schemaVersion: ${raw.schemaVersion}")
        if (raw.params.size > MAX_PARAMS) fail("At most $MAX_PARAMS params are allowed")

        val seen = HashSet<String>()
        val params = raw.params.map { p ->
            parseParam(p).also { if (!seen.add(it.key)) fail("Duplicate key: ${it.key}") }
        }
        return ScriptParams(params)
    }

    private fun parseParam(raw: RawParam): ScriptParam {
        val key = raw.key
        if (!KEY.matches(key)) fail("Invalid key: $key")
        val label = checkText(raw.label, "$key.label")
        val description = raw.description?.let { checkText(it, "$key.description") }
        val default = raw.default?.takeIf { it !is JsonNull }?.let {
            it as? JsonPrimitive ?: fail("$key.default must be a ${raw.type}")
        }

        val param = when (raw.type) {
            "bool" -> ScriptParam.Bool(
                key, label, description,
                default = default?.let { it.takeIf { !it.isString }?.booleanOrNull ?: fail("$key.default must be a boolean") }
                    ?: false,
            )

            "enum" -> {
                val raws = raw.options ?: fail("$key.options is required")
                if (raws.size !in 1..MAX_OPTIONS) fail("$key.options must have 1..$MAX_OPTIONS entries")
                val values = HashSet<String>()
                val options = raws.map { o ->
                    if (!OPTION_VALUE.matches(o.value)) fail("$key: invalid option value ${o.value}")
                    if (!values.add(o.value)) fail("$key: duplicate option ${o.value}")
                    ScriptParam.EnumOption(o.value, o.label?.let { checkText(it, "$key.${o.value}.label") })
                }
                val defaultValue = default?.let { it.takeIf { it.isString }?.content ?: fail("$key.default must be a string") }
                    ?: options.first().value
                ScriptParam.Choice(key, label, description, options, defaultValue)
            }

            "string" -> {
                val maxLength = raw.maxLength ?: DEFAULT_MAX_LENGTH
                if (maxLength !in 1..MAX_MAX_LENGTH) fail("$key.maxLength must be 1..$MAX_MAX_LENGTH")
                val defaultValue = default?.let { it.takeIf { it.isString }?.content ?: fail("$key.default must be a string") }
                    ?: ""
                // An empty default is always fine, even when required: the user has to fill it in.
                ScriptParam.Text(key, label, description, defaultValue, maxLength, raw.required)
            }

            "int" -> {
                if (raw.min != null && raw.max != null && raw.min > raw.max) fail("$key: min > max")
                val defaultValue = default?.let { it.takeIf { !it.isString }?.intOrNull ?: fail("$key.default must be an integer") }
                    ?: raw.min ?: 0
                ScriptParam.Integer(key, label, description, defaultValue, raw.min, raw.max)
            }

            else -> fail("$key: unsupported type ${raw.type}")
        }
        param.validate(param.defaultValue)?.let { if (it != ParamError.REQUIRED) fail("$key.default is invalid: $it") }
        return param
    }

    /** Trimmed first, then 1..[MAX_TEXT_LENGTH] characters. */
    private fun checkText(text: String, what: String): String {
        val trimmed = text.trim()
        if (trimmed.isEmpty() || trimmed.length > MAX_TEXT_LENGTH) fail("$what must be 1..$MAX_TEXT_LENGTH characters")
        return trimmed
    }
}
