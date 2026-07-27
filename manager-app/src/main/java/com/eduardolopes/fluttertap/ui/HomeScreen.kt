// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.eduardolopes.fluttertap.R
import com.eduardolopes.fluttertap.data.AppInfo
import com.eduardolopes.fluttertap.data.AppRepository
import com.eduardolopes.fluttertap.data.ConfigData
import com.eduardolopes.fluttertap.data.RootBackend
import com.eduardolopes.fluttertap.data.RootManager
import com.eduardolopes.fluttertap.data.RootStatus
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(onLanguageSelected: (String?) -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val snackbarHostState = remember { SnackbarHostState() }

    var status by remember { mutableStateOf<RootStatus?>(null) }
    var config by remember { mutableStateOf(ConfigData.default()) }
    var launchableApps by remember { mutableStateOf<List<AppInfo>>(emptyList()) }
    var systemOnlyApps by remember { mutableStateOf<List<AppInfo>>(emptyList()) }
    var showSystemApps by rememberSaveable { mutableStateOf(false) }
    var loading by remember { mutableStateOf(true) }

    LaunchedEffect(Unit) {
        val loadedLaunchable = withContext(Dispatchers.Default) { AppRepository.listLaunchableApps(context) }
        val loadedSystemOnly = withContext(Dispatchers.Default) {
            AppRepository.listSystemOnlyApps(context, loadedLaunchable.map { it.packageName }.toSet())
        }
        val loadedStatus = withContext(Dispatchers.IO) { RootManager.queryStatus() }
        val loadedConfig = withContext(Dispatchers.IO) { RootManager.readConfig() }
        launchableApps = loadedLaunchable
        systemOnlyApps = loadedSystemOnly
        status = loadedStatus
        config = loadedConfig
        loading = false
    }

    fun persist(newConfig: ConfigData, notify: Boolean) {
        config = newConfig
        scope.launch {
            val ok = withContext(Dispatchers.IO) { RootManager.writeConfig(newConfig) }
            if (notify) {
                snackbarHostState.showSnackbar(
                    if (ok) context.getString(R.string.proxy_saved_confirmation)
                    else context.getString(R.string.error_root_required)
                )
            }
        }
    }

    Scaffold(
        topBar = { TopAppBar(title = { Text(stringResource(R.string.app_name)) }) },
        snackbarHost = { SnackbarHost(snackbarHostState) },
    ) { padding ->
        if (loading || status == null) {
            Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
            return@Scaffold
        }

        LazyColumn(
            modifier = Modifier.fillMaxSize().padding(padding),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            item { StatusCard(status!!) }
            item { ProxyCard(config = config, onSave = { newConfig -> persist(newConfig, notify = true) }) }
            item { LanguageCard(onLanguageSelected = onLanguageSelected) }
            item {
                AppsCard(
                    apps = if (showSystemApps) launchableApps + systemOnlyApps else launchableApps,
                    selected = config.targetPackages,
                    showSystemApps = showSystemApps,
                    onToggleShowSystemApps = { showSystemApps = it },
                    onToggle = { pkg, checked ->
                        val newSet = if (checked) config.targetPackages + pkg else config.targetPackages - pkg
                        persist(config.copy(targetPackages = newSet), notify = false)
                    },
                )
            }
            item { AboutCard() }
        }
    }
}

@Composable
private fun StatusCard(status: RootStatus) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(R.string.status_title), style = MaterialTheme.typography.titleMedium)
            Text(
                stringResource(if (status.granted) R.string.status_root_granted else R.string.status_root_denied),
                color = if (status.granted) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error,
            )
            if (status.granted) {
                Text(
                    stringResource(
                        when (status.backend) {
                            RootBackend.MAGISK -> R.string.status_backend_magisk
                            RootBackend.KERNELSU -> R.string.status_backend_kernelsu
                            RootBackend.APATCH -> R.string.status_backend_apatch
                            RootBackend.UNKNOWN -> R.string.status_backend_unknown
                        }
                    )
                )
                Text(
                    stringResource(
                        if (status.moduleInstalled) R.string.status_module_installed
                        else R.string.status_module_not_installed
                    )
                )
            }
        }
    }
}

@Composable
private fun ProxyCard(config: ConfigData, onSave: (ConfigData) -> Unit) {
    var enabled by remember { mutableStateOf(config.enabled) }
    var ipText by remember { mutableStateOf(config.proxyIp) }
    var portText by remember { mutableStateOf(config.proxyPort.toString()) }
    var ipError by remember { mutableStateOf(false) }
    var portError by remember { mutableStateOf(false) }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(stringResource(R.string.proxy_title), style = MaterialTheme.typography.titleMedium)
                Switch(checked = enabled, onCheckedChange = { enabled = it })
            }
            Text(stringResource(R.string.proxy_master_switch), style = MaterialTheme.typography.bodySmall)

            OutlinedTextField(
                value = ipText,
                onValueChange = { ipText = it; ipError = false },
                label = { Text(stringResource(R.string.proxy_ip_label)) },
                isError = ipError,
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            if (ipError) {
                Text(
                    stringResource(R.string.proxy_invalid_ip),
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall,
                )
            }

            OutlinedTextField(
                value = portText,
                onValueChange = { portText = it; portError = false },
                label = { Text(stringResource(R.string.proxy_port_label)) },
                isError = portError,
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            if (portError) {
                Text(
                    stringResource(R.string.proxy_invalid_port),
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall,
                )
            }

            TextButton(
                onClick = {
                    val ipValid = isValidIpv4(ipText)
                    val port = portText.toIntOrNull()
                    val portValid = port != null && port in 1..65535
                    ipError = !ipValid
                    portError = !portValid
                    if (ipValid && portValid) {
                        onSave(config.copy(enabled = enabled, proxyIp = ipText, proxyPort = port!!))
                    }
                },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.proxy_save_button))
            }
        }
    }
}

