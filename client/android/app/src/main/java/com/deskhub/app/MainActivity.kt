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
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
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
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
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
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.addPathNodes
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val TAG = "Deskhub"

class MainActivity : ComponentActivity() {
    private var pendingShare: HostService.ShareRequest? = null
    private var pendingInvite by mutableStateOf<String?>(null)
    private var pendingSection by mutableStateOf<Section?>(null)

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
        val lastAddress = prefs.getString("addr", "").orEmpty()

        val debuggable = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        val startSection = sectionExtra(intent)
        if (debuggable) {
            intent?.getStringExtra("addr")?.let { addr ->
                intent.removeExtra("addr")
                openStream(addr, 0)
            }
        }
        pendingInvite = pairingInviteFrom(intent)

        setContent {
            MaterialTheme(colorScheme = DeskhubDarkColors) {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    Column(modifier = Modifier.safeDrawingPadding()) {
                        MainScreen(
                            initialSection = startSection,
                            initialAddress = lastAddress,
                            invite = pendingInvite,
                            onInviteConsumed = { pendingInvite = null },
                            requestedSection = pendingSection,
                            onSectionConsumed = { pendingSection = null },
                            onRemember = { addr ->
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

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        pairingInviteFrom(intent)?.let { pendingInvite = it }
        if (intent.hasExtra(AccessRequestNotifier.EXTRA_SECTION)) pendingSection = sectionExtra(intent)
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
        sourceId: Int,
        sources: List<NativeClient.Source> = emptyList(),
    ) {
        startActivity(
            Intent(this, StreamActivity::class.java)
                .putExtra("addr", addr)
                .putExtra("source", sourceId)
                .putExtra("srcIds", sources.map { it.id }.toIntArray())
                .putExtra("srcDisplayNames", sources.map { it.displayName }.toTypedArray())
                .putExtra("srcSizeLabels", sources.map { it.sizeLabel }.toTypedArray()),
        )
    }

    private fun openShell(addr: String) {
        startActivity(
            Intent(this, TerminalActivity::class.java)
                .putExtra("addr", addr),
        )
    }
}

private const val POLL_INTERVAL_MS = 1000L
private const val PORT_SETTLE_MS = 600L
private const val UNSEEN_GENERATION = -1L
private const val QR_QUIET_ZONE_MODULES = 4
private const val QR_CELL_OVERLAP_PX = 0.5f
private const val QR_WIDTH_FRACTION = 0.7f

private const val OPAQUE_ALPHA = 0xFF000000.toInt()

private fun darkThemeColor(id: Int) = Color(OPAQUE_ALPHA or NativeClient.themeColor(id, dark = true))

private val AccentColor = darkThemeColor(NativeClient.THEME_ACCENT)
private val HeadingColor = darkThemeColor(NativeClient.THEME_HEADING)
private val MutedColor = darkThemeColor(NativeClient.THEME_MUTED)
private val OnlineColor = darkThemeColor(NativeClient.THEME_ONLINE)
private val PageColor = darkThemeColor(NativeClient.THEME_PAGE)

private val DeskhubDarkColors =
    darkColorScheme(
        primary = AccentColor,
        onPrimary = Color.White,
        background = PageColor,
        surface = PageColor,
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
    val name = intent?.getStringExtra(AccessRequestNotifier.EXTRA_SECTION) ?: return Section.CLIENT
    return Section.entries.firstOrNull { it.name.equals(name, ignoreCase = true) } ?: Section.CLIENT
}

private fun pairingInviteFrom(intent: Intent?): String? {
    if (intent?.action != Intent.ACTION_VIEW) return null
    return intent.dataString?.takeIf { NativeClient.isPairingInvite(it) }
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
    invite: String?,
    onInviteConsumed: () -> Unit,
    requestedSection: Section?,
    onSectionConsumed: () -> Unit,
    onRemember: (String) -> Unit,
    onOpenStream: (String, Int, List<NativeClient.Source>) -> Unit,
    onOpenShell: (String) -> Unit,
    onStartSharing: (HostService.ShareRequest) -> Unit,
    onStopSharing: () -> Unit,
) {
    var step by remember { mutableStateOf<Step>(Step.Address) }
    var address by remember { mutableStateOf(NativeClient.addressHost(initialAddress)) }
    var connectPort by remember { mutableStateOf(portFieldText(initialAddress)) }
    var connectError by remember { mutableStateOf("") }
    var pendingTrust by remember { mutableStateOf<TrustRequest?>(null) }
    var authed by remember { mutableStateOf<NativeClient.HostQuery?>(null) }
    var authedAddr by remember { mutableStateOf("") }
    var querySeq by remember { mutableStateOf(0L) }
    var deviceRows by remember { mutableStateOf(emptyList<NativeClient.DeviceRow>()) }
    var sendingTo by remember { mutableStateOf<FileSendDriver?>(null) }
    var section by remember { mutableStateOf(initialSection) }
    var port by remember { mutableStateOf(NativeClient.settingsPort()) }
    var scanning by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    BackHandler(enabled = step != Step.Address) { step = Step.Address }

    LaunchedEffect(port) {
        while (true) {
            deviceRows = NativeClient.deviceRows()
            delay(POLL_INTERVAL_MS)
        }
    }

    val adoptAddress: (String) -> Unit = { addr ->
        if (addr.isNotBlank()) {
            address = NativeClient.addressHost(addr)
            connectPort = portFieldText(addr)
        }
    }

    val settle: suspend (NativeClient.QueryOutcome, String) -> Unit = { outcome, fallback ->
        val answered = outcome.answeredAddress.ifBlank { fallback }
        when (outcome) {
            is NativeClient.QueryOutcome.Failed -> {
                if (outcome.awaitingApproval) adoptAddress(answered)
                connectError = outcome.reason
            }

            is NativeClient.QueryOutcome.UnknownHost -> pendingTrust = TrustRequest(answered, outcome.fingerprint)
            is NativeClient.QueryOutcome.Reached -> {
                adoptAddress(answered)
                onRemember(answered)
                deviceRows = NativeClient.deviceRows()
                authed = outcome.query
                authedAddr = answered
            }
        }
    }

    val startQuery: (String, suspend () -> NativeClient.QueryOutcome) -> Unit = { fallback, query ->
        connectError = ""
        authed = null
        NativeClient.setDeviceName(NativeClient.sessionDeviceName())
        val mine = Step.Querying(++querySeq)
        step = mine
        scope.launch {
            val outcome = query()
            if (step != mine) return@launch
            step = Step.Address
            settle(outcome, fallback)
        }
    }

    val connectByInvite: (String) -> Unit = inviteLambda@{ invite ->
        val first = NativeClient.pairingInviteAddress(invite)
        if (first.isEmpty()) {
            connectError = NativeClient.string(NativeClient.STR_INVITE_INVALID)
            return@inviteLambda
        }
        adoptAddress(first)
        startQuery(first) { NativeClient.queryHostByInvite(invite) }
    }

    val connect: (String) -> Unit = connectLambda@{ text ->
        val trimmed = text.trim()
        if (NativeClient.isPairingInvite(trimmed)) {
            connectByInvite(trimmed)
            return@connectLambda
        }
        if (!NativeClient.parseAddress(trimmed)) {
            connectError = NativeClient.string(NativeClient.STR_INVALID_ADDRESS_HINT)
            return@connectLambda
        }
        startQuery(trimmed) { NativeClient.queryHost(trimmed) }
    }

    LaunchedEffect(invite) {
        if (invite == null) return@LaunchedEffect
        onInviteConsumed()
        scanning = false
        section = Section.CLIENT
        connect(invite)
    }

    LaunchedEffect(requestedSection) {
        if (requestedSection == null) return@LaunchedEffect
        onSectionConsumed()
        section = requestedSection
    }

    val openDesktop: () -> Unit = {
        authed?.takeIf { it.sources.isNotEmpty() }?.let { query ->
            val decision = NativeClient.connectDecision(query.sources)
            if (decision >= 0) {
                onOpenStream(authedAddr, decision, query.sources)
            } else {
                step = Step.Picking(query.sources)
            }
        }
    }

    val openShell: () -> Unit = {
        if (authed?.terminal == true) onOpenShell(authedAddr)
    }

    val openFileSend: () -> Unit = {
        if (authed?.files == true) {
            sendingTo = StandaloneFileSendDriver(authedAddr, NativeClient.sessionDeviceName())
        }
    }

    val disconnect: () -> Unit = {
        authed = null
        connectError = ""
    }

    val pickDevice: (String) -> Unit = { addr ->
        address = NativeClient.addressHost(addr)
        connectPort = portFieldText(addr)
        connect(NativeClient.composeAddress(address, connectPort))
    }

    val trust = pendingTrust
    if (trust != null) {
        TrustNewHostDialog(
            request = trust,
            onCancel = { pendingTrust = null },
            onTrust = {
                pendingTrust = null
                val failure = NativeClient.trustNewHost(trust.address, trust.fingerprint)
                if (failure != null) {
                    connectError = failure
                } else {
                    connect(trust.address)
                }
            },
        )
    } else if (step is Step.Querying) {
        QueryingDialog(
            onCancel = {
                NativeClient.cancelListSources()
                step = Step.Address
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

    if (scanning) {
        QrScanScreen(
            hint = NativeClient.string(NativeClient.STR_QR_HINT),
            accepts = { NativeClient.isPairingInvite(it) },
            onDecoded = { decoded ->
                scanning = false
                connect(decoded)
            },
            onClose = { scanning = false },
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
                busy = step is Step.Querying,
                authed = authed,
                authedAddr = authedAddr,
                onConnect = connect,
                onScanQr = { scanning = true },
                onDisconnect = disconnect,
                onOpenDesktop = openDesktop,
                onOpenShell = openShell,
                onOpenFileSend = openFileSend,
                deviceRows = deviceRows,
                onPickDevice = pickDevice,
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
                    onOpenStream(authedAddr, source.id, s.sources)
                },
            )
    }
}

private data class TrustRequest(
    val address: String,
    val fingerprint: String,
)

@Composable
private fun QueryingDialog(onCancel: () -> Unit) {
    var status by remember { mutableStateOf(NativeClient.sourceQueryStatus()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(POLL_INTERVAL_MS)
            status = NativeClient.sourceQueryStatus()
        }
    }
    AlertDialog(
        onDismissRequest = onCancel,
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    CircularProgressIndicator(modifier = Modifier.size(24.dp), strokeWidth = 2.dp)
                    Text(NativeClient.string(NativeClient.STR_QUERYING_SOURCES))
                }
                if (status.isNotEmpty()) {
                    Text(status, style = MaterialTheme.typography.bodyMedium, color = MutedColor)
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onCancel) {
                Text(NativeClient.string(NativeClient.STR_CANCEL_ACTION))
            }
        },
    )
}

@Composable
private fun TrustNewHostDialog(
    request: TrustRequest,
    onCancel: () -> Unit,
    onTrust: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text(NativeClient.string(NativeClient.STR_TRUST_NEW_HOST_TITLE)) },
        text = { Text(NativeClient.trustNewHostPrompt(request.address, request.fingerprint)) },
        confirmButton = {
            TextButton(onClick = onTrust) {
                Text(NativeClient.string(NativeClient.STR_TRUST_NEW_HOST_ACTION))
            }
        },
        dismissButton = {
            TextButton(onClick = onCancel) { Text(NativeClient.string(NativeClient.STR_CANCEL_ACTION)) }
        },
    )
}

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
private fun HomeScreen(
    section: Section,
    onSectionChange: (Section) -> Unit,
    address: String,
    onAddressChange: (String) -> Unit,
    connectPort: String,
    onConnectPortChange: (String) -> Unit,
    busy: Boolean,
    authed: NativeClient.HostQuery?,
    authedAddr: String,
    onConnect: (String) -> Unit,
    onScanQr: () -> Unit,
    onDisconnect: () -> Unit,
    onOpenDesktop: () -> Unit,
    onOpenShell: () -> Unit,
    onOpenFileSend: () -> Unit,
    deviceRows: List<NativeClient.DeviceRow>,
    onPickDevice: (String) -> Unit,
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
                        busy = busy,
                        authed = authed,
                        authedAddr = authedAddr,
                        onConnect = onConnect,
                        onScanQr = onScanQr,
                        onDisconnect = onDisconnect,
                        onOpenDesktop = onOpenDesktop,
                        onOpenShell = onOpenShell,
                        onOpenFileSend = onOpenFileSend,
                        deviceRows = deviceRows,
                        onPickDevice = onPickDevice,
                    )

                Section.HOST ->
                    HostScreen(
                        port = port,
                        onStartSharing = onStartSharing,
                        onStopSharing = onStopSharing,
                    )

                Section.DEVICES ->
                    DevicesScreen(
                        onConnectHost = { addr ->
                            onSectionChange(Section.CLIENT)
                            onPickDevice(addr)
                        },
                    )

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
    var state by remember { mutableStateOf(NativeHost.shareState) }
    var error by remember { mutableStateOf(NativeHost.shareError) }
    var rows by remember { mutableStateOf(emptyList<NativeHost.HostRow>()) }
    var addresses by remember { mutableStateOf(NativeHost.localAddresses()) }
    var requests by remember { mutableStateOf(emptyList<NativeClient.AccessRequest>()) }
    var requestsGeneration by remember { mutableStateOf(UNSEEN_GENERATION) }
    var qrInvite by remember { mutableStateOf<String?>(null) }
    val context = LocalContext.current
    val refreshRequests = {
        requestsGeneration = NativeClient.accessRequestsGeneration()
        requests = NativeClient.accessRequests()
        AccessRequestNotifier.announce(context, requests)
    }
    val hideQr = {
        if (qrInvite != null) NativeClient.pairingRevoke()
        qrInvite = null
    }

    LaunchedEffect(Unit) {
        while (true) {
            state = NativeHost.shareState
            error = NativeHost.shareError
            rows = if (state == NativeHost.ShareState.SHARING) NativeHost.hostRows() else emptyList()
            addresses = NativeHost.localAddresses()
            if (NativeClient.accessRequestsGeneration() != requestsGeneration) refreshRequests()
            if (state == NativeHost.ShareState.SHARING && !NativeHost.isRunning()) onStopSharing()
            delay(POLL_INTERVAL_MS)
        }
    }

    val sharing = state == NativeHost.ShareState.SHARING
    val starting = state == NativeHost.ShareState.STARTING
    LaunchedEffect(sharing) { if (!sharing) hideQr() }
    val latestHideQr by rememberUpdatedState(hideQr)
    DisposableEffect(Unit) { onDispose { latestHideQr() } }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Heading(NativeClient.string(NativeClient.STR_SIDEBAR_HOST))

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

        if (sharing) {
            OutlinedButton(
                onClick = {
                    if (qrInvite != null) {
                        hideQr()
                    } else {
                        qrInvite = NativeClient.pairingInvite(port, bindIp)
                    }
                },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(
                    NativeClient.string(
                        if (qrInvite != null) NativeClient.STR_HIDE_QR_ACTION else NativeClient.STR_SHOW_QR_ACTION,
                    ),
                )
            }
            qrInvite?.let { QrInvitePanel(invite = it) }
        }

        SectionLabel(NativeClient.string(NativeClient.STR_HOST_HEADING))

        Text(
            NativeClient.string(
                if (sharing) {
                    NativeClient.STR_SHARE_STATE_ON
                } else {
                    NativeClient.STR_SHARE_STATE_OFF
                },
            ),
            style = MaterialTheme.typography.titleMedium,
            color = if (sharing) OnlineColor else MutedColor,
        )

        Button(
            onClick = {
                if (sharing) {
                    hideQr()
                    onStopSharing()
                    return@Button
                }
                val defaults = NativeHost.shareDefaults()
                onStartSharing(
                    HostService.ShareRequest(
                        fps = defaults.fps,
                        bitrateMbps = defaults.bitrateMbps,
                        maxDim = defaults.maxDim,
                        port = port,
                    ),
                )
            },
            enabled = sharing || !starting,
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
            if (sharing) {
                NativeHost.sharingStatus(port, true, false)
            } else {
                NativeHost.idleStatus(port)
            },
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )

        if (error.isNotEmpty()) {
            Text(error, color = MaterialTheme.colorScheme.error)
        }

        AccessRequestsSection(requests = requests, onDecided = refreshRequests)

        HostRowList(rows = rows, sharing = sharing)

        HorizontalDivider()

        SectionLabel(NativeClient.string(NativeClient.STR_FILES_PICKER_LABEL))

        if (receiving) {
            Text(
                NativeClient.string(NativeClient.STR_RECEIVING_FILES_STATE),
                style = MaterialTheme.typography.titleMedium,
                color = OnlineColor,
            )
            Text(
                NativeHost.sharingStatus(port, false, true),
                style = MaterialTheme.typography.bodyMedium,
                color = MutedColor,
            )
        }

        Text(
            NativeClient.string(NativeClient.STR_MOBILE_TAKES_FILES_NOTE),
            style = MaterialTheme.typography.bodySmall,
            color = MutedColor,
        )
    }
}

