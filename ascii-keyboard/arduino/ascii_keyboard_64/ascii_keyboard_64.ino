

//--------------------------------------------------------------------
// 64-key ASCII Keyboard
// To use with an Apple-1 replica 
// 2022-01-03 Stéphane LALLEMAGNE - First prototype
//--------------------------------------------------------------------

// Hardware and wiring

// 1x Arduino Nano or mini 5v (serial is only used for debugging)
// 1x 595 (for row scanning) connected to pins D10,D11,D12
// 1x HCT595 (for ASCII output) connected to pins A0,A1,A2 (HCT = TTL compatible)
// Direct read of column states (via pins D2..D9)
// Pin 13 (Builtin LED) is used as "Caps Lock" indicator

// Main principles

// 8x8 key matrix
// Loop scan each row 0..7, then check for active columns (indicating that a key is pressed at this row+column)
// All inactive row/column lines are HIGH (because of handy pullups) ; so LOW values mean key pressed
// Some keys have special behaviours (modifiers: shift, ctrl, caps lock)


// Row lines (write)
int latchPin = 10; // Connected to ST_CP of 74HC595
int clockPin = 12; // Connected to SH_CP of 74HC595
int dataPin = 11;  // Connected to DS of 74HC595

// Column lines (read)
int inPin[] = {2, 3, 4, 5, 6, 7, 8, 9};

// Output port
int outClock = A2; // Connected to SH_CP of 74HCT595 // Nota : could be pin D12 as we do not write both 595s simultaneously
int outLatch = A1; // Connected to ST_CP of 74HCT595
int outData = A0;  // Connected to DS of 74HCT595

int KBD_STROBE = A3; // Output (to device)
int KBD_READY = A4; // Input (from device)

#define KBD_SEND_TIMEOUT 17

#define DEBOUNCE_TIME_MS             15 // Debounce time (in milliseconds)
#define AUTOREPEAT_INITIAL_TIME_MS  750 // Initial time before repeating chars (if key pressed for a long time)
#define AUTOREPEAT_TIME_MS           40 // Time between repeated chars (if key pressed for a long time)

#define UPPERCASE_ONLY true // Apple I only accepts capital letters

// Keyboard pressed states (we use unsigned long to store full elapsed millis)
unsigned long kbd[8][8] = {0};
unsigned long kbd_rpt[8][8] = {0};

bool capsLocked = false; 

// Character assignation to keys
// 8 rows of 8 keys
// 3 values for each key: "Normal", "w/Shift", "w/Ctrl"

byte charmap[8][8][3] = {
  {
    {'1', '!'},
    {'2', '@', 0x00},
    {'3', '#'},
    {'4', '$'},
    {'5', '%'},
    {'6', '^', 0x1E},
    {'7', '&'},
    {'8', '*'}
  },
  {  
    {'9', '('},
    {'0', ')'},
    {'-', '_', 0x1F},
    {'=', '+'},
    {'_', 0x08, 0x7F}, // _ is an alternative for backspace in Apple I Basic, shift = real BACKSPACE, ctrl = DEL (not sure to be used)
    {'?', '?'}, // F1
    {'?', '?'}, // F2
    {'?', '?'}  // F3
  },
  {
    {0x1B, 0x1B, 0x1B}, // ESC
    {0x09, 0x09, '~'}, // TAB
    {'q', 'Q', 0x11}, // 
    {'w', 'W', 0x17}, // 
    {'e', 'E', 0x05}, // 
    {'r', 'R', 0x12}, // 
    {'t', 'T', 0x14}, // 
    {'y', 'Y', 0x19}  // 
  },
  {
    {'u', 'U', 0x15}, // 
    {'i', 'I', 0x09}, // 
    {'o', 'O', 0x0F}, // 
    {'p', 'P', 0x10}, // 
    {'[', '{', 0x1B}, // 
    {']', '}', 0x1D}, // 
    {'\\','|', 0x1C}, // 
    {'?', '?'}, // Unused
  },
  {
    {0xFF, 0xFF}, // Caps Lock
    {'a', 'A', 0x01}, // 
    {'s', 'S', 0x13}, // 
    {'d', 'D', 0x04}, // 
    {'f', 'F', 0x06}, // 
    {'g', 'G', 0x07}, // 
    {'h', 'H', 0x08}, // 
    {'j', 'J', 0x0A}, // 
  },
  {
    {'k', 'K', 0x0B}, // 
    {'l', 'L', 0x0C}, // 
    {';', ':'}, // 
    {'\'','"', '`'}, // ctrl = backquote
    {0x0D, 0x0D, 0x0D}, // Carriage Return 
    {'?', '?'}, // Unused
    {'?', '?'}, // Unused
    {'?', '?'}, // Unused
  },
  {
    {0xFF, 0xFF}, // Left Shift
    {0x0A, 0x0A, 0x0A}, // Line feed
    {'z', 'Z', 0x1A}, // 
    {'x', 'X', 0x18}, // 
    {'c', 'C', 0x03}, // Control C = ETX (0x03)
    {'v', 'V', 0x16}, // 
    {'b', 'B', 0x02}, // 
    {'?', '?'}, // Unused
  },
  {
    {'n', 'N', 0x0E}, // 
    {'m', 'M', 0x0D}, // 
    {',', '<'}, // 
    {'.', '>'}, // 
    {'/', '?'}, // 
    {0xFF, 0xFF}, // Right Shift 
    {0xFF, 0xFF}, // CTRL 
    {' ', ' ', 0x00}, // SPACE
  }
};

