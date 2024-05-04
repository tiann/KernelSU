package me.weishu.kernelsu.ui.viewmodel

import android.os.Parcelable
import android.util.Log
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.profile.Capabilities
import me.weishu.kernelsu.profile.Groups
import me.weishu.kernelsu.ui.util.getAppProfileTemplate
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.setAppProfileTemplate
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.text.Collator
import java.util.Locale
import java.util.concurrent.TimeUnit


/**
 * @author weishu
 * @date 2023/10/20.
 */
const val TEMPLATE_INDEX_URL = "https://kernelsu.org/templates/index.json"
const val TEMPLATE_URL = "https://kernelsu.org/templates/%s"

const val TAG = "TemplateViewModel"

class TemplateViewModel : ViewModel() {
    companion object {

        private var templates by mutableStateOf<List<TemplateInfo>>(emptyList())
    }

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
    ) : Parcelable

    var isRefreshing by mutableStateOf(false)
        private set

    val templateList by derivedStateOf {
        val comparator = compareBy(TemplateInfo::local).reversed().then(
            compareBy(
                Collator.getInstance(Locale.getDefault()), TemplateInfo::id
            )
        )
        templates.sortedWith(comparator).apply {
            isRefreshing = false
        }
    }

    suspend fun fetchTemplates(sync: Boolean = false) {
        isRefreshing = true
        withContext(Dispatchers.IO) {
            val localTemplateIds = listAppProfileTemplates()
            Log.i(TAG, "localTemplateIds: $localTemplateIds")
            if (localTemplateIds.isEmpty() || sync) {
                // if no templates, fetch remote templates
                fetchRemoteTemplates()
            }

            // fetch templates again
            templates = listAppProfileTemplates().mapNotNull(::getTemplateInfoById)

            isRefreshing = false
        }
    }

    suspend fun importTemplates(
        templates: String,
        onSuccess: suspend () -> Unit,
        onFailure: suspend (String) -> Unit
    ) {
        withContext(Dispatchers.IO) {
            runCatching {
                JSONArray(templates)
            }.getOrElse {
                runCatching {
                    val json = JSONObject(templates)
                    JSONArray().apply { put(json) }
                }.getOrElse {
                    onFailure("invalid templates: $templates")
                    return@withContext
                }
            }.let {
                0.until(it.length()).forEach { i ->
                    runCatching {
                        val template = it.getJSONObject(i)
                        val id = template.getString("id")
                        template.put("local", true)
                        setAppProfileTemplate(id, template.toString())
                    }.onFailure { e ->
                        Log.e(TAG, "ignore invalid template: $it", e)
                    }
                }
                onSuccess()
            }
        }
    }

    suspend fun exportTemplates(onTemplateEmpty: () -> Unit, callback: (String) -> Unit) {
        withContext(Dispatchers.IO) {
            val templates = listAppProfileTemplates().mapNotNull(::getTemplateInfoById).filter {
                it.local
            }
            templates.ifEmpty {
                onTemplateEmpty()
                return@withContext
            }
            JSONArray(templates.map {
                it.toJSON()
            }).toString().let(callback)
        }
    }
}

private fun fetchRemoteTemplates() {
    runCatching {
        val client: OkHttpClient = OkHttpClient.Builder()
            .connectTimeout(5, TimeUnit.SECONDS)
            .writeTimeout(5, TimeUnit.SECONDS)
            .readTimeout(10, TimeUnit.SECONDS)
            .build()

        client.newCall(
            Request.Builder().url(TEMPLATE_INDEX_URL).build()
        ).execute().use { response ->
            if (!response.isSuccessful) {
                return
            }
            val remoteTemplateIds = JSONArray(response.body!!.string())
            Log.i(TAG, "fetchRemoteTemplates: $remoteTemplateIds")
            0.until(remoteTemplateIds.length()).forEach { i ->
                val id = remoteTemplateIds.getString(i)
                Log.i(TAG, "fetch template: $id")
                val templateJson = client.newCall(
                    Request.Builder().url(TEMPLATE_URL.format(id)).build()
                ).runCatching {
                    execute().use { response ->
                        if (!response.isSuccessful) {
                            return@forEach
                        }
                        response.body!!.string()
                    }
                }.getOrNull() ?: return@forEach
                Log.i(TAG, "template: $templateJson")

                // validate remote template
                runCatching {
                    val json = JSONObject(templateJson)
                    fromJSON(json)?.let {
                        // force local template
                        json.put("local", false)
                        setAppProfileTemplate(id, json.toString())
                    }
                }.onFailure {
                    Log.e(TAG, "ignore invalid template: $it", it)
                    return@forEach
                }
            }
        }
    }.onFailure { Log.e(TAG, "fetchRemoteTemplates: $it", it) }
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

fun getTemplateInfoById(id: String): TemplateViewModel.TemplateInfo? {
    return runCatching {
        fromJSON(JSONObject(getAppProfileTemplate(id)))
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
        it.optJSONObject(localeKey)?.let { json->
            return json.optString(key, fallback)
        }
        // fallback to language
        it.optJSONObject(locale.language)?.let { json->
            return json.optString(key, fallback)
        }
    }
    return fallback
}

private fun fromJSON(templateJson: JSONObject): TemplateViewModel.TemplateInfo? {
    return runCatching {
        val groupsJsonArray = templateJson.optJSONArray("groups")
        val capabilitiesJsonArray = templateJson.optJSONArray("capabilities")
        val context = templateJson.optString("context").takeIf { it.isNotEmpty() }
            ?: Natives.KERNEL_SU_DOMAIN
        val namespace = templateJson.optString("namespace").takeIf { it.isNotEmpty() }
            ?: Natives.Profile.Namespace.INHERITED.name

        val rulesJsonArray = templateJson.optJSONArray("rules")
        val templateInfo = TemplateViewModel.TemplateInfo(
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

fun TemplateViewModel.TemplateInfo.toJSON(): JSONObject {
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
            put("groups", JSONArray(
                Groups.entries.filter {
                    template.groups.contains(it.gid)
                }.map {
                    it.name
                }
            ))
        }

        if (template.capabilities.isNotEmpty()) {
            put("capabilities", JSONArray(
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

@Suppress("unused")
fun generateTemplates() {
    val templateJson = JSONObject()
    templateJson.put("id", "com.example")
    templateJson.put("name", "Example")
    templateJson.put("description", "This is an example template")
    templateJson.put("local", true)
    templateJson.put("namespace", Natives.Profile.Namespace.INHERITED.name)
    templateJson.put("uid", 0)
    templateJson.put("gid", 0)

    templateJson.put("groups", JSONArray().apply { put(Groups.INET.name) })
    templateJson.put("capabilities", JSONArray().apply { put(Capabilities.CAP_NET_RAW.name) })
    templateJson.put("context", "u:r:su:s0")
    Log.i(TAG, "$templateJson")
}