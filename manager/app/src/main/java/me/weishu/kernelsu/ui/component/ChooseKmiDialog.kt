package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.util.getCurrentKmi
import me.weishu.kernelsu.ui.util.getSupportedKmis
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.extra.CheckboxLocation
import top.yukonga.miuix.kmp.extra.SuperCheckbox
import top.yukonga.miuix.kmp.extra.SuperDialog

@Composable
fun ChooseKmiDialog(
    showDialog: MutableState<Boolean>,
    onSelected: (String?) -> Unit
) {
    val supportedKMIs by produceState(initialValue = emptyList()) {
        value = getSupportedKmis()
    }
    val currentKmi by produceState(initialValue = "") {
        value = getCurrentKmi()
    }
    val currentSelection = rememberSaveable(currentKmi) { mutableStateOf(currentKmi) }
    SuperDialog(
        show = showDialog,
        title = stringResource(R.string.select_kmi),
        summary = stringResource(R.string.current_kmi, currentKmi.let { it.ifBlank { "Unknown" } }),
        insideMargin = DpSize(0.dp, 24.dp),
        onDismissRequest = {
            showDialog.value = false
            currentSelection.value = currentKmi
        },
    ) {
        Column(modifier = Modifier.heightIn(max = 500.dp)) {
            LazyColumn(modifier = Modifier.weight(1f, fill = false)) {
                items(supportedKMIs) { kmi ->
                    SuperCheckbox(
                        title = kmi,
                        summary = if (kmi == currentKmi) stringResource(R.string.current_device_kmi) else null,
                        insideMargin = PaddingValues(horizontal = 30.dp, vertical = 16.dp),
                        checkboxLocation = CheckboxLocation.End,
                        checked = currentSelection.value == kmi,
                        holdDownState = currentSelection.value == kmi,
                        onCheckedChange = { _ ->
                            currentSelection.value = kmi
                        }
                    )
                }
            }
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.padding(horizontal = 24.dp),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                TextButton(
                    onClick = {
                        showDialog.value = false
                        currentSelection.value = currentKmi
                    },
                    text = stringResource(android.R.string.cancel),
                    modifier = Modifier.weight(1f),
                )
                Spacer(modifier = Modifier.width(20.dp))
                TextButton(
                    enabled = supportedKMIs.contains(currentSelection.value),
                    onClick = {
                        onSelected(currentSelection.value)
                        showDialog.value = false
                    },
                    text = stringResource(R.string.confirm),
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.textButtonColorsPrimary()
                )
            }
        }
    }
}
