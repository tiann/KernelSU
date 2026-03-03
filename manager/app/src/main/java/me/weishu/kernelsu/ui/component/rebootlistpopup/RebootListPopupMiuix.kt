package me.weishu.kernelsu.ui.component.rebootlistpopup

import android.content.Context
import android.os.Build
import android.os.PowerManager
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.util.reboot
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.ListPopupDefaults
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.extra.SuperListPopup
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Close2
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun RebootListPopupMiuix(
    modifier: Modifier = Modifier,
    alignment: PopupPositionProvider.Align = PopupPositionProvider.Align.TopEnd
) {
    val showTopPopup = remember { mutableStateOf(false) }
    KsuIsValid {
        IconButton(
            modifier = modifier,
            onClick = { showTopPopup.value = true },
            holdDownState = showTopPopup.value
        ) {
            Icon(
                imageVector = MiuixIcons.Close2,
                contentDescription = stringResource(id = R.string.reboot),
                tint = colorScheme.onBackground
            )
        }
        SuperListPopup(
            show = showTopPopup,
            popupPositionProvider = ListPopupDefaults.ContextMenuPositionProvider,
            alignment = alignment,
            onDismissRequest = {
                showTopPopup.value = false
            }
        ) {
            val pm = LocalContext.current.getSystemService(Context.POWER_SERVICE) as PowerManager?

            @Suppress("DEPRECATION")
            val isRebootingUserspaceSupported =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && pm?.isRebootingUserspaceSupported == true

            ListPopupColumn {
                val rebootOptions = mutableListOf(
                    Pair(R.string.reboot, ""),
                    Pair(R.string.reboot_recovery, "recovery"),
                    Pair(R.string.reboot_bootloader, "bootloader"),
                    Pair(R.string.reboot_download, "download"),
                    Pair(R.string.reboot_edl, "edl")
                )
                if (isRebootingUserspaceSupported) {
                    rebootOptions.add(1, Pair(R.string.reboot_userspace, "userspace"))
                }
                rebootOptions.forEachIndexed { idx, (id, reason) ->
                    RebootDropdownItem(
                        id = id,
                        reason = reason,
                        showTopPopup = showTopPopup,
                        optionSize = rebootOptions.size,
                        index = idx
                    )
                }
            }
        }
    }
}

@Composable
fun RebootDropdownItem(
    id: Int,
    reason: String = "",
    showTopPopup: MutableState<Boolean>,
    optionSize: Int,
    index: Int,
) {
    me.weishu.kernelsu.ui.component.miuix.DropdownItem(
        text = stringResource(id),
        optionSize = optionSize,
        onSelectedIndexChange = {
            reboot(reason)
            showTopPopup.value = false
        },
        index = index
    )
}
