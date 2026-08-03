# Arsalan Traders — Pesticide Inventory Manager

A C++ program (with a real SQLite database) that serves a mobile-friendly
web app. **It can run directly on your Android phone** — no laptop, no PC,
nothing else needs to stay on. Your phone runs the server *and* is the
device you use it on.

## What's inside

```
arsalan-traders/
├── src/
│   ├── main.cpp         C++ server: REST API + serves the web UI
│   ├── database.h/.cpp  SQLite database layer (all inventory logic)
├── public/
│   ├── index.html        The app screens (Dashboard/Inventory/Add/Stock)
│   ├── style.css
│   └── app.js
├── include/              (you'll add two header-only libraries here)
└── CMakeLists.txt
```

---

## Option C: Laptop → GitHub → free always-on cloud server (recommended if you want it reachable from anywhere, not tied to your phone's battery)

Write and update code on your laptop, push it to GitHub, and a free
always-on server runs it — your phone connects over the internet, no
laptop needed day-to-day. **See [DEPLOYMENT.md](DEPLOYMENT.md) for the
full walkthrough** (GitHub setup + Oracle Cloud's free tier + keeping it
running permanently with systemd).

---

## Option A: Run it entirely on your Android phone, using Termux

[Termux](https://termux.dev) is a free app that turns your Android phone into
a small Linux computer — you can install a C++ compiler and run real
programs on it, no laptop involved, ever.

**1. Install Termux.**
Get it from [F-Droid](https://f-droid.org/packages/com.termux/) (recommended —
the Play Store version is outdated and often broken). Search "Termux" on
F-Droid, or download the APK directly from its GitHub releases.

**2. Open Termux and install what you need:**
```bash
pkg update
pkg install clang cmake sqlite git wget
```

**3. Get the project onto your phone.**
Easiest way: copy the `arsalan-traders` folder onto your phone (via USB
cable, Bluetooth, or a cloud drive like Google Drive), then in Termux:
```bash
termux-setup-storage      # allow Termux to see your phone's files, one time only
cp -r /sdcard/Download/arsalan-traders ~/arsalan-traders
cd ~/arsalan-traders
```

**4. Download two small header-only libraries** (well-known open-source
libraries, one file each — Termux has its own internet access, mobile
data or WiFi, no laptop needed):
```bash
wget -O include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
wget -O include/json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
```

**5. Build and run:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
./arsalan_traders
```

**6. Open the app.** In your phone's browser go to `http://localhost:8080`.
Then use the browser's menu → "Add to Home Screen" so it behaves like a
real app icon.

### Keeping it running in the background

Android will eventually stop Termux if you swipe it away or the screen
stays locked a long time. To avoid that:
```bash
termux-wake-lock
```
Run this once per Termux session — it tells Android not to kill Termux to
save battery. Also, in your phone's battery settings, find Termux and set
it to "unrestricted" / "no battery optimization," so Android doesn't close
it in the background.

Simplest daily routine: keep a Termux session open with a saved shortcut
for `cd ~/arsalan-traders/build && ./arsalan_traders`, and run it each
morning before opening the shop. Termux also supports `termux-services` and
boot scripts if you want it fully automatic — ask me if you want that set up.

### About the database file

Your data lives in `arsalan_traders.db`, created next to the program the
first time you run it (inside Termux's storage on your phone). **Back it
up regularly** — e.g. copy it to Google Drive or email it to yourself
weekly — since phone storage can be lost if the phone is damaged, lost, or
reset. This is the one real tradeoff of running everything on a phone
instead of a dedicated server.

---

## Option B: A small always-on device instead of your phone

If you'd rather not tie your shop's inventory to your phone's battery and
storage, a cheap dedicated device (old PC, Raspberry Pi, or a few-dollars-
a-month cloud server) can run the exact same program 24/7, with your phone
just connecting to it — nothing to start each morning, data isn't tied to
one phone.

```bash
sudo apt update && sudo apt install build-essential cmake libsqlite3-dev
# download the same two headers as in Option A into include/
mkdir build && cd build && cmake .. && cmake --build .
./arsalan_traders
```
Then open `http://<device-ip>:8080` from your phone's browser (same WiFi,
or from anywhere if it's a cloud server). Let me know if you want this
route and I'll help with auto-restart on reboot or remote access from
outside the shop's WiFi.

---

## What it does

- **Dashboard** — total stock on hand, total inventory value, a low-stock
  warning list, and a feed of recent stock movements.
- **Inventory** — search/browse every pesticide, with a stock-level gauge
  per item; edit or delete from here.
- **Add** — add a new pesticide (name, category, price, unit, starting
  quantity, low-stock threshold).
- **Stock** — log stock coming in (new delivery) or going out (sold), which
  automatically updates quantities and keeps a full transaction history.

## Extending it later

- Add a login/PIN screen before letting staff log stock movements
- Add a "reports" tab (e.g. sales by week) — the transaction table already
  has everything needed for this
- Add barcode scanning using the phone camera (a JS barcode library talking
  to the same API)
