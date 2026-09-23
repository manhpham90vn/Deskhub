package com.deskhub.app

object NativeTerminal {
    const val STATE_IDLE = 0
    const val STATE_DECIDING = 2
    const val STATE_REATTACHING = 5
    const val STATE_REFUSED = 6
    const val STATE_FAILED = 7

    const val KEY_CHAR = 0
    const val KEY_ENTER = 1
    const val KEY_BACKSPACE = 2
    const val KEY_TAB = 3
    const val KEY_ESCAPE = 4
    const val KEY_UP = 5
    const val KEY_DOWN = 6
    const val KEY_RIGHT = 7
    const val KEY_LEFT = 8
    const val KEY_HOME = 9
    const val KEY_END = 10
    const val KEY_PAGE_UP = 11
    const val KEY_PAGE_DOWN = 12
    const val KEY_INSERT = 13
    const val KEY_DELETE = 14
    const val KEY_F1 = 15

    const val ATTR_BOLD = 1
    const val ATTR_UNDERLINE = 8

    private const val SHELL_FIELDS = 3
    private const val HEADER_INTS = 9
    private const val INTS_PER_CELL = 3

    init {
        System.loadLibrary("deskhub")
    }

    class Grid(
        private val data: IntArray,
    ) {
        val rows: Int get() = data[0]
        val cols: Int get() = data[1]
        val cursorRow: Int get() = data[2]
        val cursorCol: Int get() = data[3]
        val cursorVisible: Boolean get() = data[4] != 0
        val scrollbackRows: Int get() = data[5]
        val scrollOffset: Int get() = data[6]
        val revision: Long
            get() =
                (data[8].toLong() shl 32) or (data[7].toLong() and 0xFFFFFFFFL)

        private fun base(
            row: Int,
            col: Int,
        ): Int = HEADER_INTS + (row * cols + col) * INTS_PER_CELL

        fun codepoint(
            row: Int,
            col: Int,
        ): Int = data[base(row, col)]

        fun foreground(
            row: Int,
            col: Int,
        ): Int = data[base(row, col) + 1]

        fun background(
            row: Int,
            col: Int,
        ): Int = data[base(row, col) + 2] and 0xFFFFFF

        fun attrs(
            row: Int,
            col: Int,
        ): Int = (data[base(row, col) + 2] ushr 24) and 0xFF
    }

    private external fun nativeOpen(
        addr: String,
        passcode: String,
        cols: Int,
        rows: Int,
    ): Boolean

    private external fun nativeStop()

    private external fun nativeShells(): Array<String>?

    private external fun nativeShellsKnown(): Boolean

    private external fun nativeRequestShells()

    private external fun nativeResumeShell(termId: Int)

    private external fun nativeCloseShell(termId: Int)

    private external fun nativeOpenFresh()

    private external fun nativeState(): Int

    private external fun nativeMessage(): String

    private external fun nativeFingerprint(): String

    private external fun nativeVerdict(): Int

    private external fun nativeAcceptKey()

    private external fun nativeRejectKey()

    private external fun nativeGrid(scrollOffset: Int): IntArray?

    private external fun nativeSendKey(
        key: Int,
        codepoint: Int,
        shift: Boolean,
        alt: Boolean,
        ctrl: Boolean,
    )

    private external fun nativeSendText(text: String)

    private external fun nativeResize(
        cols: Int,
        rows: Int,
    )

    fun open(
        addr: String,
        passcode: String,
        cols: Int,
        rows: Int,
    ): Boolean = nativeOpen(addr, passcode, cols, rows)

    fun stop() = nativeStop()

    data class Shell(
        val termId: Int,
        val line: String,
        val resumable: Boolean,
        val closable: Boolean,
    )

    fun shells(): List<Shell> {
        val packed = nativeShells() ?: return emptyList()
        val out = ArrayList<Shell>(packed.size / SHELL_FIELDS)
        var at = 0
        while (at + SHELL_FIELDS <= packed.size) {
            val termId = packed[at].toIntOrNull()
            val flags = packed[at + 1].toIntOrNull()
            if (termId != null && flags != null && termId > 0) {
                out.add(
                    Shell(
                        termId = termId,
                        line = packed[at + 2],
                        resumable = flags and 1 != 0,
                        closable = flags and 2 != 0,
                    ),
                )
            }
            at += SHELL_FIELDS
        }
        return out
    }

    fun shellsKnown(): Boolean = nativeShellsKnown()

    fun requestShells() = nativeRequestShells()

    fun resumeShell(termId: Int) = nativeResumeShell(termId)

    fun closeShell(termId: Int) = nativeCloseShell(termId)

    fun openFresh() = nativeOpenFresh()

    fun state(): Int = nativeState()

    fun message(): String = nativeMessage()

    fun fingerprint(): String = nativeFingerprint()

    fun verdict(): Int = nativeVerdict()

    fun acceptKey() = nativeAcceptKey()

    fun rejectKey() = nativeRejectKey()

    fun grid(scrollOffset: Int): Grid? {
        val data = nativeGrid(scrollOffset) ?: return null
        if (data.size < HEADER_INTS) return null
        return Grid(data)
    }

    fun sendKey(
        key: Int,
        codepoint: Int = 0,
        shift: Boolean = false,
        alt: Boolean = false,
        ctrl: Boolean = false,
    ) = nativeSendKey(key, codepoint, shift, alt, ctrl)

    fun sendText(text: String) = nativeSendText(text)

    fun resize(
        cols: Int,
        rows: Int,
    ) = nativeResize(cols, rows)
}
