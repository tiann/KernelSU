package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.BasicAlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.component.KsuIsValid
import top.yukonga.miuix.kmp.basic.Icon as MiuixIcon
import top.yukonga.miuix.kmp.basic.IconButton as MiuixIconButton
import top.yukonga.miuix.kmp.theme.MiuixTheme

private data class RebootDialogStyle(
    val containerColor: Color,
    val itemColor: Color,
    val contentColor: Color,
    val iconContainerColor: Color,
    val iconColor: Color,
    val titleStyle: TextStyle,
    val itemStyle: TextStyle,
)

@Composable
private fun rebootDialogStyle(): RebootDialogStyle = when (LocalUiMode.current) {
    UiMode.Material -> RebootDialogStyle(
        containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
        itemColor = MaterialTheme.colorScheme.surfaceBright,
        contentColor = MaterialTheme.colorScheme.onSurface,
        iconContainerColor = MaterialTheme.colorScheme.secondaryContainer,
        iconColor = MaterialTheme.colorScheme.onSecondaryContainer,
        titleStyle = MaterialTheme.typography.titleLarge,
        itemStyle = MaterialTheme.typography.bodyLarge,
    )

    UiMode.Miuix -> RebootDialogStyle(
        containerColor = MiuixTheme.colorScheme.surfaceContainerHigh,
        itemColor = MiuixTheme.colorScheme.surface,
        contentColor = MiuixTheme.colorScheme.onSurface,
        iconContainerColor = MiuixTheme.colorScheme.secondaryContainer,
        iconColor = MiuixTheme.colorScheme.onSecondaryContainer,
        titleStyle = MiuixTheme.textStyles.title4,
        itemStyle = MiuixTheme.textStyles.body1,
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RebootDialog(
    show: Boolean,
    onDismissRequest: () -> Unit,
    onReboot: (String) -> Unit,
) {
    if (!show) return

    val options = getRebootListOption()
    val style = rebootDialogStyle()

    BasicAlertDialog(onDismissRequest = onDismissRequest) {
        Surface(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(28.dp),
            color = style.containerColor,
            contentColor = style.contentColor,
        ) {
            Column(
                modifier = Modifier
                    .heightIn(max = 640.dp)
                    .verticalScroll(rememberScrollState())
                    .padding(24.dp),
            ) {
                Text(
                    text = stringResource(R.string.reboot),
                    style = style.titleStyle,
                    fontWeight = FontWeight.Medium,
                    color = style.contentColor,
                )

                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 20.dp)
                        .clip(RoundedCornerShape(20.dp)),
                    verticalArrangement = Arrangement.spacedBy(2.dp),
                ) {
                    options.forEachIndexed { index, option ->
                        val shape = RoundedCornerShape(
                            topStart = if (index == 0) 20.dp else 4.dp,
                            topEnd = if (index == 0) 20.dp else 4.dp,
                            bottomStart = if (index == options.lastIndex) 20.dp else 4.dp,
                            bottomEnd = if (index == options.lastIndex) 20.dp else 4.dp,
                        )
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(shape)
                                .background(style.itemColor)
                                .clickable {
                                    onDismissRequest()
                                    onReboot(option.reason)
                                }
                                .padding(horizontal = 16.dp, vertical = 16.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(40.dp)
                                    .background(style.iconContainerColor, CircleShape),
                                contentAlignment = Alignment.Center,
                            ) {
                                Icon(
                                    imageVector = option.icon,
                                    contentDescription = null,
                                    modifier = Modifier.size(20.dp),
                                    tint = style.iconColor,
                                )
                            }
                            Text(
                                text = stringResource(option.labelRes),
                                modifier = Modifier.padding(start = 16.dp),
                                style = style.itemStyle,
                                color = style.contentColor,
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun RebootDialogButton() {
    var showDialog by remember { mutableStateOf(false) }

    KsuIsValid {
        val onReboot = rememberRebootAction()

        when (LocalUiMode.current) {
            UiMode.Material -> IconButton(onClick = { showDialog = true }) {
                Icon(
                    imageVector = Icons.Filled.Refresh,
                    contentDescription = stringResource(R.string.reboot),
                )
            }

            UiMode.Miuix -> MiuixIconButton(
                onClick = { showDialog = true },
                holdDownState = showDialog,
            ) {
                MiuixIcon(
                    imageVector = Icons.Filled.Refresh,
                    contentDescription = stringResource(R.string.reboot),
                    tint = MiuixTheme.colorScheme.onBackground,
                )
            }
        }

        RebootDialog(
            show = showDialog,
            onDismissRequest = { showDialog = false },
            onReboot = onReboot,
        )
    }
}
