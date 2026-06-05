package me.weishu.kernelsu.ui.util

import android.app.Activity
import android.hardware.biometrics.BiometricManager
import android.hardware.biometrics.BiometricPrompt
import android.os.CancellationSignal
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material3.AlertDialog
import me.weishu.kernelsu.R
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.DialogProperties
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.window.WindowDialog
import kotlin.system.exitProcess

@Composable
fun BiometricBlockingDialogMaterial(
    show: Boolean,
    startVerify: () -> Unit
) {
    LaunchedEffect(show) {
        if (show) {
            startVerify()
        }
    }
    if (show) {
        AlertDialog(
            onDismissRequest = {},
            properties = DialogProperties(
                dismissOnBackPress = false,
                dismissOnClickOutside = false
            ),
            title = { Text(text=stringResource(id=R.string.biometric_verify_dialog_title)) },
            text = { Text(text=stringResource(id=R.string.biometric_verify_dialog_description)) },
            confirmButton = {
                TextButton(
                    content = { Text(text = stringResource(id = R.string.biometric_verify)) },
                    onClick = {
                        startVerify()
                    }
                )
            },
            dismissButton = {
                TextButton(
                    content = { Text(text = stringResource(id = R.string.exit)) },
                    onClick = {
                        exitProcess(0);
                    }
                )
            }
        )
    }
}

@Composable
fun BiometricBlockingDialogMiuix(
    show: Boolean,
    startVerify: () -> Unit
) {
    LaunchedEffect(show) {
        if (show) {
            startVerify()
        }
    }
    if (show) {
        WindowDialog(
            show = show,
            onDismissRequest = {},
            title = stringResource(R.string.biometric_verify_dialog_title),
            content = {
                Column {
                    top.yukonga.miuix.kmp.basic.Text(text = stringResource(R.string.biometric_verify_dialog_description))
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        TextButton(
                            text = stringResource(id = R.string.exit),
                            onClick = {
                                exitProcess(0)
                            },
                            modifier = Modifier.weight(1f)
                        )
                        Spacer(modifier = Modifier.width(10.dp))
                        TextButton(
                            text = stringResource(id = R.string.biometric_verify),
                            onClick = {
                                startVerify()
                            },
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.textButtonColorsPrimary()
                        )
                    }
                }
            }
        )
    }
}

fun startBiometric(context: Activity, callback: (result: Boolean) -> Unit) {
    BiometricPrompt.Builder(context)
        .setTitle(context.getString(R.string.biometric_verify_dialog_title))
        .setSubtitle(context.getString(R.string.biometric_verify_dialog_description))
        .setNegativeButton(context.getString(R.string.download_cancel), context.mainExecutor) { _, _ ->
            callback(false)
        }
        .build()
        .authenticate(
            CancellationSignal(),
            context.mainExecutor,
            object : BiometricPrompt.AuthenticationCallback() {
                override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult?) {
                    super.onAuthenticationSucceeded(result)
                    callback(true)
                }

                override fun onAuthenticationError(
                    errorCode: Int,
                    errString: CharSequence?
                ) {
                    super.onAuthenticationError(errorCode, errString)
                    callback(false)
                }
            })
}

fun isSupportBiometric(context: Activity): Boolean {
    val biometricService =
        context.getSystemService(BiometricManager::class.java) as BiometricManager
    return biometricService.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK) == BiometricManager.BIOMETRIC_SUCCESS
}