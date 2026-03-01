package me.weishu.kernelsu.data.model

import android.os.Parcelable
import android.util.Log
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

@Parcelize
data class TemplateInfo(
    val id: String = "",
    val name: String = "",
    val description: String = "",
    val author: String = "",
    val local: Boolean = true,
    val namespace: Int = Natives.Profile.Namespace.INHERITED.ordinal,
    val uid: Int = Natives.ROOT_UID,
    val gid: Int = Natives.ROOT_GID,
    val groups: List<Int> = mutableListOf(),
    val capabilities: List<Int> = mutableListOf(),
    val context: String = Natives.KERNEL_SU_DOMAIN,
    val rules: List<String> = mutableListOf(),
) : Parcelable {
    companion object {
        private const val TAG = "TemplateInfo"

        fun fromJSON(templateJson: JSONObject): TemplateInfo? {
            return runCatching {
                val groupsJsonArray = templateJson.optJSONArray("groups")
                val capabilitiesJsonArray = templateJson.optJSONArray("capabilities")
                val context = templateJson.optString("context").takeIf { it.isNotEmpty() }
                    ?: Natives.KERNEL_SU_DOMAIN
                val namespace = templateJson.optString("namespace").takeIf { it.isNotEmpty() }
                    ?: Natives.Profile.Namespace.INHERITED.name

                val rulesJsonArray = templateJson.optJSONArray("rules")
                val templateInfo = TemplateInfo(
                    id = templateJson.getString("id"),
                    name = getLocaleString(templateJson, "name"),
                    description = getLocaleString(templateJson, "description"),
                    author = templateJson.optString("author"),
                    local = templateJson.optBoolean("local"),
                    namespace = Natives.Profile.Namespace.valueOf(
                        namespace.uppercase()
                    ).ordinal,
                    uid = templateJson.optInt("uid", Natives.ROOT_UID),
                    gid = templateJson.optInt("gid", Natives.ROOT_GID),
                    groups = getEnumOrdinals(groupsJsonArray, Groups::class.java).map { it.gid },
                    capabilities = getEnumOrdinals(
                        capabilitiesJsonArray, Capabilities::class.java
                    ).map { it.cap },
                    context = context,
                    rules = rulesJsonArray?.mapCatching<String, String>({ it }, {
                        Log.e(TAG, "ignore invalid rule: $it", it)
                    }).orEmpty()
                )
                templateInfo
            }.onFailure {
                Log.e(TAG, "ignore invalid template: $it", it)
            }.getOrNull()
        }

        private fun getLocaleString(json: JSONObject, key: String): String {
            val fallback = json.getString(key)
            val locale = Locale.getDefault()
            val localeKey = "${locale.language}_${locale.country}"
            json.optJSONObject("locales")?.let {
                // check locale first
                it.optJSONObject(localeKey)?.let { json ->
                    return json.optString(key, fallback)
                }
                // fallback to language
                it.optJSONObject(locale.language)?.let { json ->
                    return json.optString(key, fallback)
                }
            }
            return fallback
        }

        @Suppress("UNCHECKED_CAST")
        private fun <T, R> JSONArray.mapCatching(
            transform: (T) -> R, onFail: (Throwable) -> Unit
        ): List<R> {
            return List(length()) { i -> get(i) as T }.mapNotNull { element ->
                runCatching {
                    transform(element)
                }.onFailure(onFail).getOrNull()
            }
        }

        private inline fun <reified T : Enum<T>> getEnumOrdinals(
            jsonArray: JSONArray?, enumClass: Class<T>
        ): List<T> {
            return jsonArray?.mapCatching<String, T>({ name ->
                enumValueOf(name.uppercase())
            }, {
                Log.e(TAG, "ignore invalid enum ${enumClass.simpleName}: $it", it)
            }).orEmpty()
        }
    }

    fun toJSON(): JSONObject {
        val template = this
        return JSONObject().apply {

            put("id", template.id)
            put("name", template.name.ifBlank { template.id })
            put("description", template.description.ifBlank { template.id })
            if (template.author.isNotEmpty()) {
                put("author", template.author)
            }
            put("namespace", Natives.Profile.Namespace.entries[template.namespace].name)
            put("uid", template.uid)
            put("gid", template.gid)

            if (template.groups.isNotEmpty()) {
                put(
                    "groups", JSONArray(
                        Groups.entries.filter {
                            template.groups.contains(it.gid)
                        }.map {
                            it.name
                        }
                    ))
            }

            if (template.capabilities.isNotEmpty()) {
                put(
                    "capabilities", JSONArray(
                        Capabilities.entries.filter {
                            template.capabilities.contains(it.cap)
                        }.map {
                            it.name
                        }
                    ))
            }

            if (template.context.isNotEmpty()) {
                put("context", template.context)
            }

            if (template.rules.isNotEmpty()) {
                put("rules", JSONArray(template.rules))
            }
        }
    }
}
