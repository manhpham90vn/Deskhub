package com.deskhub.app

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.addPathNodes
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val TAG = "Deskhub"

class MainActivity : ComponentActivity() {
    private var pendingShare: HostService.ShareRequest? = null

    private val projectionConsent =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            val consent = result.data
            val request = pendingShare
            pendingShare = null
            if (result.resultCode != RESULT_OK || consent == null || request == null) {
                NativeHost.reportFailure("")
                return@registerForActivityResult
            }
            HostService.start(this, result.resultCode, consent, request)
        }

    private val notificationConsent =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    private val audioConsent =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            val request = pendingShare
            if (!granted) {
                Log.i(TAG, "[audio] evt=capture_skip reason=viewer declined the recording prompt")
            }
            if (request != null) startProjectionConsent(request)
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NativeClient.useAppDataDir(this)
        NativeHost.publishScreenSize(this)
        FilesHost.bind(application)
        askForNotifications()
        val prefs = getSharedPreferences("deskhub", Context.MODE_PRIVATE)
        prefs.edit().remove("passcode").apply()
        val lastAddress = prefs.getString("addr", "").orEmpty()

        val debuggable = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        var startSection = Section.CLIENT
        if (debuggable) {
            startSection = sectionExtra(intent)
            intent?.getStringExtra("addr")?.let { addr ->
                val passcode = intent.getStringExtra("passcode").orEmpty()
                intent.removeExtra("addr")
                openStream(addr, passcode, 0)
            }
        }

        setContent {
            MaterialTheme(colorScheme = DeskhubDarkColors) {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    PairingPrompt()
                    Column(modifier = Modifier.safeDrawingPadding()) {
                        MainScreen(
                            initialSection = startSection,
                            initialAddress = lastAddress,
                            initialPasscode = NativeClient.recentPasscode(lastAddress),
                            onRemember = { addr, _ ->
                                prefs.edit().putString("addr", addr).apply()
                            },
                            onOpenStream = ::openStream,
                            onOpenShell = ::openShell,
                            onStartSharing = ::requestSharing,
                            onStopSharing = { HostService.stop(this@MainActivity) },
                        )
                    }
                }
            }
        }
    }

    private fun askForNotifications() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        notificationConsent.launch(Manifest.permission.POST_NOTIFICATIONS)
    }

    private fun requestSharing(request: HostService.ShareRequest) {
        pendingShare = request
        NativeHost.awaitStart()
        if (AudioShare.isSupported && !AudioShare.permissionGranted(this)) {
            audioConsent.launch(Manifest.permission.RECORD_AUDIO)
            return
        }
        startProjectionConsent(request)
    }

    private fun startProjectionConsent(request: HostService.ShareRequest) {
        val manager = getSystemService(MediaProjectionManager::class.java) ?: return
        pendingShare = request
        projectionConsent.launch(manager.createScreenCaptureIntent())
    }

    private fun openStream(
        addr: String,
        passcode: String,
        sourceId: Int,
        sources: List<NativeClient.Source> = emptyList(),
    ) {
        startActivity(
            Intent(this, StreamActivity::class.java)
                .putExtra("addr", addr)
                .putExtra("passcode", passcode)
                .putExtra("source", sourceId)
                .putExtra("srcIds", sources.map { it.id }.toIntArray())
                .putExtra("srcDisplayNames", sources.map { it.displayName }.toTypedArray())
                .putExtra("srcSizeLabels", sources.map { it.sizeLabel }.toTypedArray()),
        )
    }

    private fun openShell(
        addr: String,
        passcode: String,
    ) {
        startActivity(
            Intent(this, TerminalActivity::class.java)
                .putExtra("addr", addr)
                .putExtra("passcode", passcode),
        )
    }
}

private const val POLL_INTERVAL_MS = 1000L
private const val PORT_SETTLE_MS = 600L

private val AccentColor = Color(0xFF2563EB)
private val HeadingColor = Color(0xFFE5E7EB)
private val MutedColor = Color(0xFF9CA3AF)
private val OnlineColor = Color(0xFF4ADE80)
private val OfflineColor = Color(0xFFF87171)

private val DeskhubDarkColors =
    darkColorScheme(
        primary = AccentColor,
        onPrimary = Color.White,
        background = Color.Black,
        surface = Color.Black,
    )

private fun tabIcon(pathData: String): ImageVector =
    ImageVector
        .Builder(
            name = "tab",
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).addPath(pathData = addPathNodes(pathData), fill = SolidColor(Color.White))
        .build()

private val ClientTabIcon =
    tabIcon(
        "M21,2L3,2c-1.1,0 -2,0.9 -2,2v12c0,1.1 0.9,2 2,2h7v2L8,20v2h8v-2l-2,-2v-2h7c1.1,0 2,-0.9 " +
            "2,-2L23,4c0,-1.1 -0.9,-2 -2,-2zM21,16L3,16L3,4h18v12z",
    )

private val HostTabIcon =
    tabIcon(
        "M16,1L4,1c-1.1,0 -2,0.9 -2,2v14h2L4,3h12L16,1zM19,5L8,5c-1.1,0 -2,0.9 -2,2v14c0,1.1 " +
            "0.9,2 2,2h11c1.1,0 2,-0.9 2,-2L21,7c0,-1.1 -0.9,-2 -2,-2zM19,21L8,21L8,7h11v14z",
    )

