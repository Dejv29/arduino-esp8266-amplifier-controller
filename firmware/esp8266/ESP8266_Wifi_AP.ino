#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define BUTTON_PIN 0      // Tlačítko na GPIO0
#define LED_PIN    2      // Vestavěná LED na ESP-01 (GPIO2)
#define EEPROM_SIZE 96

unsigned long buttonPressTime = 0;
bool wasLongPressHandled = false;

char ssid[32] = "";
char password[64] = "";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String serialBuffer = "";
bool serialChanged = false;

bool inAPMode = false;

String waitForSerialResponse(unsigned long timeout = 1000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (Serial.available()) {
      response += Serial.readStringUntil('\n') + "\n";
    }
    if (response.length() > 0) break;
    delay(10);
  }
  return response.length() > 0 ? response : "Žádná odpověď ze sériové linky.";
}

String getNetworkStatus() {
  String status = "";
  status += "SSID: " + String(WiFi.SSID()) + "\n";
  status += "IP: " + WiFi.localIP().toString() + "\n";
  status += "MAC: " + WiFi.macAddress() + "\n";
  status += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
  return status;
}

void sendHttpCommand(const char* url) {
  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("HTTP begin failed");
  }
}

void handleSerial() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();  // odstraní \r a mezery

    if (line.length() > 0) {
      serialBuffer += line + "\n";
      serialChanged = true;

      // zpracování speciálních příkazů
      if (line == "SEND_MEDIA_PLAY") {
        sendHttpCommand("http://192.168.0.105:1817/?message=MediaPlay");
      }
      else if (line == "SEND_MEDIA_START") {
        sendHttpCommand("http://192.168.0.105:1817/?message=MediaStart");
      }
      else if (line == "SEND_MEDIA_PREV") {
        sendHttpCommand("http://192.168.0.105:1817/?message=MediaPrev");
      }
      else if (line == "SEND_MEDIA_NEXT") {
        sendHttpCommand("http://192.168.0.105:1817/?message=MediaNext");
      }
    }
  }
}

