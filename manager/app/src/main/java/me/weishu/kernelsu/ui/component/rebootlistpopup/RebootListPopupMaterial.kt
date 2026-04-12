package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.KsuIsValid
import me.weishu.kernelsu.ui.util.reboot

@Composable
fun RebootDropdownItems(onItemClick: (String) -> Unit) {
    getRebootListOption().forEach { option ->
        DropdownMenuItem(
            text = { Text("  " + stringResource(option.labelRes)) },
            onClick = { onItemClick(option.reason) }
        )
    }
}

@Composable
fun RebootListPopupMaterial() {
    var expanded by remember { mutableStateOf(false) }

    KsuIsValid {
        IconButton(onClick = { expanded = true }) {
            Icon(
                imageVector = Icons.Filled.PowerSettingsNew,
                contentDescription = stringResource(id = R.string.reboot)
            )
        }

        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false }
        ) {
            RebootDropdownItems { reason ->
                expanded = false
                reboot(reason)
            }
        }
    }
}