private val DevicesTabIcon =
    tabIcon(
        "M12,1L3,5v6c0,5.55 3.84,10.74 9,12 5.16,-1.26 9,-6.45 9,-12L21,5l-9,-4zM10,17l-4,-4 " +
            "1.41,-1.41L10,14.17l6.59,-6.59L18,9l-8,8z",
    )

private val SettingsTabIcon =
    tabIcon(
        "M19.14,12.94c0.04,-0.3 0.06,-0.61 0.06,-0.94c0,-0.32 -0.02,-0.64 -0.07,-0.94l2.03," +
            "-1.58c0.18,-0.14 0.23,-0.41 0.12,-0.61l-1.92,-3.32c-0.12,-0.22 -0.37,-0.29 -0.59," +
            "-0.22l-2.39,0.96c-0.5,-0.38 -1.03,-0.7 -1.62,-0.94L14.4,2.81c-0.04,-0.24 -0.24," +
            "-0.41 -0.48,-0.41h-3.84c-0.24,0 -0.43,0.17 -0.47,0.41L9.25,5.35C8.66,5.59 8.12," +
            "5.92 7.63,6.29L5.24,5.33c-0.22,-0.08 -0.47,0 -0.59,0.22L2.74,8.87C2.62,9.08 2.66," +
            "9.34 2.86,9.48l2.03,1.58C4.84,11.36 4.8,11.69 4.8,12s0.02,0.64 0.07,0.94l-2.03," +
            "1.58c-0.18,0.14 -0.23,0.41 -0.12,0.61l1.92,3.32c0.12,0.22 0.37,0.29 0.59,0.22l2.39," +
            "-0.96c0.5,0.38 1.03,0.7 1.62,0.94l0.36,2.54c0.05,0.24 0.24,0.41 0.48,0.41h3.84c0.24," +
            "0 0.44,-0.17 0.47,-0.41l0.36,-2.54c0.59,-0.24 1.13,-0.56 1.62,-0.94l2.39,0.96c0.22," +
            "0.08 0.47,0 0.59,-0.22l1.92,-3.32c0.12,-0.22 0.07,-0.47 -0.12,-0.61L19.14,12.94zM12," +
            "15.6c-1.98,0 -3.6,-1.62 -3.6,-3.6s1.62,-3.6 3.6,-3.6s3.6,1.62 3.6,3.6S13.98,15.6 " +
            "12,15.6z",
    )

@Composable
private fun Heading(
    text: String,
    modifier: Modifier = Modifier,
) {
    Text(
        text,
        modifier = modifier,
        style = MaterialTheme.typography.titleLarge,
        fontWeight = FontWeight.Bold,
        color = HeadingColor,
    )
}

@Composable
private fun SectionLabel(text: String) {
    Text(
        text,
        style = MaterialTheme.typography.titleMedium,
        fontWeight = FontWeight.Bold,
        color = HeadingColor,
    )
}

@Composable
private fun HeadingRow(
    text: String,
    onRefresh: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Heading(text, modifier = Modifier.weight(1f))
        TextButton(onClick = onRefresh) {
            Text(NativeClient.string(NativeClient.STR_REFRESH_NOW))
        }
    }
}

private enum class Section(
    val labelId: Int,
    val icon: ImageVector,
) {
    CLIENT(NativeClient.STR_SIDEBAR_CLIENT, ClientTabIcon),
    HOST(NativeClient.STR_SIDEBAR_HOST, HostTabIcon),
    DEVICES(NativeClient.STR_SIDEBAR_DEVICES, DevicesTabIcon),
    SETTINGS(NativeClient.STR_SIDEBAR_SETTINGS, SettingsTabIcon),
}

private fun sectionExtra(intent: Intent?): Section {
    val name = intent?.getStringExtra("section") ?: return Section.CLIENT
    return Section.entries.firstOrNull { it.name.equals(name, ignoreCase = true) } ?: Section.CLIENT
}

private sealed interface Step {
    data object Address : Step

    data class Querying(
        val seq: Long,
    ) : Step

    data class Picking(
        val sources: List<NativeClient.Source>,
    ) : Step
}

