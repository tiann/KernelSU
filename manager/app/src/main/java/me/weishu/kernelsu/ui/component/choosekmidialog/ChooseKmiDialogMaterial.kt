package me.weishu.kernelsu.ui.component.choosekmidialog

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.material.ExpressiveColumn
import me.weishu.kernelsu.ui.component.material.ExpressiveRadioItem
import me.weishu.kernelsu.ui.util.getCurrentKmi
import me.weishu.kernelsu.ui.util.getSupportedKmis

@Composable
fun ChooseKmiDialogMaterial(
    showDialog: MutableState<Boolean>,
    onSelected: (String?) -> Unit
) {
    if (!showDialog.value) return

    val supportedKMIs by produceState(initialValue = emptyList<String>()) {
        value = getSupportedKmis()
    }

    val currentKmi by produceState(initialValue = "") {
        value = getCurrentKmi()
    }

    val selectedKmi = remember(currentKmi) { mutableStateOf(currentKmi) }

    AlertDialog(
        onDismissRequest = {
            showDialog.value = false
            selectedKmi.value = currentKmi
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onSelected(selectedKmi.value)
                    showDialog.value = false
                },
                enabled = supportedKMIs.contains(selectedKmi.value)
            ) {
                Text(stringResource(id = R.string.confirm))
            }
        },
        dismissButton = {
            TextButton(onClick = {
                showDialog.value = false
                selectedKmi.value = currentKmi
            }) {
                Text(stringResource(id = android.R.string.cancel))
            }
        },
        title = {
            Text(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp),
                text = stringResource(R.string.select_kmi),
                textAlign = TextAlign.Center
            )
        },
        text = {
            ExpressiveColumn(
                selectedIndices = supportedKMIs.indexOf(selectedKmi.value).let { if (it >= 0) setOf(it) else emptySet() },
                content = supportedKMIs.map { kmi ->
                    {
                        ExpressiveRadioItem(
                            title = kmi,
                            summary = if (kmi == currentKmi) stringResource(R.string.current_device_kmi) else null,
                            selected = selectedKmi.value == kmi,
                            onClick = { selectedKmi.value = kmi }
                        )
                    }
                }
            )
        }
    )
}
