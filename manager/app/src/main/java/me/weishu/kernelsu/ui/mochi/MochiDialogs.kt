package me.weishu.kernelsu.ui.mochi

import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import kotlinx.collections.immutable.persistentListOf
import kotlinx.collections.immutable.toImmutableList
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.registry.RegistryUrls
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.component.dialog.ConfirmDialogVisuals
import me.weishu.kernelsu.ui.component.dialog.rememberConfirmDialog
import me.weishu.kernelsu.ui.component.markdown.MarkdownContent
import me.weishu.kernelsu.ui.component.rebootlistpopup.getRebootListOption
import me.weishu.kernelsu.ui.component.rebootlistpopup.rememberRebootAction
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.screen.flash.UninstallType
import me.weishu.kernelsu.ui.util.getCurrentKmi
import me.weishu.kernelsu.ui.util.getSupportedKmis
import ui.feice.mochi.basic.MochiActivityIndicator
import ui.feice.mochi.basic.MochiBarIconDropdown
import ui.feice.mochi.basic.MochiDialog
import ui.feice.mochi.basic.MochiDialogScrollContent
import ui.feice.mochi.basic.MochiDialogAction
import ui.feice.mochi.basic.MochiDialogActionRole
import ui.feice.mochi.basic.MochiItem
import ui.feice.mochi.basic.MochiTextField
import ui.feice.mochi.basic.Text
import ui.feice.mochi.icons.MochiIonicon
import ui.feice.mochi.list.MochiListCard
import ui.feice.mochi.list.MochiListRow
import ui.feice.mochi.list.MochiListStatusChip
import ui.feice.mochi.theme.MochiTheme

@Composable
fun LoadingDialogMochi(showDialog: MutableState<Boolean>) {
    MochiDialog(
        visible = showDialog.value,
        onDismissRequest = { },
        title = stringResource(R.string.processing),
        actions = persistentListOf(),
        dismissOnBack = false,
    ) {
        Box(Modifier.fillMaxWidth().padding(vertical = 8.dp), contentAlignment = Alignment.Center) {
            MochiActivityIndicator(size = 28.dp)
        }
    }
}

@Composable
fun ConfirmDialogMochi(
    visuals: ConfirmDialogVisuals,
    confirm: () -> Unit,
    dismiss: () -> Unit,
    showDialog: MutableState<Boolean>,
) {
    // MochiDialog also calls onDismissRequest after a button, so remember that a result was already sent
    var answered by remember { mutableStateOf(false) }
    val rich = visuals.content != null && (visuals.isMarkdown || visuals.isHtml || visuals.isRestricted)
    MochiDialog(
        visible = showDialog.value,
        onDismissRequest = {
            if (!answered) dismiss()
            answered = false
            showDialog.value = false
        },
        title = visuals.title,
        message = visuals.content?.takeUnless { rich },
        actions = persistentListOf(
            MochiDialogAction(
                visuals.dismiss ?: stringResource(android.R.string.cancel),
                role = MochiDialogActionRole.Cancel,
                onClick = {
                    answered = true
                    dismiss()
                },
            ),
            MochiDialogAction(
                visuals.confirm ?: stringResource(android.R.string.ok),
                role = MochiDialogActionRole.Primary,
                onClick = {
                    answered = true
                    confirm()
                },
            ),
        ),
        content = if (rich) {
            {
                Box(Modifier.fillMaxWidth().heightIn(max = 420.dp)) {
                    val content = visuals.content.orEmpty()
                    when {
                        visuals.isRestricted || visuals.isHtml -> MarkdownContent(
                            content = content,
                            isMarkdown = !visuals.isHtml,
                            restrictedBaseUrl = RegistryUrls.DEFAULT_BASE_URL,
                        )

                        else -> MarkdownContent(content = content, isMarkdown = true)
                    }
                }
            }
        } else {
            null
        },
    )
}

