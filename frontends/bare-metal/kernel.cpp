#include "kernel.h"
#include "../../src/sinclair_font.h"
#include <circle/string.h>
#include <circle/usb/usbhid.h>
#include <circle/usb/usbgamepad.h>
#include <circle/usb/usbgamepadxbox360.h>
#include <string.h>

extern "C" {
#include "../../src/rom.h"
#include "../../src/splash.h"
}

static const char FromKernel[] = "zx-circle";
static const char *const KeyboardLayoutLines[] = {
    "PC -> ZX quick reference",
    "Shift -> CAPS SHIFT",
    "Ctrl  -> SYMBOL SHIFT",
    "A..Z / 0..9 -> same keys",
    "Enter -> ENTER",
    "Space -> BREAK/SPACE",
    "Backspace -> CAPS SHIFT + 0",
    "Arrows -> Kempston U/D/L/R",
    "Tab -> Kempston FIRE",
    "F2 reset  F3 play tape",
    "F4 stop tape  F6 turbo"
};
static const unsigned KeyboardLayoutLineCount =
    sizeof(KeyboardLayoutLines) / sizeof(KeyboardLayoutLines[0]);

static char zx_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - ('a' - 'A')) : c;
}

/* USB HID boot keyboard usage IDs we need. */
enum
{
    HID_A = 0x04,
    HID_B = 0x05,
    HID_C = 0x06,
    HID_D = 0x07,
    HID_E = 0x08,
    HID_F = 0x09,
    HID_G = 0x0A,
    HID_H = 0x0B,
    HID_I = 0x0C,
    HID_J = 0x0D,
    HID_K = 0x0E,
    HID_L = 0x0F,
    HID_M = 0x10,
    HID_N = 0x11,
    HID_O = 0x12,
    HID_P = 0x13,
    HID_Q = 0x14,
    HID_R = 0x15,
    HID_S = 0x16,
    HID_T = 0x17,
    HID_U = 0x18,
    HID_V = 0x19,
    HID_W = 0x1A,
    HID_X = 0x1B,
    HID_Y = 0x1C,
    HID_Z = 0x1D,
    HID_1 = 0x1E,
    HID_2 = 0x1F,
    HID_3 = 0x20,
    HID_4 = 0x21,
    HID_5 = 0x22,
    HID_6 = 0x23,
    HID_7 = 0x24,
    HID_8 = 0x25,
    HID_9 = 0x26,
    HID_0 = 0x27,
    HID_ESCAPE = 0x29,
    HID_ENTER = 0x28,
    HID_BACKSPACE = 0x2A,
    HID_TAB = 0x2B,
    HID_SPACE = 0x2C,
    HID_F1 = 0x3A,
    HID_F2 = 0x3B,
    HID_F3 = 0x3C,
    HID_F4 = 0x3D,
    HID_F6 = 0x3F,
    HID_RIGHT = 0x4F,
    HID_LEFT = 0x50,
    HID_DOWN = 0x51,
    HID_UP = 0x52
};

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel(void)
    :   m_Screen(m_Options.GetWidth(), m_Options.GetHeight(), Font12x22),
    m_Timer(&m_Interrupt),
    m_Logger(m_Options.GetLogLevel(), &m_Timer),
    m_SoundOut(&m_Interrupt, ZX_AUDIO_RATE, 384 * 10),
    m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
    m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
    m_pKeyboard(0),
    m_pGamePad(0),
    m_ShutdownMode(ShutdownNone),
    m_DrawX(0),
    m_DrawY(0),
    m_Scale(1),
    m_InputSequence(0),
    m_InputModifiers(0),
    m_AppliedSequence(0),
    m_GamePadSequence(0),
    m_AppliedGamePadSequence(0),
    m_LastGamePadButtons(0),
    m_LastGamePadLeftX(128),
    m_LastGamePadLeftY(128),
    m_JoyPressed(0),
    m_F2Pressed(FALSE),
    m_ResetRequested(FALSE),
    m_F1Pressed(FALSE),
    m_OsdActive(FALSE),
    m_OsdDirty(FALSE),
    m_FileSystemMounted(FALSE),
    m_ShowKeyboardLayout(FALSE),
    m_SnapshotCount(0),
    m_SelectedSnapshot(0),
    m_OsdTopRow(0),
    m_pSnapshotBuffer(0),
    m_pTapeBuffer(0),
    m_p128KROM0(0),
    m_p128KROM1(0),
    m_Have128KROM0(FALSE),
    m_Have128KROM1(FALSE),
    m_TapeLoaded(FALSE),
    m_F3Pressed(FALSE),
    m_F4Pressed(FALSE),
    m_F6Pressed(FALSE),
    m_FastTapeMode(TRUE),
    m_TurboFrameCounter(0),
    m_AudioActive(FALSE),
    m_AudioArmed(FALSE)
{
    memset((void *)m_InputKeys, 0, sizeof(m_InputKeys));
    memset(m_LastInputKeys, 0, sizeof(m_LastInputKeys));
    memset((void *)&m_GamePadState, 0, sizeof(m_GamePadState));
    memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
    memset(m_Framebuffer, 0, sizeof(m_Framebuffer));
    memset(m_SnapshotNames, 0, sizeof(m_SnapshotNames));
    m_OsdStatus[0] = '\0';
    m_pSnapshotBuffer = new uint8_t[SnapshotBufferSize];
    m_pTapeBuffer = new uint8_t[TapeBufferSize];
    m_p128KROM0 = new uint8_t[ROM128Size];
    m_p128KROM1 = new uint8_t[ROM128Size];
    memset(&m_Tape, 0, sizeof(m_Tape));
    s_pThis = this;
    m_ActLED.Blink(5);
}

CKernel::~CKernel(void)
{
    delete[] m_pSnapshotBuffer;
    delete[] m_pTapeBuffer;
    delete[] m_p128KROM0;
    delete[] m_p128KROM1;
    s_pThis = 0;
}

