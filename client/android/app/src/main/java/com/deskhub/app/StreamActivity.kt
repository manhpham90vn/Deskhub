package com.deskhub.app

import android.content.Context
import android.os.Build
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.calculateCentroid
import androidx.compose.foundation.gestures.calculatePan
import androidx.compose.foundation.gestures.calculateZoom
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.PointerInputScope
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.delay
import kotlin.math.roundToInt

private const val LINK_POLL_MS = 1000L

class StreamActivity : ComponentActivity() {
    private var session by mutableStateOf(0L)

    private var screenPx: Pair<Int, Int> = 0 to 0

    private var currentSourceId by mutableIntStateOf(0)
    private var sources: List<NativeClient.Source> = emptyList()
    private var address = ""
    private var passcode = ""

    private val holderCallback =
        object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                NativeClient.nativeSetSurface(holder.surface)
            }

            override fun surfaceChanged(
                h: SurfaceHolder,
                f: Int,
                w: Int,
                ht: Int,
            ) {}

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                NativeClient.nativeReleaseSurface(holder.surface)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NativeClient.useAppDataDir(this)
        if (NativeClient.keepAwake()) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }

        address = intent.getStringExtra("addr").orEmpty()
        passcode = intent.getStringExtra("passcode").orEmpty()
        currentSourceId = intent.getIntExtra("source", 0)
        sources = readSources(intent)
        screenPx = NativeClient.screenSizePx(this)
        session =
            NativeClient.nativeStart(
                address,
                currentSourceId,
                screenPx.first,
                screenPx.second,
                passcode,
            )

        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                StreamScreen(
                    address = address,
                    sessionKey = session,
                    sources = sources,
                    currentSourceId = currentSourceId,
                    holderCallback = holderCallback,
                    onSwitchSource = ::switchSource,
                    onDismiss = { finish() },
                )
            }
        }
    }

    private fun readSources(intent: android.content.Intent): List<NativeClient.Source> {
        val ids = intent.getIntArrayExtra("srcIds") ?: return emptyList()
        val displayNames = intent.getStringArrayExtra("srcDisplayNames") ?: return emptyList()
        val sizeLabels = intent.getStringArrayExtra("srcSizeLabels") ?: return emptyList()
        if (displayNames.size != ids.size || sizeLabels.size != ids.size) return emptyList()
        return ids.indices.map {
            NativeClient.Source(ids[it], displayNames[it], sizeLabels[it])
        }
    }

    private fun switchSource(sourceId: Int) {
        if (sourceId == currentSourceId) return
        if (session != 0L) {
            NativeClient.releaseAllInput()
            NativeClient.nativeStop(session)
        }
        currentSourceId = sourceId
        session =
            NativeClient.nativeStart(
                address,
                sourceId,
                screenPx.first,
                screenPx.second,
                passcode,
            )
    }

    override fun onStop() {
        super.onStop()
        if (session != 0L) NativeClient.releaseAllInput()
        if (!isFinishing && !isChangingConfigurations) finish()
    }

    override fun onDestroy() {
        if (session != 0L) NativeClient.nativeStop(session)
        super.onDestroy()
    }
}