@Composable
fun DownloadDialogMochi(show: Boolean, onConfirm: (String) -> Unit, onDismiss: () -> Unit) {
    var url by rememberSaveable { mutableStateOf("") }
    val valid = isHttpsUrl(url.trim())
    MochiDialog(
        visible = show,
        onDismissRequest = onDismiss,
        title = stringResource(R.string.download_dialog_title),
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), role = MochiDialogActionRole.Cancel),
            MochiDialogAction(
                stringResource(android.R.string.ok),
                role = MochiDialogActionRole.Primary,
                enabled = valid,
                onClick = { onConfirm(url.trim()) },
            ),
        ),
    ) {
        MochiTextField(
            value = url,
            onValueChange = { url = it },
            placeholder = stringResource(R.string.download_dialog_msg),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

private fun isHttpsUrl(url: String): Boolean {
    if (url.isEmpty()) return false
    val uri = url.toUri()
    return uri.scheme.equals("https", ignoreCase = true) && !uri.host.isNullOrEmpty() && !uri.path.isNullOrEmpty()
}

@Composable
fun ChooseKmiDialogMochi(show: Boolean, onDismissRequest: () -> Unit, onSelected: (String?) -> Unit) {
    val supported by produceState(initialValue = emptyList<String>()) { value = getSupportedKmis() }
    val current by produceState(initialValue = "") { value = getCurrentKmi() }
    var selection by rememberSaveable(current) { mutableStateOf(current) }
    MochiDialog(
        visible = show,
        onDismissRequest = {
            onDismissRequest()
            selection = current
        },
        title = stringResource(R.string.select_kmi),
        message = stringResource(R.string.current_kmi, current.ifBlank { "Unknown" }),
        actions = persistentListOf(
            MochiDialogAction(stringResource(android.R.string.cancel), role = MochiDialogActionRole.Cancel),
            MochiDialogAction(
                stringResource(android.R.string.ok),
                role = MochiDialogActionRole.Primary,
                enabled = selection in supported,
                onClick = { onSelected(selection) },
            ),
        ),
    ) {
        MochiDialogScrollContent(maxHeight = 360.dp) {
            MochiListCard {
                supported.forEach { kmi ->
                    MochiListRow(
                        kmi,
                        subtitle = if (kmi == current) stringResource(R.string.current_device_kmi) else null,
                        checked = selection == kmi,
                        onClick = { selection = kmi },
                    )
                }
            }
        }
    }
}

private val UninstallIcons = mapOf(
    UninstallType.TEMPORARY to "shield-half",
    UninstallType.PERMANENT to "trash",
    UninstallType.RESTORE_STOCK_IMAGE to "refresh-circle",
)

@Composable
fun UninstallDialogMochi(show: Boolean, onDismissRequest: () -> Unit) {
    val context = LocalContext.current
    val navigator = LocalNavigator.current
    var pending by remember { mutableStateOf<UninstallType?>(null) }
    val options = listOf(UninstallType.PERMANENT, UninstallType.RESTORE_STOCK_IMAGE)
    val confirmDialog = rememberConfirmDialog(
        onConfirm = {
            when (pending) {
                UninstallType.PERMANENT -> navigator.push(Route.Flash(FlashIt.FlashUninstall))
                UninstallType.RESTORE_STOCK_IMAGE -> navigator.push(Route.Flash(FlashIt.FlashRestore))
                UninstallType.TEMPORARY -> Toast.makeText(context, "TODO", Toast.LENGTH_SHORT).show()
                else -> Unit
            }
            pending = null
        },
        onDismiss = { pending = null },
    )
    val titles = options.associateWith { stringResource(it.title) }
    val messages = options.associateWith { stringResource(it.message) }
    MochiDialog(
        visible = show,
        onDismissRequest = onDismissRequest,
        title = stringResource(R.string.uninstall),
        actions = persistentListOf(MochiDialogAction(stringResource(android.R.string.cancel), role = MochiDialogActionRole.Cancel)),
    ) {
        MochiListCard {
            options.forEach { type ->
                MochiListRow(
                    titles.getValue(type),
                    leading = { MochiIonicon(UninstallIcons.getValue(type)) },
                    destructive = type == UninstallType.PERMANENT,
                    onClick = {
                        onDismissRequest()
                        pending = type
                        confirmDialog.showConfirm(title = titles.getValue(type), content = messages.getValue(type))
                    },
                )
            }
        }
    }
}

@Composable
fun RebootListPopupMochi() {
    KsuIsValid {
        val onReboot = rememberRebootAction()
        val options = getRebootListOption()
        val labels = options.map { stringResource(it.labelRes) }
        var expanded by remember { mutableStateOf(false) }
        val reboot = stringResource(R.string.reboot)
        MochiBarIconDropdown(
            expanded = expanded,
            onExpandedChange = { expanded = it },
            items = labels.map { MochiItem(it) }.toImmutableList(),
            onItemClick = { index -> onReboot(options[index].reason) },
            modifier = Modifier.semantics { contentDescription = reboot },
            navId = "ksu.reboot",
            contentKey = "power",
        ) {
            MochiIonicon("power")
        }
    }
}

@Composable
fun StatusTagMochi(label: String, modifier: Modifier, backgroundColor: Color, contentColor: Color) {
    MochiListStatusChip(label, modifier, containerColor = backgroundColor, contentColor = contentColor)
}

@Composable
internal fun MochiFootnote(text: String, modifier: Modifier = Modifier, color: Color = MochiTheme.colorScheme.onSurfaceVariant) {
    Text(text, modifier, style = MochiTheme.textStyles.footnote, color = color)
}