@Composable
private fun MainScreen(
    initialSection: Section,
    initialAddress: String,
    initialPasscode: String,
    onRemember: (String, String) -> Unit,
    onOpenStream: (String, String, Int, List<NativeClient.Source>) -> Unit,
    onOpenShell: (String, String) -> Unit,
    onStartSharing: (HostService.ShareRequest) -> Unit,
    onStopSharing: () -> Unit,
) {
    var step by remember { mutableStateOf<Step>(Step.Address) }
    var address by remember { mutableStateOf(NativeClient.addressHost(initialAddress)) }
    var connectPort by remember { mutableStateOf(portFieldText(initialAddress)) }
    var passcode by remember { mutableStateOf(initialPasscode) }
    var deviceName by remember {
        mutableStateOf(NativeClient.deviceName().ifBlank { Build.MODEL.orEmpty() })
    }
    var connectError by remember { mutableStateOf("") }
    var authed by remember { mutableStateOf<NativeClient.HostQuery?>(null) }
    var authedAddr by remember { mutableStateOf("") }
    var authedCode by remember { mutableStateOf("") }
    var querySeq by remember { mutableStateOf(0L) }
    var deviceRows by remember { mutableStateOf(emptyList<NativeClient.DeviceRow>()) }
    var scanStatus by remember { mutableStateOf("") }
    var pendingPick by remember { mutableStateOf<PendingPick?>(null) }
    var sendingTo by remember { mutableStateOf<FileSendDriver?>(null) }
    var section by remember { mutableStateOf(initialSection) }
    var port by remember { mutableStateOf(NativeClient.settingsPort()) }
    val scope = rememberCoroutineScope()
    val rescanTicks = remember { NativeClient.rescanSeconds() }

    BackHandler(enabled = step != Step.Address) { step = Step.Address }

    DisposableEffect(Unit) {
        onDispose { NativeClient.scanCancel() }
    }

    LaunchedEffect(port) {
        NativeClient.watchRecent()
        NativeClient.scanRestart(port)
        var idleTicks = 0
        while (true) {
            deviceRows = NativeClient.deviceRows()
            scanStatus = NativeClient.scanStatusText(port)
            if (NativeClient.scanRunning()) {
                idleTicks = 0
            } else {
                idleTicks++
                if (idleTicks >= rescanTicks) {
                    idleTicks = 0
                    NativeClient.scanStart(port)
                }
            }
            delay(POLL_INTERVAL_MS)
        }
    }

    val connect: (String) -> Unit = connectLambda@{ addr ->
        if (!NativeClient.parseAddress(addr)) {
            connectError = NativeClient.string(NativeClient.STR_INVALID_ADDRESS_HINT)
            return@connectLambda
        }
        val code = passcode.trim()
        if (code.isNotEmpty() && !NativeClient.isValidPasscode(code)) {
            connectError = NativeClient.string(NativeClient.STR_PASSCODE_INVALID)
            return@connectLambda
        }
        connectError = ""
        authed = null
        deviceName = deviceName.trim().ifBlank { Build.MODEL.orEmpty() }
        NativeClient.setDeviceName(deviceName)
        val mine = Step.Querying(++querySeq)
        step = mine
        scope.launch {
            val queried = NativeClient.queryHost(addr, code)
            if (step != mine) return@launch
            step = Step.Address
            if (queried == null) {
                connectError = NativeClient.sourceQueryFailed(addr)
                return@launch
            }
            onRemember(addr, code)
            NativeClient.recentTouch(addr, code)
            NativeClient.watchRecent()
            deviceRows = NativeClient.deviceRows()
            authed = queried
            authedAddr = addr
            authedCode = code
        }
    }

    val openDesktop: () -> Unit = {
        authed?.takeIf { it.sources.isNotEmpty() }?.let { query ->
            val decision = NativeClient.connectDecision(query.sources)
            if (decision >= 0) {
                onOpenStream(authedAddr, authedCode, decision, query.sources)
            } else {
                step = Step.Picking(query.sources)
            }
        }
    }

    val openShell: () -> Unit = {
        if (authed?.terminal == true) onOpenShell(authedAddr, authedCode)
    }

    val openFileSend: () -> Unit = {
        if (authed?.files == true) {
            sendingTo = StandaloneFileSendDriver(authedAddr, authedCode, deviceName)
        }
    }

    val disconnect: () -> Unit = {
        authed = null
        connectError = ""
    }

    val pickDevice: (String, String) -> Unit = { addr, code ->
        connectError = ""
        pendingPick = PendingPick(addr, code)
    }

    if (step is Step.Querying) {
        AlertDialog(
            onDismissRequest = { step = Step.Address },
            text = {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    CircularProgressIndicator(modifier = Modifier.size(24.dp), strokeWidth = 2.dp)
                    Text(NativeClient.string(NativeClient.STR_QUERYING_SOURCES))
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = { step = Step.Address }) { Text("Cancel") }
            },
        )
    } else if (connectError.isNotEmpty()) {
        AlertDialog(
            onDismissRequest = { connectError = "" },
            title = { Text("Deskhub") },
            text = { Text(connectError) },
            confirmButton = {
                TextButton(onClick = { connectError = "" }) { Text("OK") }
            },
        )
    }

    val sending = sendingTo
    if (sending != null) {
        FileSendScreen(
            driver = sending,
            subtitle = authedAddr,
            onClose = { sendingTo = null },
        )
        return
    }

    when (val s = step) {
        is Step.Address, is Step.Querying ->
            HomeScreen(
                section = section,
                onSectionChange = { section = it },
                address = address,
                onAddressChange = {
                    address = it
                    authed = null
                },
                connectPort = connectPort,
                onConnectPortChange = {
                    connectPort = it
                    authed = null
                },
                passcode = passcode,
                onPasscodeChange = {
                    passcode = it
                    authed = null
                },
                deviceName = deviceName,
                onDeviceNameChange = { deviceName = it },
                busy = step is Step.Querying,
                authed = authed,
                authedAddr = authedAddr,
                onConnect = connect,
                onDisconnect = disconnect,
                onOpenDesktop = openDesktop,
                onOpenShell = openShell,
                onOpenFileSend = openFileSend,
                deviceRows = deviceRows,
                scanStatus = scanStatus,
                onPickDevice = pickDevice,
                onRescan = { scope.launch { NativeClient.scanRestart(port) } },
                onRefreshStatus = { scope.launch { NativeClient.statusRefreshNow() } },
                port = port,
                onPortChange = { chosen ->
                    NativeClient.setSettingsPort(chosen)
                    port = chosen
                },
                onStartSharing = onStartSharing,
                onStopSharing = onStopSharing,
            )

        is Step.Picking ->
            SourcePickerScreen(
                address = authedAddr,
                sources = s.sources,
                onPick = { source ->
                    step = Step.Address
                    onOpenStream(authedAddr, authedCode, source.id, s.sources)
                },
            )
    }

    pendingPick?.let { pick ->
        PasscodeDialog(
            addr = pick.addr,
            initial = pick.passcode,
            onDismiss = { pendingPick = null },
            onConfirm = { chosenAddr, code ->
                pendingPick = null
                address = NativeClient.addressHost(chosenAddr)
                connectPort = portFieldText(chosenAddr)
                passcode = code
                connect(chosenAddr)
            },
        )
    }
}

