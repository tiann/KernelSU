package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.ListPopupDefaults
import me.weishu.kernelsu.ui.util.reboot
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Close2
import top.yukonga.miuix.kmp.overlay.OverlayListPopup
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
        OverlayListPopup(
            show = showTopPopup.value,
            popupPositionProvider = ListPopupDefaults.MenuPositionProvider,
            alignment = alignment,
            onDismissRequest = {
                showTopPopup.value = false
            },
            content = {
                val rebootOptions = getRebootListOption()

                ListPopupColumn {
                    rebootOptions.forEachIndexed { idx, option ->
                        RebootDropdownItem(
                            option = option,
                            showTopPopup = showTopPopup,
                            optionSize = rebootOptions.size,
                            index = idx
                        )
                    }
                }
            }
        )
    }
}

@Composable
fun RebootDropdownItem(
    option: RebootListOption,
    showTopPopup: MutableState<Boolean>,
    optionSize: Int,
    index: Int,
) {
    me.weishu.kernelsu.ui.component.miuix.DropdownItem(
        text = stringResource(option.labelRes),
        optionSize = optionSize,
        onSelectedIndexChange = {
            reboot(option.reason)
            showTopPopup.value = false
        },
        index = index
    )
}