@Composable
private fun StreamScreen(
    address: String,
    sessionKey: Long,
    sources: List<NativeClient.Source>,
    currentSourceId: Int,
    holderCallback: SurfaceHolder.Callback,
    onSwitchSource: (Int) -> Unit,
    onDismiss: () -> Unit,
) {
    val started = sessionKey != 0L
    var sessionPhase by remember { mutableIntStateOf(NativeClient.PHASE_IDLE) }
    var statusLine by remember { mutableStateOf("") }
    var endReason by remember { mutableStateOf("") }
    var videoW by remember { mutableIntStateOf(0) }
    var videoH by remember { mutableIntStateOf(0) }

    DisposableEffect(sessionKey) {
        sessionPhase = NativeClient.PHASE_IDLE
        statusLine = ""
        endReason = ""
        videoW = 0
        videoH = 0
        if (!started) return@DisposableEffect onDispose {}
        val listener =
            object : NativeClient.SessionListener {
                override fun onStatus(
                    line: String,
                    phase: Int,
                ) {
                    statusLine = line
                    sessionPhase = phase
                }

                override fun onSize(
                    width: Int,
                    height: Int,
                ) {
                    videoW = width
                    videoH = height
                }

                override fun onEnded(reason: String) {
                    endReason = reason
                    sessionPhase = NativeClient.PHASE_ENDED
                }

            }
        NativeClient.sessionListener = listener
        NativeClient.nativeSnapshot()?.let { snap ->
            sessionPhase = snap.phase
            statusLine = snap.statusLine
            videoW = snap.videoWidth
            videoH = snap.videoHeight
            if (sessionPhase == NativeClient.PHASE_ENDED) endReason = snap.endReason
        }
        onDispose {
            if (NativeClient.sessionListener === listener) NativeClient.sessionListener = null
        }
    }

    val streaming = sessionPhase == NativeClient.PHASE_STREAMING
    val reattaching = sessionPhase == NativeClient.PHASE_REATTACHING

    LaunchedEffect(sessionKey) {
        if (!started) return@LaunchedEffect
        while (sessionPhase != NativeClient.PHASE_ENDED) {
            NativeClient.nativeSnapshot()?.let { snap ->
                sessionPhase = snap.phase
                if (snap.phase == NativeClient.PHASE_ENDED && endReason.isEmpty()) {
                    endReason = snap.endReason
                }
            }
            delay(LINK_POLL_MS)
        }
    }

    val appContext = LocalContext.current.applicationContext
    LaunchedEffect(sessionKey, streaming) {
        if (!streaming || !NativeClient.clipboardSync()) return@LaunchedEffect
        ClipboardPump.run(
            appContext,
            take = { NativeClient.clipTake() },
            offer = { NativeClient.clipOffer(it) },
        )
    }

    var keyboardOn by remember { mutableStateOf(false) }
    var keyView by remember { mutableStateOf<KeyInputView?>(null) }
    LaunchedEffect(keyboardOn) {
        val v = keyView ?: return@LaunchedEffect
        val imm = v.context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        if (!keyboardOn) {
            imm.hideSoftInputFromWindow(v.windowToken, 0)
            return@LaunchedEffect
        }
        v.requestFocus()
        imm.showSoftInput(v, 0)

        if (Build.VERSION.SDK_INT < 30) return@LaunchedEffect
        var seen = false
        while (true) {
            val visible =
                v.rootWindowInsets?.isVisible(
                    android.view.WindowInsets.Type
                        .ime(),
                ) == true
            if (visible) {
                seen = true
            } else if (seen) {
                keyboardOn = false
                return@LaunchedEffect
            }
            delay(200)
        }
    }

    var controlsOpen by remember { mutableStateOf(false) }

    var zoom by remember { mutableFloatStateOf(1f) }
    var pan by remember { mutableStateOf(Offset.Zero) }
    var viewport by remember { mutableStateOf(IntSize.Zero) }
    val aspect = if (videoW > 0 && videoH > 0) videoW.toFloat() / videoH else 16f / 9f
    val zoomed = NativeClient.isZoomed(zoom)

    var panMode by remember { mutableStateOf(false) }
    LaunchedEffect(zoomed) { panMode = zoomed }

    LaunchedEffect(sessionKey) {
        zoom = 1f
        pan = Offset.Zero
    }

    fun applyTransform(
        factor: Float,
        centroid: Offset,
        panDelta: Offset,
    ) {
        if (viewport.width <= 0 || viewport.height <= 0) return
        val next =
            NativeClient.applyGesture(
                NativeClient.Transform(zoom, pan.x, pan.y),
                factor,
                centroid.x,
                centroid.y,
                panDelta.x,
                panDelta.y,
                viewport.width.toFloat(),
                viewport.height.toFloat(),
                aspect,
            )
        zoom = next.zoom
        pan = Offset(next.panX, next.panY)
    }

    val scrollCarry = remember { doubleArrayOf(0.0) }

    val onTransform by rememberUpdatedState(
        newValue = { factor: Float, centroid: Offset, delta: Offset ->
            if (factor != 1f || zoomed) {
                scrollCarry[0] = 0.0
                applyTransform(factor, centroid, delta)
            } else {
                val notches = NativeClient.takeScrollNotches(delta.y, scrollCarry)
                if (notches != 0) NativeClient.mouseWheel(notches)
            }
        },
    )

    Box(
        modifier =
            Modifier
                .fillMaxSize()
                .background(Color.Black),
    ) {
        Box(
            modifier =
                Modifier
                    .fillMaxSize()
                    .windowInsetsPadding(
                        WindowInsets.displayCutout.only(WindowInsetsSides.Horizontal),
                    ).onSizeChanged { viewport = it }
                    .pointerInput(Unit) {
                        detectZoomPan { factor, centroid, delta ->
                            onTransform(factor, centroid, delta)
                        }
                    },
        ) {
            if (started) {
                val base = videoFrame(viewport, aspect, 1f, Offset.Zero)
                val rect = videoFrame(viewport, aspect, zoom, pan)
                if (!base.isEmpty) {
                    val density = LocalDensity.current
                    Box(
                        modifier =
                            Modifier
                                .offset {
                                    IntOffset(base.left.roundToInt(), base.top.roundToInt())
                                }.size(
                                    with(density) { base.width.toDp() },
                                    with(density) { base.height.toDp() },
                                ),
                    ) {
                        AndroidView(
                            factory = { ctx ->
                                SurfaceView(ctx).apply { holder.addCallback(holderCallback) }
                            },
                            modifier = Modifier.fillMaxSize(),
                            update = { view ->
                                view.pivotX = 0f
                                view.pivotY = 0f
                                view.scaleX = zoom
                                view.scaleY = zoom
                                view.translationX = rect.left - base.left
                                view.translationY = rect.top - base.top
                            },
                        )
                    }
                }

                if (streaming) {
                    TrackpadOverlay(
                        videoRect = rect,
                        panMode = panMode,
                        onPanRequest = { delta -> applyTransform(1f, Offset.Zero, delta) },
                        modifier = Modifier.fillMaxSize(),
                    )
                }

                AndroidView(
                    factory = { ctx ->
                        KeyInputView(ctx)
                            .apply {
                                onChar = { cp -> NativeClient.charTap(cp) }
                                onKey = { vk, down ->
                                    NativeClient.key(vk, NativeClient.vkScancode(vk), down)
                                }
                            }.also { keyView = it }
                    },
                    modifier = Modifier.size(1.dp),
                )
            }

            if (!started || sessionPhase == NativeClient.PHASE_ENDED) {
                EndedOverlay(
                    reason = if (!started) NativeClient.couldNotConnect(address) else endReason,
                    onBack = onDismiss,
                )
            } else if (reattaching) {
                ReattachingBanner()
            } else if (!streaming) {
                ConnectingOverlay(address = address)
            }
        }

        SessionCloseButton(
            onClick = onDismiss,
            modifier =
                Modifier
                    .align(Alignment.TopEnd)
                    .safeDrawingPadding()
                    .consumeTouches()
                    .padding(12.dp),
        )

        Column(
            modifier =
                Modifier
                    .align(Alignment.BottomEnd)
                    .safeDrawingPadding()
                    .consumeTouches()
                    .padding(12.dp),
            horizontalAlignment = Alignment.End,
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (zoomed) {
                Pill(text = if (panMode) "Pan" else "Pointer", onClick = { panMode = !panMode })
                Pill(
                    text = NativeClient.zoomLabel(zoom),
                    onClick = {
                        zoom = 1f
                        pan = Offset.Zero
                    },
                )
            }
            if (controlsOpen) {
                ControlPanel(
                    address = address,
                    videoW = videoW,
                    videoH = videoH,
                    statusLine = statusLine,
                    streaming = streaming,
                    keyboardOn = keyboardOn,
                    sources = sources,
                    currentSourceId = currentSourceId,
                    onToggleKeyboard = { keyboardOn = !keyboardOn },
                    onSwitchSource = onSwitchSource,
                    onCollapse = { controlsOpen = false },
                )
            } else {
                ExpandButton(onClick = { controlsOpen = true })
            }
        }
    }
}