private data class PendingPick(
    val addr: String,
    val passcode: String,
)

private fun portFieldText(addr: String): String {
    val explicit = NativeClient.addressPort(addr)
    val port = if (explicit != 0) explicit else NativeClient.defaultPort()
    return port.toString()
}

private fun copyToClipboard(
    context: Context,
    text: String,
) {
    val clipboard =
        context.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager ?: return
    clipboard.setPrimaryClip(ClipData.newPlainText("Deskhub", text))
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
        Toast.makeText(context, "Copied", Toast.LENGTH_SHORT).show()
    }
}

@Composable
private fun PasscodeDialog(
    addr: String,
    initial: String,
    onDismiss: () -> Unit,
    onConfirm: (String, String) -> Unit,
) {
    val host = NativeClient.addressHost(addr)
    var typed by remember(addr, initial) { mutableStateOf(initial) }
    var typedPort by remember(addr) { mutableStateOf(portFieldText(addr)) }
    val ready = typed.trim().isEmpty() || NativeClient.isValidPasscode(typed.trim())
    val confirm = {
        if (ready) onConfirm(NativeClient.composeAddress(host, typedPort), typed.trim())
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(NativeClient.string(NativeClient.STR_CONNECT_PROMPT_TITLE)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(host, style = MaterialTheme.typography.titleMedium)
                OutlinedTextField(
                    value = typedPort,
                    onValueChange = { entered ->
                        typedPort = entered.filter { it.isDigit() }.take(5)
                    },
                    label = { Text(NativeClient.string(NativeClient.STR_UDP_PORT_LABEL)) },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                )
                OutlinedTextField(
                    value = typed,
                    onValueChange = { entered ->
                        typed =
                            entered.filter { it.isDigit() }.take(NativeClient.passcodeDigits())
                    },
                    label = { Text(NativeClient.string(NativeClient.STR_CLIENT_PASSCODE_PROMPT)) },
                    supportingText = {
                        Text(NativeClient.string(NativeClient.STR_CLIENT_PASSCODE_HINT))
                    },
                    singleLine = true,
                    keyboardOptions =
                        KeyboardOptions(
                            keyboardType = KeyboardType.NumberPassword,
                            imeAction = ImeAction.Go,
                        ),
                    keyboardActions = KeyboardActions(onGo = { confirm() }),
                )
            }
        },
        confirmButton = {
            TextButton(onClick = confirm, enabled = ready) {
                Text(NativeClient.string(NativeClient.STR_CONNECT_BUTTON))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}

@Composable
private fun HomeScreen(
    section: Section,
    onSectionChange: (Section) -> Unit,
    address: String,
    onAddressChange: (String) -> Unit,
    connectPort: String,
    onConnectPortChange: (String) -> Unit,
    passcode: String,
    onPasscodeChange: (String) -> Unit,
    deviceName: String,
    onDeviceNameChange: (String) -> Unit,
    busy: Boolean,
    authed: NativeClient.HostQuery?,
    authedAddr: String,
    onConnect: (String) -> Unit,
    onDisconnect: () -> Unit,
    onOpenDesktop: () -> Unit,
    onOpenShell: () -> Unit,
    onOpenFileSend: () -> Unit,
    deviceRows: List<NativeClient.DeviceRow>,
    scanStatus: String,
    onPickDevice: (String, String) -> Unit,
    onRescan: () -> Unit,
    onRefreshStatus: () -> Unit,
    port: Int,
    onPortChange: (Int) -> Unit,
    onStartSharing: (HostService.ShareRequest) -> Unit,
    onStopSharing: () -> Unit,
) {
    Column(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier.weight(1f)) {
            when (section) {
                Section.CLIENT ->
                    AddressScreen(
                        address = address,
                        onAddressChange = onAddressChange,
                        connectPort = connectPort,
                        onConnectPortChange = onConnectPortChange,
                        passcode = passcode,
                        onPasscodeChange = onPasscodeChange,
                        deviceName = deviceName,
                        onDeviceNameChange = onDeviceNameChange,
                        busy = busy,
                        authed = authed,
                        authedAddr = authedAddr,
                        onConnect = onConnect,
                        onDisconnect = onDisconnect,
                        onOpenDesktop = onOpenDesktop,
                        onOpenShell = onOpenShell,
                        onOpenFileSend = onOpenFileSend,
                        deviceRows = deviceRows,
                        scanStatus = scanStatus,
                        onPickDevice = onPickDevice,
                        onRescan = onRescan,
                        onRefreshStatus = onRefreshStatus,
                    )

                Section.HOST ->
                    HostScreen(
                        port = port,
                        onStartSharing = onStartSharing,
                        onStopSharing = onStopSharing,
                    )

                Section.DEVICES -> DevicesScreen()

                Section.SETTINGS -> SettingsScreen(port = port, onPortChange = onPortChange)
            }
        }

        NavigationBar {
            Section.entries.forEach { tab ->
                NavigationBarItem(
                    selected = section == tab,
                    onClick = { onSectionChange(tab) },
                    icon = { Icon(tab.icon, contentDescription = null) },
                    label = { Text(NativeClient.string(tab.labelId)) },
                    colors =
                        NavigationBarItemDefaults.colors(
                            selectedIconColor = AccentColor,
                            selectedTextColor = AccentColor,
                            unselectedIconColor = MutedColor,
                            unselectedTextColor = MutedColor,
                            indicatorColor = Color.Transparent,
                        ),
                )
            }
        }
    }
}

@Composable
private fun HostScreen(
    port: Int,
    onStartSharing: (HostService.ShareRequest) -> Unit,
    onStopSharing: () -> Unit,
) {
    var passcode by remember { mutableStateOf(NativeHost.passcode()) }
    var state by remember { mutableStateOf(NativeHost.shareState) }
    var error by remember { mutableStateOf(NativeHost.shareError) }
    var rows by remember { mutableStateOf(emptyList<NativeHost.HostRow>()) }
    var addresses by remember { mutableStateOf(NativeHost.localAddresses()) }

    LaunchedEffect(Unit) {
        while (true) {
            state = NativeHost.shareState
            error = NativeHost.shareError
            rows = if (state == NativeHost.ShareState.SHARING) NativeHost.hostRows() else emptyList()
            addresses = NativeHost.localAddresses()
            if (state == NativeHost.ShareState.SHARING && !NativeHost.isRunning()) onStopSharing()
            delay(POLL_INTERVAL_MS)
        }
    }

    val sharing = state == NativeHost.ShareState.SHARING
    val starting = state == NativeHost.ShareState.STARTING
    val trimmedCode = passcode.trim()
    val ready = trimmedCode.isEmpty() || NativeClient.isValidPasscode(trimmedCode)

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Heading(NativeClient.string(NativeClient.STR_HOST_HEADING))

        if (!NativeHost.isSupported) {
            Text(
                NativeClient.string(NativeClient.STR_SHARE_START_FAILED),
                color = MaterialTheme.colorScheme.error,
            )
            return@Column
        }

        var receiving by remember { mutableStateOf(NativeHost.filesActive()) }
        LaunchedEffect(Unit) {
            while (true) {
                receiving = NativeHost.filesActive()
                delay(POLL_INTERVAL_MS)
            }
        }
        val live = sharing || receiving

        Text(
            NativeClient.string(
                if (live) NativeClient.STR_SHARE_STATE_ON else NativeClient.STR_SHARE_STATE_OFF,
            ),
            style = MaterialTheme.typography.titleMedium,
            color = if (live) OnlineColor else MutedColor,
        )

        OutlinedTextField(
            value = passcode,
            onValueChange = { typed ->
                passcode = typed.filter { it.isDigit() }.take(NativeClient.passcodeDigits())
            },
            label = { Text(NativeClient.string(NativeClient.STR_PASSCODE_LABEL)) },
            singleLine = true,
            enabled = !sharing && !starting,
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
        )

        var bindIp by remember { mutableStateOf(NativeHost.bindIp()) }
        var bindMenuOpen by remember { mutableStateOf(false) }
        val bindStale = bindIp.isNotEmpty() && addresses.none { it.ip == bindIp }
        val bindLabel =
            when {
                bindIp.isEmpty() -> NativeClient.string(NativeClient.STR_BIND_ALL_INTERFACES)
                bindStale ->
                    "$bindIp (${NativeClient.string(NativeClient.STR_BIND_NOT_CONNECTED)})"
                else -> bindIp
            }
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                NativeClient.string(NativeClient.STR_BIND_INTERFACE_LABEL),
                modifier = Modifier.weight(1f),
            )
            Box {
                TextButton(
                    onClick = { bindMenuOpen = true },
                    enabled = !sharing && !starting,
                ) { Text(bindLabel) }
                DropdownMenu(
                    expanded = bindMenuOpen,
                    onDismissRequest = { bindMenuOpen = false },
                ) {
                    DropdownMenuItem(
                        text = {
                            Text(NativeClient.string(NativeClient.STR_BIND_ALL_INTERFACES))
                        },
                        onClick = {
                            bindIp = ""
                            NativeHost.setBindIp("")
                            bindMenuOpen = false
                        },
                    )
                    addresses.forEach { address ->
                        DropdownMenuItem(
                            text = { Text("${address.ip}  (${address.name})") },
                            onClick = {
                                bindIp = address.ip
                                NativeHost.setBindIp(address.ip)
                                bindMenuOpen = false
                            },
                        )
                    }
                }
            }
        }

        Button(
            onClick = {
                if (sharing) {
                    onStopSharing()
                    return@Button
                }
                val trimmed = passcode.trim()
                NativeHost.savePasscode(trimmed)
                val defaults = NativeHost.shareDefaults()
                onStartSharing(
                    HostService.ShareRequest(
                        fps = defaults.fps,
                        bitrateMbps = defaults.bitrateMbps,
                        maxDim = defaults.maxDim,
                        port = port,
                        passcode = trimmed,
                    ),
                )
            },
            enabled = sharing || (ready && !starting),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                NativeClient.string(
                    when {
                        sharing -> NativeClient.STR_STOP_SHARING
                        starting -> NativeClient.STR_STARTING_SHARE
                        else -> NativeClient.STR_START_SHARING
                    },
                ),
            )
        }

        Text(
            if (sharing || receiving) {
                NativeHost.sharingStatus(port, passcode.trim(), sharing)
            } else {
                NativeHost.idleStatus(port)
            },
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )

        if (!ready) {
            Text(
                NativeClient.string(NativeClient.STR_PASSCODE_INVALID),
                color = MaterialTheme.colorScheme.error,
            )
        }

        if (error.isNotEmpty()) {
            Text(error, color = MaterialTheme.colorScheme.error)
        }

        Heading(NativeClient.string(NativeClient.STR_HOST_IP_INTRO))
        if (addresses.isEmpty()) {
            Text(
                NativeClient.string(NativeClient.STR_NO_NETWORK_ADDRESS),
                style = MaterialTheme.typography.bodyMedium,
                color = MutedColor,
            )
        } else {
            val context = LocalContext.current
            for (address in addresses.filter { bindIp.isEmpty() || it.ip == bindIp }) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(address.name, modifier = Modifier.weight(1f), color = MutedColor)
                    Text(address.ip, fontWeight = FontWeight.Bold, color = HeadingColor)
                    TextButton(onClick = { copyToClipboard(context, address.ip) }) {
                        Text("Copy")
                    }
                }
            }
        }

        Text(
            NativeClient.string(NativeClient.STR_SHARING_CONNECT_HINT),
            style = MaterialTheme.typography.bodySmall,
            color = MutedColor,
        )

        HostRowList(rows = rows, sharing = sharing)
    }
}