void notifyClients() {
  if (serialChanged && serialBuffer.length() > 0) {
    ws.textAll(serialBuffer);
    serialBuffer = "";
    serialChanged = false;
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP Ovládání</title>
  <style>
    body { font-family: sans-serif; margin: 0; background: #f4f4f4; }

    .tabs {
      display: flex;
      height: 100vh;
    }

    .tab-buttons {
      display: flex;
      flex-direction: column;
      width: 200px;
      background: #333;
    }

    .tab-buttons button {
      background: none;
      color: white;
      border: none;
      padding: 15px;
      text-align: left;
      cursor: pointer;
      width: 100%;
    }

    .tab-buttons button.active {
      background: #555;
    }

    .tab-content {
      flex: 1;
      padding: 20px;
      display: none;
    }

    .tab-content.active {
      display: block;
    }

    button { margin: 5px; padding: 10px 20px; }
    .group { margin-bottom: 20px; }

    pre, textarea {
      background: #222; color: #0f0; padding: 10px;
      font-family: monospace; white-space: pre-wrap;
    }

    textarea { width: 100%; height: 200px; resize: vertical; }
    input[type="text"] { width: 100%; padding: 10px; }
    select { padding: 5px; }
    input[type="range"] { width: 100%; }
  </style>
</head>
<body>

<div class="tabs">
  <div class="tab-buttons">
    <button onclick="showTab(0)" class="active">Ovládání</button>
    <button onclick="showTab(1)">Nastavení</button>
    <button onclick="showTab(2)">IR</button>
    <button onclick="showTab(3)">Síť a komunikace</button>
  </div>

  <div class="tab-content active">
    <h2>Ovládání</h2>
    <div class="group">
      <button onclick="sendCmd('SET_POWER_ON')">Zapnout</button>
      <button onclick="sendCmd('SET_POWER_OFF')">Vypnout</button>
    </div>

    <div>
      <label for="volume">Hlasitost: <span id="volumeLabel">50</span>%</label><br>
      <input type="range" id="volume" min="0" max="100" value="50">
      <div class="group">
        <button onclick="sendCmd('PULSE_VOLDOWN')">-</button>
        <button onclick="sendCmd('PULSE_VOLUP')">+</button>
      </div>
    </div>

    <div class="group">
      Vstup:<br>
      <button onclick="sendCmd('SET_INPUT_0')">1</button>
      <button onclick="sendCmd('SET_INPUT_1')">2</button>
      <button onclick="sendCmd('SET_INPUT_2')">3</button>
      <button onclick="sendCmd('SET_INPUT_3')">4</button>
      <button onclick="sendCmd('SET_INPUT_4')">5</button>
      <button onclick="sendCmd('SET_INPUT_5')">6</button>
    </div>

    <div class="group">
      <button onclick="sendCmd('SET_SPEAKER_A')">Repro A</button>
      <button onclick="sendCmd('SET_SPEAKER_B')">Repro B</button>
    </div>

    <div class="group">
      <button onclick="sendCmd('MEDIA_START')">START</button>
      <button onclick="sendCmd('MEDIA_PREV')"><<-</button>
      <button onclick="sendCmd('MEDIA_PLAY')">Play/Pause</button>
      <button onclick="sendCmd('MEDIA_NEXT')">->></button>
    </div>
  </div>

  <div class="tab-content">
    <h2>Nastavení</h2>
    <div class="group">
      <button onclick="sendCmd('SET_CURRENT_MAX_VOLUME')">Nastavit aktuální hlasitost jako max</button>
    </div>
    <div class="group">
      <label>Nastavit limit pro vstup</label><br>
      <select id="inputSelect">
        <option value="0">1</option><option value="1">2</option>
        <option value="2">3</option><option value="3">4</option>
        <option value="4">5</option><option value="5">6</option>
      </select>
      na hlasitost
      <select id="volSelect">
        <option value="10">10%</option><option value="20">20%</option>
        <option value="30">30%</option><option value="40">40%</option>
        <option value="50">50%</option><option value="60">60%</option>
        <option value="70">70%</option><option value="80">80%</option>
        <option value="90">90%</option><option value="100">100%</option>
      </select>
      <button onclick="setInputMaxVolume()">Nastavit</button>
    </div>
  </div>

  <div class="tab-content">
    <h2>IR Ovládání</h2>
    <div id="irButtons"></div>
    <button onclick="sendCmd('IR_PRINT')">Vypsat kódy</button>
    <h3>Výpis ze sériové linky</h3>
    <textarea id="serialLog1" readonly></textarea>
  </div>

  <div class="tab-content">
    <h2>Stav sítě</h2>
    <pre id="netStatus">Načítání...</pre>

    <h2>Výpis ze sériové linky</h2>
    <textarea id="serialLog2" readonly></textarea>

    <h2>Poslat příkaz do Arduina</h2>
    <input type="text" id="serialInput" placeholder="Zadej příkaz a stiskni Enter">
  </div>
</div>

<script>
  const ws = new WebSocket(`ws://${location.host}/ws`);
  const volumeSlider = document.getElementById("volume");
  const volumeLabel = document.getElementById("volumeLabel");
  const serialInput = document.getElementById("serialInput");
  const netStatus = document.getElementById("netStatus");
  const serialLog1 = document.getElementById("serialLog1");
  const serialLog2 = document.getElementById("serialLog2");

  function sendCmd(cmd) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(cmd);
    }
  }

  function setInputMaxVolume() {
    const input = document.getElementById("inputSelect").value;
    const vol = document.getElementById("volSelect").value;
    sendCmd(`SET_MAX_VOL_INPUT_${input}_${vol}`);
  }

  function appendSerial(data) {
    [serialLog1, serialLog2].forEach(log => {
      if (log) {
        log.value += data;
        log.scrollTop = log.scrollHeight;
        const lines = log.value.split("\n");
        if (lines.length > 100) {
          log.value = lines.slice(-100).join("\n");
        }
      }
    });
  }

  ws.onopen = () => {
    sendCmd("GET_VOLUME");
    updateNetworkStatus();
    setInterval(updateNetworkStatus, 10000);
  };

  ws.onmessage = function(event) {
    const data = event.data;

    if (data.startsWith("Volume:")) {
      const match = data.match(/Volume:\s*(\d+)\s*%/);
      if (match) {
        const v = parseInt(match[1]);
        volumeSlider.value = v;
        volumeLabel.textContent = v;
      }
    } else if (data.includes("SSID:") && data.includes("IP:")) {
      netStatus.textContent = data;
    } else {
      appendSerial(data);
    }
  };

  serialInput.addEventListener("keypress", function(e) {
    if (e.key === "Enter") {
      sendCmd(serialInput.value);
      serialInput.value = "";
    }
  });

  volumeSlider.oninput = function() {
    volumeLabel.textContent = this.value;
    sendCmd('SET_VOLUME_' + this.value);
  };

  function updateNetworkStatus() {
    sendCmd("GET_NETWORK");
  }

  const irKeys = ["POWER", "INPUT", "SPEAKERS", "VOLUP", "VOLDOWN", "PLAY", "NEXT", "PREV"];
  let html = "";
  irKeys.forEach(key => {
    html += `<div><strong>${key}</strong>:
      <button onclick="sendCmd('IR_LEARN_${key}')">LEARN</button>
      <button onclick="sendCmd('IR_REMOVE_${key}')">REMOVE</button>
      <button onclick="sendCmd('IR_REMOVE_${key}_ALL')">REMOVE_ALL</button></div>`;
  });
  document.getElementById("irButtons").innerHTML = html;

  function showTab(index) {
    const buttons = document.querySelectorAll(".tab-buttons button");
    const contents = document.querySelectorAll(".tab-content");
    buttons.forEach((btn, i) => {
      btn.classList.toggle("active", i === index);
      contents[i].classList.toggle("active", i === index);
    });
  }
</script>
</body>
</html>
)rawliteral";

