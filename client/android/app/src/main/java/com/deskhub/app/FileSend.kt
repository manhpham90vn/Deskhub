package com.deskhub.app

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import android.text.format.Formatter
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

object FileSendStaging {
    private const val FOLDER = "deskhub-send"

    fun clear(context: Context) {
        folder(context).deleteRecursively()
    }

    suspend fun stage(
        context: Context,
        uris: List<Uri>,
    ): List<File> =
        withContext(Dispatchers.IO) {
            clear(context)
            val dir = folder(context)
            dir.mkdirs()
            uris.mapNotNull { copy(context, it, dir) }
        }

    private fun folder(context: Context) = File(context.cacheDir, FOLDER)

    private fun copy(
        context: Context,
        uri: Uri,
        dir: File,
    ): File? {
        val target = freeName(dir, displayName(context, uri) ?: return null)
        return runCatching {
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input)
                target.outputStream().use { output -> input.copyTo(output) }
            }
            target
        }.getOrNull()
    }

    private fun displayName(
        context: Context,
        uri: Uri,
    ): String? {
        val columns = arrayOf(OpenableColumns.DISPLAY_NAME)
        context.contentResolver.query(uri, columns, null, null, null)?.use { cursor ->
            val column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (column >= 0 && cursor.moveToFirst()) {
                val name = cursor.getString(column)
                if (!name.isNullOrBlank()) return name
            }
        }
        return uri.lastPathSegment?.substringAfterLast('/')?.takeIf { it.isNotBlank() }
    }

    private fun freeName(
        dir: File,
        name: String,
    ): File {
        var candidate = File(dir, name)
        var attempt = 1
        while (candidate.exists()) {
            attempt += 1
            val base = name.substringBeforeLast('.', name)
            val ext = name.substringAfterLast('.', "")
            candidate = File(dir, if (ext.isEmpty()) "$base-$attempt" else "$base-$attempt.$ext")
        }
        return candidate
    }
}

interface FileSendDriver {
    fun begin(paths: List<String>): Boolean

    fun cancel()

    fun poll(): NativeClient.Transfer

    fun error(): String

    fun release()
}

class StandaloneFileSendDriver(
    private val address: String,
    private val deviceName: String,
) : FileSendDriver {
    private var handle = 0L
    private var lastError = ""

    override fun begin(paths: List<String>): Boolean {
        release()
        lastError = NativeClient.sendCheck(paths)
        if (lastError.isNotEmpty()) return false
        handle = NativeClient.sendStart(address, deviceName, paths)
        if (handle == 0L) {
            lastError = NativeClient.couldNotConnect(address)
            return false
        }
        return true
    }

    override fun cancel() {
        if (handle != 0L) NativeClient.sendCancel(handle)
    }

    override fun poll(): NativeClient.Transfer =
        if (handle == 0L) NativeClient.Transfer() else NativeClient.sendSnapshot(handle)

    override fun error(): String = lastError

    override fun release() {
        if (handle != 0L) NativeClient.sendStop(handle)
        handle = 0L
    }
}

private data class SentRow(
    val name: String,
    val detail: String,
    val ok: Boolean,
)