@Composable
private fun HostRowList(
    rows: List<NativeHost.HostRow>,
    sharing: Boolean,
) {
    if (!sharing || rows.isEmpty()) {
        Text(
            NativeClient.string(
                if (sharing) NativeClient.STR_NOTHING_SHARED else NativeClient.STR_NOT_SHARING,
            ),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )
        return
    }

    for (row in rows) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    if (row.viewer) row.client else row.source,
                    color = if (row.online) OnlineColor else HeadingColor,
                )
                Text(
                    if (row.viewer) "${row.rtt}  ${row.mbps}" else "${row.size}  ${row.viewers}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MutedColor,
                )
            }
            if (row.viewer) {
                TextButton(onClick = { NativeHost.kickViewer(row.sourceId, row.viewerAddr) }) {
                    Text(NativeClient.string(NativeClient.STR_DISCONNECT_VIEWER_ACTION))
                }
            }
        }
    }
}

@Composable
private fun DevicesScreen() {
    var devices by remember { mutableStateOf(NativeClient.pairedDevices()) }
    var allowPairing by remember { mutableStateOf(NativeClient.allowPairing()) }
    var confirmForgetAll by remember { mutableStateOf(false) }
    val dateText: (Long) -> String = { unix ->
        if (unix <= 0) {
            "-"
        } else {
            java.text
                .SimpleDateFormat("yyyy-MM-dd HH:mm", java.util.Locale.US)
                .format(java.util.Date(unix * 1000))
        }
    }

    if (confirmForgetAll) {
        AlertDialog(
            onDismissRequest = { confirmForgetAll = false },
            title = { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) },
            text = { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL_PROMPT)) },
            confirmButton = {
                TextButton(
                    onClick = {
                        NativeClient.pairedForgetAll()
                        devices = NativeClient.pairedDevices()
                        confirmForgetAll = false
                    },
                ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) }
            },
            dismissButton = {
                TextButton(onClick = { confirmForgetAll = false }) { Text("Cancel") }
            },
        )
    }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Heading(NativeClient.string(NativeClient.STR_PAIRED_HEADING))
        Text(
            NativeClient.string(NativeClient.STR_PAIRED_HINT),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )

        if (devices.isEmpty()) {
            Text(
                NativeClient.string(NativeClient.STR_PAIRED_EMPTY),
                style = MaterialTheme.typography.bodyMedium,
                color = MutedColor,
            )
        } else {
            for (device in devices) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            device.name.ifBlank { "(unnamed)" },
                            color = HeadingColor,
                        )
                        Text(
                            "${device.shortKey}  ·  ${dateText(device.pairedUnix)}  ·  " +
                                dateText(device.lastSeenUnix),
                            style = MaterialTheme.typography.bodySmall,
                            color = MutedColor,
                        )
                    }
                    TextButton(
                        onClick = {
                            NativeClient.pairedForget(device.fingerprint)
                            devices = NativeClient.pairedDevices()
                        },
                    ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET)) }
                }
            }
        }

        TextButton(
            onClick = { confirmForgetAll = true },
            enabled = devices.isNotEmpty(),
        ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) }

        SwitchRow(
            label = NativeClient.string(NativeClient.STR_ALLOW_PAIRING_LABEL),
            checked = allowPairing,
        ) {
            allowPairing = it
            NativeClient.setAllowPairing(it)
        }
        Text(
            NativeClient.string(NativeClient.STR_ALLOW_PAIRING_HINT),
            style = MaterialTheme.typography.bodySmall,
            color = MutedColor,
        )

        SectionLabel(NativeClient.string(NativeClient.STR_THIS_MACHINE_HEADING))
        Text(
            NativeClient.ownFingerprint(),
            style = MaterialTheme.typography.bodySmall,
            color = HeadingColor,
        )
        Text(
            NativeClient.string(NativeClient.STR_THIS_MACHINE_HINT),
            style = MaterialTheme.typography.bodySmall,
            color = MutedColor,
        )
    }
}