@Composable
private fun QrInvitePanel(invite: String) {
    val code = remember(invite) { NativeClient.qrEncode(invite) }
    if (code != null) {
        Box(modifier = Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
            QrCodeImage(code = code, modifier = Modifier.fillMaxWidth(QR_WIDTH_FRACTION))
        }
    }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        SelectionContainer(modifier = Modifier.weight(1f)) {
            Text(
                invite,
                style = MaterialTheme.typography.bodySmall,
                fontFamily = FontFamily.Monospace,
                color = HeadingColor,
            )
        }
        CopyTextButton(NativeClient.string(NativeClient.STR_COPY_BUTTON)) { invite }
    }
    Hint(NativeClient.string(NativeClient.STR_QR_HINT))
}

@Composable
private fun QrCodeImage(
    code: NativeClient.QrCode,
    modifier: Modifier = Modifier,
) {
    Canvas(
        modifier =
            modifier
                .aspectRatio(1f)
                .background(Color.White),
    ) {
        val cells = code.size + 2 * QR_QUIET_ZONE_MODULES
        val cell = size.minDimension / cells
        val square = Size(cell + QR_CELL_OVERLAP_PX, cell + QR_CELL_OVERLAP_PX)
        for (y in 0 until code.size) {
            for (x in 0 until code.size) {
                if (!code.dark(x, y)) continue
                val left = (x + QR_QUIET_ZONE_MODULES) * cell
                val top = (y + QR_QUIET_ZONE_MODULES) * cell
                drawRect(Color.Black, topLeft = Offset(left, top), size = square)
            }
        }
    }
}

