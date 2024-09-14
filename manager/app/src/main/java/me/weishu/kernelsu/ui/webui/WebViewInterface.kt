package me.weishu.kernelsu.ui.webui

import android.app.Activity
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.view.Window
import android.webkit.JavascriptInterface
import android.webkit.WebView
import android.widget.Toast
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.topjohnwu.superuser.CallbackList
import com.topjohnwu.superuser.ShellUtils
import me.weishu.kernelsu.ui.util.listModules
import me.weishu.kernelsu.ui.util.createRootShell
import me.weishu.kernelsu.ui.util.withNewRootShell
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CompletableFuture
import java.io.File

class WebViewInterface(val context: Context, private val webView: WebView, private val modDir: String) {

    @JavascriptInterface
    fun exec(cmd: String): String {
        return withNewRootShell(true) { ShellUtils.fastCmd(this, cmd) }
    }

    @JavascriptInterface
    fun exec(cmd: String, callbackFunc: String) {
        exec(cmd, null, callbackFunc)
    }

    private fun processOptions(sb: StringBuilder, options: String?) {
        val opts = if (options == null) JSONObject() else {
            JSONObject(options)
        }

        val cwd = opts.optString("cwd")
        if (!TextUtils.isEmpty(cwd)) {
            sb.append("cd ${cwd};")
        }

        opts.optJSONObject("env")?.let { env ->
            env.keys().forEach { key ->
                sb.append("export ${key}=${env.getString(key)};")
            }
        }
    }

    @JavascriptInterface
    fun exec(
        cmd: String,
        options: String?,
        callbackFunc: String
    ) {
        val finalCommand = StringBuilder()
        processOptions(finalCommand, options)
        finalCommand.append(cmd)

        val result = withNewRootShell(true) {
            newJob().add(finalCommand.toString()).to(ArrayList(), ArrayList()).exec()
        }
        val stdout = result.out.joinToString(separator = "\n")
        val stderr = result.err.joinToString(separator = "\n")

        val jsCode =
            "javascript: (function() { try { ${callbackFunc}(${result.code}, ${
                JSONObject.quote(
                    stdout
                )
            }, ${JSONObject.quote(stderr)}); } catch(e) { console.error(e); } })();"
        webView.post {
            webView.loadUrl(jsCode)
        }
    }

    @JavascriptInterface
    fun spawn(command: String, args: String, options: String?, callbackFunc: String) {
        val finalCommand = StringBuilder()

        processOptions(finalCommand, options)

        if (!TextUtils.isEmpty(args)) {
            finalCommand.append(command).append(" ")
            JSONArray(args).let { argsArray ->
                for (i in 0 until argsArray.length()) {
                    finalCommand.append(argsArray.getString(i))
                    finalCommand.append(" ")
                }
            }
        } else {
            finalCommand.append(command)
        }

        val shell = createRootShell(true)

        val emitData = fun(name: String, data: String) {
            val jsCode =
                "javascript: (function() { try { ${callbackFunc}.${name}.emit('data', ${
                    JSONObject.quote(
                        data
                    )
                }); } catch(e) { console.error('emitData', e); } })();"
            webView.post {
                webView.loadUrl(jsCode)
            }
        }

        val stdout = object : CallbackList<String>() {
            override fun onAddElement(s: String) {
                emitData("stdout", s)
            }
        }

        val stderr = object : CallbackList<String>() {
            override fun onAddElement(s: String) {
                emitData("stderr", s)
            }
        }

        val future = shell.newJob().add(finalCommand.toString()).to(stdout, stderr).enqueue()
        val completableFuture = CompletableFuture.supplyAsync {
            future.get()
        }

        completableFuture.thenAccept { result ->
            val emitExitCode =
                "javascript: (function() { try { ${callbackFunc}.emit('exit', ${result.code}); } catch(e) { console.error(`emitExit error: \${e}`); } })();"
            webView.post {
                webView.loadUrl(emitExitCode)
            }

            if (result.code != 0) {
                val emitErrCode =
                    "javascript: (function() { try { var err = new Error(); err.exitCode = ${result.code}; err.message = ${
                        JSONObject.quote(
                            result.err.joinToString(
                                "\n"
                            )
                        )
                    };${callbackFunc}.emit('error', err); } catch(e) { console.error('emitErr', e); } })();"
                webView.post {
                    webView.loadUrl(emitErrCode)
                }
            }
        }.whenComplete { _, _ ->
            runCatching { shell.close() }
        }
    }

    @JavascriptInterface
    fun toast(msg: String) {
        webView.post {
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
        }
    }

    @JavascriptInterface
    fun fullScreen(enable: Boolean) {
        if (context is Activity) {
            Handler(Looper.getMainLooper()).post {
                if (enable) {
                    hideSystemUI(context.window)
                } else {
                    showSystemUI(context.window)
                }
            }
        }
    }

    @JavascriptInterface
    fun moduleInfo(): String {
        val moduleInfos = JSONArray(listModules())
        var currentModuleInfo = JSONObject()
        currentModuleInfo.put("moduleDir", modDir)
        val moduleId = File(modDir).getName()
        for (i in 0 until moduleInfos.length()) {
            val currentInfo = moduleInfos.getJSONObject(i)

            if (currentInfo.getString("id") != moduleId) {
                continue
            }

            var keys = currentInfo.keys()
            for(key in keys) {
                currentModuleInfo.put(key, currentInfo.get(key));
            }
            break;
        }
        return currentModuleInfo.toString();
    }
}

fun hideSystemUI(window: Window) {
    WindowCompat.setDecorFitsSystemWindows(window, false)
    WindowInsetsControllerCompat(window, window.decorView).let { controller ->
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}

fun showSystemUI(window: Window) {
    WindowCompat.setDecorFitsSystemWindows(window, true)
    WindowInsetsControllerCompat(
        window,
        window.decorView
    ).show(WindowInsetsCompat.Type.systemBars())
}