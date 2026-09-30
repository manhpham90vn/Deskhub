package com.deskhub.app

import android.content.Context
import androidx.activity.compose.BackHandler
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.google.zxing.BinaryBitmap
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.ReaderException
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeReader
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

@Composable
fun QrScanScreen(
    hint: String,
    accepts: (String) -> Boolean,
    onDecoded: (String) -> Unit,
    onClose: () -> Unit,
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val latestAccepts by rememberUpdatedState(accepts)
    val latestOnDecoded by rememberUpdatedState(onDecoded)
    val camera = remember { ScannerCamera(context) }

    BackHandler(onBack = onClose)
    DisposableEffect(camera) { onDispose { camera.release() } }

    Column(modifier = Modifier.fillMaxSize()) {
        Row(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(
                text = NativeClient.string(NativeClient.STR_SCAN_QR_ACTION),
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.weight(1f),
            )
            SessionCloseButton(onClick = onClose, enabled = true)
        }

        AndroidView(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .weight(1f),
            factory = { viewContext ->
                PreviewView(viewContext).also { view ->
                    camera.start(lifecycleOwner, view) { text ->
                        if (latestAccepts(text)) latestOnDecoded(text)
                    }
                }
            },
        )

        Text(
            text = hint,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(16.dp),
        )
    }
}

private class ScannerCamera(
    private val context: Context,
) {
    private val analysisExecutor = Executors.newSingleThreadExecutor()
    private val mainExecutor: Executor = ContextCompat.getMainExecutor(context)
    private var provider: ProcessCameraProvider? = null

    fun start(
        owner: LifecycleOwner,
        view: PreviewView,
        onText: (String) -> Unit,
    ) {
        val delivered = AtomicBoolean(false)
        val analyzer =
            QrFrameAnalyzer { text ->
                if (delivered.compareAndSet(false, true)) mainExecutor.execute { onText(text) }
            }
        val pending = ProcessCameraProvider.getInstance(context)
        pending.addListener({
            val ready = pending.get()
            provider = ready
            bind(ready, owner, view, analyzer)
        }, mainExecutor)
    }

    private fun bind(
        ready: ProcessCameraProvider,
        owner: LifecycleOwner,
        view: PreviewView,
        analyzer: ImageAnalysis.Analyzer,
    ) {
        if (analysisExecutor.isShutdown) return
        val preview = Preview.Builder().build()
        preview.surfaceProvider = view.surfaceProvider
        val analysis =
            ImageAnalysis
                .Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()
        analysis.setAnalyzer(analysisExecutor, analyzer)
        ready.unbindAll()
        runCatching {
            ready.bindToLifecycle(owner, CameraSelector.DEFAULT_BACK_CAMERA, preview, analysis)
        }
    }

    fun release() {
        provider?.unbindAll()
        provider = null
        analysisExecutor.shutdown()
    }
}

private class QrFrameAnalyzer(
    private val onText: (String) -> Unit,
) : ImageAnalysis.Analyzer {
    private val reader = QRCodeReader()

    override fun analyze(image: ImageProxy) {
        image.use { frame -> decode(frame)?.let(onText) }
    }

    private fun decode(frame: ImageProxy): String? {
        val luminance = lumaBytes(frame)
        val source =
            PlanarYUVLuminanceSource(
                luminance,
                frame.width,
                frame.height,
                0,
                0,
                frame.width,
                frame.height,
                false,
            )
        return try {
            reader.decode(BinaryBitmap(HybridBinarizer(source))).text
        } catch (_: ReaderException) {
            null
        } finally {
            reader.reset()
        }
    }

    private fun lumaBytes(frame: ImageProxy): ByteArray {
        val plane = frame.planes[0]
        val buffer = plane.buffer
        val width = frame.width
        val height = frame.height
        val out = ByteArray(width * height)
        if (plane.rowStride == width && plane.pixelStride == 1) {
            buffer.get(out, 0, minOf(out.size, buffer.remaining()))
            return out
        }
        val row = ByteArray(plane.rowStride)
        for (y in 0 until height) {
            buffer.position(y * plane.rowStride)
            val available = minOf(row.size, buffer.remaining())
            buffer.get(row, 0, available)
            for (x in 0 until width) {
                val index = x * plane.pixelStride
                if (index < available) out[y * width + x] = row[index]
            }
        }
        return out
    }
}
