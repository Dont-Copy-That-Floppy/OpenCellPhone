# OpenCellPhone

**An open hardware + software cell phone using the global GSM network.**

Arduino-based and fully open-source, OpenCellPhone is a DIY mobile phone project that runs on AT commands, handles SMS/calls, and supports a touch-based interface with a custom-built GUI.

[2014/15 Hackaday Quarterfinalist](https://hackaday.io/project/2478-open-source-cell-phone)

---

## 🚀 Features

- Full support for **SMS send/receive**
- **Touch-based dial pad** and passcode lock screen
- **Call handling** (dial, receive, hang up)
- Basic **notifications**, **call timer**, and **incoming caller display**
- Real-time **clock display** from GSM module (`AT+CCLK`)
- Visual **signal strength** display (`AT+CSQ`)
- **Low-power display handling** with timeout/dim/sleep modes

---

## 🧠 Planned and Future Features

### Needed Features

- Lookup table for pixel width for screen formatting
- Sent SMS display and storage
- Secondary storage for messages
- Emergency calling from passcode screen
- In-call volume adjustment
- Call waiting support (`AT+CHLD`)
- Missed call tracking
- SMS management (delete, reorder, store)
- Signal strength meter
- Re-dial last number (`ATDL`)
- Notification screen with better intelligence

### Future Features

- Bluetooth & FM Radio
- Contact List / Phone Book
- Adjustable screen brightness
- Airplane mode (`AT+CFUN`)
- SMS sorting by sender/contact
- Swipe gestures for UI
- MMS view/send, image handling
- Light sensor and auto-brightness
- MP3/audio playback
- GPS & accelerometer
- Display output
- Web browser
- Ringtones
- Configurable settings via touch UI

---

## 📋 SIM Limitations

- SIM card storage is limited to **30 SMS messages**

---

## ⚙️ Build Info

### Architecture

- Built with **Arduino IDE**
- Written in C/C++ with **low-level serial AT command** integration
- GUI rendered using **RA8875 touchscreen driver**
- Touchscreen and backlight managed through **PWM and pin control**

### Libraries & Tools

- `RA8875` display/touch driver
- Standard `Serial`, `Wire`, and `SPI` Arduino libs
- Custom utility functions for character buffering and cursor logic

---

## 🧪 AT Command References

| Command                                   | Description                |
|-------------------------------------------|----------------------------|
| `ATD*99#`                                 | Dial Access Point          |
| `AT+CGDCONT=1,"IP","access.point.name"`   | Defines PDP context        |
| `AT+VRX`                                  | Record in-call audio       |
| `AT+COPS=?`                               | List available networks     |
| `AT+CSQ`                                  | Check signal strength      |
| `ATDL`                                    | Redial last number         |
| `AT+CHLD`                                 | Call waiting options       |

---

## 🧰 Development Setup

### Folder Structure

```text
/PrototypePhone
│
├── /lib                # Optional custom libraries
├── PrototypePhone.ino  # main file
