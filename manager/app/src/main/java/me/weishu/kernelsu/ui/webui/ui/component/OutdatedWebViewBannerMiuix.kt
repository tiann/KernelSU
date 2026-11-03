package me.weishu.kernelsu.ui.webui.ui.component

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.SwipeToDismissBox
import androidx.compose.material3.rememberSwipeToDismissBoxState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.webui.util.WebViewVersionUtil
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Close
import top.yukonga.miuix.kmp.theme.MiuixTheme

/**
 * Shown once per session when the page asked for edge-to-edge on a WebView that cannot provide
 * safe-area insets.
 *
 * The whole card is the update action, so the close button is the only way to dismiss it without
 * leaving the page. Two things are deliberate: the card carries a button role for TalkBack, which
 * a bare `Card(onClick)` does not, and the close button sits outside the card's clickable area by
 * being an `IconButton` of its own - otherwise tapping it would also fire the update.
 */
@Composable
fun OutdatedWebViewBannerMiuix(
    currentVersion: Int,
    requiredVersion: Int,
    onDismiss: () -> Unit,
    onUpdateClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val dismissState = rememberSwipeToDismissBoxState()
    SwipeToDismissBox(
        state = dismissState,
        backgroundContent = {},
        onDismiss = {
            onDismiss()
        },
        modifier = modifier.fillMaxWidth(),
    ) {
        Card(
            modifier = Modifier
                .padding(horizontal = 8.dp)
                .fillMaxWidth()
                .semantics {
                    role = Role.Button
                    onClick(label = "") {
                        onUpdateClick()
                        true
                    }
                },
            colors = CardDefaults.defaultColors(
                color = MiuixTheme.colorScheme.errorContainer,
                contentColor = MiuixTheme.colorScheme.onErrorContainer,
            ),
            onClick = onUpdateClick,
            showIndication = true,
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(
                    imageVector = Icons.Default.Warning,
                    contentDescription = null,
                    tint = MiuixTheme.colorScheme.onErrorContainer
                )
                Spacer(modifier = Modifier.width(8.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = stringResource(R.string.webview_outdated_title),
                        fontSize = 14.sp,
                    )
                    Text(
                        text = stringResource(R.string.webview_outdated_message, currentVersion, requiredVersion),
                        fontSize = 12.sp,
                    )
                }
                IconButton(onClick = onDismiss) {
                    Icon(
                        imageVector = MiuixIcons.Close,
                        contentDescription = stringResource(R.string.close),
                        tint = MiuixTheme.colorScheme.onErrorContainer,
                    )
                }
            }
        }
    }
}

@Preview
@Composable
private fun PreviewOutdatedWebViewBannerMiuix() {
    OutdatedWebViewBannerMiuix(
        currentVersion = 1,
        requiredVersion = WebViewVersionUtil.MIN_INSETS_VERSION,
        onDismiss = { },
        onUpdateClick = { },
    )
}