void setup() {
  //Start Serial for debuging purposes
  Serial.begin(115200);
  
  //set pins to output because they are addressed in the main loop
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  pinMode(outLatch, OUTPUT);
  pinMode(outClock, OUTPUT);
  pinMode(outData, OUTPUT);

  pinMode(KBD_STROBE, OUTPUT);
  pinMode(KBD_READY, INPUT);

  for (int i=0; i<8; i++) {
    pinMode(inPin[i], INPUT_PULLUP);
  }
 
  pinMode(LED_BUILTIN, OUTPUT);

  writeOutputPort(0);
}

void writeOutputPort(char c) {
  // Write ASCII to ouput port (using HCT595 shift out)
  digitalWrite(outLatch, LOW);
  shiftOut(outData, outClock, MSBFIRST, c);
  digitalWrite(outLatch, HIGH);
}

char map_to_ascii(int c) {
  /* Convert ESC key */
  if (c == 203) {
    c = 27;
  }

  /* Ctrl A-Z */
  if (c > 576 && c < 603) {
    c -= 576;
  }

  /* Convert lowercase keys to UPPERCASE */
  if (UPPERCASE_ONLY) {
    if (c > 96 && c < 123) {
      c -= 32;
    }
  }
  
  return c;
}

void pia_send(int ic) {

  //Make sure STROBE signal is off 
  digitalWrite(KBD_STROBE, LOW);

  // Calculate right ASCII char to send
  char c = map_to_ascii(ic);

  writeOutputPort(c | 128); // force 8th bit HIGH (RC6502 SerialIO board, as original Apple 1 kbd connector have only 7 data bits)

  digitalWrite(KBD_STROBE, HIGH);
  
  byte timeout;

  // Wait for KBD_READY to go HIGH 
  timeout = KBD_SEND_TIMEOUT;
  while(digitalRead(KBD_READY) != HIGH) {
    delay(1);
    if (timeout == 0) break;
    else timeout--;
  }
  digitalWrite(KBD_STROBE, LOW);

  // Wait for KBD_READY to go LOW 
  timeout = KBD_SEND_TIMEOUT;
  while(digitalRead(KBD_READY) != LOW) {
    delay(1);
    if (timeout == 0) break;
    else timeout--;
  }

  // Set Strobe LOW afterwards (?)
  digitalWrite(KBD_STROBE, LOW); 
   
}

void sendChar(char c) {
  
  //Send ASCII value to output port (this is an ASCII keyboard, finally!)
  pia_send(c);

  //Send via serial (debug)
  if (c == 0x0D) {
    Serial.println();
  } else {
    Serial.print(char(c));
  }
  
}

void loop() {

  //count up routine
  int nbDown = 0;
  for (int j=0; j<8; j++) {
    
    //ground latchPin and hold low for as long as you are transmitting
    digitalWrite(latchPin, LOW);

    // All High except one row (low)
    shiftOut(dataPin, clockPin, MSBFIRST, ~(1 << j));
    
    //return the latch pin high to signal chip that it
    //no longer needs to listen for information
    digitalWrite(latchPin, HIGH);

    //Read column states
    byte val = ~((digitalRead(inPin[0])) 
      | (digitalRead(inPin[1]) << 1)
      | (digitalRead(inPin[2]) << 2)
      | (digitalRead(inPin[3]) << 3)
      | (digitalRead(inPin[4]) << 4)
      | (digitalRead(inPin[5]) << 5)
      | (digitalRead(inPin[6]) << 6)
      | (digitalRead(inPin[7]) << 7));

      // Calculates modifiers states (from previous key presses)
      bool bShift = (kbd[6][0] or kbd[7][5]) xor capsLocked; // left or right shift -xor- capslocked (revert shift if over capslock)
      bool bCtrl = kbd[7][6]; // CTRL key
      byte idx = bCtrl ? 2 : (bShift ? 1 : 0);

      unsigned long now = millis(); // millis() overflows every 50 days, so no big risk for our use
      for (int i=0; i<8; i++) {
        if ((val & (1 << i)) != 0) {
          nbDown++;
          byte c = charmap[j][i][idx];
          if (kbd[j][i] == 0) {
            // --- New key is pressed
            if (j==4 && i==0) {
              // CAPSLOCK
              capsLocked = !capsLocked; 
              digitalWrite(LED_BUILTIN, capsLocked ? HIGH : LOW);
            } else if (c == 0xFF) {
              // Do nothing
            }
            else {
              sendChar(char(c));
            }
            // Set key press start time
            kbd[j][i] = now;
          } else {
            // --- key was already pressed down
            // Check if repeat is needed
            if (c>=32 && c<128) { // Only repeat displayable characters
              if ((now - kbd[j][i]) > AUTOREPEAT_INITIAL_TIME_MS) {
                if ((now - kbd_rpt[j][i]) > AUTOREPEAT_TIME_MS) {
                  sendChar(char(c));
                  kbd_rpt[j][i] = now;
                }
              }
            }
          }
          
        } else {
          //Key up ?
          if (kbd[j][i] != 0) {
            //debounce = ensure to wait enough time before allowing a second keypress
            if ((now - kbd[j][i]) > DEBOUNCE_TIME_MS) {
              kbd[j][i] = 0;
              kbd_rpt[j][i] = 0;
            }
          }
        }   
     }

  }
    
}
