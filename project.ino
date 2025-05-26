#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <DFRobotDFPlayerMini.h>
#include <FastLED.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>


const char* ssid = "iPhone";
const char* password = "divin357";


WebServer server(80);


#define LED_PIN              26
#define NUM_LEDS             8

// Buttons for manual control.
#define BUTTON_NEXT_PIN      14
#define BUTTON_PREV_PIN      12
#define BUTTON_PLAY_PIN      13

// PIR Sensor
#define PIR_PIN              23

// DFPlayer Mini
#define DFPLAYER_RX_PIN      4
#define DFPLAYER_TX_PIN      5

// FILE on Our SD Card 
#define FILE_WELCOME   1      // 1.mp3: Welcome message
#define FILE_FIRST     4      // 2.mp3: First song of the playlist
#define FILE_PERIODIC  2      // 3.mp3: Periodic voice message (after every 2 songs)
#define FILE_SHUTDOWN  3      // 4.mp3: Shutdown message


// Playlist
unsigned int currentSong = FILE_FIRST;
unsigned int songsSincePeriodic = 0;

// when true, the system is paused (all motion, gestures, and LED animations are stopped).
bool systemPaused = false;

// Shutdown conditions.
unsigned long lastMotionTime;
unsigned long lastCommandTime;
uint8_t currentVolume = 15;

// DFPlayer and sensor objects.
HardwareSerial DFPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// LED array.
CRGB leds[NUM_LEDS];

// For gesture detection.
uint16_t previousDistance = 0;


const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Smart Speaker Control</title>
  <style>
    body {
      background: #1a202c;
      color: #fff;
      font-family: Arial, sans-serif;
      padding: 24px;
    }
    .container {
      max-width: 480px;
      margin: 0 auto;
      text-align: center;
    }
    .title {
      font-size: 2.5rem;
      font-weight: bold;
      margin-bottom: 16px;
    }
    .subtitle {
      color: #a0aec0;
      margin-bottom: 32px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 16px;
      margin-bottom: 24px;
      justify-items: center;
    }
    .btn {
      padding: 16px;
      border-radius: 16px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.15);
      font-size: 1.1rem;
      font-weight: 500;
      border: none;
      cursor: pointer;
      transition: background 0.2s, transform 0.2s;
    }
    .btn:active {
      transform: scale(0.96);
      filter: brightness(0.9);
    }
    .btn-prev, .btn-next {
      background: #2563eb;
      color: #fff;
    }
    .btn-play {
      background: #16a34a;
      color: #fff;
    }
    .btn-pause {
      background: #eab308;
      color: #fff;
    }
    .btn-volup, .btn-voldown {
      background: #7c3aed;
      color: #fff;
    }
    .status {
      font-size: 1rem;
      color: #4ade80;
      margin-bottom: 24px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1 class="title">🎵 Smart Speaker Control</h1>
    <p class="subtitle">Gesture & Voice Controlled Bluetooth Speaker</p>

    <div class="grid">
      <button class="btn btn-prev">⏮ Prev</button>
      <button class="btn btn-play">▶️ Play</button>
      <button class="btn btn-pause">⏸ Pause</button>
      <button class="btn btn-next">⏭ Next</button>
      <button class="btn btn-volup">🔊 Vol +</button>
      <button class="btn btn-voldown">🔉 Vol -</button>
    </div>

    <div class="status">Status: Waiting for command...</div>
  </div>
  <script>
    // Bind button clicks to their corresponding API commands.
    document.querySelector(".btn-prev").addEventListener("click", () => sendCommand("prev"));
    document.querySelector(".btn-play").addEventListener("click", () => sendCommand("play"));
    document.querySelector(".btn-pause").addEventListener("click", () => sendCommand("pause"));
    document.querySelector(".btn-next").addEventListener("click", () => sendCommand("next"));
    document.querySelector(".btn-volup").addEventListener("click", () => sendCommand("volup"));
    document.querySelector(".btn-voldown").addEventListener("click", () => sendCommand("voldown"));

    function sendCommand(cmd) {
      fetch("/api/" + cmd)
        .then(response => {
          if (!response.ok) throw new Error("Network error");
          return response.text();
        })
        .then(text => {
          document.querySelector(".status").textContent = "Status: " + text;
        })
        .catch(error => {
          document.querySelector(".status").textContent = "Status: Failed to send " + cmd;
        });
    }
  </script>
</body>
</html>
)rawliteral";



