package me.weishu.kernelsu.ui.webui.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.materialkolor.PaletteStyle
import com.materialkolor.dynamiccolor.ColorSpec
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.theme.AppSettings
import me.weishu.kernelsu.ui.theme.ColorMode
import me.weishu.kernelsu.ui.theme.KernelSUTheme
import me.weishu.kernelsu.ui.webui.model.WebUIIntent
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
internal fun ExternalLinkWarningOverlay(
    externalLinkUrl: String?,
    dispatch: (WebUIIntent) -> Unit,
) {
    val visibilityState = remember { MutableTransitionState(false) }
    var displayedExternalLinkUrl by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(externalLinkUrl) {
        if (externalLinkUrl != null) {
            displayedExternalLinkUrl = externalLinkUrl
        }
        visibilityState.targetState = externalLinkUrl != null
    }

    LaunchedEffect(visibilityState.isIdle, visibilityState.currentState) {
        if (visibilityState.isIdle && !visibilityState.currentState) {
            displayedExternalLinkUrl = null
        }
    }

    AnimatedVisibility(
        visibleState = visibilityState,
        enter = fadeIn(),
        exit = fadeOut(),
    ) {
        displayedExternalLinkUrl?.let { url ->
            WebUIExternalLinkWarning(
                url = url,
                dispatch = dispatch,
            )
        }
    }
}


@Composable
private fun WebUIExternalLinkWarning(
    url: String,
    dispatch: (WebUIIntent) -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ExternalLinkWarningMiuix(url, dispatch)
        UiMode.Material -> ExternalLinkWarningMaterial(url, dispatch)
    }
}

@Composable
private fun ExternalLinkWarningMaterial(
    url: String,
    dispatch: (WebUIIntent) -> Unit,
) {
    androidx.compose.material3.Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
                modifier = Modifier.widthIn(max = 500.dp),
            ) {
                Icon(
                    imageVector = Icons.Rounded.Warning,
                    contentDescription = null,
                    modifier = Modifier.size(64.dp),
                    tint = MaterialTheme.colorScheme.onPrimaryContainer,
                )
                Spacer(modifier = Modifier.height(16.dp))
                androidx.compose.material3.Text(
                    text = stringResource(R.string.webui_external_link_warning_title),
                    style = MaterialTheme.typography.headlineSmall,
                    textAlign = TextAlign.Center,
                )
                Spacer(modifier = Modifier.height(8.dp))
                androidx.compose.material3.Text(
                    text = stringResource(R.string.webui_external_link_warning_message),
                    style = MaterialTheme.typography.bodyMedium,
                    textAlign = TextAlign.Center,
                )
                Spacer(modifier = Modifier.height(16.dp))
                androidx.compose.material3.Surface(
                    modifier = Modifier
                        .fillMaxWidth(),
                    shape = MaterialTheme.shapes.medium,
                    color = MaterialTheme.colorScheme.surfaceVariant
                ) {
                    SelectionContainer(modifier = Modifier.padding(12.dp)) {
                        androidx.compose.material3.Text(
                            text = url,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 5,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
                Spacer(modifier = Modifier.height(24.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.Center,
                ) {
                    androidx.compose.material3.OutlinedButton(
                        onClick = { dispatch(WebUIIntent.ExternalLinkGoBack) },
                        modifier = Modifier.weight(1f),
                    ) {
                        androidx.compose.material3.Text(
                            text = stringResource(R.string.webui_go_back),
                        )
                    }
                    Spacer(modifier = Modifier.width(12.dp))
                    androidx.compose.material3.Button(
                        onClick = { dispatch(WebUIIntent.ExternalLinkOpenBrowser(url)) },
                        modifier = Modifier.weight(1f),
                    ) {
                        androidx.compose.material3.Text(
                            text = stringResource(R.string.webui_open_in_external),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ExternalLinkWarningMiuix(
    url: String,
    dispatch: (WebUIIntent) -> Unit,
) {
    top.yukonga.miuix.kmp.basic.Surface(
        modifier = Modifier
            .fillMaxSize()
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = {},
            ),
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
                modifier = Modifier.widthIn(max = 500.dp),
            ) {
                Icon(
                    imageVector = Icons.Rounded.Warning,
                    contentDescription = null,
                    modifier = Modifier.size(64.dp),
                    tint = MiuixTheme.colorScheme.onPrimaryVariant,
                )
                Spacer(modifier = Modifier.height(16.dp))
                top.yukonga.miuix.kmp.basic.Text(
                    text = stringResource(R.string.webui_external_link_warning_title),
                    style = MiuixTheme.textStyles.title2,
                    textAlign = TextAlign.Center,
                )
                Spacer(modifier = Modifier.height(8.dp))
                top.yukonga.miuix.kmp.basic.Text(
                    text = stringResource(R.string.webui_external_link_warning_message),
                    style = MiuixTheme.textStyles.body2,
                    textAlign = TextAlign.Center,
                )
                Spacer(modifier = Modifier.height(16.dp))
                top.yukonga.miuix.kmp.basic.Surface(
                    modifier = Modifier.fillMaxWidth(),
                    color = MiuixTheme.colorScheme.surfaceContainer,
                    shape = RoundedCornerShape(8.dp)
                ) {
                    SelectionContainer(modifier = Modifier.padding(12.dp)) {
                        top.yukonga.miuix.kmp.basic.Text(
                            text = url,
                            style = MiuixTheme.textStyles.body2,
                            color = MiuixTheme.colorScheme.onSurfaceContainer,
                            maxLines = 5,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
                Spacer(modifier = Modifier.height(24.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    top.yukonga.miuix.kmp.basic.TextButton(
                        onClick = { dispatch(WebUIIntent.ExternalLinkGoBack) },
                        text = stringResource(R.string.webui_go_back),
                        modifier = Modifier.weight(1f),
                    )
                    Spacer(modifier = Modifier.width(20.dp))
                    top.yukonga.miuix.kmp.basic.TextButton(
                        onClick = { dispatch(WebUIIntent.ExternalLinkOpenBrowser(url)) },
                        text = stringResource(R.string.webui_open_in_external),
                        modifier = Modifier.weight(1f),
                        colors = top.yukonga.miuix.kmp.basic.ButtonDefaults.textButtonColorsPrimary(),
                    )
                }
            }
        }
    }
}

@Preview(name = "Material", showBackground = true)
@Composable
private fun WebUIExternalLinkWarningMaterialPreview() {
    WebUIExternalLinkWarningPreview(uiMode = UiMode.Material)
}

@Preview(name = "Miuix", showBackground = true)
@Composable
private fun WebUIExternalLinkWarningMiuixPreview() {
    WebUIExternalLinkWarningPreview(uiMode = UiMode.Miuix)
}

@Composable
private fun WebUIExternalLinkWarningPreview(uiMode: UiMode) {
    CompositionLocalProvider(LocalUiMode provides uiMode) {
        KernelSUTheme(
            appSettings = previewAppSettings,
            uiMode = uiMode,
        ) {
            WebUIExternalLinkWarning(
                url = "https://example.com/module/path",
                dispatch = {},
            )
        }
    }
}

private val previewAppSettings = AppSettings(
    colorMode = ColorMode.LIGHT,
    keyColor = 0xFF6750A4.toInt(),
    paletteStyle = PaletteStyle.TonalSpot,
    colorSpec = ColorSpec.SpecVersion.Default,
)