@Composable
private fun AccessRequestsSection(
    requests: List<NativeClient.AccessRequest>,
    onDecided: () -> Unit,
) {
    SectionLabel(NativeClient.string(NativeClient.STR_ACCESS_REQUESTS_HEADING))
    if (requests.isEmpty()) {
        Text(
            NativeClient.string(NativeClient.STR_ACCESS_REQUESTS_EMPTY),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )
        return
    }
    for (request in requests) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(request.name.ifBlank { NativeClient.string(NativeClient.STR_UNNAMED_CLIENT) }, color = HeadingColor)
                Text(
                    "${request.shortKey}  ·  ${request.address}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MutedColor,
                )
            }
            TextButton(
                onClick = {
                    NativeClient.accessApprove(request.fingerprint)
                    onDecided()
                },
            ) { Text(NativeClient.string(NativeClient.STR_APPROVE_ACTION)) }
            TextButton(
                onClick = {
                    NativeClient.accessDeny(request.fingerprint)
                    onDecided()
                },
            ) { Text(NativeClient.string(NativeClient.STR_DENY_ACTION), color = MaterialTheme.colorScheme.error) }
        }
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

private const val COPIED_FEEDBACK_MS = 1500L

@Composable
private fun Hint(text: String) {
    Text(text, style = MaterialTheme.typography.bodySmall, color = MutedColor)
}