// FUNCTIONS
void playWelcomeMessage();
void playShutdownMessage();
void playNextTrack();
void playPreviousTrack();
void togglePlayPause();
void danceModeLEDs();
void detectGesture();


void playWelcomeMessage() {
  Serial.println("Playing welcome message...");
  dfPlayer.play(FILE_WELCOME);
  delay(15000);  
}


void playShutdownMessage() {
  Serial.println("Playing shutdown message...");
  dfPlayer.play(FILE_SHUTDOWN);
  delay(5000);   
}


//  After every two songs, play the periodic message for 15 sec before continuing.
void playNextTrack() {
  songsSincePeriodic++;
  if (songsSincePeriodic >= 2) {
    Serial.println("Playing periodic voice message...");
    dfPlayer.play(FILE_PERIODIC);
    delay(5000);  
    songsSincePeriodic = 0; 
    currentSong++; 
    Serial.print("Resuming playlist with song ");
    Serial.println(currentSong);
    dfPlayer.play(currentSong);
  } else {
    currentSong++;
    Serial.print("Playing next song: ");
    Serial.println(currentSong);
    dfPlayer.play(currentSong);

   }
   lastCommandTime = millis();  // Update command
}

void playPreviousTrack() {
  if (currentSong > FILE_FIRST) {
  currentSong--;
  songsSincePeriodic = 0;  // Reset the counter if going back.
    Serial.print("Playing previous song: ");
    Serial.println(currentSong);
    dfPlayer.play(currentSong);
  } else {
    Serial.println("Already at the first song in the playlist.");
   }
  lastCommandTime = millis();
}


// Paused Everything
void togglePlayPause() {
  if (!systemPaused) {
    Serial.println("Pausing system completely.");
    dfPlayer.pause();   
    systemPaused = true;
  } else {
    Serial.println("Resuming system.");
    dfPlayer.start();    
    systemPaused = false;
    lastCommandTime = millis();
    lastMotionTime = millis();
    // Refresh the gesture baseline.
    previousDistance = lox.readRange();
  }
  delay(300); 
}

// LED animation for "dance mode".
void danceModeLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t hue = (i * 32 + millis() / 2) % 255;
    leds[i] = CHSV(hue, 255, 150);
  }
  FastLED.show();
}

// LIDAR sensor to detect swipe gestures (next/previous track).
void detectGesture() {
  uint16_t currentDistance = lox.readRange();
  if (lox.timeoutOccurred()) {
    Serial.println("Warning: VL53L0X timeout");
    return;
  }

  static unsigned long lastGestureTime = 0;
  const unsigned long gestureInterval = 3000;  // Interval between gesture
  
  if (millis() - lastGestureTime > gestureInterval) {
    int diff = (int)previousDistance - (int)currentDistance;
    
    const int threshold = 100; 
    if (diff > threshold) {
      Serial.println("Gesture detected: swipe IN (next track)");
      playNextTrack();
      lastGestureTime = millis();
    } else if (diff < -threshold) {
      Serial.println("Gesture detected: swipe OUT (previous track)");
      playPreviousTrack();
      lastGestureTime = millis();
    }
    previousDistance = currentDistance;
  }
}

//SETUP FUNCTION 
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(100);
  
  Serial.println("Starting ESP32 Speaker System Project");
  
  // Starting DFPlayer Mini.
  DFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!dfPlayer.begin(DFPlayerSerial)) {
    Serial.println("Error: DFPlayer Mini failed to Start!");
    while(true) { delay(1000); }
  }
  Serial.println("DFPlayer Mini initialized.");
  dfPlayer.volume(15);
  
  // LEDs.
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
  
  // physical buttons.
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PREV_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PLAY_PIN, INPUT_PULLUP);
  
  // PIR sensor.
  pinMode(PIR_PIN, INPUT);
  

  delay(10);
  if (!lox.begin()) {
    Serial.println("Error: VL53L0X failed to Start!");
    while (true) { delay(1000); }
  }
  previousDistance = lox.readRange();
  

  // timers.
  lastMotionTime = millis();
  lastCommandTime = millis();
  
  // Play the welcome message.
  playWelcomeMessage();
  
  // Start the playlist.
  Serial.println("Playing first song from playlist...");
  

  dfPlayer.play(currentSong);



