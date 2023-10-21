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
        val uid: Int = 0,
        val gid: Int = 0,
        val groups: List<Int> = mutableListOf(),
        val capabilities: List<Int> = mutableListOf(),
        val context: String = "u:r:su:s0",
        val rules: List<String> = mutableListOf(),
    ) : Parcelable

    var isRefreshing by mutableStateOf(false)
        private set

    val templateList by derivedStateOf {
        val comparator = compareBy(TemplateInfo::local).then(
            compareBy(
                Collator.getInstance(Locale.getDefault()), TemplateInfo::name
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
}

private fun fetchRemoteTemplates() {
    OkHttpClient().newCall(
        Request.Builder().url(TEMPLATE_INDEX_URL).build()
    ).runCatching {
        execute().use { response ->
            if (!response.isSuccessful) {
                return
            }
            val remoteTemplateIds = JSONArray(response.body!!.string())
            Log.i(TAG, "fetchRemoteTemplates: $remoteTemplateIds")
            0.until(remoteTemplateIds.length()).forEach { i ->
                val id = remoteTemplateIds.getString(i)
                val templateJson = OkHttpClient().newCall(
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
    }.onFailure {
        Log.e(TAG, "fetchRemoteTemplates error", it)
    }
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
    jsonArray: JSONArray, enumClass: Class<T>
): List<T> {
    return jsonArray.mapCatching<String, T>({ name ->
        enumValueOf(name.uppercase())
    }, {
        Log.e(TAG, "ignore invalid enum ${enumClass.simpleName}: $it", it)
    })
}

fun getTemplateInfoById(id: String): TemplateViewModel.TemplateInfo? {
    return runCatching {
        fromJSON(JSONObject(getAppProfileTemplate(id)))
    }.onFailure {
        Log.e(TAG, "ignore invalid template: $it", it)
    }.getOrNull()
}

private fun fromJSON(templateJson: JSONObject): TemplateViewModel.TemplateInfo? {
    return runCatching {
        val groupsJsonArray = templateJson.getJSONArray("groups")
        val capabilitiesJsonArray = templateJson.getJSONArray("capabilities")
        val rulesJsonArray = templateJson.optJSONArray("rules")
        val templateInfo = TemplateViewModel.TemplateInfo(
            id = templateJson.getString("id"),
            name = templateJson.getString("name"),
            description = templateJson.getString("description"),
            local = templateJson.getBoolean("local"),
            namespace = Natives.Profile.Namespace.valueOf(
                templateJson.getString("namespace").uppercase()
            ).ordinal,
            uid = templateJson.getInt("uid"),
            gid = templateJson.getInt("gid"),
            groups = getEnumOrdinals(groupsJsonArray, Groups::class.java).map { it.gid },
            capabilities = getEnumOrdinals(
                capabilitiesJsonArray, Capabilities::class.java
            ).map { it.cap },
            context = templateJson.getString("context"),
            rules = rulesJsonArray?.mapCatching<String, String>({ it }, {
                Log.e(TAG, "ignore invalid rule: $it", it)
            }).orEmpty()
        )
        templateInfo
    }.onFailure {
        Log.e(TAG, "ignore invalid template: $it", it)
    }.getOrNull()
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