@Composable
private fun CopyTextButton(
    label: String,
    text: () -> String,
) {
    val context = LocalContext.current
    var copied by remember { mutableStateOf(false) }
    LaunchedEffect(copied) {
        if (!copied) return@LaunchedEffect
        delay(COPIED_FEEDBACK_MS)
        copied = false
    }
    TextButton(
        onClick = {
            copyToClipboard(context, text())
            copied = true
        },
    ) {
        Text(if (copied) NativeClient.string(NativeClient.STR_COPIED_BUTTON) else label)
    }
}

@Composable
private fun DevicesScreen(onConnectHost: (String) -> Unit) {
    var allowedRevision by remember { mutableStateOf(0) }
    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ThisMachineSection()
        DevicesAccessRequests(onDecided = { allowedRevision += 1 })
        AllowedClientsSection(revision = allowedRevision)
        SavedHostsSection(onConnectHost = onConnectHost)
    }
}

@Composable
private fun DevicesAccessRequests(onDecided: () -> Unit) {
    val context = LocalContext.current
    var requests by remember { mutableStateOf(emptyList<NativeClient.AccessRequest>()) }
    var requestsGeneration by remember { mutableStateOf(UNSEEN_GENERATION) }
    val refreshRequests = {
        requestsGeneration = NativeClient.accessRequestsGeneration()
        requests = NativeClient.accessRequests()
        AccessRequestNotifier.announce(context, requests)
    }
    LaunchedEffect(Unit) {
        while (true) {
            if (NativeClient.accessRequestsGeneration() != requestsGeneration) refreshRequests()
            delay(POLL_INTERVAL_MS)
        }
    }
    AccessRequestsSection(
        requests = requests,
        onDecided = {
            refreshRequests()
            onDecided()
        },
    )
}

