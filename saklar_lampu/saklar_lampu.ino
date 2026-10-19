  /*********
  Saklar Lampu - ESP8266 Async Web Server
  Dashboard modern dengan update status via AJAX (tanpa refresh halaman)

  Based on: https://randomnerdtutorials.com/esp8266-relay-module-ac-web-server/

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include "ESP8266WiFi.h"
#include "ESPAsyncWebServer.h"
#include "icons.h"   // ikon PWA (icon-192.png / icon-512.png)

// Set to true to define Relay as Normally Open (NO)
#define RELAY_NO    true

// Set number of relays
#define NUM_RELAYS  8

// Assign each GPIO to a relay
int relayGPIOs[NUM_RELAYS] = {5, 4, 0, 2, 14, 12, 13, 15};
// Nama lampu berdasarkan GPIOnya
const char* relayNAMEs[NUM_RELAYS] = {"Teras Depan", "Warnet 1", "Warnet 2", "Teras Belakang", "Teras Rumah", "Ruang Tamu", "Kamar", "Dapur+Km. Mandi"};
// Ikon untuk tiap kartu di dashboard
const char* relayICONs[NUM_RELAYS] = {"💡", "📡", "💻", "🌿", "🏠", "🛋️", "🛏️", "🚿"};
// Replace with your network credentials
const char* ssid = "Djatun-HotSpot";
const char* password = "";

const char* PARAM_INPUT_1 = "relay";
const char* PARAM_INPUT_2 = "state";

// Kredensial akses dashboard & API (Basic Auth)
// Contoh pemakaian dari MikroTik:
//   /tool fetch url="http://192.168.12.16/update?relay=1&state=1" user="bluematrix" password="P0o9i8u7" keep-result=no;
const char* HTTP_USER = "username";
const char* HTTP_PASS = "Password";

const char* ssidAP = "Saklar-Lampu"; // The name of the Wi-Fi network that will be created
const char* passwordAP = "123456***78";   // The password required to connect to it, leave blank for an open network

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// true = request belum terautentikasi; balasan 401 sudah dikirim.
// Verifikasi dilakukan manual (base64 "user:pass") karena authenticate()
// pada ESPAsyncWebServer 3.1.0 menolak kredensial yang benar.
// bisa gunakan tool https://www.base64encode.org/
bool tolakTanpaLogin(AsyncWebServerRequest *request){
  // "username:Password" -> base64
  static const char TARGET[] = "dXNlcm5hbWU6UGFzc3dvcmQ=";
  bool sah = false;

  if(request->hasHeader("Authorization")){
    String h = request->getHeader("Authorization")->value();
    int spasi = h.indexOf(' ');
    if(spasi > 0 && h.substring(0, spasi).equalsIgnoreCase("Basic")){
      String b64 = h.substring(spasi + 1);
      b64.trim();
      sah = b64.equals(TARGET);
    }
  }

  if(sah) return false;

  AsyncWebServerResponse *r = request->beginResponse(401, "text/plain", "Login diperlukan");
  r->addHeader("WWW-Authenticate", "Basic realm=\"Saklar Lampu\"");
  r->addHeader("Cache-Control", "no-store");
  request->send(r);
  return true;
}

// Status relay: true = lampu menyala
bool relayIsOn(int numRelay){
  bool level = digitalRead(relayGPIOs[numRelay - 1]);
  return RELAY_NO ? !level : level;
}

String relayState(int numRelay){
  return relayIsOn(numRelay) ? "checked" : "";
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <meta name="theme-color" content="#070b16">
  <meta name="description" content="Dashboard kendali saklar lampu berbasis ESP8266 dengan pembaruan status otomatis lewat AJAX.">
  <title>Saklar Lampu</title>
  <link rel="icon" type="image/png" href="/icon-192.png">
  <link rel="apple-touch-icon" href="/icon-192.png">
  <link rel="manifest" href="/manifest.json">
  <meta name="application-name" content="Saklar Lampu">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-title" content="Saklar Lampu">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <style>
    :root{
      --bg:#070b16;
      --panel:rgba(255,255,255,.055);
      --stroke:rgba(255,255,255,.10);
      --txt:#eaf0ff;
      --muted:#adbcda;
      --cyan:#22d3ee;
      --indigo:#6366f1;
      --green:#34d399;
      --red:#f87171;
    }
    *{box-sizing:border-box}
    html,body{margin:0;padding:0}
    body{
      min-height:100vh;
      font-family:'Segoe UI',system-ui,-apple-system,Roboto,'Helvetica Neue',Arial,sans-serif;
      color:var(--txt);
      background-color:var(--bg);
      -webkit-font-smoothing:antialiased;
      -webkit-tap-highlight-color:transparent;
      overflow-x:hidden;
    }
    .blob{position:fixed;border-radius:999px;filter:blur(100px);pointer-events:none;z-index:0}
    .blob.a{width:440px;height:440px;top:-160px;left:-140px;background:#4f46e5;opacity:.55}
    .blob.b{width:400px;height:400px;top:-120px;right:-150px;background:#06b6d4;opacity:.32}
    .blob.c{width:520px;height:520px;bottom:-260px;left:0;right:0;margin:0 auto;background:#7c3aed;opacity:.28}

    .wrap{position:relative;z-index:1;max-width:1120px;margin:0 auto;padding:26px 20px 44px}

    header.top{display:flex;align-items:center;justify-content:space-between;gap:16px;flex-wrap:wrap}
    .brand{display:flex;align-items:center;gap:14px}
    .logo{width:52px;height:52px;border-radius:16px;display:flex;align-items:center;justify-content:center;font-size:26px;background:linear-gradient(135deg,var(--indigo),var(--cyan));box-shadow:0 12px 32px rgba(99,102,241,.45)}
    .brand h1{font-size:1.35rem;font-weight:800;letter-spacing:-.02em;margin:0}
    .brand p{font-size:.82rem;color:var(--muted);margin:3px 0 0}
    .right{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
    .pill{display:inline-flex;align-items:center;gap:8px;padding:9px 15px;border-radius:999px;border:1px solid var(--stroke);background:var(--panel);font-size:.8rem;font-weight:600;color:var(--muted);backdrop-filter:blur(8px)}
    .pill.bad{border-color:rgba(248,113,113,.5);color:#fca5a5;background:rgba(248,113,113,.10)}
    .dot{width:9px;height:9px;border-radius:999px;background:var(--green);box-shadow:0 0 0 0 rgba(52,211,153,.55);animation:pulse 2s infinite}
    .pill.bad .dot{background:var(--red);animation:none;box-shadow:none}
    .btn{cursor:pointer;font:inherit;transition:.2s}
    .btn:hover{border-color:rgba(255,255,255,.28);color:var(--txt);transform:translateY(-1px)}
    .btn:active{transform:translateY(0)}
    @keyframes pulse{from{box-shadow:0 0 0 0 rgba(52,211,153,.5)}to{box-shadow:0 0 0 9px rgba(52,211,153,0)}}

    .stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px;margin:24px 0 22px}
    .stat{background:var(--panel);border:1px solid var(--stroke);border-radius:18px;padding:16px 18px;display:flex;flex-direction:column;gap:7px;backdrop-filter:blur(8px)}
    .stat.accent{background:linear-gradient(150deg,rgba(34,211,238,.16),rgba(99,102,241,.08));border-color:rgba(34,211,238,.32)}
    .lab{font-size:.7rem;font-weight:700;text-transform:uppercase;letter-spacing:.13em;color:var(--muted)}
    .num{font-size:1.9rem;font-weight:800;letter-spacing:-.03em;line-height:1.1}
    .num.sm{font-size:1rem;font-weight:700;letter-spacing:0;color:#c7d4ee}
    .dots{display:flex;gap:6px;flex-wrap:wrap}
    .dots i{display:block;width:9px;height:9px;border-radius:999px;background:#2b374f;transition:.25s}
    .dots i.lit{background:linear-gradient(135deg,var(--cyan),var(--indigo));box-shadow:0 0 10px rgba(34,211,238,.75)}

    .grid{display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(150px,1fr))}
    .card{position:relative;overflow:hidden;background:linear-gradient(160deg,rgba(255,255,255,.07),rgba(255,255,255,.02));border:1px solid var(--stroke);border-radius:20px;padding:16px;display:flex;flex-direction:column;gap:12px;transition:transform .25s, border-color .25s, box-shadow .25s, background .25s}
    .card:hover{transform:translateY(-4px);border-color:rgba(255,255,255,.22)}
    .card.on{border-color:rgba(34,211,238,.45);background:linear-gradient(160deg,rgba(34,211,238,.14),rgba(99,102,241,.06));box-shadow:0 16px 36px rgba(34,211,238,.14)}
    .glow{position:absolute;top:0;left:0;right:0;height:2px;background:linear-gradient(90deg,rgba(34,211,238,0),var(--cyan),rgba(99,102,241,0));opacity:0;transition:.3s}
    .card.on .glow{opacity:1}
    .head{display:flex;align-items:center;justify-content:space-between;gap:8px}
    .ico{width:42px;height:42px;border-radius:14px;display:flex;align-items:center;justify-content:center;font-size:21px;background:rgba(255,255,255,.08);border:1px solid var(--stroke)}
    .card.on .ico{background:rgba(34,211,238,.18);border-color:rgba(34,211,238,.35)}
    .chip{font-size:.63rem;font-weight:700;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);border:1px solid var(--stroke);border-radius:8px;padding:4px 7px;white-space:nowrap}
    .name{margin:0;font-size:1rem;font-weight:700;line-height:1.3;word-break:break-word}
    .foot{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-top:auto}
    .state{font-size:.72rem;font-weight:800;letter-spacing:.1em;color:var(--muted)}
    .card.on .state{color:#67e8f9}

    .switch{position:relative;display:inline-block;width:56px;height:30px;flex:0 0 auto}
    .switch input{position:absolute;opacity:0;width:0;height:0}
    .track{position:absolute;inset:0;background:#26324a;border:1px solid rgba(255,255,255,.08);border-radius:999px;transition:.28s cubic-bezier(.4,0,.2,1)}
    .track:before{content:"";position:absolute;top:3px;left:3px;width:22px;height:22px;border-radius:999px;background:#94a3b8;transition:.28s cubic-bezier(.4,0,.2,1)}
    .switch input:checked + .track{background:linear-gradient(135deg,var(--cyan),var(--indigo));border-color:transparent;box-shadow:0 6px 18px rgba(34,211,238,.4)}
    .switch input:checked + .track:before{transform:translateX(26px);background:#fff}
    .switch input:focus-visible + .track{outline:2px solid var(--cyan);outline-offset:3px}

    .foot-note{margin-top:30px;text-align:center;color:var(--muted);font-size:.76rem;letter-spacing:.02em}

    .btn.install{color:#67e8f9;border-color:rgba(34,211,238,.38);background:rgba(34,211,238,.10)}
    .btn.install:hover{color:#a5f3fc;border-color:rgba(34,211,238,.7);background:rgba(34,211,238,.16)}
    .btn.install:disabled{cursor:default;color:#6ee7b7;border-color:rgba(110,231,183,.4);background:rgba(110,231,183,.12);transform:none}

    .modal{position:fixed;inset:0;z-index:60;display:none;align-items:center;justify-content:center;padding:20px;background:rgba(3,6,14,.75);backdrop-filter:blur(7px)}
    .modal.show{display:flex}
    .modal-card{flex:1 1 auto;max-width:440px;background:linear-gradient(160deg,#141b2e,#0a0f1d);border:1px solid rgba(255,255,255,.12);border-radius:22px;padding:22px;box-shadow:0 30px 70px rgba(0,0,0,.6);display:flex;flex-direction:column;gap:0}
    .modal-card h2{margin:0 0 6px;font-size:1.08rem;font-weight:800;letter-spacing:-.01em}
    .modal-card p{margin:0 0 14px;font-size:.83rem;line-height:1.55;color:var(--muted)}
    .steps{display:flex;flex-direction:column;gap:9px;margin:0 0 14px;padding:0;list-style:none}
    .steps li{display:flex;gap:10px;align-items:flex-start;font-size:.83rem;line-height:1.45;color:#d9e2f4;background:rgba(255,255,255,.045);border:1px solid rgba(255,255,255,.08);border-radius:13px;padding:11px 12px}
    .steps b{color:#fff;font-weight:700}
    .steps .n{flex:0 0 auto;width:21px;height:21px;border-radius:999px;background:linear-gradient(135deg,var(--indigo),var(--cyan));color:#08111f;font-size:.72rem;font-weight:800;display:flex;align-items:center;justify-content:center}
    .modal-card .close{align-self:stretch;justify-content:center;color:#dbe5f7}

    @media (min-width:720px){
      .grid{grid-template-columns:repeat(auto-fill,minmax(232px,1fr))}
    }
    @media (max-width:560px){
      .wrap{padding:20px 14px 36px}
      .brand h1{font-size:1.15rem}
      .logo{width:44px;height:44px;font-size:22px;border-radius:14px}
      .stat{padding:13px 14px}
      .num{font-size:1.55rem}
      .card{padding:14px;gap:10px}
      .ico{width:38px;height:38px;font-size:19px}
      .name{font-size:.94rem}
      .foot-note{font-size:.72rem}
    }
    @media (prefers-reduced-motion:reduce){
      *{animation:none !important;transition:none !important}
    }
  </style>
</head>
<body>
  <div class="blob a"></div>
  <div class="blob b"></div>
  <div class="blob c"></div>

  <div class="wrap">
    <header class="top">
      <div class="brand">
        <div class="logo">💡</div>
        <div>
          <h1>Saklar Lampu</h1>
          <p>Dashboard Kendali Pencahayaan</p>
        </div>
      </div>
      <div class="right">
        <button class="pill btn install" id="installBtn" type="button">📥 Install</button>
        <span class="pill" id="status"><span class="dot"></span><span id="statusText">Terhubung</span></span>
        <button class="pill btn" id="refreshBtn" type="button">⟳ Segarkan</button>
      </div>
    </header>

    <main>
    <section class="stats">
      <div class="stat accent">
        <span class="lab">Menyala</span>
        <span class="num" id="onCount">0</span>
        <div class="dots" id="dots"></div>
      </div>
      <div class="stat">
        <span class="lab">Mati</span>
        <span class="num" id="offCount">0</span>
      </div>
      <div class="stat">
        <span class="lab">Total Ruang</span>
        <span class="num" id="total">0</span>
      </div>
      <div class="stat">
        <span class="lab">Diperbarui</span>
        <span class="num sm" id="sync">Memuat...</span>
      </div>
    </section>

    %BUTTONPLACEHOLDER%

    <footer class="foot-note">Status diperbarui otomatis lewat AJAX setiap 2 detik — halaman tidak perlu di-refresh.</footer>
    </main>
  </div>

  <div class="modal" id="installModal" role="dialog" aria-modal="true" aria-labelledby="installTitle">
    <div class="modal-card">
      <h2 id="installTitle">Pasang sebagai Aplikasi</h2>
      <p>Tambahkan dashboard ini ke layar utama supaya terbuka layaknya aplikasi, tanpa alamat browser.</p>
      <ul class="steps" id="installSteps"></ul>
      <p id="installHint"></p>
      <button class="pill btn close" id="installClose" type="button">Mengerti</button>
    </div>
  </div>

<script>
  var cards = Array.prototype.slice.call(document.querySelectorAll('.card'));
  var dotsBox = document.getElementById('dots');
  var statusBox = document.getElementById('status');
  var statusText = document.getElementById('statusText');
  var syncLabel = document.getElementById('sync');
  var totalLabel = document.getElementById('total');
  var onLabel = document.getElementById('onCount');
  var offLabel = document.getElementById('offCount');

  document.getElementById('total').textContent = cards.length;

  for (var d = 0; d < cards.length; d++) {
    dotsBox.appendChild(document.createElement('i'));
  }
  var dots = Array.prototype.slice.call(dotsBox.children);

  function clock() {
    var n = new Date();
    function p(v) { return String(v).length < 2 ? '0' + String(v) : String(v); }
    return p(n.getHours()) + ':' + p(n.getMinutes()) + ':' + p(n.getSeconds());
  }

  function setStatus(ok) {
    if (ok) {
      statusBox.classList.remove('bad');
      statusText.textContent = 'Terhubung';
    } else {
      statusBox.classList.add('bad');
      statusText.textContent = 'Terputus';
    }
  }

  function paint(card, on) {
    card.classList.toggle('on', on);
    var label = card.querySelector('.state');
    if (label) { label.textContent = on ? 'MENYALA' : 'MATI'; }
  }

  function render(states) {
    var onCount = 0;
    for (var i = 0; i < cards.length; i++) {
      var card = cards[i];
      var input = card.querySelector('input');
      var isOn = states[i] === 1;
      if (input.getAttribute('data-busy') !== '1') { input.checked = isOn; }
      paint(card, input.checked);
      if (input.checked) {
        onCount++;
        if (dots[i]) { dots[i].classList.add('lit'); }
      } else if (dots[i]) {
        dots[i].classList.remove('lit');
      }
    }
    onLabel.textContent = onCount;
    offLabel.textContent = cards.length - onCount;
    syncLabel.textContent = clock();
  }

  async function load() {
    if (document.hidden) { return; }
    try {
      var res = await fetch('/states', { cache: 'no-store' });
      if (res.status === 401) {
        setStatus(false);
        statusText.textContent = 'Login diperlukan';
        syncLabel.textContent = '401';
        return;
      }
      if (!res.ok) { throw new Error('HTTP ' + res.status); }
      var data = await res.json();
      if (!data || !data.s) { throw new Error('format salah'); }
      render(data.s);
      setStatus(true);
    } catch (err) {
      setStatus(false);
      syncLabel.textContent = 'Gagal';
    }
  }

  async function toggleRelay(input) {
    var card = input.closest('.card');
    var relay = card.getAttribute('data-relay');
    var on = input.checked;
    input.setAttribute('data-busy', '1');
    paint(card, on);
    try {
      var res = await fetch('/update?relay=' + relay + '&state=' + (on ? '1' : '0'));
      if (!res.ok) { throw new Error('HTTP ' + res.status); }
      setStatus(true);
      await new Promise(function (r) { setTimeout(r, 300); });
    } catch (err) {
      input.checked = !on;
      paint(card, input.checked);
      setStatus(false);
    }
    input.removeAttribute('data-busy');
    load();
  }

  cards.forEach(function (card) {
    var input = card.querySelector('input');
    input.addEventListener('change', function () { toggleRelay(input); });
  });

  document.getElementById('refreshBtn').addEventListener('click', load);
  document.addEventListener('visibilitychange', function () { if (!document.hidden) { load(); } });

  load();
  setInterval(load, 2000);

  // ---------- Pasang sebagai aplikasi (Add to Home Screen) ----------
  var installBtn = document.getElementById('installBtn');
  var modal = document.getElementById('installModal');
  var stepsBox = document.getElementById('installSteps');
  var hintBox = document.getElementById('installHint');
  var promptTersedia = null;
  var sudahPasang = window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;

  var PANDUAN = {
    android: [
      'Buka menu <b>⋮ (tiga titik)</b> di pojok kanan atas Chrome.',
      'Pilih <b>Instal aplikasi</b> atau <b>Tambahkan ke layar utama</b>.',
      'Tekan <b>Instal / Tambahkan</b> untuk konfirmasi.'
    ],
    ios: [
      'Buka halaman ini dengan <b>Safari</b> (bukan Chrome).',
      'Tekan tombol <b>Bagikan</b> (kotak dengan panah ke atas).',
      'Gulir, lalu pilih <b>Tambah ke Layar Utama</b>.',
      'Tekan <b>Tambah</b> di pojok kanan atas.'
    ],
    desktop: [
      'Buka lewat <b>Chrome</b> atau <b>Edge</b>.',
      'Klik ikon <b>pasang</b> di ujung address bar, atau menu <b>⋮</b> lalu <b>Instal Saklar Lampu...</b>.',
      'Konfirmasi dengan menekan <b>Instal</b>.'
    ]
  };

  function platform() {
    var ua = navigator.userAgent;
    if (/iPhone|iPad|iPod/.test(ua)) { return 'ios'; }
    if (/Android/.test(ua)) { return 'android'; }
    return 'desktop';
  }

  function segarkanTombol() {
    if (sudahPasang) {
      installBtn.textContent = '✓ Terpasang';
      installBtn.disabled = true;
      return;
    }
    installBtn.disabled = false;
    installBtn.textContent = promptTersedia ? '📥 Install' : '📥 Cara Pasang';
  }

  function bukaPanduan() {
    var daftar = PANDUAN[platform()];
    var html = '';
    for (var i = 0; i < daftar.length; i++) {
      html += '<li><span class="n">' + (i + 1) + '</span><span>' + daftar[i] + '</span></li>';
    }
    stepsBox.innerHTML = html;
    hintBox.textContent = (platform() === 'ios')
      ? 'Catatan: menu pasang di iOS hanya muncul lewat Safari.'
      : 'Catatan: tombol pasang otomatis hanya muncul lewat HTTPS. Dibuka lewat alamat IP lokal, ikuti langkah di atas — hasilnya sama.';
    modal.classList.add('show');
  }

  function tutupPanduan() { modal.classList.remove('show'); }

  window.addEventListener('beforeinstallprompt', function (e) {
    e.preventDefault();
    promptTersedia = e;
    segarkanTombol();
  });
  window.addEventListener('appinstalled', function () {
    sudahPasang = true;
    promptTersedia = null;
    segarkanTombol();
  });

  installBtn.addEventListener('click', function () {
    if (sudahPasang) { return; }
    if (promptTersedia) {
      var p = promptTersedia;
      promptTersedia = null;
      try { p.prompt(); } catch (err) {}
      if (p.userChoice && p.userChoice.then) {
        p.userChoice.then(function () { segarkanTombol(); });
      } else { segarkanTombol(); }
      return;
    }
    bukaPanduan();
  });

  document.getElementById('installClose').addEventListener('click', tutupPanduan);
  modal.addEventListener('click', function (e) { if (e.target === modal) { tutupPanduan(); } });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') { tutupPanduan(); } });

  segarkanTombol();
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons.reserve(4096);
    buttons += "<div class=\"grid\">";
    for(int i=1; i<=NUM_RELAYS; i++){
      bool isOn = relayIsOn(i);
      buttons += "<article class=\"card";
      if(isOn){ buttons += " on"; }
      buttons += "\" data-relay=\"" + String(i) + "\">"
                 "<span class=\"glow\"></span>"
                 "<div class=\"head\">"
                   "<span class=\"ico\">" + String(relayICONs[i-1]) + "</span>"
                   "<span class=\"chip\">GPIO " + String(relayGPIOs[i-1]) + "</span>"
                 "</div>"
                 "<h2 class=\"name\">" + String(relayNAMEs[i-1]) + "</h2>"
                 "<div class=\"foot\">"
                   "<label class=\"switch\">"
                     "<input type=\"checkbox\" id=\"r" + String(i) + "\" " + (isOn ? "checked" : "") +
                     " aria-label=\"" + String(relayNAMEs[i-1]) + "\">"
                     "<span class=\"track\"></span>"
                   "</label>"
                   "<span class=\"state\">" + (isOn ? "MENYALA" : "MATI") + "</span>"
                 "</div>"
                 "</article>";
    }
    buttons += "</div>";
    return buttons;
  }
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Set all relays to off when the program starts - if set to Normally Open (NO), the relay is off when you set the relay to HIGH
  for(int i=1; i<=NUM_RELAYS; i++){
    pinMode(relayGPIOs[i-1], OUTPUT);
    if(RELAY_NO){
      digitalWrite(relayGPIOs[i-1], HIGH);
    }
    else{
      digitalWrite(relayGPIOs[i-1], LOW);
    }
  }

  // Mode AP + STA: dashboard tetap bisa diakses lewat AP "Saklar-Lampu"
  // meskipun router/hotspot utama sedang mati atau belum terhubung.
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP(ssidAP, passwordAP);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssidAP);
  Serial.println("\" started -> http://192.168.4.1");

  // Koneksi ke hotspot dilakukan non-blocking: dicoba terus di background
  // tanpa menunda boot. Kalau gagal, AP tetap berjalan seperti biasa.
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.println("Menghubungkan ke WiFi: " + String(ssid) + " ...");

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(tolakTanpaLogin(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html, processor);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  // Manifest & ikon agar bisa ditambahkan ke layar utama seperti aplikasi
  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request){
    static const char MANIFEST[] PROGMEM = R"rawliteral({"name":"Saklar Lampu","short_name":"Saklar Lampu","description":"Dashboard kendali lampu berbasis ESP8266","lang":"id","start_url":"/","scope":"/","display":"standalone","orientation":"any","background_color":"#070b16","theme_color":"#070b16","icons":[{"src":"/icon-192.png","sizes":"192x192","type":"image/png","purpose":"any"},{"src":"/icon-512.png","sizes":"512x512","type":"image/png","purpose":"any maskable"}]})rawliteral";
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/manifest+json", MANIFEST);
    response->addHeader("Cache-Control", "public, max-age=86400");
    request->send(response);
  });

  server.on("/icon-192.png", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", ICON_192, ICON_192_LEN);
    response->addHeader("Cache-Control", "public, max-age=86400");
    request->send(response);
  });

  server.on("/icon-512.png", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", ICON_512, ICON_512_LEN);
    response->addHeader("Cache-Control", "public, max-age=86400");
    request->send(response);
  });

  // JSON status for AJAX polling: {"s":[1,0,1,...]}  (1 = menyala)
  server.on("/states", HTTP_GET, [](AsyncWebServerRequest *request){
    if(tolakTanpaLogin(request)) return;
    String json;
    json.reserve(48);
    json += "{\"s\":[";
    for(int i=1; i<=NUM_RELAYS; i++){
      json += relayIsOn(i) ? "1" : "0";
      if(i < NUM_RELAYS){ json += ","; }
    }
    json += "]}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  // Send a GET request to <ESP_IP>/update?relay=<inputMessage>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(tolakTanpaLogin(request)) return;
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      int relayNum = request->getParam(PARAM_INPUT_1)->value().toInt();
      int state = request->getParam(PARAM_INPUT_2)->value().toInt();

      if (relayNum < 1 || relayNum > NUM_RELAYS) {
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"relay tidak valid\"}");
        return;
      }

      if(RELAY_NO){
        Serial.print("NO ");
        digitalWrite(relayGPIOs[relayNum-1], !state);
      }
      else{
        Serial.print("NC ");
        digitalWrite(relayGPIOs[relayNum-1], state);
      }
      Serial.println("relay " + String(relayNum) + " -> " + String(state));
      request->send(200, "application/json", "{\"ok\":true}");
    }
    else {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"parameter kurang\"}");
    }
  });

  // Start server
  server.begin();
}

// Status koneksi STA terakhir (untuk cetak IP sekali saat tersambung)
bool staTersambung = false;

void loop(){
  bool tersambung = (WiFi.status() == WL_CONNECTED);
  if(tersambung && !staTersambung){
    // Print ESP8266 Local IP Address
    Serial.print("Terhubung ke WiFi, IP: ");
    Serial.println(WiFi.localIP());
  }
  else if(!tersambung && staTersambung){
    Serial.println("WiFi terputus - mencoba ulang (AP tetap aktif)");
  }
  staTersambung = tersambung;
  delay(200);
}
