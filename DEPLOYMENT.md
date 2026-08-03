# Deploying Arsalan Traders: laptop → GitHub → always-on free server

This gets you: you write/update code on your laptop, push it to GitHub, and
a small free cloud server runs it 24/7. Your phone talks to that server
over the internet from anywhere — home, shop, wherever — with the laptop
turned off.

---

## Part 1 — Push your code to GitHub

On your laptop, inside the `arsalan-traders` folder:

```bash
git init
git add .
git commit -m "Initial version of Arsalan Traders inventory system"
```

Create a new **empty** repository on github.com (don't let it add a README —
you already have one), then:

```bash
git remote add origin https://github.com/<your-username>/arsalan-traders.git
git branch -M main
git push -u origin main
```

Note: `.gitignore` already excludes `include/httplib.h` and `include/json.hpp`
(they're large third-party files you re-download rather than commit), the
`build/` folder, and the `.db` database file. You'll download the two
headers again on the server in Part 3, same as you did/would in Termux.

Whenever you make changes later:
```bash
git add .
git commit -m "describe what changed"
git push
```

---

## Part 2 — Create the free server (Oracle Cloud)

1. Go to **oracle.com/cloud/free/** and sign up (needs a card for identity
   verification only — the "Always Free" resources never charge unless you
   explicitly upgrade to a paid plan).
2. Once in the console, create a new **Compute Instance**:
   - Image: **Ubuntu 22.04** (or newer)
   - Shape: pick one under **"Always Free eligible"** — either the
     `VM.Standard.A1.Flex` (ARM, up to 4 CPUs/24GB free — plenty) or
     `VM.Standard.E2.1.Micro`.
   - When creating it, download/save the **SSH key pair** it generates —
     you'll need the private key file to log in.
3. Under the instance's **Networking → Subnet → Security List**, add an
   **Ingress Rule**: allow TCP traffic on port `8080` from source `0.0.0.0/0`
   (this is what lets your phone reach it from the internet).
4. Note the instance's **public IP address** — you'll use it to connect.

---

## Part 3 — Set up the server

SSH into it from your laptop (first time only — this is just to set the
server up, you won't need the laptop again after this):

```bash
ssh -i /path/to/your-key.key ubuntu@<server-public-ip>
```

Then, on the server:
```bash
sudo apt update
sudo apt install -y build-essential cmake libsqlite3-dev git

git clone https://github.com/<your-username>/arsalan-traders.git
cd arsalan-traders

wget -O include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
wget -O include/json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

mkdir build && cd build
cmake ..
cmake --build .
```

Test it works:
```bash
./arsalan_traders
```
From your phone (on mobile data, not even WiFi), open
`http://<server-public-ip>:8080` — you should see the app. Press `Ctrl+C`
to stop the test run, then move to the next step so it runs permanently.

---

## Part 4 — Keep it running forever (systemd service)

This repo includes `deploy/arsalan-traders.service`, a template that
restarts the server automatically if it crashes or the machine reboots.

```bash
# Check the paths inside the file match your actual username/folder first
cat ~/arsalan-traders/deploy/arsalan-traders.service

sudo cp ~/arsalan-traders/deploy/arsalan-traders.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable arsalan-traders
sudo systemctl start arsalan-traders
```

Check it's running:
```bash
sudo systemctl status arsalan-traders
```

From now on, your phone can reach `http://<server-public-ip>:8080` any
time, from anywhere with internet — no laptop, no Termux, nothing needed
on your end except your phone.

---

## Updating the app later

On your laptop: edit code → `git add . && git commit -m "..." && git push`.

On the server:
```bash
cd ~/arsalan-traders
git pull
cd build
cmake --build .
sudo systemctl restart arsalan-traders
```

---

## A few honest notes

- **Backups matter more now**: your real shop data lives in
  `arsalan_traders.db` on the server. Periodically download a copy:
  `scp -i your-key.key ubuntu@<ip>:~/arsalan-traders/build/arsalan_traders.db ./backup.db`
- **This setup is plain HTTP**, fine for a private inventory tool, but if
  you ever want a lock icon / HTTPS (e.g. to feel safer typing a login
  password later), that's a follow-up step using Nginx + a free
  Let's Encrypt certificate — ask me when you're ready for that.
- **Bookmark the app**: since the IP is a plain number, on your phone visit
  `http://<server-public-ip>:8080` once, then "Add to Home Screen" so you
  don't have to remember or retype it.