@Composable
private fun SettingsScreen(
    port: Int,
    onPortChange: (Int) -> Unit,
) {
    var typed by remember(port) { mutableStateOf(port.toString()) }

    LaunchedEffect(typed) {
        val chosen = typed.toIntOrNull()
        if (chosen == null || chosen !in 1..65535 || chosen == port) return@LaunchedEffect
        delay(PORT_SETTLE_MS)
        onPortChange(chosen)
    }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Heading(NativeClient.string(NativeClient.STR_CLIENT_SETTINGS_HEADING))
        Text(
            NativeClient.string(NativeClient.STR_CLIENT_SETTINGS_HINT),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )

        SectionLabel(NativeClient.string(NativeClient.STR_SECTION_CONNECTION))
        OutlinedTextField(
            value = typed,
            onValueChange = { entered -> typed = entered.filter { it.isDigit() }.take(5) },
            label = { Text(NativeClient.string(NativeClient.STR_UDP_PORT_LABEL)) },
            supportingText = { Text(NativeClient.udpPortLine(port)) },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        )

        SectionLabel(NativeClient.string(NativeClient.STR_SECTION_SESSION))
        var clipboardSync by remember { mutableStateOf(NativeClient.clipboardSync()) }
        SwitchRow(
            label = NativeClient.string(NativeClient.STR_CLIPBOARD_SYNC_LABEL),
            checked = clipboardSync,
        ) {
            clipboardSync = it
            NativeClient.setClipboardSync(it)
        }
        var shareAudio by remember { mutableStateOf(NativeClient.shareAudio()) }
        SwitchRow(
            label = NativeClient.string(NativeClient.STR_SHARE_AUDIO_LABEL),
            checked = shareAudio,
        ) {
            shareAudio = it
            NativeClient.setShareAudio(it)
        }
        var playAudio by remember { mutableStateOf(NativeClient.playAudio()) }
        SwitchRow(
            label = NativeClient.string(NativeClient.STR_PLAY_AUDIO_LABEL),
            checked = playAudio,
        ) {
            playAudio = it
            NativeClient.setPlayAudio(it)
        }
        var keepAwake by remember { mutableStateOf(NativeClient.keepAwake()) }
        SwitchRow(
            label = NativeClient.string(NativeClient.STR_KEEP_AWAKE_LABEL),
            checked = keepAwake,
        ) {
            keepAwake = it
            NativeClient.setKeepAwake(it)
        }

        ProjectFooter()
    }
}

