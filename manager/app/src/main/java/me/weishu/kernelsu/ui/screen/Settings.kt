package me.weishu.kernelsu.ui.screen

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.core.content.FileProvider
import com.alorma.compose.settings.ui.*
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.AboutDialog
import me.weishu.kernelsu.ui.component.LoadingDialog
import me.weishu.kernelsu.ui.util.LocalDialogHost
import me.weishu.kernelsu.ui.util.getBugreportFile

/**
 * @author weishu
 * @date 2023/1/1.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Destination
@Composable
fun SettingScreen(navigator: DestinationsNavigator) {

    Scaffold(
        topBar = {
            TopBar(onBack = {
                navigator.popBackStack()
            })
        }
    ) { paddingValues ->
        LoadingDialog()

        val showAboutDialog = remember { mutableStateOf(false) }
        AboutDialog(showAboutDialog)

        Column(modifier = Modifier.padding(paddingValues)) {

            val context = LocalContext.current
            val scope = rememberCoroutineScope()
            val dialogHost = LocalDialogHost.current
            SettingsMenuLink(
                title = {
                    Text(stringResource(id = R.string.send_log))
                },
                onClick = {
                    scope.launch {
                        val bugreport = dialogHost.withLoading {
                            withContext(Dispatchers.IO) {
                                getBugreportFile(context)
                            }
                        }

                        val uri: Uri =
                            FileProvider.getUriForFile(
                                context,
                                "${BuildConfig.APPLICATION_ID}.fileprovider",
                                bugreport
                            )

                        val shareIntent = Intent(Intent.ACTION_SEND)
                        shareIntent.putExtra(Intent.EXTRA_STREAM, uri)
                        shareIntent.setDataAndType(uri, "application/zip")
                        shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)

                        context.startActivity(
                            Intent.createChooser(
                                shareIntent,
                                context.getString(R.string.send_log)
                            )
                        )
                    }
                }
            )

            val about = stringResource(id = R.string.about)
            SettingsMenuLink(
                title = {
                    Text(about)
                },
                onClick = {
                    showAboutDialog.value = true
                }
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit = {}) {
    TopAppBar(
        title = { Text(stringResource(R.string.settings)) },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
    )
}