boolean CKernel::Initialize(void)
{
    boolean ok = TRUE;

    if (ok) ok = m_Screen.Initialize();
    if (ok) ok = m_Serial.Initialize(115200);

    if (ok) ok = m_Logger.Initialize(&m_Serial);

    if (ok) ok = m_Interrupt.Initialize();
    if (ok) ok = m_Timer.Initialize();
    if (ok) {
        if (m_SoundOut.AllocateQueue(150)) {
            m_SoundOut.SetWriteFormat(SoundFormatSigned16, 1);
            if (m_SoundOut.Start()) {
                m_AudioActive = TRUE;
            } else {
                m_Logger.Write(FromKernel, LogWarning, "HDMI audio unavailable; continuing without sound");
            }
        } else {
            m_Logger.Write(FromKernel, LogWarning, "Cannot allocate HDMI audio queue; continuing without sound");
        }
    }
    if (ok) ok = m_EMMC.Initialize();
    if (ok) ok = m_USBHCI.Initialize();

    if (!ok) return FALSE;

    if (m_Screen.GetWidth() < ZX_FB_WIDTH || m_Screen.GetHeight() < ZX_FB_HEIGHT) {
        m_Logger.Write(FromKernel, LogPanic,
                       "Screen too small for 320x256 Spectrum output (%ux%u)",
                       m_Screen.GetWidth(), m_Screen.GetHeight());
        return FALSE;
    }

    m_Scale = m_Screen.GetWidth() / ZX_FB_WIDTH;
    const unsigned scale_y = m_Screen.GetHeight() / ZX_FB_HEIGHT;
    if (scale_y < m_Scale) {
        m_Scale = scale_y;
    }
    if (m_Scale == 0) {
        m_Scale = 1;
    }

    const unsigned draw_w = ZX_FB_WIDTH * m_Scale;
    const unsigned draw_h = ZX_FB_HEIGHT * m_Scale;
    m_DrawX = (m_Screen.GetWidth() - draw_w) / 2;
    m_DrawY = (m_Screen.GetHeight() - draw_h) / 2;

    CDevice *pPartition = m_DeviceNameService.GetDevice("emmc1-1", TRUE);
    if (pPartition != 0 && m_FileSystem.Mount(pPartition)) {
        m_FileSystemMounted = TRUE;
        m_Logger.Write(FromKernel, LogNotice, "Snapshot storage mounted: emmc1-1");
        LoadOptional128KROMs();
    } else {
        m_Logger.Write(FromKernel, LogWarning, "Cannot mount emmc1-1 (.z80 loader disabled)");
    }

    ResetSpectrum();
    return TRUE;
}

TShutdownMode CKernel::Run(void)
{
    m_Logger.Write(FromKernel, LogNotice, "ZX Spectrum bare-metal frontend (Circle)");
    m_Logger.Write(FromKernel, LogNotice, "Attach USB keyboard. F1 OSD, F2 reset, F3 play tape, F4 stop tape, F6 turbo.");

    /* Show animated splash screen. */
    RenderSplashWithFade();

    while (m_ShutdownMode == ShutdownNone) {
        const boolean updated = m_USBHCI.UpdatePlugAndPlay();

    if (updated && m_pKeyboard == 0) {
            m_pKeyboard = (CUSBKeyboardDevice *)m_DeviceNameService.GetDevice("ukbd1", FALSE);
            if (m_pKeyboard != 0) {
                m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler);
                m_pKeyboard->RegisterKeyStatusHandlerRaw(KeyboardRawHandler, FALSE);
                m_Logger.Write(FromKernel, LogNotice, "USB keyboard connected.");
            }
        }

        if (updated && m_pGamePad == 0) {
            m_pGamePad = (CUSBGamePadDevice *)m_DeviceNameService.GetDevice("upad", 1, FALSE);
            if (m_pGamePad != 0) {
                m_pGamePad->RegisterRemovedHandler(GamePadRemovedHandler);
                m_pGamePad->RegisterStatusHandler(GamePadStatusHandler);

                const TGamePadState *pState = m_pGamePad->GetInitialState();
                if (pState != 0) {
                    GamePadStatusHandler(0, pState);
                }

                m_Logger.Write(FromKernel, LogNotice, "USB gamepad connected.");
            }
        }

        if (m_InputSequence != m_AppliedSequence || m_GamePadSequence != m_AppliedGamePadSequence) {
            ApplyInputReport();
        }

        if (m_ResetRequested) {
            m_ResetRequested = FALSE;
            ResetSpectrum();
            m_Logger.Write(FromKernel, LogNotice, "ZX reset");
        }

        if (m_OsdActive) {
            if (m_OsdDirty) {
                RenderOSD();
                m_OsdDirty = FALSE;
            }
            if (m_AudioActive) {
                memset(m_ZX.audio_buffer, 0, sizeof(m_ZX.audio_buffer));
                (void)m_SoundOut.Write(m_ZX.audio_buffer, (size_t)(ZX_AUDIO_SAMPLES * sizeof(int16_t)));
            }
            CTimer::SimpleusDelay(5000);
            continue;
        }

        const u64 frame_start = CTimer::GetClockTicks64();

        const boolean tape_playing = (m_TapeLoaded && tzx_is_playing(&m_Tape));
        const boolean turbo_tape = (tape_playing && m_FastTapeMode);

        if (tape_playing) {
            tzx_set_machine(&m_Tape, m_ZX.machine_128k);
            do {
                zx_set_ear(&m_ZX, tzx_update(&m_Tape, m_ZX.cpu.clocks));
            } while (!zx_tick(&m_ZX, 0));
        } else {
            zx_frame(&m_ZX);
        }
        if (!turbo_tape) {
            m_TurboFrameCounter = 0;
            BlitSpectrumFramebuffer();
            if (m_AudioActive) {
                const int bytes = (int)(ZX_AUDIO_SAMPLES * sizeof(int16_t));
                if (m_AudioArmed) {
                    (void)m_SoundOut.Write(m_ZX.audio_buffer, (size_t)bytes);
                } else {
                    memset(m_ZX.audio_buffer, 0, sizeof(m_ZX.audio_buffer));
                    (void)m_SoundOut.Write(m_ZX.audio_buffer, (size_t)bytes);
                }
            }
        } else {
            m_TurboFrameCounter++;
            if ((m_TurboFrameCounter & 7) == 0) {
                BlitSpectrumFramebuffer();
            }
        }

        const u64 elapsed = CTimer::GetClockTicks64() - frame_start;
        if (!turbo_tape && elapsed < 20000) {
            CTimer::SimpleusDelay((unsigned)(20000 - elapsed));
        }
    }

    return m_ShutdownMode;
}