@Composable
private fun ThisMachineSection() {
    val context = LocalContext.current
    val fingerprint = remember { NativeClient.hostFingerprint() }
    val deviceName = remember { NativeClient.sessionDeviceName() }
    SectionLabel(NativeClient.string(NativeClient.STR_THIS_MACHINE_HEADING))
    Text(
        NativeClient.string(NativeClient.STR_THIS_MACHINE_HINT),
        style = MaterialTheme.typography.bodyMedium,
        color = MutedColor,
    )
    LabeledValue(NativeClient.string(NativeClient.STR_DEVICE_NAME_LABEL), deviceName)
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        SelectionContainer(modifier = Modifier.weight(1f)) {
            Text(
                fingerprint,
                style = MaterialTheme.typography.bodySmall,
                fontFamily = FontFamily.Monospace,
                color = HeadingColor,
            )
        }
        CopyTextButton(NativeClient.string(NativeClient.STR_COPY_BUTTON)) { fingerprint }
    }
    OutlinedButton(onClick = { copyToClipboard(context, NativeClient.hostPublicKey()) }) {
        Text(NativeClient.string(NativeClient.STR_COPY_PUBLIC_KEY_ACTION))
    }
}

@Composable
private fun LabeledValue(
    label: String,
    value: String,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(label, style = MaterialTheme.typography.bodySmall, color = MutedColor)
        Text(value, modifier = Modifier.weight(1f), color = HeadingColor)
    }
}

