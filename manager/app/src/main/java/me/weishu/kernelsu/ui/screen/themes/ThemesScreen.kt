package me.weishu.kernelsu.ui.screen.themes

import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FileOpen
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.dropUnlessResumed
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.theme.engine.SygiThemeImporter
import me.weishu.kernelsu.ui.theme.engine.SygiThemeInfo
import me.weishu.kernelsu.ui.theme.engine.SygiThemeManager

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ThemesScreen() {
    val navigator = LocalNavigator.current
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var themes by remember { mutableStateOf(SygiThemeManager.list()) }
    var activeId by remember { mutableStateOf(SygiThemeManager.activeId()) }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            val result = withContext(Dispatchers.IO) {
                runCatching { SygiThemeImporter.importZip(context, uri) }
            }
            result.onSuccess { info ->
                themes = SygiThemeManager.list()
                Toast.makeText(context, context.getString(R.string.sygi_themes_imported, info.name), Toast.LENGTH_SHORT).show()
            }.onFailure { e ->
                Toast.makeText(context, context.getString(R.string.sygi_themes_import_failed, e.message ?: "?"), Toast.LENGTH_LONG).show()
            }
        }
    }

    fun apply(id: String?) {
        SygiThemeManager.apply(id)
        activeId = SygiThemeManager.activeId()
        Toast.makeText(context, context.getString(R.string.sygi_themes_applied), Toast.LENGTH_SHORT).show()
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.sygi_themes)) },
                navigationIcon = {
                    IconButton(onClick = dropUnlessResumed { navigator.pop() }) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
                    }
                }
            )
        },
        floatingActionButton = {
            ExtendedFloatingActionButton(
                onClick = { picker.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) },
                icon = { Icon(Icons.Filled.FileOpen, contentDescription = null) },
                text = { Text(stringResource(R.string.sygi_themes_import)) },
            )
        },
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 96.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            item {
                ThemeRow(
                    title = stringResource(R.string.sygi_themes_default),
                    subtitle = stringResource(R.string.sygi_themes_default_summary),
                    selected = activeId == null,
                    onSelect = { apply(null) },
                    onDelete = null,
                )
            }
            if (themes.isEmpty()) {
                item {
                    Text(
                        stringResource(R.string.sygi_themes_empty),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(vertical = 12.dp),
                    )
                }
            }
            items(themes, key = { it.id }) { info: SygiThemeInfo ->
                ThemeRow(
                    title = info.name,
                    subtitle = listOf(info.author, info.version).filter { it.isNotBlank() }.joinToString(" · "),
                    selected = activeId == info.id,
                    onSelect = { apply(info.id) },
                    onDelete = {
                        SygiThemeManager.delete(info.id)
                        themes = SygiThemeManager.list()
                        activeId = SygiThemeManager.activeId()
                    },
                )
            }
            item {
                Spacer(Modifier.height(8.dp))
                Text(
                    stringResource(R.string.sygi_themes_hint),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun ThemeRow(
    title: String,
    subtitle: String,
    selected: Boolean,
    onSelect: () -> Unit,
    onDelete: (() -> Unit)?,
) {
    Card(
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.cardColors(
            containerColor = if (selected) MaterialTheme.colorScheme.secondaryContainer
            else MaterialTheme.colorScheme.surfaceContainer
        ),
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onSelect),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            RadioButton(selected = selected, onClick = onSelect)
            Column(modifier = Modifier.weight(1f).padding(start = 4.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium)
                if (subtitle.isNotBlank()) {
                    Text(
                        subtitle,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            if (onDelete != null) {
                IconButton(onClick = onDelete) {
                    Icon(Icons.Filled.Delete, contentDescription = stringResource(R.string.sygi_themes_delete))
                }
            }
        }
    }
}