void CKernel::KeyboardRawHandler(unsigned char modifiers, const unsigned char raw_keys[6])
{
    if (s_pThis == 0) {
        return;
    }

    s_pThis->m_InputModifiers = modifiers;
    for (unsigned i = 0; i < 6; i++) {
        s_pThis->m_InputKeys[i] = raw_keys[i];
    }
    s_pThis->m_InputSequence++;
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext)
{
    (void)pDevice;
    (void)pContext;
    if (s_pThis == 0) {
        return;
    }
    CLogger::Get()->Write(FromKernel, LogNotice, "USB keyboard removed.");
    s_pThis->m_pKeyboard = 0;
}

void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState)
{
    (void)nDeviceIndex;
    if (s_pThis == 0 || pState == 0) {
        return;
    }

    s_pThis->m_GamePadState = *pState;
    s_pThis->m_GamePadSequence++;
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext)
{
    (void)pDevice;
    (void)pContext;
    if (s_pThis == 0) {
        return;
    }

    CLogger::Get()->Write(FromKernel, LogNotice, "USB gamepad removed.");
    s_pThis->m_pGamePad = 0;
    memset((void *)&s_pThis->m_GamePadState, 0, sizeof(s_pThis->m_GamePadState));
    s_pThis->m_GamePadSequence++;
}

void CKernel::SetZXKeyState(int row, int bit, boolean pressed)
{
    if (row < 0 || row >= 8 || bit < 0 || bit >= 5) {
        return;
    }
    if (pressed == m_KeyPressed[row][bit]) {
        return;
    }
    m_KeyPressed[row][bit] = pressed;
    if (pressed) {
        zx_key_down(&m_ZX, row, bit);
    } else {
        zx_key_up(&m_ZX, row, bit);
    }
}

void CKernel::SetZXJoyState(unsigned mask, boolean pressed)
{
    const boolean was_pressed = (m_JoyPressed & mask) != 0;
    if (pressed == was_pressed) {
        return;
    }
    if (pressed) {
        m_JoyPressed |= mask;
        zx_joy_down(&m_ZX, mask);
    } else {
        m_JoyPressed &= (unsigned char)~mask;
        zx_joy_up(&m_ZX, mask);
    }
}