@Composable
private fun AllowedClientsSection(revision: Int) {
    var devices by remember { mutableStateOf(NativeClient.pairedDevices()) }
    var confirmForgetAll by remember { mutableStateOf(false) }
    val refresh: () -> Unit = { devices = NativeClient.pairedDevices() }
    LaunchedEffect(revision) { refresh() }

    if (confirmForgetAll) {
        AlertDialog(
            onDismissRequest = { confirmForgetAll = false },
            title = { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) },
            text = { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL_PROMPT)) },
            confirmButton = {
                TextButton(
                    onClick = {
                        NativeClient.pairedForgetAll()
                        refresh()
                        confirmForgetAll = false
                    },
                ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) }
            },
            dismissButton = {
                TextButton(onClick = { confirmForgetAll = false }) {
                    Text(NativeClient.string(NativeClient.STR_CANCEL_ACTION))
                }
            },
        )
    }

    SectionLabel(NativeClient.string(NativeClient.STR_PAIRED_HEADING))
    Text(
        NativeClient.string(NativeClient.STR_PAIRED_HINT),
        style = MaterialTheme.typography.bodyMedium,
        color = MutedColor,
    )
    AllowClientForm(onAllowed = refresh)

    if (devices.isEmpty()) {
        Text(
            NativeClient.string(NativeClient.STR_PAIRED_EMPTY),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )
    }
    for (device in devices) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(device.name.ifBlank { NativeClient.string(NativeClient.STR_UNNAMED_CLIENT) }, color = HeadingColor)
                Text(
                    device.shortKey,
                    style = MaterialTheme.typography.bodySmall,
                    color = MutedColor,
                )
            }
            TextButton(
                onClick = {
                    NativeClient.pairedForget(device.fingerprint)
                    refresh()
                },
            ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET)) }
        }
    }

    TextButton(
        onClick = { confirmForgetAll = true },
        enabled = devices.isNotEmpty(),
    ) { Text(NativeClient.string(NativeClient.STR_PAIRED_FORGET_ALL)) }
    Hint(NativeClient.string(NativeClient.STR_PAIRED_FORGET_NOTE))
}

