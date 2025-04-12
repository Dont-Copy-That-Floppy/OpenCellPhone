---

# 📋 Bugs

- [ ] Delete message needs to go to the **nearest SMS**
- [ ] **Time update** on first start-up takes longer than usual
- [ ] **Not all touches** are registered
- [ ] `clearNotifications` call on line 433 needs more intelligence
- [ ] Prevent user/system from navigating **past available SMS messages**
- [ ] **Reorganize** SMS messages when a lower-numbered one is deleted

---

# 🚧 Needed Features

- [ ] Lookup table for **pixel width** (for screen formatting and text)
- [ ] Function for **user input** to change text position
- [ ] **SMS send** function
- [ ] Ability to **read sent messages**
- [ ] **Store SMS** on secondary storage
- [ ] **Emergency call** from passcode screen
- [ ] Store **missed calls count and number**
- [ ] **Notification screen**
- [ ] **Call time** counter
- [ ] **Call record** screen and data
- [ ] **Clear notifications** functionality
- [ ] **In-call volume** control
- [ ] **Signal strength** display (`AT+CSQ`)
- [ ] **Redial** previous number (`ATDL`)
- [ ] **Call waiting** support (`AT+CHLD=<N>`)
  - `N = 1`: terminate current call, answer call waiting  
  - `N = 2`: set current call to busy, answer waiting call

---

# 🌟 Future Features

- [ ] **Bluetooth**
- [ ] **FM Radio**
- [ ] **Contact list / Phone book**
- [ ] Adjustable **display brightness**
- [ ] **Airplane Mode**
  - Enable: `AT+CFUN=4`
  - Disable: `AT+CFUN=1`
- [ ] Sort SMS by **sender** or **contact**
- [ ] **Swipe screen** left-right for sidebar (e.g., volume control)
- [ ] Show **character limit** during SMS composition
- [ ] **View MMS**
- [ ] **Send MMS**
- [ ] **View images** in MMS
- [ ] **Swipe to clear** notifications
- [ ] **Light sensor** for proximity-based brightness control
- [ ] **Transistor-controlled** ground for speaker (only active in-call)
- [ ] **Transistor-controlled** power for display driver (remove `digitalRead`)
- [ ] Dedicated **Settings screen**
  - Change **screen timeout**
  - Add/Remove **passcode**
  - Adjust **display brightness**
  - Configure **auto brightness thresholds**
  - Set SMS **viewing index manually**
- [ ] **Accelerometer** support (e.g., auto orientation)
- [ ] Use **custom ringtones**
- [ ] **MP3/Audio player**
- [ ] **GPS** sensor
- [ ] **Display-Out** support
- [ ] **Web browser**

---
