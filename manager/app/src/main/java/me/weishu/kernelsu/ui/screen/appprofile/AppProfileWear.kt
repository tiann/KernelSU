package me.weishu.kernelsu.ui.screen.appprofile

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.wear.compose.foundation.lazy.TransformingLazyColumn
import androidx.wear.compose.foundation.lazy.rememberTransformingLazyColumnState
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.ButtonDefaults
import androidx.wear.compose.material3.Card
import androidx.wear.compose.material3.ListHeader
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.ScreenScaffold
import androidx.wear.compose.material3.Text
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.wear.wearHorizontalPadding

@Composable
fun AppProfileScreenWear(
    state: AppProfileUiState,
    actions: AppProfileActions,
) {
    val listState = rememberTransformingLazyColumnState()
    val horizontalPadding = wearHorizontalPadding()
    val profile = state.profile

    ScreenScaffold(
        scrollState = listState,
    ) { contentPadding ->
        TransformingLazyColumn(
            state = listState,
            contentPadding = contentPadding,
        ) {
            item {
                ListHeader {
                    Text(stringResource(R.string.profile))
                }
            }

            item {
                Card(
                    onClick = {},
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                ) {
                    Text(
                        text = state.packageName,
                        style = MaterialTheme.typography.titleSmall,
                    )
                    Text(
                        text = "UID: ${state.uid}",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    Text(
                        text = if (profile.allowSu) stringResource(R.string.superuser_allow) else stringResource(
                            R.string.superuser_deny
                        ),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }

            item {
                Button(
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                    onClick = {
                        actions.onProfileChange(profile.copy(allowSu = !profile.allowSu))
                    },
                    colors = if (profile.allowSu) ButtonDefaults.buttonColors() else ButtonDefaults.filledTonalButtonColors(),
                    label = {
                        Text(
                            if (profile.allowSu) stringResource(R.string.superuser_allow)
                            else stringResource(R.string.superuser_deny)
                        )
                    },
                    secondaryLabel = { Text(stringResource(R.string.superuser_allow_root_access)) },
                )
            }

            item {
                Button(
                    modifier = Modifier
                        .padding(horizontal = horizontalPadding)
                        .fillMaxWidth(),
                    onClick = actions.onManageTemplate,
                    colors = ButtonDefaults.filledTonalButtonColors(),
                    label = { Text(stringResource(R.string.settings_profile_template)) },
                )
            }
        }
    }
}