// Connect to WiFi 
  Serial.print("Connecting to WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());


  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", webpage);
  });
  
  // Previous track.
  server.on("/api/prev", HTTP_GET, []() {
    playPreviousTrack();
    server.send(200, "text/plain", "Playing previous track.");
  });
  
  // Play command.
  server.on("/api/play", HTTP_GET, []() {
    if (systemPaused) {
      togglePlayPause(); // Resume if paused.
      server.send(200, "text/plain", "Resumed playback.");
    }
    else {
      server.send(200, "text/plain", "Already playing.");
    }
  });
  
  // Pause command.
  server.on("/api/pause", HTTP_GET, []() {
    if (!systemPaused) {
      togglePlayPause(); // Pause if playing.
      server.send(200, "text/plain", "Paused playback.");
    }
    else {
      server.send(200, "text/plain", "Already paused.");
    }
  });
  
  // Next track.
  server.on("/api/next", HTTP_GET, []() {
    playNextTrack();
    server.send(200, "text/plain", "Playing next track.");
  });
  
  // Increase volume.
  server.on("/api/volup", HTTP_GET, []() {
    if (currentVolume < 30) {  // Maximum volume limit.
      currentVolume++;
      dfPlayer.volume(currentVolume);
      server.send(200, "text/plain", "Volume increased.");
    }
    else {
      server.send(200, "text/plain", "Max volume reached.");
    }
  });
  
  // Decrease volume.
  server.on("/api/voldown", HTTP_GET, []() {
    if (currentVolume > 0) {
      currentVolume--;
      dfPlayer.volume(currentVolume);
      server.send(200, "text/plain", "Volume decreased.");
    }
    else {
      server.send(200, "text/plain", "Min volume reached.");
    }
  });
  
  // Start the web server.
  server.begin();
  Serial.println("Web server started.");

}

// MAIN LOOP 
void loop() {

    server.handleClient();

  // If the system is paused, only check for resume.
  if (systemPaused) {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    // Checks if the play/pause button is pressed to resume.
    if (digitalRead(BUTTON_PLAY_PIN) == LOW) {
      Serial.println("Resume button pressed.");
      togglePlayPause();
      delay(300);
    }
    delay(50);
    return;  // Skip everything if paused
  }
  

  danceModeLEDs();
  
  if (digitalRead(PIR_PIN) == HIGH) {
    lastMotionTime = millis();
  }
  
  detectGesture();

  if (digitalRead(BUTTON_NEXT_PIN) == LOW) {
    Serial.println("Next button pressed.");
    playNextTrack();
    delay(300);  
  }
  if (digitalRead(BUTTON_PREV_PIN) == LOW) {
    Serial.println("Previous button pressed.");
    playPreviousTrack();
    delay(300);  
  }
  if (digitalRead(BUTTON_PLAY_PIN) == LOW) {
    Serial.println("Play/Pause button pressed.");
    togglePlayPause();
    delay(300);  
  }
  

  // If no motion for 3 minutes, immediate shutdown.
  if (millis() - lastMotionTime > 3UL * 60 * 1000) {
    Serial.println("No motion detected for 3 minutes. Initiating shutdown.");
    playShutdownMessage();
    while(true) { delay(1000); }  
  }
  
  // If no commands (buttons or gestures) for 10 minutes, shutdown.
  if (millis() - lastCommandTime > 10UL * 60 * 1000) {
    Serial.println("No commands issued for 10 minutes. Initiating shutdown.");
    playShutdownMessage();
    while(true) { delay(1000); }
  }
  
  delay(50); 
}