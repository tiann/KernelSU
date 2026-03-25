package me.weishu.kernelsu.ui.component.material

import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.net.Uri
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalIconButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.LineHeightStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.BuildConfig
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.dialog.rememberLoadingDialog
import me.weishu.kernelsu.ui.util.LocalSnackbarHost
import me.weishu.kernelsu.ui.util.getBugreportFile
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

private tailrec fun Context.findComponentActivity(): ComponentActivity? {
    return when (this) {
        is ComponentActivity -> this
        is ContextWrapper -> baseContext.findComponentActivity()
        else -> null
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SendLogBottomSheet(onDismiss: () -> Unit) {
    val context = LocalContext.current
    val activity = context.findComponentActivity()
    val logSaved = stringResource(R.string.log_saved)
    val sendLog = stringResource(R.string.send_log)
    val snackBarHost = LocalSnackbarHost.current
    val loadingDialog = rememberLoadingDialog()
    val haptic = LocalHapticFeedback.current
    val sheetState = rememberModalBottomSheetState()
    val scope = rememberCoroutineScope()
    val dismiss = {
        scope.launch { sheetState.hide() }
        onDismiss()
    }

    val exportBugreportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/gzip")
    ) { uri: Uri? ->
        if (uri == null) {
            dismiss()
            return@rememberLauncherForActivityResult
        }
        val lifecycleScope = activity?.lifecycleScope ?: scope
        lifecycleScope.launch {
            loadingDialog.show()
            try {
                withContext(Dispatchers.IO) {
                    context.contentResolver.openOutputStream(uri)?.use { output ->
                        getBugreportFile(context).inputStream().use {
                            it.copyTo(output)
                        }
                    }
                }
            } finally {
                loadingDialog.hide()
            }
            dismiss()
            snackBarHost.currentSnackbarData?.dismiss()
            snackBarHost.showSnackbar(logSaved)
        }
    }
    ModalBottomSheet(
        onDismissRequest = { dismiss() },
        containerColor = MaterialTheme.colorScheme.surfaceContainer,
        content = {
            Row(
                modifier = Modifier
                    .padding(top = 16.dp, bottom = 32.dp)
                    .align(Alignment.CenterHorizontally)
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    FilledTonalIconButton(
                        modifier = Modifier.size(64.dp),
                        onClick = {
                            haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                            val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH_mm")
                            val current = LocalDateTime.now().format(formatter)
                            exportBugreportLauncher.launch("KernelSU_bugreport_${current}.tar.gz")
                        }) {
                        Icon(
                            Icons.Filled.Save,
                            contentDescription = stringResource(id = R.string.save_log),
                            modifier = Modifier.align(Alignment.CenterHorizontally)
                        )
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = stringResource(id = R.string.save_log), textAlign = TextAlign.Center.also {
                            LineHeightStyle(
                                alignment = LineHeightStyle.Alignment.Center, trim = LineHeightStyle.Trim.None
                            )
                        }
                    )
                }
                Spacer(Modifier.width(16.dp))
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    FilledTonalIconButton(
                        modifier = Modifier.size(64.dp),
                        onClick = {
                            haptic.performHapticFeedback(HapticFeedbackType.VirtualKey)
                            scope.launch {
                                val bugreport = loadingDialog.withLoading {
                                    withContext(Dispatchers.IO) {
                                        getBugreportFile(context)
                                    }
                                }

                                val uri: Uri = FileProvider.getUriForFile(
                                    context, "${BuildConfig.APPLICATION_ID}.fileprovider", bugreport
                                )

                                val shareIntent = Intent(Intent.ACTION_SEND).apply {
                                    putExtra(Intent.EXTRA_STREAM, uri)
                                    setDataAndType(uri, "application/gzip")
                                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                                }

                                context.startActivity(
                                    Intent.createChooser(
                                        shareIntent, sendLog
                                    )
                                )
                            }
                        }) {
                        Icon(
                            Icons.Filled.Share,
                            contentDescription = stringResource(id = R.string.send_log),
                            modifier = Modifier.align(Alignment.CenterHorizontally)
                        )
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = stringResource(id = R.string.send_log), textAlign = TextAlign.Center.also {
                            LineHeightStyle(
                                alignment = LineHeightStyle.Alignment.Center, trim = LineHeightStyle.Trim.None
                            )
                        })
                }
            }
        })
}