private fun isValidIpv4(value: String): Boolean {
    val parts = value.split(".")
    if (parts.size != 4) return false
    return parts.all { part -> part.toIntOrNull()?.let { it in 0..255 } == true && part.isNotEmpty() }
}

private data class LanguageOption(val tag: String?, val labelRes: Int)

private val LANGUAGE_OPTIONS = listOf(
    LanguageOption(null, R.string.language_system),
    LanguageOption("pt-BR", R.string.language_pt_br),
    LanguageOption("en", R.string.language_en),
    LanguageOption("zh", R.string.language_zh),
)

@Composable
private fun LanguageCard(onLanguageSelected: (String?) -> Unit) {
    var expanded by rememberSaveable { mutableStateOf(false) }
    var selectedIndex by rememberSaveable { mutableStateOf(0) }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.language_title), style = MaterialTheme.typography.titleMedium)
            Box(modifier = Modifier.padding(top = 8.dp)) {
                OutlinedTextField(
                    value = stringResource(LANGUAGE_OPTIONS[selectedIndex].labelRes),
                    onValueChange = {},
                    readOnly = true,
                    trailingIcon = { Icon(Icons.Filled.ArrowDropDown, contentDescription = null) },
                    modifier = Modifier.fillMaxWidth().clickable { expanded = true },
                    enabled = false,
                    colors = OutlinedTextFieldDefaults.colors(
                        disabledTextColor = MaterialTheme.colorScheme.onSurface,
                        disabledBorderColor = MaterialTheme.colorScheme.outline,
                        disabledTrailingIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                    ),
                )
                DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                    LANGUAGE_OPTIONS.forEachIndexed { index, option ->
                        DropdownMenuItem(
                            text = { Text(stringResource(option.labelRes)) },
                            onClick = {
                                selectedIndex = index
                                expanded = false
                                onLanguageSelected(option.tag)
                            },
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun AppsCard(
    apps: List<AppInfo>,
    selected: Set<String>,
    showSystemApps: Boolean,
    onToggleShowSystemApps: (Boolean) -> Unit,
    onToggle: (String, Boolean) -> Unit,
) {
    var query by remember { mutableStateOf("") }
    val filtered = remember(apps, query, selected) {
        val base = if (query.isBlank()) apps else apps.filter { it.label.contains(query, ignoreCase = true) }
        // Selected apps float to the top so the user doesn't have to scroll to find them
        // again; alphabetical order is preserved within each group.
        base.sortedWith(
            compareByDescending<AppInfo> { selected.contains(it.packageName) }
                .thenBy { it.label.lowercase() }
        )
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.apps_title), style = MaterialTheme.typography.titleMedium)
            Text(stringResource(R.string.apps_subtitle), style = MaterialTheme.typography.bodySmall)
            Text(
                stringResource(R.string.apps_selected_count, selected.size),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
            )

            OutlinedTextField(
                value = query,
                onValueChange = { query = it },
                placeholder = { Text(stringResource(R.string.apps_search_hint)) },
                leadingIcon = { Icon(Icons.Filled.Search, contentDescription = null) },
                singleLine = true,
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
            )

            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(stringResource(R.string.apps_show_system), style = MaterialTheme.typography.bodyMedium)
                Switch(checked = showSystemApps, onCheckedChange = onToggleShowSystemApps)
            }

            if (filtered.isEmpty()) {
                Text(
                    stringResource(R.string.apps_empty),
                    modifier = Modifier.padding(top = 16.dp),
                    style = MaterialTheme.typography.bodyMedium,
                )
            } else {
                // Bounded height list inside the card; the outer screen is a
                // LazyColumn already, so this inner list intentionally isn't lazy
                // beyond a reasonable cap to avoid nested-scroll complexity.
                Column(Modifier.padding(top = 8.dp)) {
                    filtered.take(500).forEach { app ->
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            Column(Modifier.padding(vertical = 4.dp).weight(1f)) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(
                                        app.label,
                                        style = MaterialTheme.typography.bodyLarge,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        modifier = Modifier.weight(1f, fill = false),
                                    )
                                    if (app.isSystemApp) {
                                        Text(
                                            stringResource(R.string.apps_system_badge),
                                            style = MaterialTheme.typography.labelSmall,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                            maxLines = 1,
                                            softWrap = false,
                                            modifier = Modifier.padding(start = 6.dp),
                                        )
                                    }
                                }
                                Text(
                                    app.packageName,
                                    style = MaterialTheme.typography.bodySmall,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            Checkbox(
                                checked = selected.contains(app.packageName),
                                onCheckedChange = { checked -> onToggle(app.packageName, checked) },
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun AboutCard() {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(R.string.about_title), style = MaterialTheme.typography.titleMedium)
            Text(stringResource(R.string.about_description), style = MaterialTheme.typography.bodyMedium)
            Text(
                stringResource(R.string.about_version, "1.0.0"),
                style = MaterialTheme.typography.bodySmall,
            )
            Text(
                stringResource(R.string.about_developer),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
            )
        }
    }
}
