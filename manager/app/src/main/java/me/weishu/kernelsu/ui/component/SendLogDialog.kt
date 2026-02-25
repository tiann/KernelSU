package me.weishu.kernelsu.ui.component

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material.icons.rounded.Share
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.getBugreportFile
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperDialog
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

@Composable
fun SendLogDialog(
    showDialog: MutableState<Boolean>,
    loadingDialog: LoadingDialogHandle,
) {
    val context = LocalContext.current
    val resources = LocalResources.current
    val scope = rememberCoroutineScope()
    val exportBugreportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/gzip")
    ) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch(Dispatchers.IO) {
            loadingDialog.show()
            context.contentResolver.openOutputStream(uri)?.use { output ->
                getBugreportFile(context).inputStream().use {
                    it.copyTo(output)
                }
            }
            loadingDialog.hide()
            withContext(Dispatchers.Main) {
                Toast.makeText(context, resources.getString(R.string.log_saved), Toast.LENGTH_SHORT).show()
            }
        }
    }
    SuperDialog(
        show = showDialog,
        insideMargin = DpSize(0.dp, 0.dp),
        onDismissRequest = {
            showDialog.value = false
        },
        content = {
            Text(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 24.dp, bottom = 12.dp),
                text = stringResource(R.string.send_log),
                fontSize = MiuixTheme.textStyles.title4.fontSize,
                fontWeight = FontWeight.Medium,
                textAlign = TextAlign.Center,
                color = colorScheme.onSurface
            )
            SuperArrow(
                title = stringResource(id = R.string.save_log),
                startAction = {
                    Icon(
                        Icons.Rounded.Save,
                        contentDescription = null,
                        modifier = Modifier.padding(end = 16.dp),
                        tint = colorScheme.onSurface
                    )
                },
                onClick = {
                    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm")
                    val current = LocalDateTime.now().format(formatter)
                    exportBugreportLauncher.launch("KernelSU_bugreport_${current}.tar.gz")
                    showDialog.value = false
                },
                insideMargin = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
            )
            SuperArrow(
                title = stringResource(id = R.string.send_log),
                startAction = {
                    Icon(
                        Icons.Rounded.Share,
                        contentDescription = null,
                        modifier = Modifier.padding(end = 16.dp),
                        tint = colorScheme.onSurface
                    )
                },
                onClick = {
                    scope.launch {
                        showDialog.value = false
                        val bugreport = loadingDialog.withLoading {
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

                        val shareIntent = Intent(Intent.ACTION_SEND).apply {
                            putExtra(Intent.EXTRA_STREAM, uri)
                            setDataAndType(uri, "application/gzip")
                            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        }

                        context.startActivity(
                            Intent.createChooser(
                                shareIntent,
                                resources.getString(R.string.send_log)
                            )
                        )
                    }
                },
                insideMargin = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
            )
            TextButton(
                text = stringResource(id = android.R.string.cancel),
                onClick = {
                    showDialog.value = false
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 12.dp, bottom = 24.dp)
                    .padding(horizontal = 24.dp)
            )
        }
    )
}
