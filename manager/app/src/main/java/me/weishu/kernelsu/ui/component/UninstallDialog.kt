package me.weishu.kernelsu.ui.component

import android.widget.Toast
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.screen.FlashIt
import me.weishu.kernelsu.ui.screen.UninstallType
import me.weishu.kernelsu.ui.screen.UninstallType.NONE
import me.weishu.kernelsu.ui.screen.UninstallType.PERMANENT
import me.weishu.kernelsu.ui.screen.UninstallType.RESTORE_STOCK_IMAGE
import me.weishu.kernelsu.ui.screen.UninstallType.TEMPORARY
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperDialog
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun UninstallDialog(
    showDialog: MutableState<Boolean>,
    navigator: DestinationsNavigator,
) {
    val context = LocalContext.current
    val options = listOf(
        // TEMPORARY,
        PERMANENT,
        RESTORE_STOCK_IMAGE
    )
    val showTodo = {
        Toast.makeText(context, "TODO", Toast.LENGTH_SHORT).show()
    }
    val showConfirmDialog = remember(showDialog.value) { mutableStateOf(false) }
    val runType = remember(showDialog.value) { mutableStateOf<UninstallType?>(null) }

    val run = { type: UninstallType ->
        when (type) {
            PERMANENT -> navigator.navigate(FlashScreenDestination(FlashIt.FlashUninstall)) {
                popUpTo(FlashScreenDestination) {
                    inclusive = true
                }
                launchSingleTop = true
            }

            RESTORE_STOCK_IMAGE -> navigator.navigate(FlashScreenDestination(FlashIt.FlashRestore)) {
                popUpTo(FlashScreenDestination) {
                    inclusive = true
                }
                launchSingleTop = true
            }

            TEMPORARY -> showTodo()
            NONE -> Unit
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
                text = stringResource(R.string.uninstall),
                fontSize = MiuixTheme.textStyles.title4.fontSize,
                fontWeight = FontWeight.Medium,
                textAlign = TextAlign.Center,
                color = MiuixTheme.colorScheme.onSurface
            )
            options.forEachIndexed { index, type ->
                SuperArrow(
                    onClick = {
                        showConfirmDialog.value = true
                        runType.value = type
                    },
                    title = stringResource(type.title),
                    leftAction = {
                        Icon(
                            imageVector = type.icon,
                            contentDescription = null,
                            modifier = Modifier.padding(end = 16.dp),
                            tint = MiuixTheme.colorScheme.onSurface
                        )
                    },
                    insideMargin = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
                )
            }
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
    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            showConfirmDialog.value = false
            showDialog.value = false
            runType.value?.let { type ->
                run(type)
            }
        },
        onDismiss = {
            showConfirmDialog.value = false
        }
    )
    val dialogTitle = runType.value?.let { type ->
        options.find { it == type }?.let { stringResource(it.title) }
    } ?: ""
    val dialogContent = runType.value?.let { type ->
        options.find { it == type }?.let { stringResource(it.message) }
    }
    if (showConfirmDialog.value) {
        confirmDialog.showConfirm(title = dialogTitle, content = dialogContent)
    }
}
