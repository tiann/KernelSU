package me.weishu.kernelsu.ui.component.rebootlistpopup

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material3.DropdownMenuGroup
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.DropdownMenuPopup
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MenuDefaults
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
    val options = getRebootListOption()
    options.forEachIndexed { index, option ->
        DropdownMenuItem(
            selected = false,
            onClick = { onItemClick(option.reason) },
            text = { Text("  " + stringResource(option.labelRes)) },
            shapes = MenuDefaults.itemShape(index = index, count = options.size),
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

        DropdownMenuPopup(
            expanded = expanded,
            onDismissRequest = { expanded = false }
        ) {
            DropdownMenuGroup(shapes = MenuDefaults.groupShapes()) {
                RebootDropdownItems { reason ->
                    expanded = false
                    reboot(reason)
                }
            }
        }
    }
}
