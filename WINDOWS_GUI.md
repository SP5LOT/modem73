# modem73 Windows GUI — User Guide

> **Also available in Polish:** [WINDOWS_GUI_PL.md](WINDOWS_GUI_PL.md)

---

## What is modem73?

**modem73** is a KISS TNC (Terminal Node Controller) built around the [aicodix](https://github.com/aicodix/modem) OFDM modem. It allows digital data transmission over any SSB or FM transceiver using a standard PC sound card or USB audio interface — no dedicated hardware TNC required.

The **Windows GUI** (`modem73_gui.exe`) provides a graphical interface with real-time signal monitoring, easy configuration, and native Windows integration.

Key features:
- Multiple OFDM modes — trade off throughput vs. robustness depending on band conditions
- All PTT methods: VOX, Hamlib rigctld, serial RTS/DTR
- Real-time constellation display, SNR history, and signal level meter
- KISS protocol over TCP — compatible with Dire Wolf, Pat Winlink, Xastir, YAAC, and others
- Settings auto-saved between sessions
- No installation required — just unzip and run

---

## System Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Windows 10 or newer (64-bit) |
| CPU | Any modern x86-64 processor |
| RAM | ~50 MB |
| Audio | USB audio adapter or built-in sound card connected to transceiver |
| Transceiver | Any SSB or FM rig with audio in/out |
| PTT | VOX (no hardware needed), serial RTS/DTR, or Hamlib rigctld |

---

## Download & Installation

1. Go to the [Releases](../../releases) page and download `modem73_gui_windows.zip`
2. Unzip the archive to any folder, for example `C:\modem73\`
3. The folder will contain two files:
   ```
   modem73_gui.exe       (the program)
   libwinpthread-1.dll   (required, must stay in the same folder)
   ```
4. Double-click `modem73_gui.exe` to launch

**No installation wizard, no MSYS2, no Visual C++ Redistributable needed.**

Settings are automatically saved to:
```
C:\Users\<YourName>\AppData\Roaming\modem73\settings.ini
```

---

## Hardware Setup

```
Transceiver
 ├── Audio OUT (speaker/phone jack)  ──►  Audio interface MIC/LINE IN  ──►  PC
 └── Audio IN  (mic jack)            ◄──  Audio interface LINE OUT      ◄──  PC
 └── PTT                             ◄──  Serial RTS/DTR  (optional)
```

- Use a **USB audio interface** (any cheap USB sound card works) for galvanic isolation
- Connect the transceiver's **receive audio output** to the interface's **line input**
- Connect the interface's **line output** to the transceiver's **microphone or data input**
- Adjust transceiver audio levels so the input bar in modem73 reads around **-20 to -6 dBFS** on a received signal

---

## GUI Overview

The interface is divided into two panels:

| Panel | Contents |
|-------|----------|
| **Left** | All settings: Identity, Audio, Modem, PTT, CSMA, Network |
| **Right** | Real-time monitoring: level bar, constellation, SNR history, log |

---

## Configuration

### Identity

| Field | Description |
|-------|-------------|
| **Callsign** | Your amateur radio callsign (e.g. `SP1ABC`) — required for operation |

---

### Audio

| Field | Description |
|-------|-------------|
| **Capture device** | Audio input — connect to transceiver's audio output (RX audio) |
| **Playback device** | Audio output — connect to transceiver's audio input (TX audio) |
| **Refresh** | Rescan for newly connected audio devices |

> If your USB audio interface doesn't appear, click **Refresh** after plugging it in.

---

### Modem

| Field | Description |
|-------|-------------|
| **Modulation** | OFDM mode: number of subcarriers × bits per symbol. Higher = faster but less robust on poor bands |
| **Symbol Rate** | OFDM symbols per second — affects bandwidth and robustness |
| **Center Freq (Hz)** | Audio tone center frequency. **1500 Hz** is standard for SSB (USB/LSB). Must fit within your transceiver's passband |

modem73 requires **2400 Hz of audio bandwidth**. Make sure your transceiver's audio passband covers the range `Center Freq ± 1200 Hz`.

**Typical settings for HF SSB:**
- Mode: USB
- Center Freq: 1500 Hz (signal occupies 300–2700 Hz)
- Modem: start with a lower-order mode on noisy bands

---

### PTT

Choose the PTT method that matches your setup:

| Method | Description |
|--------|-------------|
| **NONE** | No PTT control — use your transceiver's built-in VOX or push PTT manually |
| **VOX** | Software VOX — modem73 sends a tone before TX to key the rig via its own VOX circuit |
| **rigctl** | Hamlib rigctld over network — works with almost any modern transceiver |
| **serial** | Serial port RTS or DTR line — works with most transceivers via a USB-serial adapter |

#### VOX settings

| Setting | Description |
|---------|-------------|
| **Lead time (ms)** | Delay after keying before TX audio starts — allows rig's VOX to activate (200–500 ms typical) |
| **Tail time (ms)** | How long to hold PTT after audio ends (100–300 ms typical) |
| **Freq (Hz)** | Frequency of the VOX activation tone |

#### rigctl settings

| Setting | Description |
|---------|-------------|
| **Host** | Address of rigctld — default `localhost:4532` |

Start rigctld before launching modem73:
```
rigctld -m <model_id> -r <port>
```
Example for testing without a rig: `rigctld -m 1 -r none`

#### Serial PTT settings

| Setting | Description |
|---------|-------------|
| **COM Port** | Windows COM port number (e.g. `COM3`) — check Device Manager |
| **Line** | **RTS** or **DTR** — depends on your cable/interface wiring |

---

### CSMA

CSMA (Carrier Sense Multiple Access) avoids transmit collisions on shared channels.

| Setting | Description |
|---------|-------------|
| **P (0–255)** | Transmit probability when channel is free. `255` = transmit immediately (no CSMA). Lower values introduce random backoff |
| **Slot (ms)** | Time slot duration for backoff calculation |

> For simplex / point-to-point links, P=255 (no CSMA) is fine. For shared channels (APRS, digipeater), use P=63–128.

---

### Network

| Setting | Description |
|---------|-------------|
| **KISS Port** | TCP port for KISS protocol connections — default `8001` |

---

## Starting the TNC

1. Configure all settings in the left panel
2. Click **Apply** — settings are saved and the modem is prepared
3. Click **START** — the TNC begins listening; the button turns red
4. Connect your application to `localhost:8001` using the KISS protocol
5. Click **STOP** to end the session

---

## Signal Monitoring (Right Panel)

| Display | Description |
|---------|-------------|
| **Level bar** | Real-time audio input level. Target: −20 to −6 dBFS during RX. Avoid clipping (red) on TX |
| **Constellation** | IQ constellation of the received OFDM signal. Well-defined clusters = good signal quality |
| **SNR history** | Signal-to-noise ratio over time |
| **Log** | Color-coded messages: green = received frames, yellow = warnings, red = errors |

A good constellation shows tight clusters at the expected symbol positions. A noisy or scattered constellation indicates weak signal, QRM, or audio level issues.

---

## Saving and Loading Settings

### Automatic saving
Settings are saved every time you click **Apply** and restored automatically on next launch.

### Manual presets (file-based)

In the settings panel you'll find two buttons:

| Button | Action |
|--------|--------|
| **Save to file...** | Opens a save dialog — save current settings as a `.ini` file anywhere |
| **Load from file...** | Opens an open dialog — load a previously saved preset |

Useful for switching quickly between configurations (different bands, transceivers, or digipeater vs. simplex).

### Command-line config

Launch modem73 with a specific settings file:
```
modem73_gui.exe --config C:\path\to\my_settings.ini
```

---

## Connecting Applications

### Reticulum Network Stack (primary use case)

modem73 was primarily developed to bring [Reticulum](https://reticulum.network) — a cryptography-based mesh networking stack — to radio links on Windows. Reticulum handles routing, encryption, and reliable delivery; modem73 is its radio interface.

Once modem73 is running, configure Reticulum to use it as a KISS TNC interface. Add the following to your Reticulum configuration file (`%APPDATA%\Local\Programs\reticulum\config` or wherever your instance stores it):

```
[[modem73 Radio]]
  type = TCPClientInterface
  enabled = yes
  kiss_framing = True
  target_host = 127.0.0.1
  target_port = 8001
```

> **Note:** `TCPClientInterface` with `kiss_framing = True` is the correct type for soundmodems and software TNCs that expose a KISS interface over TCP — which is exactly what modem73 does. The `KISSInterface` type is only for physical TNCs connected via serial port.

With Reticulum connected to modem73, the following applications work over the radio link out of the box — **no Linux required**:

| Application | Description | Link |
|-------------|-------------|------|
| **MeshChat** | Browser-based mesh chat for Reticulum — easy to set up, works on Windows | [github.com/liamcottle/reticulum-meshchat](https://github.com/liamcottle/reticulum-meshchat) |
| **MeshChatX** | Extended MeshChat with additional features | [git.quad4.io/RNS-Things/MeshChatX](https://git.quad4.io/RNS-Things/MeshChatX) |
| **Sideband** | Full-featured Reticulum client: messages, file transfers, maps | [github.com/markqvist/Sideband](https://github.com/markqvist/Sideband) |

> **Why Reticulum over radio?** Unlike AX.25/APRS, Reticulum provides end-to-end encryption, multi-hop mesh routing, and works without any central infrastructure or internet connection. It is particularly well suited for emergency communications and off-grid use.

---

### AX.25 / APRS / Winlink

modem73 also works as a standard KISS TNC for traditional packet radio software. Connect any KISS-capable application to `TCP localhost:8001`:

| Application | Configuration |
|-------------|---------------|
| **Dire Wolf** | `KISSPORT 8001` in direwolf.conf |
| **Pat Winlink** | KISS TNC mode, host `localhost`, port `8001` |
| **Xastir** | Interface type: KISS TNC, hostname `localhost`, port `8001` |
| **YAAC** | Serial port: TCP: `localhost:8001` |
| **APRSdroid** | Requires a local WiFi bridge tool |

---

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| No audio devices in list | Device not detected | Click **Refresh**; check USB connection |
| Constellation shows only noise | No or wrong audio input | Check cable; verify transceiver sends audio on RX |
| Audio level too low | Input gain too low | Raise PC microphone boost or transceiver output volume |
| Audio level clipping | Input gain too high | Lower PC input level or transceiver output volume |
| PTT not working (rigctl) | rigctld not running | Start rigctld before modem73 |
| PTT not working (serial) | Wrong COM port or line | Check Device Manager; try RTS vs DTR |
| KISS connection refused | Firewall blocking port | Allow `modem73_gui.exe` in Windows Firewall |
| Program won't start | Missing DLL | Ensure `libwinpthread-1.dll` is in the same folder as the exe |
| Corrupt settings | Bad settings.ini | Delete `%APPDATA%\modem73\settings.ini` and restart |

---

## Tips for Ham Radio Operators

- **SSB dial vs. signal frequency**: modem73 transmits at the audio *center frequency* you set. If your dial reads 14.100 MHz USB and you set center freq to 1500 Hz, your RF signal is at 14.101.5 MHz.
- **Check your audio passband**: Many modern transceivers have adjustable bandpass filters — widen them to at least 2.5 kHz for modem73.
- **ALC**: Keep ALC deflection minimal on TX. Set transceiver power to 50–80% and let the audio drive control the level.
- **FM vs SSB**: modem73 works on FM too (VHF/UHF packet). For FM, PTT timing is more forgiving; set a generous lead time.
- **Simplex testing**: You can test locally by connecting two instances of modem73 back-to-back with audio loopback cables or software loopback.

---

## Acknowledgments

This project is a **fork** of the original **modem73** by [RFnexus](https://github.com/RFnexus):

> **[https://github.com/RFnexus/modem73](https://github.com/RFnexus/modem73)**

All credit for the core TNC engine, KISS protocol implementation, and the modem73 concept goes to the original author. This fork adds:
- A native Windows GUI (Dear ImGui + GLFW + OpenGL)
- Windows build support (MSYS2 UCRT64 toolchain)
- Pre-built Windows binaries for easy distribution

Many thanks to **RFnexus** for creating modem73 and making it open-source, and to **[aicodix](https://github.com/aicodix/modem)** for the excellent OFDM modem library that powers it all.

---

## License

modem73 is open-source. See [LICENSE](LICENSE) for details.
Windows GUI port — source available at this repository.