@Composable
private fun AllowClientForm(onAllowed: () -> Unit) {
    var publicKey by remember { mutableStateOf("") }
    var invalid by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        OutlinedTextField(
            value = publicKey,
            onValueChange = {
                publicKey = it
                invalid = false
            },
            placeholder = { Text(NativeClient.string(NativeClient.STR_ALLOW_CLIENT_PLACEHOLDER)) },
            singleLine = true,
            modifier = Modifier.weight(1f),
        )
        Button(
            onClick = {
                if (!NativeClient.pairedAddPublicKey(publicKey.trim())) {
                    invalid = true
                    return@Button
                }
                publicKey = ""
                invalid = false
                onAllowed()
            },
            enabled = publicKey.isNotBlank(),
        ) { Text(NativeClient.string(NativeClient.STR_ALLOW_CLIENT_ACTION)) }
    }
    if (invalid) {
        Text(
            NativeClient.string(NativeClient.STR_ALLOW_CLIENT_INVALID),
            color = MaterialTheme.colorScheme.error,
        )
    }
}

@Composable
private fun SavedHostsSection(onConnectHost: (String) -> Unit) {
    var profiles by remember { mutableStateOf(NativeClient.hostProfiles()) }
    var rowError by remember { mutableStateOf("") }
    val refresh: () -> Unit = { profiles = NativeClient.hostProfiles() }

    SectionLabel(NativeClient.string(NativeClient.STR_SAVED_HOSTS_HEADING))
    Text(
        NativeClient.string(NativeClient.STR_SAVED_HOSTS_HINT),
        style = MaterialTheme.typography.bodyMedium,
        color = MutedColor,
    )

    when (val loaded = profiles) {
        is NativeClient.HostProfiles.Unreadable ->
            Text(loaded.message, color = MaterialTheme.colorScheme.error)

        is NativeClient.HostProfiles.Loaded ->
            SavedHostRows(
                hosts = loaded.hosts,
                onConnectHost = onConnectHost,
                onRemove = { alias ->
                    rowError = NativeClient.removeHostProfile(alias).orEmpty()
                    refresh()
                },
            )
    }

    if (rowError.isNotEmpty()) {
        Text(rowError, color = MaterialTheme.colorScheme.error)
    }
}

@Composable
private fun SavedHostRows(
    hosts: List<NativeClient.HostProfile>,
    onConnectHost: (String) -> Unit,
    onRemove: (String) -> Unit,
) {
    if (hosts.isEmpty()) {
        Text(
            NativeClient.string(NativeClient.STR_SAVED_HOSTS_EMPTY),
            style = MaterialTheme.typography.bodyMedium,
            color = MutedColor,
        )
        return
    }

    for (host in hosts) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(host.alias.ifBlank { host.endpoint }, color = HeadingColor)
                Text(
                    "${NativeClient.string(NativeClient.STR_HOST_LAST_ADDRESS_LABEL)}: ${host.endpoint}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MutedColor,
                )
                Text(
                    host.fingerprint,
                    style = MaterialTheme.typography.labelSmall,
                    color = MutedColor,
                )
            }
            TextButton(onClick = { onConnectHost(host.endpoint) }) {
                Text(NativeClient.string(NativeClient.STR_CONNECT_BUTTON))
            }
            TextButton(onClick = { onRemove(host.alias) }) {
                Text(NativeClient.string(NativeClient.STR_REMOVE_HOST_ACTION))
            }
        }
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

        DeviceNameField()

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
private fun DeviceNameField() {
    var name by remember { mutableStateOf(NativeClient.deviceName()) }
    val focusManager = LocalFocusManager.current
    val commit = {
        if (name.trim() != NativeClient.deviceName()) {
            NativeClient.setDeviceName(name.trim())
            name = NativeClient.deviceName()
        }
    }
    val latestCommit by rememberUpdatedState(commit)
    DisposableEffect(Unit) { onDispose { latestCommit() } }

    OutlinedTextField(
        value = name,
        onValueChange = { name = it },
        label = { Text(NativeClient.string(NativeClient.STR_DEVICE_NAME_LABEL)) },
        placeholder = { Text(Build.MODEL.orEmpty()) },
        supportingText = { Text(NativeClient.string(NativeClient.STR_DEVICE_NAME_HINT)) },
        singleLine = true,
        modifier =
            Modifier
                .fillMaxWidth()
                .onFocusChanged { if (!it.isFocused) commit() },
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focusManager.clearFocus() }),
    )
}