@Composable
fun FileSendScreen(
    driver: FileSendDriver,
    subtitle: String,
    onClose: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var chosen by remember { mutableStateOf(emptyList<File>()) }
    var transfer by remember { mutableStateOf(NativeClient.Transfer()) }
    var error by remember { mutableStateOf("") }
    var history by remember { mutableStateOf(emptyList<SentRow>()) }
    var settled by remember { mutableStateOf(true) }
    var staging by remember { mutableStateOf(false) }

    val settleNow = {
        if (!settled && (transfer.done || transfer.failed)) {
            history =
                history +
                chosen.mapIndexed { index, file ->
                    val sent = transfer.done || index < transfer.fileIndex
                    SentRow(
                        name = file.name,
                        detail =
                            if (sent) {
                                Formatter.formatFileSize(context, file.length())
                            } else {
                                transfer.message
                            },
                        ok = sent,
                    )
                }
            chosen = emptyList()
            settled = true
        }
    }

    val adopt: (List<Uri>) -> Unit = { uris ->
        if (uris.isNotEmpty()) {
            val capped = uris.take(NativeClient.maxTransferFiles)
            staging = true
            scope.launch {
                chosen = FileSendStaging.stage(context, capped)
                staging = false
                error =
                    if (uris.size > capped.size) {
                        NativeClient.string(NativeClient.STR_TRANSFER_TOO_MANY_FILES)
                    } else {
                        ""
                    }
            }
        }
    }

    val photos =
        rememberLauncherForActivityResult(
            ActivityResultContracts.PickMultipleVisualMedia(
                maxOf(2, NativeClient.maxTransferFiles),
            ),
            adopt,
        )
    val documents =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments(), adopt)

    LaunchedEffect(transfer.active) {
        if (!transfer.active) {
            settleNow()
            return@LaunchedEffect
        }
        while (true) {
            delay(POLL_MS)
            transfer = driver.poll()
            if (!transfer.active) break
        }
    }

    val close = {
        driver.release()
        FileSendStaging.clear(context)
        onClose()
    }

    BackHandler { if (!transfer.active) close() }

    Column(
        modifier =
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(
                text = NativeClient.string(NativeClient.STR_TRANSFER_SEND_HEADING),
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.weight(1f),
            )
            SessionCloseButton(onClick = close, enabled = !transfer.active)
        }

        Text(
            text = subtitle,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                onClick = {
                    error = ""
                    photos.launch(
                        PickVisualMediaRequest(
                            ActivityResultContracts.PickVisualMedia.ImageAndVideo,
                        ),
                    )
                },
                enabled = !transfer.active && !staging,
            ) { Text("Photos") }
            OutlinedButton(
                onClick = {
                    error = ""
                    documents.launch(arrayOf("*/*"))
                },
                enabled = !transfer.active && !staging,
            ) { Text("Files") }
        }

        if (chosen.isEmpty()) {
            Text(NativeClient.string(NativeClient.STR_TRANSFER_NONE_CHOSEN))
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                chosen.forEach { file ->
                    FileRow(file.name, Formatter.formatFileSize(context, file.length()))
                }
            }
        }

        if (error.isNotEmpty()) {
            Text(text = error, color = MaterialTheme.colorScheme.error)
        }

        Button(
            onClick = {
                error = ""
                if (driver.begin(chosen.map { it.absolutePath })) {
                    settled = false
                    transfer = NativeClient.Transfer(active = true)
                } else {
                    error = driver.error()
                }
            },
            enabled = chosen.isNotEmpty() && !transfer.active && !staging,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Send") }

        if (!transfer.idle) {
            LinearProgressIndicator(
                progress = { transfer.fraction },
                modifier = Modifier.fillMaxWidth(),
            )
            FileRow(statusText(transfer), transfer.step)
            if (transfer.active) {
                OutlinedButton(
                    onClick = { driver.cancel() },
                    modifier = Modifier.fillMaxWidth(),
                ) { Text(NativeClient.string(NativeClient.STR_TRANSFER_CANCEL_BUTTON)) }
            }
        }

        if (history.isNotEmpty()) {
            Text(
                text = NativeClient.string(NativeClient.STR_TRANSFER_SENT_HEADING),
                style = MaterialTheme.typography.labelMedium,
            )
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                history.forEach { row ->
                    FileRow("${if (row.ok) "✓" else "✕"} ${row.name}", row.detail)
                }
            }
        }
    }
}

private const val POLL_MS = 200L

private fun statusText(transfer: NativeClient.Transfer): String {
    if (!transfer.active) return transfer.message
    if (transfer.name.isNotEmpty()) return transfer.name
    if (transfer.message.isNotEmpty()) return transfer.message
    return NativeClient.string(NativeClient.STR_TRANSFER_SENDING)
}

@Composable
private fun FileRow(
    name: String,
    detail: String,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = name,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodySmall,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(text = detail, style = MaterialTheme.typography.bodySmall, maxLines = 1)
    }
}
