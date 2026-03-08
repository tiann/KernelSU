package me.weishu.kernelsu.data.repository

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.data.model.TemplateInfo
import me.weishu.kernelsu.ksuApp
import me.weishu.kernelsu.ui.util.getAppProfileTemplate
import me.weishu.kernelsu.ui.util.listAppProfileTemplates
import me.weishu.kernelsu.ui.util.setAppProfileTemplate
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject

class TemplateRepositoryImpl : TemplateRepository {

    companion object {
        private const val TAG = "TemplateRepository"
        private const val TEMPLATE_INDEX_URL = "https://kernelsu.org/templates/index.json"
        private const val TEMPLATE_URL = "https://kernelsu.org/templates/%s"
    }

    override suspend fun getTemplates(sync: Boolean): Result<List<TemplateInfo>> = withContext(Dispatchers.IO) {
        runCatching {
            val localTemplateIds = listAppProfileTemplates()
            Log.i(TAG, "localTemplateIds: $localTemplateIds")
            if (localTemplateIds.isEmpty() || sync) {
                fetchRemoteTemplates()
            }
            listAppProfileTemplates().mapNotNull { getTemplateInfoById(it) }
        }
    }

    override suspend fun importTemplates(jsonString: String): Result<Unit> = withContext(Dispatchers.IO) {
        runCatching {
            val array = try {
                JSONArray(jsonString)
            } catch (e: Exception) {
                try {
                    val json = JSONObject(jsonString)
                    JSONArray().apply { put(json) }
                } catch (e: Exception) {
                    throw Exception("invalid templates: $jsonString")
                }
            }

            (0 until array.length()).forEach { i ->
                runCatching {
                    val template = array.getJSONObject(i)
                    val id = template.getString("id")
                    template.put("local", true)
                    setAppProfileTemplate(id, template.toString())
                }.onFailure { e ->
                    Log.e(TAG, "ignore invalid template: $array", e)
                }
            }
        }
    }

    override suspend fun exportTemplates(): Result<String> = withContext(Dispatchers.IO) {
        runCatching {
            val templates = listAppProfileTemplates()
                .mapNotNull { getTemplateInfoById(it) }
                .filter { it.local }
            if (templates.isEmpty()) {
                throw Exception("No templates to export")
            }
            JSONArray(templates.map { it.toJSON() }).toString()
        }
    }

    override suspend fun getTemplate(id: String): Result<TemplateInfo> = withContext(Dispatchers.IO) {
        runCatching {
            getTemplateInfoById(id) ?: throw Exception("Template not found: $id")
        }
    }

    private fun fetchRemoteTemplates() {
        runCatching {
            ksuApp.okhttpClient.newCall(
                Request.Builder().url(TEMPLATE_INDEX_URL).build()
            ).execute().use { response ->
                if (!response.isSuccessful) {
                    return
                }
                val remoteTemplateIds = JSONArray(response.body.string())
                Log.i(TAG, "fetchRemoteTemplates: $remoteTemplateIds")
                (0 until remoteTemplateIds.length()).forEach { i ->
                    val id = remoteTemplateIds.getString(i)
                    Log.i(TAG, "fetch template: $id")
                    val templateJson = ksuApp.okhttpClient.newCall(
                        Request.Builder().url(TEMPLATE_URL.format(id)).build()
                    ).runCatching {
                        execute().use { response ->
                            if (!response.isSuccessful) {
                                return@forEach
                            }
                            response.body.string()
                        }
                    }.getOrNull() ?: return@forEach
                    Log.i(TAG, "template: $templateJson")

                    // validate remote template
                    runCatching {
                        val json = JSONObject(templateJson)
                        TemplateInfo.fromJSON(json)?.let {
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

    private fun getTemplateInfoById(id: String): TemplateInfo? {
        return runCatching {
            TemplateInfo.fromJSON(JSONObject(getAppProfileTemplate(id)))
        }.onFailure {
            Log.e(TAG, "ignore invalid template: $it", it)
        }.getOrNull()
    }
}
