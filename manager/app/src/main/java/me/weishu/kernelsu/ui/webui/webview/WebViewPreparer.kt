package me.weishu.kernelsu.ui.webui.webview

import android.app.Activity
import androidx.webkit.WebViewAssetLoader
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.ModuleRepositoryImpl
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.webui.bridge.WebViewInterface
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import me.weishu.kernelsu.ui.webui.model.WebUIState
import me.weishu.kernelsu.ui.webui.runtime.WebUIRuntime
import me.weishu.kernelsu.ui.webui.util.setTaskDescription
import me.weishu.kernelsu.ui.webui.webview.asset.SuFilePathHandler
import java.io.File

internal suspend fun prepareWebView(
    activity: Activity,
    moduleId: String,
    runtime: WebUIRuntime,
    getState: () -> WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    withContext(Dispatchers.IO) {
        val repo = ModuleRepositoryImpl()
        val modules = repo.getModules().getOrDefault(emptyList())
        val moduleInfo = modules.find { info -> info.id == moduleId }

        if (moduleInfo == null) {
            withContext(Dispatchers.Main) {
                dispatch(WebUIIntent.Error(activity.getString(R.string.no_such_module, moduleId)))
            }
            return@withContext
        }

        if (!moduleInfo.hasWebUi || !moduleInfo.enabled || moduleInfo.update || moduleInfo.remove) {
            withContext(Dispatchers.Main) {
                dispatch(WebUIIntent.Error(activity.getString(R.string.module_unavailable, moduleInfo.name)))
            }
            return@withContext
        }

        val moduleDir = "/data/adb/modules/${moduleId}"

        if (SuperUserViewModel.apps.isEmpty()) {
            SuperUserViewModel().fetchAppList()
        }
        val shell = createRootShell(true)

        withContext(Dispatchers.Main) {
            attachWebView(activity, moduleInfo.name, moduleDir, shell, runtime, getState, dispatch)
        }
    }
}

private fun attachWebView(
    activity: Activity,
    moduleName: String,
    moduleDir: String,
    shell: Shell,
    runtime: WebUIRuntime,
    getState: () -> WebUIState,
    dispatch: (WebUIIntent) -> Unit,
) {
    activity.setTaskDescription(activity.getString(R.string.app_name) + " - $moduleName")

    val webView = createWebView(activity)
    val webViewAssetLoader = createAssetLoader(activity, moduleDir, shell, getState, dispatch)

    webView.webViewClient = createWebViewClient(activity, webViewAssetLoader, runtime, getState, dispatch)
    webView.webChromeClient = createWebChromeClient(runtime, dispatch)

    val webviewInterface = WebViewInterface(runtime, { moduleDir }, dispatch)
    webView.addJavascriptInterface(webviewInterface, KSU_JS_INTERFACE_NAME)
    dispatch(WebUIIntent.ModuleReady(moduleName, moduleDir, shell, webView))
}

private fun createAssetLoader(
    activity: Activity,
    moduleDir: String,
    shell: Shell,
    getState: () -> WebUIState,
    dispatch: (WebUIIntent) -> Unit,
): WebViewAssetLoader {
    val webRoot = File("${moduleDir}/webroot")
    return WebViewAssetLoader.Builder()
        .setDomain(WEBUI_DOMAIN)
        .addPathHandler(
            "/",
            SuFilePathHandler(
                activity,
                webRoot,
                shell,
                { getState().currentInsets },
                { enable -> dispatch(WebUIIntent.InsetsEnabledChanged(enable)) })
        )
        .build()
}
