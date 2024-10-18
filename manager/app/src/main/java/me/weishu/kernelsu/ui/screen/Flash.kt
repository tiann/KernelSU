package me.weishu.kernelsu.ui.screen

import android.net.Uri
import android.os.Environment
import android.os.Parcelable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Save
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.TopAppBarScrollBehavior
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.ramcosta.composedestinations.navigation.EmptyDestinationsNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KeyEventBlocker
import me.weishu.kernelsu.ui.util.LkmSelection
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.util.flashModule
import me.weishu.kernelsu.ui.util.installBoot
import me.weishu.kernelsu.ui.util.reboot
import me.weishu.kernelsu.ui.util.restoreBoot
import me.weishu.kernelsu.ui.util.uninstallPermanently
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

enum class FlashingStatus {
    FLASHING,
    SUCCESS,
    FAILED
}

/**
 * @author weishu
 * @date 2023/1/1.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
@Destination<RootGraph>
fun FlashScreen(navigator: DestinationsNavigator, flashIt: FlashIt) {

    var text by rememberSaveable { mutableStateOf("") }
    val logContent = rememberSaveable { StringBuilder() }
    var showFloatAction by rememberSaveable { mutableStateOf(false) }

    val snackBarHost = LocalSnackbarHost.current
    val scope = rememberCoroutineScope()
    val scrollState = rememberScrollState()
    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior(rememberTopAppBarState())
    var flashing by rememberSaveable {
        mutableStateOf(FlashingStatus.FLASHING)
    }

    LaunchedEffect(Unit) {
        if (text.isNotEmpty()) {
            return@LaunchedEffect
        }
        withContext(Dispatchers.IO) {
            flashIt(flashIt, onFinish = { showReboot, code ->
                if (code != 0) {
                    text += "Error: exit code = $code.\nPlease save and check the log.\n"
                }
                if (showReboot) {
                    text += "\n\n\n"
                    showFloatAction = true
                }
                flashing = if (code == 0) FlashingStatus.SUCCESS else FlashingStatus.FAILED
            }, onStdout = {
                text += "$it\n"
                logContent.append(it).append("\n")
            }, onStderr = {
                logContent.append(it).append("\n")
            })
        }
    }

    Scaffold(
        topBar = {
            TopBar(
                flashing,
                onBack = {
                    navigator.popBackStack()
                },
                onSave = {
                    scope.launch {
                        val format = SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.getDefault())
                        val date = format.format(Date())
                        val file = File(
                            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                            "KernelSU_install_log_${date}.log"
                        )
                        file.writeText(logContent.toString())
                        snackBarHost.showSnackbar("Log saved to ${file.absolutePath}")
                    }
                },
                scrollBehavior = scrollBehavior
            )
        },
        floatingActionButton = {
            if (showFloatAction) {
                val reboot = stringResource(id = R.string.reboot)
                ExtendedFloatingActionButton(
                    onClick = {
                        scope.launch {
                            withContext(Dispatchers.IO) {
                                reboot()
                            }
                        }
                    },
                    icon = { Icon(Icons.Filled.Refresh, reboot) },
                    text = { Text(text = reboot) },
                )
            }
        },
        snackbarHost = { SnackbarHost(hostState = snackBarHost) },
        contentWindowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal)
    ) { innerPadding ->
        KeyEventBlocker {
            it.key == Key.VolumeDown || it.key == Key.VolumeUp
        }
        Column(
            modifier = Modifier
                .fillMaxSize(1f)
                .padding(innerPadding)
                .nestedScroll(scrollBehavior.nestedScrollConnection)
                .verticalScroll(scrollState),
        ) {
            LaunchedEffect(text) {
                scrollState.animateScrollTo(scrollState.maxValue)
            }
            Text(
                modifier = Modifier.padding(8.dp),
                text = text,
                fontSize = MaterialTheme.typography.bodySmall.fontSize,
                fontFamily = FontFamily.Monospace,
                lineHeight = MaterialTheme.typography.bodySmall.lineHeight,
            )
        }
    }
}

@Parcelize
sealed class FlashIt : Parcelable {
    data class FlashBoot(val boot: Uri? = null, val lkm: LkmSelection, val ota: Boolean) :
        FlashIt()

    data class FlashModule(val uri: Uri) : FlashIt()

    data object FlashRestore : FlashIt()

    data object FlashUninstall : FlashIt()
}

fun flashIt(
    flashIt: FlashIt, onFinish: (Boolean, Int) -> Unit,
    onStdout: (String) -> Unit,
    onStderr: (String) -> Unit
) {
    when (flashIt) {
        is FlashIt.FlashBoot -> installBoot(
            flashIt.boot,
            flashIt.lkm,
            flashIt.ota,
            onFinish,
            onStdout,
            onStderr
        )

        is FlashIt.FlashModule -> flashModule(flashIt.uri, onFinish, onStdout, onStderr)

        FlashIt.FlashRestore -> restoreBoot(onFinish, onStdout, onStderr)

        FlashIt.FlashUninstall -> uninstallPermanently(onFinish, onStdout, onStderr)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(
    status: FlashingStatus,
    onBack: () -> Unit = {},
    onSave: () -> Unit = {},
    scrollBehavior: TopAppBarScrollBehavior? = null
) {
    TopAppBar(
        title = {
            Text(
                stringResource(
                    when (status) {
                        FlashingStatus.FLASHING -> R.string.flashing
                        FlashingStatus.SUCCESS -> R.string.flash_success
                        FlashingStatus.FAILED -> R.string.flash_failed
                    }
                )
            )
        },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null) }
        },
        actions = {
            IconButton(onClick = onSave) {
                Icon(
                    imageVector = Icons.Filled.Save,
                    contentDescription = "Localized description"
                )
            }
        },
        windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal),
        scrollBehavior = scrollBehavior
    )
}

@Preview
@Composable
fun InstallPreview() {
    InstallScreen(EmptyDestinationsNavigator)
}