@Composable
private fun AddressScreen(
    address: String,
    onAddressChange: (String) -> Unit,
    connectPort: String,
    onConnectPortChange: (String) -> Unit,
    busy: Boolean,
    authed: NativeClient.HostQuery?,
    authedAddr: String,
    onConnect: (String) -> Unit,
    onScanQr: () -> Unit,
    onDisconnect: () -> Unit,
    onOpenDesktop: () -> Unit,
    onOpenShell: () -> Unit,
    onOpenFileSend: () -> Unit,
    deviceRows: List<NativeClient.DeviceRow>,
    onPickDevice: (String) -> Unit,
) {
    val trimmed = address.trim()
    val ready = trimmed.isNotEmpty() && !busy
    val target =
        if (NativeClient.isPairingInvite(trimmed)) trimmed else NativeClient.composeAddress(trimmed, connectPort)
    val go = { if (ready) onConnect(target) }

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

            ConnectButtons(ready = ready, busy = busy, onConnect = go, onScanQr = onScanQr)
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

            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Box(
                    modifier =
                        Modifier
                            .size(10.dp)
                            .background(OnlineColor, CircleShape),
                )
                Text(
                    NativeClient.string(NativeClient.STR_CONNECTED_PICK_SESSION),
                    color = OnlineColor,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f),
                )
            }

            OutlinedButton(
                onClick = onOpenDesktop,
                enabled = authed.sources.isNotEmpty(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text(NativeClient.string(NativeClient.STR_OPEN_DESKTOP_LABEL)) }

            var control by remember { mutableStateOf(NativeClient.clientControl()) }
            SwitchRow(
                label = NativeClient.string(NativeClient.STR_REQUEST_CONTROL_LABEL),
                checked = control,
            ) {
                control = it
                NativeClient.setClientControl(it)
            }

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
        }

        if (authed == null) {
            DeviceSection(
                heading = NativeClient.string(NativeClient.STR_DEVICES_HEADING),
                rows = deviceRows,
                enabled = !busy,
                onPick = onPickDevice,
            )
        }
    }
}

@Composable
private fun ConnectButtons(
    ready: Boolean,
    busy: Boolean,
    onConnect: () -> Unit,
    onScanQr: () -> Unit,
) {
    val context = LocalContext.current
    var cameraDenied by remember { mutableStateOf(false) }
    val cameraConsent =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            cameraDenied = !granted
            if (granted) onScanQr()
        }
    val scan = {
        val granted =
            ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) ==
                PackageManager.PERMISSION_GRANTED
        if (granted) onScanQr() else cameraConsent.launch(Manifest.permission.CAMERA)
    }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Button(
            onClick = onConnect,
            enabled = ready,
            modifier = Modifier.weight(1f),
        ) { Text(NativeClient.string(NativeClient.STR_CONNECT_BUTTON)) }
        OutlinedButton(onClick = scan, enabled = !busy) {
            Text(NativeClient.string(NativeClient.STR_SCAN_QR_ACTION))
        }
    }
    if (cameraDenied) {
        Text(NativeClient.string(NativeClient.STR_CAMERA_DENIED), color = MaterialTheme.colorScheme.error)
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

@Composable
private fun DeviceSection(
    heading: String,
    rows: List<NativeClient.DeviceRow>,
    enabled: Boolean,
    onPick: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Heading(heading)

        for (row in rows) {
            Column(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clickable(enabled = enabled) { onPick(row.addr) }
                        .padding(vertical = 6.dp),
            ) {
                val named = row.name.isNotBlank()
                Text(if (named) row.name else row.addr, color = HeadingColor)
                val details = listOfNotNull(row.addr.takeIf { named }, row.lastConnected.takeIf { it.isNotBlank() })
                if (details.isNotEmpty()) {
                    Text(
                        details.joinToString("  ·  "),
                        style = MaterialTheme.typography.bodySmall,
                        color = MutedColor,
                    )
                }
            }
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