void CKernel::ApplyInputReport(void)
{
    unsigned char modifiers;
    unsigned char raw_keys[6];
    unsigned first_seq;
    unsigned second_seq;

    do {
        first_seq = m_InputSequence;
        modifiers = m_InputModifiers;
        for (unsigned i = 0; i < 6; i++) {
            raw_keys[i] = m_InputKeys[i];
        }
        second_seq = m_InputSequence;
    } while (first_seq != second_seq);

    m_AppliedSequence = second_seq;

    const boolean f1_now = HasRawKey(raw_keys, HID_F1);
    if (f1_now && !m_F1Pressed) {
        ToggleOSD();
    }
    m_F1Pressed = f1_now;

    const boolean f3_now = HasRawKey(raw_keys, HID_F3);
    if (f3_now && !m_F3Pressed && m_TapeLoaded) {
        tzx_play(&m_Tape, m_ZX.cpu.clocks);
        strncpy(m_OsdStatus, "Tape playing", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        m_Logger.Write(FromKernel, LogNotice, "Tape: playing");
    }
    m_F3Pressed = f3_now;

    const boolean f4_now = HasRawKey(raw_keys, HID_F4);
    if (f4_now && !m_F4Pressed && m_TapeLoaded) {
        tzx_stop(&m_Tape);
        zx_set_ear(&m_ZX, 0);
        strncpy(m_OsdStatus, "Tape stopped", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        m_Logger.Write(FromKernel, LogNotice, "Tape: stopped");
    }
    m_F4Pressed = f4_now;

    const boolean f6_now = HasRawKey(raw_keys, HID_F6);
    if (f6_now && !m_F6Pressed) {
        m_FastTapeMode = !m_FastTapeMode;
        tzx_set_speedup(&m_Tape, m_FastTapeMode ? 4 : 1);
        if (m_FastTapeMode) {
            strncpy(m_OsdStatus, "Turbo tape: 4x", sizeof(m_OsdStatus) - 1);
            m_Logger.Write(FromKernel, LogNotice, "Turbo tape mode enabled");
        } else {
            strncpy(m_OsdStatus, "Turbo tape: OFF", sizeof(m_OsdStatus) - 1);
            m_Logger.Write(FromKernel, LogNotice, "Turbo tape mode disabled");
        }
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        m_OsdDirty = TRUE;
    }
    m_F6Pressed = f6_now;

    if (m_OsdActive) {
        if (HasRawKey(raw_keys, HID_ESCAPE) && !HasRawKey(m_LastInputKeys, HID_ESCAPE)) {
            ToggleOSD();
        } else if (HasRawKey(raw_keys, HID_UP) && !HasRawKey(m_LastInputKeys, HID_UP)) {
            OSDMoveSelection(-1);
        } else if (HasRawKey(raw_keys, HID_DOWN) && !HasRawKey(m_LastInputKeys, HID_DOWN)) {
            OSDMoveSelection(1);
        } else if (HasRawKey(raw_keys, HID_ENTER) && !HasRawKey(m_LastInputKeys, HID_ENTER)) {
            if (OSDActivateSelection()) {
                m_OsdActive = FALSE;
            }
        }
    }

    for (unsigned i = 0; i < 6; i++) {
        m_LastInputKeys[i] = raw_keys[i];
    }

    TGamePadState gamepad_state;
    unsigned gamepad_first;
    unsigned gamepad_second;
    do {
        gamepad_first = m_GamePadSequence;
        gamepad_state = m_GamePadState;
        gamepad_second = m_GamePadSequence;
    } while (gamepad_first != gamepad_second);

    m_AppliedGamePadSequence = gamepad_second;
    const unsigned gamepad_buttons = gamepad_state.buttons;
    const unsigned gamepad_changed = gamepad_buttons ^ m_LastGamePadButtons;

    boolean desired_keys[8][5];
    memset(desired_keys, 0, sizeof(desired_keys));

    unsigned desired_joy = 0;
    boolean f2_now = FALSE;

    if (!m_OsdActive) {
        if (modifiers & (LSHIFT | RSHIFT)) {
            desired_keys[0][0] = TRUE; /* CAPS SHIFT */
        }
        if (modifiers & (LCTRL | RCTRL)) {
            desired_keys[7][1] = TRUE; /* SYMBOL SHIFT */
        }

        for (unsigned i = 0; i < 6; i++) {
            switch (raw_keys[i]) {
            case HID_A: desired_keys[1][0] = TRUE; break;
            case HID_B: desired_keys[7][4] = TRUE; break;
            case HID_C: desired_keys[0][3] = TRUE; break;
            case HID_D: desired_keys[1][2] = TRUE; break;
            case HID_E: desired_keys[2][2] = TRUE; break;
            case HID_F: desired_keys[1][3] = TRUE; break;
            case HID_G: desired_keys[1][4] = TRUE; break;
            case HID_H: desired_keys[6][4] = TRUE; break;
            case HID_I: desired_keys[5][2] = TRUE; break;
            case HID_J: desired_keys[6][3] = TRUE; break;
            case HID_K: desired_keys[6][2] = TRUE; break;
            case HID_L: desired_keys[6][1] = TRUE; break;
            case HID_M: desired_keys[7][2] = TRUE; break;
            case HID_N: desired_keys[7][3] = TRUE; break;
            case HID_O: desired_keys[5][1] = TRUE; break;
            case HID_P: desired_keys[5][0] = TRUE; break;
            case HID_Q: desired_keys[2][0] = TRUE; break;
            case HID_R: desired_keys[2][3] = TRUE; break;
            case HID_S: desired_keys[1][1] = TRUE; break;
            case HID_T: desired_keys[2][4] = TRUE; break;
            case HID_U: desired_keys[5][3] = TRUE; break;
            case HID_V: desired_keys[0][4] = TRUE; break;
            case HID_W: desired_keys[2][1] = TRUE; break;
            case HID_X: desired_keys[0][2] = TRUE; break;
            case HID_Y: desired_keys[5][4] = TRUE; break;
            case HID_Z: desired_keys[0][1] = TRUE; break;

            case HID_1: desired_keys[3][0] = TRUE; break;
            case HID_2: desired_keys[3][1] = TRUE; break;
            case HID_3: desired_keys[3][2] = TRUE; break;
            case HID_4: desired_keys[3][3] = TRUE; break;
            case HID_5: desired_keys[3][4] = TRUE; break;
            case HID_6: desired_keys[4][4] = TRUE; break;
            case HID_7: desired_keys[4][3] = TRUE; break;
            case HID_8: desired_keys[4][2] = TRUE; break;
            case HID_9: desired_keys[4][1] = TRUE; break;
            case HID_0: desired_keys[4][0] = TRUE; break;

            case HID_ENTER: desired_keys[6][0] = TRUE; break;
            case HID_SPACE: desired_keys[7][0] = TRUE; break;

            case HID_BACKSPACE:
                desired_keys[0][0] = TRUE; /* CAPS SHIFT */
                desired_keys[4][0] = TRUE; /* 0 */
                break;

            case HID_TAB: desired_joy |= ZX_JOY_FIRE; break;
            case HID_UP: desired_joy |= ZX_JOY_UP; break;
            case HID_DOWN: desired_joy |= ZX_JOY_DOWN; break;
            case HID_LEFT: desired_joy |= ZX_JOY_LEFT; break;
            case HID_RIGHT: desired_joy |= ZX_JOY_RIGHT; break;

            case HID_F2:
                f2_now = TRUE;
                break;

            default:
                break;
            }
        }

    }

    const int left_x = (gamepad_state.naxes > GamePadAxisLeftX) ? gamepad_state.axes[GamePadAxisLeftX].value : 128;
    const int left_y = (gamepad_state.naxes > GamePadAxisLeftY) ? gamepad_state.axes[GamePadAxisLeftY].value : 128;
    const boolean left_left = left_x < 96;
    const boolean left_right = left_x > 160;
    const boolean left_up = left_y < 96;
    const boolean left_down = left_y > 160;

    if (!m_OsdActive) {
        if ((gamepad_buttons & GamePadButtonLeft) || left_left) {
            desired_joy |= ZX_JOY_LEFT;
        }
        if ((gamepad_buttons & GamePadButtonRight) || left_right) {
            desired_joy |= ZX_JOY_RIGHT;
        }
        if ((gamepad_buttons & GamePadButtonUp) || left_up) {
            desired_joy |= ZX_JOY_UP;
        }
        if ((gamepad_buttons & GamePadButtonDown) || left_down) {
            desired_joy |= ZX_JOY_DOWN;
        }

        if (gamepad_buttons & (GamePadButtonA | GamePadButtonB | GamePadButtonRB | GamePadButtonRT)) {
            desired_joy |= ZX_JOY_FIRE;
        }
    }

    if ((gamepad_buttons & GamePadButtonStart) && (gamepad_changed & GamePadButtonStart)) {
        if (m_OsdActive) {
            if (OSDActivateSelection()) {
                m_OsdActive = FALSE;
            }
        } else {
            ToggleOSD();
        }
    }

    if ((gamepad_buttons & GamePadButtonBack) && (gamepad_changed & GamePadButtonBack)) {
        if (m_OsdActive) {
            ToggleOSD();
        } else {
            m_ResetRequested = TRUE;
        }
    }

    if (m_OsdActive) {
        if ((gamepad_buttons & GamePadButtonA) && (gamepad_changed & GamePadButtonA)) {
            if (OSDActivateSelection()) {
                m_OsdActive = FALSE;
            }
        }
        const boolean osd_up_now = ((gamepad_buttons & GamePadButtonUp) != 0) || left_up;
        const boolean osd_down_now = ((gamepad_buttons & GamePadButtonDown) != 0) || left_down;
        const boolean osd_up_prev = ((m_LastGamePadButtons & GamePadButtonUp) != 0) ||
            (m_LastGamePadLeftY < 96);
        const boolean osd_down_prev = ((m_LastGamePadButtons & GamePadButtonDown) != 0) ||
            (m_LastGamePadLeftY > 160);

        if (osd_up_now && !osd_up_prev) {
            OSDMoveSelection(-1);
        }
        if (osd_down_now && !osd_down_prev) {
            OSDMoveSelection(1);
        }
    }

    for (int row = 0; row < 8; row++) {
        for (int bit = 0; bit < 5; bit++) {
            SetZXKeyState(row, bit, desired_keys[row][bit]);
        }
    }

    SetZXJoyState(ZX_JOY_RIGHT, (desired_joy & ZX_JOY_RIGHT) != 0);
    SetZXJoyState(ZX_JOY_LEFT, (desired_joy & ZX_JOY_LEFT) != 0);
    SetZXJoyState(ZX_JOY_DOWN, (desired_joy & ZX_JOY_DOWN) != 0);
    SetZXJoyState(ZX_JOY_UP, (desired_joy & ZX_JOY_UP) != 0);
    SetZXJoyState(ZX_JOY_FIRE, (desired_joy & ZX_JOY_FIRE) != 0);

    if (f2_now && !m_F2Pressed) {
        m_ResetRequested = TRUE;
    }
    m_F2Pressed = f2_now;
    m_LastGamePadButtons = gamepad_buttons;
    m_LastGamePadLeftX = left_x;
    m_LastGamePadLeftY = left_y;
}

void CKernel::ResetSpectrum(void)
{
    memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
    m_JoyPressed = 0;
    m_F2Pressed = FALSE;
    m_F3Pressed = FALSE;
    m_F4Pressed = FALSE;
    m_F6Pressed = FALSE;
    m_TurboFrameCounter = 0;
    zx_init(&m_ZX, zx_spectrum_rom);
    if (m_Have128KROM0) {
        m_ZX.machine_128k = 1;
        zx_set_128k_roms(&m_ZX,
                         m_p128KROM0,
                         m_Have128KROM1 ? m_p128KROM1 : 0);
    }
    zx_set_framebuffer(&m_ZX, m_Framebuffer);
    if (m_TapeLoaded) {
        tzx_stop(&m_Tape);
    }
}

void CKernel::LoadOptional128KROMs(void)
{
    if (!m_FileSystemMounted || m_p128KROM0 == 0 || m_p128KROM1 == 0) {
        return;
    }

    struct TRomSpec {
        const char *name;
        uint8_t *buffer;
        boolean *loaded;
    } roms[] = {
        {"128-0.rom", m_p128KROM0, &m_Have128KROM0},
        {"128-1.rom", m_p128KROM1, &m_Have128KROM1},
    };

    for (unsigned i = 0; i < sizeof(roms) / sizeof(roms[0]); i++) {
        *roms[i].loaded = FALSE;
        unsigned file = m_FileSystem.FileOpen(roms[i].name);
        if (file == 0) {
            continue;
        }

        unsigned total = 0;
        while (total < ROM128Size) {
            unsigned got = m_FileSystem.FileRead(file, roms[i].buffer + total, ROM128Size - total);
            if (got == 0) {
                break;
            }
            if (got == FS_ERROR) {
                total = 0;
                break;
            }
            total += got;
        }
        m_FileSystem.FileClose(file);

        if (total == ROM128Size) {
            *roms[i].loaded = TRUE;
            m_Logger.Write(FromKernel, LogNotice, "Loaded optional 128K ROM: %s", roms[i].name);
        } else {
            m_Logger.Write(FromKernel, LogWarning,
                           "Ignoring %s (need exactly %u bytes)",
                           roms[i].name, ROM128Size);
        }
    }
}

void CKernel::BlitSpectrumFramebuffer(void)
{
#ifndef SCREEN_HEADLESS
    CBcmFrameBuffer *pFB = m_Screen.GetFrameBuffer();
    if (pFB != 0) {
        const u32 pitch = pFB->GetPitch();
        unsigned char *const base =
            reinterpret_cast<unsigned char *>((uintptr_t)pFB->GetBuffer());
        if (base != 0 && pitch != 0) {
            for (unsigned y = 0; y < ZX_FB_HEIGHT; y++) {
                const uint8_t *src = &m_Framebuffer[y * ZX_FB_WIDTH * 3];
                const unsigned py0 = m_DrawY + y * m_Scale;
                for (unsigned dy = 0; dy < m_Scale; dy++) {
#if DEPTH == 16
                    uint16_t *dst = reinterpret_cast<uint16_t *>(
                        base + (size_t)(py0 + dy) * pitch + (size_t)m_DrawX * sizeof(uint16_t));
                    for (unsigned x = 0; x < ZX_FB_WIDTH; x++) {
                        const uint16_t color = (uint16_t)COLOR16(src[0] >> 3, src[1] >> 3, src[2] >> 3);
                        for (unsigned dx = 0; dx < m_Scale; dx++) {
                            *dst++ = color;
                        }
                        src += 3;
                    }
#elif DEPTH == 32
                    uint32_t *dst = reinterpret_cast<uint32_t *>(
                        base + (size_t)(py0 + dy) * pitch + (size_t)m_DrawX * sizeof(uint32_t));
                    for (unsigned x = 0; x < ZX_FB_WIDTH; x++) {
                        const uint32_t color = (uint32_t)COLOR32(src[0], src[1], src[2], 255);
                        for (unsigned dx = 0; dx < m_Scale; dx++) {
                            *dst++ = color;
                        }
                        src += 3;
                    }
#else
                    uint8_t *dst = base + (size_t)(py0 + dy) * pitch + m_DrawX;
                    for (unsigned x = 0; x < ZX_FB_WIDTH; x++) {
                        const uint8_t color = src[0] ? BRIGHT_WHITE_COLOR : BLACK_COLOR;
                        for (unsigned dx = 0; dx < m_Scale; dx++) {
                            *dst++ = color;
                        }
                        src += 3;
                    }
#endif
                    src -= ZX_FB_WIDTH * 3;
                }
            }
            return;
        }
    }
#endif

    for (unsigned y = 0; y < ZX_FB_HEIGHT; y++) {
        const uint8_t *src = &m_Framebuffer[y * ZX_FB_WIDTH * 3];
        const unsigned py0 = m_DrawY + y * m_Scale;
        for (unsigned x = 0; x < ZX_FB_WIDTH; x++) {
            const unsigned px0 = m_DrawX + x * m_Scale;
#if DEPTH == 16
            const TScreenColor color = COLOR16(src[0] >> 3, src[1] >> 3, src[2] >> 3);
#elif DEPTH == 32
            const TScreenColor color = COLOR32(src[0], src[1], src[2], 255);
#else
            const TScreenColor color = src[0] ? BRIGHT_WHITE_COLOR : BLACK_COLOR;
#endif
            for (unsigned dy = 0; dy < m_Scale; dy++) {
                const unsigned py = py0 + dy;
                for (unsigned dx = 0; dx < m_Scale; dx++) {
                    m_Screen.SetPixel(px0 + dx, py, color);
                }
            }
            src += 3;
        }
    }
}

void CKernel::RenderSplashWithFade(void)
{
    const unsigned FadeSteps = 32;
    const unsigned FadeStepUs = 500000 / FadeSteps;

    /* Fade in */
    for (unsigned step = 0; step <= FadeSteps; step++) {
        unsigned alpha = (step * 256) / FadeSteps;
        for (unsigned i = 0; i < sizeof(m_Framebuffer); i++) {
            m_Framebuffer[i] = (uint8_t)((unsigned)splash_raw[i] * alpha >> 8);
        }
        BlitSpectrumFramebuffer();
        CTimer::SimpleusDelay(FadeStepUs);
    }

    /* Full visibility for 2 seconds */
    CTimer::SimpleusDelay(2000000);

    /* Fade out */
    for (unsigned step = FadeSteps; step > 0; step--) {
        unsigned alpha = (step * 256) / FadeSteps;
        for (unsigned i = 0; i < sizeof(m_Framebuffer); i++) {
            m_Framebuffer[i] = (uint8_t)((unsigned)splash_raw[i] * alpha >> 8);
        }
        BlitSpectrumFramebuffer();
        CTimer::SimpleusDelay(FadeStepUs);
    }

    memset(m_Framebuffer, 0, sizeof(m_Framebuffer));
    BlitSpectrumFramebuffer();
}

unsigned CKernel::GetOSDEntryCount(void) const
{
    if (m_ShowKeyboardLayout) {
        return 1 + KeyboardLayoutLineCount;
    }
    const unsigned snapshot_rows = (m_FileSystemMounted && m_SnapshotCount > 0) ? m_SnapshotCount : 1;
    return 1 + snapshot_rows;
}

void CKernel::OSDMoveSelection(int delta)
{
    const unsigned total = GetOSDEntryCount();
    if (total == 0 || delta == 0) {
        return;
    }

    int next = (int)m_SelectedSnapshot + delta;
    if (next < 0) {
        next = 0;
    } else if ((unsigned)next >= total) {
        next = (int)total - 1;
    }

    if ((unsigned)next == m_SelectedSnapshot) {
        return;
    }

    m_SelectedSnapshot = (unsigned)next;
    if (m_SelectedSnapshot < m_OsdTopRow) {
        m_OsdTopRow = m_SelectedSnapshot;
    } else if (m_SelectedSnapshot >= m_OsdTopRow + OSDVisibleRows) {
        m_OsdTopRow = m_SelectedSnapshot - OSDVisibleRows + 1;
    }
    m_OsdDirty = TRUE;
}

boolean CKernel::OSDActivateSelection(void)
{
    if (m_SelectedSnapshot == 0) {
        m_ShowKeyboardLayout = !m_ShowKeyboardLayout;
        m_OsdTopRow = 0;
        m_SelectedSnapshot = 0;
        if (m_ShowKeyboardLayout) {
            strncpy(m_OsdStatus, "Keyboard typing reference shown", sizeof(m_OsdStatus) - 1);
        } else {
            strncpy(m_OsdStatus, "Keyboard layout hidden", sizeof(m_OsdStatus) - 1);
        }
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        m_OsdDirty = TRUE;
        return FALSE;
    }

    if (m_ShowKeyboardLayout) {
        return FALSE;
    }

    const unsigned snapshot_index = m_SelectedSnapshot - 1;
    if (snapshot_index >= m_SnapshotCount) {
        return FALSE;
    }

    const char *name = m_SnapshotNames[snapshot_index];
    const boolean loaded = HasZ80Extension(name)
        ? LoadSnapshot(snapshot_index)
        : LoadTape(snapshot_index);
    if (!loaded) {
        m_OsdDirty = TRUE;
    }
    return loaded;
}

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

    /* Set Spectrum colors: Black on White background. (8 bytes) */
    m_Screen.Write("\x1b[30;47m", 8);
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
    line.Format("\x1b[%u;%uH\x1b[31;47m%s\x1b[30;47m", start_row, start_col, rowbuf);
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
            line.Format("\x1b[%u;%uH\x1b[30;46m%s\x1b[30;47m", row, start_col, rowbuf);
        } else {
            line.Format("\x1b[%u;%uH%s", row, start_col, rowbuf);
        }
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
    line.Format("\x1b[%u;%uH\x1b[34;47m%s\x1b[30;47m", start_row + 2 + OSDVisibleRows + 1, start_col, rowbuf);
    m_Screen.Write((const char *)line, line.GetLength());

    for (unsigned i = 0; i < width; i++) {
        rowbuf[i] = ' ';
    }
    unsigned status_len = (unsigned)strlen(m_OsdStatus);
    if (status_len > width) status_len = width;
    unsigned status_off = (width > status_len) ? (width - status_len) / 2 : 0;
    memcpy(rowbuf + status_off, m_OsdStatus, status_len);
    /* Status bar in Black on White. */
    line.Format("\x1b[%u;%uH%s", start_row + 2 + OSDVisibleRows + 2, start_col, rowbuf);
    m_Screen.Write((const char *)line, line.GetLength());

    /* Reset color and hide cursor. */
    m_Screen.Write("\x1b[0m\x1b[?25l", 10);
}