@Composable
private fun AddressScreen(
    address: String,
    onAddressChange: (String) -> Unit,
    connectPort: String,
    onConnectPortChange: (String) -> Unit,
    passcode: String,
    onPasscodeChange: (String) -> Unit,
    deviceName: String,
    onDeviceNameChange: (String) -> Unit,
    busy: Boolean,
    authed: NativeClient.HostQuery?,
    authedAddr: String,
    onConnect: (String) -> Unit,
    onDisconnect: () -> Unit,
    onOpenDesktop: () -> Unit,
    onOpenShell: () -> Unit,
    onOpenFileSend: () -> Unit,
    deviceRows: List<NativeClient.DeviceRow>,
    scanStatus: String,
    onPickDevice: (String, String) -> Unit,
    onRescan: () -> Unit,
    onRefreshStatus: () -> Unit,
) {
    val trimmed = address.trim()
    val ready = trimmed.isNotEmpty() && !busy
    val go = { if (ready) onConnect(NativeClient.composeAddress(trimmed, connectPort)) }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Heading(NativeClient.string(NativeClient.STR_CLIENT_HEADING))

        if (authed == null) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                OutlinedTextField(
                    value = address,
                    onValueChange = onAddressChange,
                    label = { Text(NativeClient.string(NativeClient.STR_CLIENT_IP_PROMPT)) },
                    singleLine = true,
                    enabled = !busy,
                    modifier = Modifier.weight(1f),
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Go),
                    keyboardActions = KeyboardActions(onGo = { go() }),
                )

                OutlinedTextField(
                    value = connectPort,
                    onValueChange = { typed ->
                        onConnectPortChange(typed.filter { it.isDigit() }.take(5))
                    },
                    label = { Text(NativeClient.string(NativeClient.STR_UDP_PORT_LABEL)) },
                    singleLine = true,
                    enabled = !busy,
                    modifier = Modifier.width(110.dp),
                    keyboardOptions =
                        KeyboardOptions(
                            keyboardType = KeyboardType.Number,
                            imeAction = ImeAction.Go,
                        ),
                    keyboardActions = KeyboardActions(onGo = { go() }),
                )
            }

            OutlinedTextField(
                value = passcode,
                onValueChange = { typed ->
                    onPasscodeChange(
                        typed.filter { it.isDigit() }.take(NativeClient.passcodeDigits()),
                    )
                },
                label = { Text(NativeClient.string(NativeClient.STR_CLIENT_PASSCODE_PROMPT)) },
                supportingText = {
                    Text(NativeClient.string(NativeClient.STR_CLIENT_PASSCODE_HINT))
                },
                singleLine = true,
                enabled = !busy,
                modifier = Modifier.fillMaxWidth(),
                keyboardOptions =
                    KeyboardOptions(
                        keyboardType = KeyboardType.NumberPassword,
                        imeAction = ImeAction.Go,
                    ),
                keyboardActions = KeyboardActions(onGo = { go() }),
            )

            OutlinedTextField(
                value = deviceName,
                onValueChange = onDeviceNameChange,
                label = { Text("Your name") },
                singleLine = true,
                enabled = !busy,
                modifier = Modifier.fillMaxWidth(),
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Go),
                keyboardActions = KeyboardActions(onGo = { go() }),
            )

            Button(
                onClick = go,
                enabled = ready,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(NativeClient.string(NativeClient.STR_CONNECT_BUTTON)) }
        } else {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(
                    authedAddr,
                    style = MaterialTheme.typography.titleMedium,
                    color = HeadingColor,
                    modifier = Modifier.weight(1f),
                )
                Button(onClick = onDisconnect) {
                    Text(NativeClient.string(NativeClient.STR_DISCONNECT_BUTTON))
                }
            }

            val connectedRow =
                deviceRows.firstOrNull { NativeClient.sameDeviceAddr(it.addr, authedAddr) }
            val liveColor = if (connectedRow?.online == false) OfflineColor else OnlineColor
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Box(
                    modifier =
                        Modifier
                            .size(10.dp)
                            .background(liveColor, CircleShape),
                )
                Text(
                    NativeClient.string(NativeClient.STR_CONNECTED_PICK_SESSION),
                    color = liveColor,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f),
                )
                if (connectedRow != null && connectedRow.ping.isNotEmpty()) {
                    Text(connectedRow.ping, color = liveColor, fontWeight = FontWeight.SemiBold)
                }
            }

            OutlinedButton(
                onClick = onOpenDesktop,
                enabled = authed.sources.isNotEmpty(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text(NativeClient.string(NativeClient.STR_OPEN_DESKTOP_LABEL)) }

            OutlinedButton(
                onClick = onOpenShell,
                enabled = authed.terminal,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(NativeClient.string(NativeClient.STR_OPEN_SHELL_LABEL)) }

            OutlinedButton(
                onClick = onOpenFileSend,
                enabled = authed.files,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(NativeClient.string(NativeClient.STR_OPEN_FILES_LABEL)) }

            var control by remember { mutableStateOf(NativeClient.clientControl()) }
            SwitchRow(
                label = NativeClient.string(NativeClient.STR_REQUEST_CONTROL_LABEL),
                checked = control,
            ) {
                control = it
                NativeClient.setClientControl(it)
            }
        }

        if (authed == null) {
            DeviceSection(
                heading = NativeClient.string(NativeClient.STR_DEVICES_HEADING),
                note = scanStatus,
                rows =
                    deviceRows.map { row ->
                        DeviceRow(
                            row.addr,
                            row.ping,
                            listOf(row.origin, row.status, row.lastConnected)
                                .filter { it.isNotEmpty() }
                                .joinToString("  "),
                            if (row.known) row.online else null,
                        )
                    },
                enabled = !busy,
                onRefresh = {
                    onRefreshStatus()
                    onRescan()
                },
                onPick = { addr ->
                    val known = deviceRows.firstOrNull { it.addr == addr }?.passcode.orEmpty()
                    onPickDevice(addr, known.ifEmpty { NativeClient.recentPasscode(addr) })
                },
            )
        }
    }
}