void saveWiFiConfig(String newSsid, String newPass) {
  newSsid.toCharArray(ssid, sizeof(ssid));
  newPass.toCharArray(password, sizeof(password));

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) EEPROM.write(i, ssid[i]);
  for (int i = 0; i < 64; i++) EEPROM.write(i + 32, password[i]);
  EEPROM.commit();
  EEPROM.end();
}

void loadWiFiConfig() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) ssid[i] = EEPROM.read(i);
  for (int i = 0; i < 64; i++) password[i] = EEPROM.read(i + 32);
  EEPROM.end();
}

void startAPMode() {
  inAPMode = true;
  WiFi.disconnect();
  delay(1000);
  WiFi.softAP("Pioneer Setup");
  Serial.println("AP mód spuštěn");

  server.reset();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form method='POST' action='/save'><input name='ssid' placeholder='SSID'><br><input name='pass' placeholder='Password'><br><button type='submit'>Ulozit a pripojit</button></form>");
  });
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssidNew, passNew;
    if (request->hasParam("ssid", true)) ssidNew = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) passNew = request->getParam("pass", true)->value();
    saveWiFiConfig(ssidNew, passNew);
    request->send(200, "text/html", "Uloženo, restartuji...");
    delay(1000);
    ESP.restart();
  });
  server.begin();
}

void blinkIPLastSegment() {
  uint8_t last = WiFi.localIP()[3];
  int digits[3] = { last / 100, (last / 10) % 10, last % 10 };

  for (int i = 0; i < 3; i++) {
    if (i > 0) delay(600); // pauza mezi čísly

    if (digits[i] == 0) {
      // Dlouhý blik pro nulu
      digitalWrite(LED_PIN, LOW);  // zapnout LED
      delay(1000);                 // držet 1 sekundu
      digitalWrite(LED_PIN, HIGH); // vypnout LED
      delay(200);
    } else {
      // Normální blikání pro 1–9
      for (int b = 0; b < digits[i]; b++) {
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
      }
    }
  }
}

void connectToWiFi() {
  loadWiFiConfig();
  WiFi.begin(ssid, password);
  Serial.println("Připojuji se k WiFi...");
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(9600);
  delay(500);

  connectToWiFi();

  if (!inAPMode) {
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                  void *arg, uint8_t *data, size_t len) {
      if (type == WS_EVT_DATA) {
        String msg = String((char*)data).substring(0, len);
        if (msg == "GET_NETWORK") {
          client->text(getNetworkStatus());
          return;
        }
        Serial.println(msg);
      }
    });

    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html);
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
      String path = request->url();
      if (path.startsWith("/")) path = path.substring(1);

      Serial.println(path);

      String response = "";
      unsigned long start = millis();
      const unsigned long totalTimeout = 1500;
      const unsigned long gapTimeout = 100;
      unsigned long lastCharTime = millis();

      while (millis() - start < totalTimeout) {
        while (Serial.available()) {
          char c = Serial.read();
          response += c;
          lastCharTime = millis();
        }
        if (response.length() > 0 && millis() - lastCharTime > gapTimeout) {
          break;
        }
        delay(5);
      }

      if (response.length() == 0) response = "Prikaz proveden bez odpovedi";
      request->send(200, "text/plain", response);
    });

    server.begin();
  }
}

void loop() {
  handleSerial();
  notifyClients();

  static bool buttonPrev = HIGH;
  bool buttonNow = digitalRead(BUTTON_PIN);

  // Obsluha tlačítka
  if (buttonPrev == HIGH && buttonNow == LOW) {
    buttonPressTime = millis();
    wasLongPressHandled = false;
  }

  if (buttonPrev == LOW && buttonNow == HIGH) {
    unsigned long pressDuration = millis() - buttonPressTime;
    if (pressDuration < 1000 && !wasLongPressHandled) {
      blinkIPLastSegment();
    }
  }

  if (buttonNow == LOW && (millis() - buttonPressTime >= 3000) && !wasLongPressHandled) {
    wasLongPressHandled = true;
    startAPMode();
  }

  buttonPrev = buttonNow;

  // Opakované pokusy o připojení
  static unsigned long lastAttempt = 0;
  if (!inAPMode && WiFi.status() != WL_CONNECTED && millis() - lastAttempt > 10000) {
    Serial.println("WiFi není připojeno, zkouším znovu...");
    WiFi.begin(ssid, password);
    lastAttempt = millis();
  }
  // Blikání LED podle stavu WiFi nebo trvale svítící v AP módu
static unsigned long ledTimer = 0;
unsigned long now = millis();

if (inAPMode) {
  digitalWrite(LED_PIN, LOW);  // LED trvale svítí v AP módu
}
else if (WiFi.status() == WL_CONNECTED) {
  digitalWrite(LED_PIN, HIGH); // LED zhasnutá (WiFi připojena)
} else {
  unsigned long t = (now - ledTimer) % 3000;
  if (t < 1000) {
    digitalWrite(LED_PIN, LOW);  // Bliknutí: 1s svítí
  } else {
    digitalWrite(LED_PIN, HIGH); // 2s zhasnutá
  }
}
}