void CKernel::ToggleOSD(void)
{
    m_OsdActive = !m_OsdActive;
    if (m_OsdActive) {
        if (!m_FileSystemMounted) {
            CDevice *pPartition = m_DeviceNameService.GetDevice("emmc1-1", TRUE);
            if (pPartition != 0 && m_FileSystem.Mount(pPartition)) {
                m_FileSystemMounted = TRUE;
                m_Logger.Write(FromKernel, LogNotice, "Snapshot storage mounted: emmc1-1");
            }
        }
        ReloadSnapshots();
        m_ShowKeyboardLayout = FALSE;
        m_SelectedSnapshot = 0;
        m_OsdTopRow = 0;
        strncpy(m_OsdStatus, "Enter toggles keyboard layout or loads file", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        m_OsdDirty = TRUE;
    } else {
        const unsigned cols = m_Screen.GetColumns();
        const unsigned rows = m_Screen.GetRows();
        unsigned draw_col0 = (m_DrawX * cols) / m_Screen.GetWidth();
        unsigned draw_row0 = (m_DrawY * rows) / m_Screen.GetHeight();
        unsigned draw_col1 = ((m_DrawX + ZX_FB_WIDTH * m_Scale) * cols) / m_Screen.GetWidth();
        unsigned draw_row1 = ((m_DrawY + ZX_FB_HEIGHT * m_Scale) * rows) / m_Screen.GetHeight();

        if (draw_col1 <= draw_col0) draw_col1 = draw_col0 + 1;
        if (draw_row1 <= draw_row0 + 2) draw_row1 = draw_row0 + 3;

        unsigned start_col = draw_col0 + 1;
        unsigned start_row = (m_DrawY * rows) / m_Screen.GetHeight() + 2;
        const unsigned total_rows = OSDVisibleRows + 5;
        if (start_row + total_rows + 1 >= draw_row1) {
            start_row = (draw_row1 > total_rows + 1) ? (draw_row1 - total_rows - 1) : 1;
        }
        unsigned end_col = (draw_col1 > 0) ? (draw_col1 - 1) : 0;
        if (start_col < 1) start_col = 1;
        if (end_col > cols) end_col = cols;
        if (end_col < start_col) end_col = start_col;
        unsigned width = end_col - start_col + 1;
        if (width > 128) width = 128;

        CString line;
        char blank[129];
        for (unsigned i = 0; i < width; i++) {
            blank[i] = ' ';
        }
        blank[width] = '\0';
        for (unsigned i = 0; i < total_rows; i++) {
            line.Format("\x1b[%u;%uH%s", start_row + i, start_col, blank);
            m_Screen.Write((const char *)line, line.GetLength());
        }
        m_OsdDirty = FALSE;
    }
}

void CKernel::ReloadSnapshots(void)
{
    m_SnapshotCount = 0;
    m_SelectedSnapshot = 0;
    m_OsdTopRow = 0;
    memset(m_SnapshotNames, 0, sizeof(m_SnapshotNames));

    if (!m_FileSystemMounted) {
        strncpy(m_OsdStatus, "Storage not mounted", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return;
    }

    auto add_snapshot = [this](const char *name) {
        if (name == 0 || name[0] == '\0') {
            return;
        }
        if (!(HasZ80Extension(name) || HasTapeExtension(name))) {
            return;
        }

        unsigned pos = m_SnapshotCount;
        while (pos > 0 && strcmp(name, m_SnapshotNames[pos - 1]) < 0) {
            memcpy(m_SnapshotNames[pos], m_SnapshotNames[pos - 1], sizeof(m_SnapshotNames[pos]));
            pos--;
        }
        strncpy(m_SnapshotNames[pos], name, sizeof(m_SnapshotNames[pos]) - 1);
        m_SnapshotNames[pos][sizeof(m_SnapshotNames[pos]) - 1] = '\0';
        m_SnapshotCount++;
    };

    TDirentry entry;
    TFindCurrentEntry current;
    unsigned found = m_FileSystem.RootFindFirst(&entry, &current);
    while (found != 0 && m_SnapshotCount < MaxSnapshots) {
        add_snapshot(entry.chTitle);
        found = m_FileSystem.RootFindNext(&entry, &current);
    }

    m_Logger.Write(FromKernel, LogNotice, "Snapshots found: %u", m_SnapshotCount);
    if (m_SnapshotCount == 0) {
        strncpy(m_OsdStatus, "No TAP/TZX/Z80 files found", sizeof(m_OsdStatus) - 1);
    } else {
        m_OsdStatus[0] = '\0';
    }
    m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
}

boolean CKernel::LoadSnapshot(unsigned index)
{
    if (!m_FileSystemMounted || index >= m_SnapshotCount || m_pSnapshotBuffer == 0) {
        return FALSE;
    }

    unsigned file = m_FileSystem.FileOpen(m_SnapshotNames[index]);
    if (file == 0) {
        m_Logger.Write(FromKernel, LogError, "Cannot open snapshot: %s", m_SnapshotNames[index]);
        return FALSE;
    }

    unsigned total = 0;
    while (total < SnapshotBufferSize) {
        unsigned got = m_FileSystem.FileRead(file, m_pSnapshotBuffer + total, SnapshotBufferSize - total);
        if (got == 0) {
            break;
        }
        if (got == FS_ERROR) {
            m_FileSystem.FileClose(file);
            m_Logger.Write(FromKernel, LogError, "Read failed: %s", m_SnapshotNames[index]);
            return FALSE;
        }
        total += got;
    }
    m_FileSystem.FileClose(file);

    if (total == SnapshotBufferSize) {
        m_Logger.Write(FromKernel, LogError, "Snapshot too big: %s", m_SnapshotNames[index]);
        strncpy(m_OsdStatus, "Snapshot too large", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    if (total < 30) {
        m_Logger.Write(FromKernel, LogError, "Snapshot too small: %s", m_SnapshotNames[index]);
        strncpy(m_OsdStatus, "Invalid snapshot (too small)", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    const uint16_t pc = (uint16_t)(m_pSnapshotBuffer[6] | (m_pSnapshotBuffer[7] << 8));
    if (pc == 0) {
        if (total < 35) {
            strncpy(m_OsdStatus, "Invalid extended .z80 header", sizeof(m_OsdStatus) - 1);
            m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
            return FALSE;
        }
    }

    if (zx_load_z80(&m_ZX, m_pSnapshotBuffer, (int)total) != 0) {
        m_Logger.Write(FromKernel, LogError, "Invalid .z80 snapshot: %s", m_SnapshotNames[index]);
        strncpy(m_OsdStatus, "Failed to parse .z80 snapshot", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
    m_JoyPressed = 0;
    m_Logger.Write(FromKernel, LogNotice, "Loaded snapshot: %s", m_SnapshotNames[index]);
    strncpy(m_OsdStatus, "Snapshot loaded", sizeof(m_OsdStatus) - 1);
    m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
    m_AudioArmed = TRUE;
    return TRUE;
}

boolean CKernel::LoadTape(unsigned index)
{
    if (!m_FileSystemMounted || index >= m_SnapshotCount || m_pTapeBuffer == 0) {
        return FALSE;
    }

    unsigned file = m_FileSystem.FileOpen(m_SnapshotNames[index]);
    if (file == 0) {
        m_Logger.Write(FromKernel, LogError, "Cannot open tape: %s", m_SnapshotNames[index]);
        strncpy(m_OsdStatus, "Cannot open tape file", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    unsigned total = 0;
    while (total < TapeBufferSize) {
        unsigned got = m_FileSystem.FileRead(file, m_pTapeBuffer + total, TapeBufferSize - total);
        if (got == 0) {
            break;
        }
        if (got == FS_ERROR) {
            m_FileSystem.FileClose(file);
            m_Logger.Write(FromKernel, LogError, "Read failed: %s", m_SnapshotNames[index]);
            strncpy(m_OsdStatus, "Tape read failed", sizeof(m_OsdStatus) - 1);
            m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
            return FALSE;
        }
        total += got;
    }
    m_FileSystem.FileClose(file);

    if (total == 0 || total == TapeBufferSize) {
        strncpy(m_OsdStatus, "Invalid tape size", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    if (tzx_load(&m_Tape, m_pTapeBuffer, (int)total) != 0) {
        m_Logger.Write(FromKernel, LogError, "Invalid tape file: %s", m_SnapshotNames[index]);
        strncpy(m_OsdStatus, "Invalid TAP/TZX file", sizeof(m_OsdStatus) - 1);
        m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
        return FALSE;
    }

    m_TapeLoaded = TRUE;
    tzx_stop(&m_Tape);
    tzx_set_speedup(&m_Tape, m_FastTapeMode ? 4 : 1);
    strncpy(m_OsdStatus, "Tape loaded. Type LOAD \"\" then F3", sizeof(m_OsdStatus) - 1);
    m_OsdStatus[sizeof(m_OsdStatus) - 1] = '\0';
    m_Logger.Write(FromKernel, LogNotice, "Tape loaded: %s", m_SnapshotNames[index]);
    m_AudioArmed = TRUE;
    return TRUE;
}

boolean CKernel::HasRawKey(const unsigned char raw_keys[6], unsigned char key)
{
    for (unsigned i = 0; i < 6; i++) {
        if (raw_keys[i] == key) {
            return TRUE;
        }
    }
    return FALSE;
}

boolean CKernel::HasZ80Extension(const char *name)
{
    if (name == 0) {
        return FALSE;
    }

    char normalized[FS_TITLE_LEN + 1];
    unsigned out = 0;
    for (unsigned i = 0; name[i] != '\0' && out < FS_TITLE_LEN; i++) {
        if (name[i] != ' ') {
            normalized[out++] = zx_upper(name[i]);
        }
    }
    normalized[out] = '\0';

    if (out < 3) {
        return FALSE;
    }

    for (unsigned i = 0; i + 3 < out; i++) {
        if (normalized[i] == '.' &&
            normalized[i + 1] == 'Z' &&
            normalized[i + 2] == '8' &&
            normalized[i + 3] == '0') {
            return TRUE;
        }
    }

    return out >= 3 &&
           normalized[out - 3] == 'Z' &&
           normalized[out - 2] == '8' &&
           normalized[out - 1] == '0';
}

boolean CKernel::HasTapeExtension(const char *name)
{
    if (name == 0) {
        return FALSE;
    }

    char normalized[FS_TITLE_LEN + 1];
    unsigned out = 0;
    for (unsigned i = 0; name[i] != '\0' && out < FS_TITLE_LEN; i++) {
        if (name[i] != ' ') {
            normalized[out++] = zx_upper(name[i]);
        }
    }
    normalized[out] = '\0';
    if (out < 3) {
        return FALSE;
    }

    for (unsigned i = 0; i + 3 < out; i++) {
        if (normalized[i] == '.') {
            if (normalized[i + 1] == 'T' &&
                normalized[i + 2] == 'A' &&
                normalized[i + 3] == 'P') {
                return TRUE;
            }
            if (i + 3 < out &&
                normalized[i + 1] == 'T' &&
                normalized[i + 2] == 'Z' &&
                normalized[i + 3] == 'X') {
                return TRUE;
            }
        }
    }

    return (out >= 3 &&
            normalized[out - 3] == 'T' &&
            normalized[out - 2] == 'A' &&
            normalized[out - 1] == 'P')
        || (out >= 3 &&
            normalized[out - 3] == 'T' &&
            normalized[out - 2] == 'Z' &&
            normalized[out - 1] == 'X');
}
