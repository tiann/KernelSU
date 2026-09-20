package me.weishu.kernelsu.ui.webui.webview

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.webkit.WebView
import androidx.webkit.WebViewAssetLoader
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.repository.ModuleRepositoryImpl
import me.weishu.kernelsu.data.repository.SettingsRepositoryImpl
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.viewmodel.SuperUserViewModel
import me.weishu.kernelsu.ui.webui.WebUIRuntime
import me.weishu.kernelsu.ui.webui.WebViewInterface
import me.weishu.kernelsu.ui.webui.model.ModuleInfo
import me.weishu.kernelsu.ui.webui.model.WebUILoadState
import me.weishu.kernelsu.ui.webui.util.setTaskDescription
import me.weishu.kernelsu.ui.webui.viewmodel.WebUIViewModel
import me.weishu.kernelsu.ui.webui.webview.asset.SuFilePathHandler
import java.io.File

internal suspend fun prepareWebView(
    activity: Activity,
    runtime: WebUIRuntime,
    viewModel: WebUIViewModel,
) {
    val moduleId = activity.intent.data?.getQueryParameter("id")
    if (moduleId == null) {
        viewModel.notifyError(activity.getString(R.string.webui_invalid_module_id))
        return
    }

    val moduleName = if (viewModel.isModuleValidated(moduleId)) {
        viewModel.recreateWebView()
        viewModel.state.value.moduleInfo?.name
    } else {
        viewModel.checkWebviewFeature(activity)
        checkModule(viewModel, moduleId, activity)?.also(viewModel::onModuleValidated)?.name
    } ?: return

    val label = activity.getString(R.string.app_name) + " - $moduleName"
    activity.setTaskDescription(label)

    val moduleDir = "/data/adb/modules/${moduleId}"
    val shell = withContext(Dispatchers.IO) { createRootShell(true) }

    val webView = createWebView(activity)
    val webViewAssetLoader = createAssetLoader(activity, moduleDir, shell, viewModel)
    webView.webViewClient = WebUiClient(activity, webViewAssetLoader, viewModel)
    webView.webChromeClient = WebUiChromeClient(runtime, viewModel)
    val webviewInterface = WebViewInterface(runtime, moduleDir, viewModel)
    webView.addJavascriptInterface(webviewInterface, KSU_JS_INTERFACE_NAME)
    runtime.registerShell(shell)
    runtime.attach(webView)

    viewModel.setLoadState(WebUILoadState.LoadingPage)

}

private suspend fun checkModule(
    viewModel: WebUIViewModel,
    moduleId: String,
    context: Context,
): ModuleInfo? {
    viewModel.setLoadState(WebUILoadState.LoadingModule)
    val module = withContext(Dispatchers.IO) {
        val repo = ModuleRepositoryImpl()
        val modules = repo.getModules().getOrDefault(emptyList())
        val moduleInfo = modules.find { info -> info.id == moduleId }

        if (moduleInfo == null) {
            viewModel.notifyError(context.getString(R.string.no_such_module, moduleId))
            return@withContext null
        }

        if (!moduleInfo.hasWebUi || !moduleInfo.enabled || moduleInfo.update || moduleInfo.remove) {
            viewModel.notifyError(context.getString(R.string.module_unavailable, moduleInfo.name))
            return@withContext null
        }

        if (SuperUserViewModel.apps.isEmpty()) {
            SuperUserViewModel().fetchAppList()
        }
        moduleInfo
    } ?: return null

    return ModuleInfo(moduleId, module.name)
}

private fun createAssetLoader(
    activity: Activity,
    moduleDir: String,
    shell: Shell,
    viewModel: WebUIViewModel,
): WebViewAssetLoader {
    val webRoot = File("${moduleDir}/webroot")
    return WebViewAssetLoader.Builder()
        .setDomain(WEBUI_DOMAIN)
        .addPathHandler(
            "/",
            SuFilePathHandler(
                activity,
                webRoot,
                shell
            ) { enable ->
                viewModel.onEdgeToEdgeChanged(enable)
            }
        )
        .build()
}

@SuppressLint("SetJavaScriptEnabled")
private fun createWebView(activity: Activity): WebView {
    val webView = WebView(activity)
    webView.setBackgroundColor(Color.TRANSPARENT)

    val repo = SettingsRepositoryImpl()
    WebView.setWebContentsDebuggingEnabled(repo.enableWebDebugging)

    webView.settings.apply {
        javaScriptEnabled = true
        domStorageEnabled = true
        allowFileAccess = false
    }
    return webView
}