private fun Modifier.consumeTouches(): Modifier =
    pointerInput(Unit) {
        awaitPointerEventScope {
            while (true) {
                awaitPointerEvent().changes.forEach { it.consume() }
            }
        }
    }

private suspend fun PointerInputScope.detectZoomPan(onTransform: (Float, Offset, Offset) -> Unit) {
    awaitEachGesture {
        awaitFirstDown(requireUnconsumed = false, pass = PointerEventPass.Initial)
        var multiTouch = false
        do {
            val event = awaitPointerEvent(PointerEventPass.Initial)
            if (event.changes.count { it.pressed } >= 2) {
                multiTouch = true
                val zoom = event.calculateZoom()
                val pan = event.calculatePan()
                if (zoom != 1f || pan != Offset.Zero) {
                    onTransform(zoom, event.calculateCentroid(useCurrent = true), pan)
                }
            }
            if (multiTouch) event.changes.forEach { it.consume() }
        } while (event.changes.any { it.pressed })
    }
}

@Composable
private fun DisplayPickerDialog(
    sources: List<NativeClient.Source>,
    currentSourceId: Int,
    onPick: (Int) -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Display") },
        text = {
            Column {
                sources.forEach { source ->
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable {
                                    onPick(source.id)
                                    onDismiss()
                                }.padding(vertical = 8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                    ) {
                        RadioButton(
                            selected = source.id == currentSourceId,
                            onClick = {
                                onPick(source.id)
                                onDismiss()
                            },
                        )
                        Column {
                            Text(source.displayName)
                            Text(
                                text = source.sizeLabel,
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun Pill(
    text: String,
    onClick: () -> Unit,
) {
    Box(
        modifier =
            Modifier
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.45f))
                .border(1.dp, Color.White.copy(alpha = 0.25f), CircleShape)
                .clickable(onClick = onClick)
                .padding(horizontal = 10.dp, vertical = 6.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(text = text, color = Color.White, style = MaterialTheme.typography.labelMedium)
    }
}

@Composable
private fun ExpandButton(onClick: () -> Unit) {
    Box(
        modifier =
            Modifier
                .size(48.dp)
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.45f))
                .border(1.dp, Color.White.copy(alpha = 0.25f), CircleShape)
                .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(text = "☰", color = Color.White)
    }
}

@Composable
private fun ControlPanel(
    address: String,
    videoW: Int,
    videoH: Int,
    statusLine: String,
    streaming: Boolean,
    keyboardOn: Boolean,
    sources: List<NativeClient.Source>,
    currentSourceId: Int,
    onToggleKeyboard: () -> Unit,
    onSwitchSource: (Int) -> Unit,
    onCollapse: () -> Unit,
) {
    var pickerOpen by remember { mutableStateOf(false) }

    if (pickerOpen) {
        DisplayPickerDialog(
            sources = sources,
            currentSourceId = currentSourceId,
            onPick = onSwitchSource,
            onDismiss = { pickerOpen = false },
        )
    }

    Column(
        modifier =
            Modifier
                .fillMaxWidth()
                .background(Color.Black.copy(alpha = 0.75f), RoundedCornerShape(16.dp))
                .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = NativeClient.hostTitle(address, videoW, videoH),
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.White,
                    maxLines = 1,
                )
                if (streaming && statusLine.isNotEmpty()) {
                    Text(
                        text = statusLine,
                        style = MaterialTheme.typography.bodySmall,
                        color = Color.White.copy(alpha = 0.8f),
                        maxLines = 1,
                    )
                }
            }
            Box(
                modifier =
                    Modifier
                        .size(32.dp)
                        .clip(CircleShape)
                        .clickable(onClick = onCollapse),
                contentAlignment = Alignment.Center,
            ) {
                Text(text = "⌄", color = Color.White)
            }
        }

        Row(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            NativeClient.hotkeys.forEach { hk ->
                OutlinedButton(
                    onClick = { NativeClient.hotkey(hk) },
                    enabled = streaming,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                ) { Text(hk.label) }
            }
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            OutlinedButton(onClick = onToggleKeyboard, enabled = streaming) {
                Text(if (keyboardOn) "Hide keyboard" else "Keyboard")
            }
            if (sources.size > 1) {
                OutlinedButton(onClick = { pickerOpen = true }) { Text("Display") }
            }
        }
    }
}

@Composable
private fun ReattachingBanner() {
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.TopCenter,
    ) {
        Text(
            NativeClient.string(NativeClient.STR_LINK_REATTACHING),
            color = Color.White,
            modifier =
                Modifier
                    .padding(12.dp)
                    .background(Color.Black.copy(alpha = 0.75f))
                    .padding(horizontal = 12.dp, vertical = 6.dp),
        )
    }
}