@Composable
private fun SwitchRow(
    label: String,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(label, modifier = Modifier.weight(1f))
        Switch(checked = checked, onCheckedChange = onCheckedChange, enabled = enabled)
    }
}

@Composable
private fun ProjectFooter() {
    val uriHandler = LocalUriHandler.current
    val url = NativeClient.string(NativeClient.STR_PROJECT_URL)

    Column(
        modifier = Modifier.padding(top = 8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(
            NativeClient.string(NativeClient.STR_PROJECT_LINK_LABEL),
            color = MaterialTheme.colorScheme.primary,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.clickable { uriHandler.openUri(url) },
        )
        Text(
            NativeClient.versionLine(),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

private data class DeviceRow(
    val addr: String,
    val ping: String,
    val detail: String,
    val online: Boolean?,
)

@Composable
private fun DeviceSection(
    heading: String,
    note: String,
    rows: List<DeviceRow>,
    enabled: Boolean,
    onRefresh: () -> Unit,
    onPick: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        HeadingRow(heading, onRefresh)

        for (row in rows) {
            val tint =
                when (row.online) {
                    true -> OnlineColor
                    false -> OfflineColor
                    null -> HeadingColor
                }
            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clickable(enabled = enabled) { onPick(row.addr) }
                        .padding(vertical = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(row.addr, color = tint)
                    if (row.detail.isNotBlank()) {
                        Text(
                            row.detail,
                            style = MaterialTheme.typography.bodySmall,
                            color = MutedColor,
                        )
                    }
                }
                Text(row.ping, style = MaterialTheme.typography.bodySmall, color = tint)
            }
        }

        if (note.isNotEmpty()) {
            Text(note, style = MaterialTheme.typography.bodySmall, color = MutedColor)
        }
    }
}

@Composable
private fun SourcePickerScreen(
    address: String,
    sources: List<NativeClient.Source>,
    onPick: (NativeClient.Source) -> Unit,
) {
    var pickedId by remember { mutableStateOf(sources.first().id) }

    Column(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier =
                Modifier
                    .weight(1f)
                    .verticalScroll(rememberScrollState())
                    .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(text = address, style = MaterialTheme.typography.titleMedium)

            sources.forEach { source ->
                Row(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .clickable { pickedId = source.id }
                            .padding(vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    RadioButton(
                        selected = source.id == pickedId,
                        onClick = { pickedId = source.id },
                    )
                    Column {
                        Text(
                            text = source.displayName,
                            style = MaterialTheme.typography.bodyLarge,
                        )
                        Text(
                            text = source.sizeLabel,
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                }
            }
        }

        Button(
            onClick = { sources.firstOrNull { it.id == pickedId }?.let(onPick) },
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
        ) { Text("Start viewing") }
    }
}
