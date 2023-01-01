package me.weishu.kernelsu.ui.screen

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.alorma.compose.settings.ui.*
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.LinkifyText

/**
 * @author weishu
 * @date 2023/1/1.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Destination
@Composable
fun SettingScreen(navigator: DestinationsNavigator) {

    Scaffold(
        topBar = {
            TopBar(onBack = {
                navigator.popBackStack()
            })
        }
    ) { paddingValues ->

        var openDialog by remember { mutableStateOf(false) }

        if (openDialog) {
            AlertDialog(
                onDismissRequest = {
                    openDialog = false
                },
                title = {
                    Text(text = stringResource(id = R.string.about))
                },
                text = {
                    SupportCard()
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            openDialog = false
                        }
                    ) {
                        Text(stringResource(id = android.R.string.ok))
                    }
                },
            )
        }

        Column(modifier = Modifier.padding(paddingValues)) {

            SettingsSwitch(
                title = {
                    Text(stringResource(id = R.string.settings_system_rw))
                },
                subtitle = {
                    Text(stringResource(id = R.string.settings_system_rw_summary))
                }
            )
            SettingsMenuLink(title = {
                Text(stringResource(id = R.string.about))
            },
                onClick = {
                    openDialog = true
                }
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TopBar(onBack: () -> Unit = {}) {
    TopAppBar(
        title = { Text(stringResource(R.string.settings)) },
        navigationIcon = {
            IconButton(
                onClick = onBack
            ) { Icon(Icons.Filled.ArrowBack, contentDescription = null) }
        },
    )
}

@Preview
@Composable
private fun SupportCard() {
    Column(
        modifier = Modifier
            .fillMaxWidth()
    ) {
        CompositionLocalProvider(LocalTextStyle provides MaterialTheme.typography.bodyMedium) {
            LinkifyText("Author: weishu")
            LinkifyText("Github: https://github.com/tiann/KernelSU")
            LinkifyText("Telegram: https://t.me/KernelSU")
            LinkifyText("QQ: https://pd.qq.com/s/8lipl1brp")
        }
    }
}