@Composable
private fun ConnectingOverlay(address: String) {
    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            CircularProgressIndicator()
            Text(text = NativeClient.connectingTo(address), color = Color.White)
        }
    }
}

@Composable
private fun EndedOverlay(
    reason: String,
    onBack: () -> Unit,
) {
    Box(
        modifier =
            Modifier
                .fillMaxSize()
                .clickable(onClick = onBack),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            modifier = Modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(
                text = NativeClient.string(NativeClient.STR_SESSION_ENDED),
                style = MaterialTheme.typography.titleMedium,
                color = Color.White,
            )
            Text(text = reason, color = Color.White)
            TextButton(onClick = onBack) { Text("Back") }
        }
    }
}

@Composable
private fun TrackpadOverlay(
    videoRect: Rect,
    panMode: Boolean,
    onPanRequest: (Offset) -> Unit,
    modifier: Modifier,
) {
    var cursor by remember { mutableStateOf(NativeClient.Cursor()) }
    var bounds by remember { mutableStateOf(IntSize.Zero) }

    val rect by rememberUpdatedState(videoRect)
    val requestPan by rememberUpdatedState(onPanRequest)

    fun viewport(): Size = Size(bounds.width.toFloat(), bounds.height.toFloat())

    fun sendMove() {
        NativeClient.cursorMouseMove(cursor, rect)
    }

    LaunchedEffect(videoRect, bounds) {
        cursor = NativeClient.cursorClamped(cursor, rect, viewport())
    }

    fun moveBy(delta: Offset) {
        cursor = NativeClient.cursorMoved(cursor, delta, rect, viewport())
        sendMove()
    }

    fun clickAt(button: Int) {
        sendMove()
        NativeClient.mouseButton(button, true)
        NativeClient.mouseButton(button, false)
    }

    Box(
        modifier =
            modifier
                .onSizeChanged { sz -> bounds = sz }
                .pointerInput(panMode) {
                    if (panMode) return@pointerInput
                    detectTapGestures(
                        onTap = { clickAt(NativeClient.MOUSE_LEFT) },
                        onDoubleTap = { clickAt(NativeClient.MOUSE_RIGHT) },
                    )
                }.pointerInput(panMode) {
                    detectDragGestures(
                        onDrag = { change, delta ->
                            change.consume()
                            if (panMode) requestPan(delta) else moveBy(delta)
                        },
                    )
                }.pointerInput(panMode) {
                    if (panMode) return@pointerInput
                    detectDragGesturesAfterLongPress(
                        onDragStart = {
                            sendMove()
                            NativeClient.mouseButton(NativeClient.MOUSE_LEFT, true)
                        },
                        onDrag = { change, delta ->
                            change.consume()
                            moveBy(delta)
                        },
                        onDragEnd = {
                            NativeClient.mouseButton(NativeClient.MOUSE_LEFT, false)
                        },
                        onDragCancel = {
                            NativeClient.mouseButton(NativeClient.MOUSE_LEFT, false)
                        },
                    )
                },
    ) {
        if (!rect.isEmpty) {
            CursorArrow(
                modifier =
                    Modifier.offset {
                        val p = NativeClient.cursorScreenPoint(cursor, rect) ?: Offset.Zero
                        IntOffset(p.x.roundToInt(), p.y.roundToInt())
                    },
            )
        }
    }
}

@Composable
private fun CursorArrow(modifier: Modifier) {
    Canvas(modifier = modifier.size(18.dp)) {
        val w = size.width
        val h = size.height
        val p =
            Path().apply {
                moveTo(0f, 0f)
                lineTo(0f, h * 0.80f)
                lineTo(w * 0.22f, h * 0.62f)
                lineTo(w * 0.40f, h * 0.98f)
                lineTo(w * 0.55f, h * 0.90f)
                lineTo(w * 0.37f, h * 0.55f)
                lineTo(w * 0.63f, h * 0.55f)
                close()
            }
        drawPath(p, Color.White)
        drawPath(p, Color.Black, style = Stroke(width = 1.dp.toPx()))
    }
}

private fun videoFrame(
    viewport: IntSize,
    aspect: Float,
    zoom: Float,
    pan: Offset,
): Rect {
    if (viewport.width <= 0 || viewport.height <= 0) return Rect.Zero
    val r =
        NativeClient.nativeVideoFrame(
            viewport.width.toFloat(),
            viewport.height.toFloat(),
            aspect,
            zoom,
            pan.x,
            pan.y,
        )
    return Rect(r[0], r[1], r[0] + r[2], r[1] + r[3])
}
