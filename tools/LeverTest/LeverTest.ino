// ================================================================
// NX-ISD Lever & Button Hardware Diagnostic Script (SILENT)
// Test purely the Lever Switch (Left, Push, Right) and Main Button
// ================================================================

#define PIN_BTN         13
#define PIN_LEVER_LEFT  16
#define PIN_LEVER_PUSH  15
#define PIN_LEVER_RIGHT 14

int lastBtn   = -1;
int lastLeft  = -1;
int lastPush  = -1;
int lastRight = -1;

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for USB CDC to connect

  // Configure all buttons as INPUT_PULLUP (Active-LOW: 1 = Idle/Open, 0 = Pressed/GND)
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LEVER_LEFT, INPUT_PULLUP);
  pinMode(PIN_LEVER_PUSH, INPUT_PULLUP);
  pinMode(PIN_LEVER_RIGHT, INPUT_PULLUP);

  Serial.println("\n=============================================");
  Serial.println("       NX-ISD LEVER & BUTTON TEST (SILENT)   ");
  Serial.println("=============================================");
  Serial.println("Active-LOW logic: 1 = Idle (3.3V) | 0 = Shorted to GND");
  Serial.println("Move the lever Left, Push, Right, or press BTN...\n");
}

void loop() {
  int btn   = digitalRead(PIN_BTN);
  int left  = digitalRead(PIN_LEVER_LEFT);
  int push  = digitalRead(PIN_LEVER_PUSH);
  int right = digitalRead(PIN_LEVER_RIGHT);

  // Print whenever any button state changes
  if (btn != lastBtn || left != lastLeft || push != lastPush || right != lastRight) {
    Serial.printf("[PIN STATE] BTN(13): %d | LEFT(14): %d | PUSH(15): %d | RIGHT(16): %d",
                  btn, left, push, right);

    if (left == LOW && lastLeft == HIGH) {
      Serial.print("  <-- LEVER LEFT!");
    }
    if (push == LOW && lastPush == HIGH) {
      Serial.print("  [X] LEVER PUSH!");
    }
    if (right == LOW && lastRight == HIGH) {
      Serial.print("  --> LEVER RIGHT!");
    }
    if (btn == LOW && lastBtn == HIGH) {
      Serial.print("  (O) MAIN BTN!");
    }

    Serial.println();

    lastBtn   = btn;
    lastLeft  = left;
    lastPush  = push;
    lastRight = right;
  }

  delay(20);
}
