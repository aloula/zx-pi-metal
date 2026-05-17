void CKernel::RenderOSD(void)
{
    static const unsigned MaxPanelChars = 128;
    const unsigned panel_rows = OSDVisibleRows + 5;
    const unsigned cols = m_Screen.GetColumns();
    const unsigned rows = m_Screen.GetRows();

    unsigned draw_col0 = (m_DrawX * cols) / m_Screen.GetWidth();
    unsigned draw_row0 = (m_DrawY * rows) / m_Screen.GetHeight();
    unsigned draw_col1 = ((m_DrawX + ZX_FB_WIDTH * m_Scale) * cols) / m_Screen.GetWidth();
    unsigned draw_row1 = ((m_DrawY + ZX_FB_HEIGHT * m_Scale) * rows) / m_Screen.GetHeight();

    if (draw_col1 <= draw_col0) draw_col1 = draw_col0 + 1;
    if (draw_row1 <= draw_row0 + 2) draw_row1 = draw_row0 + 3;

    unsigned start_col = draw_col0 + 1;
    unsigned start_row = draw_row0 + 2;
    if (start_row + panel_rows + 1 >= draw_row1) {
        start_row = (draw_row1 > panel_rows + 1) ? (draw_row1 - panel_rows - 1) : 1;
    }

    unsigned end_col = (draw_col1 > 0) ? (draw_col1 - 1) : 0;
    if (start_col < 1) start_col = 1;
    if (end_col > cols) end_col = cols;
    if (end_col < start_col) end_col = start_col;
    if (start_row < 1) start_row = 1;

    unsigned width = end_col - start_col + 1;
    if (width > MaxPanelChars) {
        width = MaxPanelChars;
    }

    CString line;
    char rowbuf[MaxPanelChars + 1];
    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    rowbuf[width] = '\0';

    /* Set Spectrum colors: Black on White background. (Split ANSI) */
    m_Screen.Write("\x1b[30m", 5);
    m_Screen.Write("\x1b[47m", 5);
    for (unsigned i = 0; i < panel_rows; i++) {
        line.Format("\x1b[%u;%uH%s", start_row + i, start_col, rowbuf);
        m_Screen.Write((const char *)line, line.GetLength());
    }

    BlitSpectrumFramebuffer();

    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    const char *title = " [ ZX PI METAL LOADER ] ";
    unsigned title_len = (unsigned)strlen(title);
    if (title_len > width) {
        title_len = width;
    }
    const unsigned title_off = (width > title_len) ? (width - title_len) / 2 : 0;
    memcpy(rowbuf + title_off, title, title_len);
    /* Title in Red on White background. */
    m_Screen.Write("\x1b[31m", 5);
    m_Screen.Write("\x1b[47m", 5);
    line.Format("\x1b[%u;%uH%s", start_row, start_col, rowbuf);
    m_Screen.Write((const char *)line, line.GetLength());

    for (unsigned i = 0; i < OSDVisibleRows; i++) {
        const unsigned row = start_row + 2 + i;
        const unsigned entry_index = m_OsdTopRow + i;
        boolean selected = (entry_index == m_SelectedSnapshot);

        for (unsigned c = 0; c < width; c++) {
            rowbuf[c] = ' ';
        }
        rowbuf[width] = '\0';

        if (width > 0) {
            rowbuf[0] = selected ? '>' : ' ';
        }

        if (entry_index == 0) {
            const char *toggle_msg = m_ShowKeyboardLayout
                ? "Keyboard layout: ON (Enter to hide)"
                : "Keyboard layout: OFF (Enter to show)";
            unsigned c = 0;
            while (toggle_msg[c] != '\0' && c + 2 < width) {
                rowbuf[2 + c] = toggle_msg[c];
                c++;
            }
        } else if (m_ShowKeyboardLayout) {
            const unsigned line_index = entry_index - 1;
            if (line_index < KeyboardLayoutLineCount) {
                const char *msg = KeyboardLayoutLines[line_index];
                unsigned c = 0;
                while (msg[c] != '\0' && c + 2 < width) {
                    rowbuf[2 + c] = msg[c];
                    c++;
                }
            }
        } else {
            const unsigned snapshot_index = entry_index - 1;
            if (!m_FileSystemMounted && snapshot_index == 0) {
                const char *msg = "Storage not mounted (emmc1-1)";
                unsigned c = 0;
                while (msg[c] != '\0' && c + 2 < width) {
                    rowbuf[2 + c] = msg[c];
                    c++;
                }
            } else if (m_SnapshotCount == 0 && snapshot_index == 0) {
                const char *msg = "No TAP/TZX/Z80 files in SD root";
                unsigned c = 0;
                while (msg[c] != '\0' && c + 2 < width) {
                    rowbuf[2 + c] = msg[c];
                    c++;
                }
            } else if (snapshot_index < m_SnapshotCount) {
                const char *name = m_SnapshotNames[snapshot_index];
                unsigned c = 0;
                while (name[c] != '\0' && c + 2 < width) {
                    rowbuf[2 + c] = name[c];
                    c++;
                }
            }
        }
        /* Selection in Black on Cyan, normal in Black on White. */
        if (selected) {
            m_Screen.Write("\x1b[30m", 5);
            m_Screen.Write("\x1b[46m", 5);
        } else {
            m_Screen.Write("\x1b[30m", 5);
            m_Screen.Write("\x1b[47m", 5);
        }
        line.Format("\x1b[%u;%uH%s", row, start_col, rowbuf);
        m_Screen.Write((const char *)line, line.GetLength());
    }

    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    line.Format("Up/Down select  Enter toggle/load  F3/F4 tape  F6 turbo  [%s]",
                m_ZX.machine_128k ? "128K" : "48K");
    const char *help = (const char *)line;
    unsigned help_len = (unsigned)strlen(help);
    if (help_len > width) help_len = width;
    unsigned help_off = (width > help_len) ? (width - help_len) / 2 : 0;

    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    memcpy(rowbuf + help_off, help, help_len);
    /* Help text in Blue on White. */
    m_Screen.Write("\x1b[34m", 5);
    m_Screen.Write("\x1b[47m", 5);
    line.Format("\x1b[%u;%uH%s", start_row + 2 + OSDVisibleRows + 1, start_col, rowbuf);
    m_Screen.Write((const char *)line, line.GetLength());

    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    unsigned status_len = (unsigned)strlen(m_OsdStatus);
    if (status_len > width) status_len = width;
    unsigned status_off = (width > status_len) ? (width - status_len) / 2 : 0;
    memcpy(rowbuf + status_off, m_OsdStatus, status_len);
    /* Status bar in Black on White. */
    m_Screen.Write("\x1b[30m", 5);
    m_Screen.Write("\x1b[47m", 5);
    line.Format("\x1b[%u;%uH%s", start_row + 2 + OSDVisibleRows + 2, start_col, rowbuf);
    m_Screen.Write((const char *)line, line.GetLength());

    /* Reset color and hide cursor. */
    m_Screen.Write("\x1b[0m", 4);
    m_Screen.Write("\x1b[?25l", 6);
}
