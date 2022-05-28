//YWROBOT
//Compatible with the Arduino IDE 1.0
//Library version:1.1
#include <LiquidCrystal_I2C.h>
#include "words.h"
#include "song.h"
#include "sd_helpers.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

// pins for buttons and other accessories
const int SELECT = 9;
const int DOWN = 8;
const int UP = 7;
const int RIGHT = 6;
const int LEFT = 5;
const int RESET = 4;
const int LED = 3;
const int BUZZER = 2;

int triedMasks[26], guessedMask;
// triedMasks['C' - 'A'] = b10001 means that, based on user's close guesses, C can only be at first and last position

int wordCursor, lastBlinked, idGuess;
String guess, toGuess;
bool pressedReset = false, cardError = true, isEnglish = false, selectedLanguage;

struct Button {
  int pin;
  int lastState = HIGH;
  int state = HIGH;
} up, down, left, right, reset, select;

void playSong() {
  // iterate over the notes of the melody.
  // Remember, the array is twice the number of notes (notes + durations)
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider = melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(BUZZER, melody[thisNote], noteDuration * 0.9);
    digitalWrite(LED, HIGH);

    // Wait for the specief duration before playing the next note.
    delay(noteDuration);

    // stop the waveform generation before the next note.
    digitalWrite(LED, LOW);
    noTone(BUZZER);

  }
}

bool readCard() {
  // re-open the file for reading:

  char* fileName = "ro_words.txt";
  if (isEnglish) fileName = "en_words.txt";
  if (!file.open(fileName, O_READ)) {
    sd.errorPrint("opening file for read failed");
    return true;
  }
  file.seek(0);

  int data;
  noWords = (unsigned long)file.size() / 7; // 5 letters and 2 special characters
  long toGuessIndex = getRandom(noWords);
  long seekValue = 7 * toGuessIndex % file.size();
  file.seek(seekValue);

  toGuess = String();
  for (int pos = 0; pos < 5; ++pos) {
    data = file.read();
    if (data < 0) {
      break;
    }
    toGuess = toGuess + String((char)data);
  }
  Serial.println(toGuess);

  // close the file:
  file.seek(0);
  file.close();
  return false;
}

long getRandom(int maximum) {
  randomSeed(millis());
  long value = random(maximum) % maximum;
  return value;
}

void lcdPrintGuess() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Guess " + String(idGuess) + ": ");

  for (int pos = 0; pos < 5; ++pos) {
    if (!(guessedMask & (1 << pos))) {
      // Only keep guessed letters
      guess[pos] = '@';
    }
    else {
      tone(BUZZER, 1000); // Send 1KHz sound signal...
      digitalWrite(LED, HIGH);
      delay(100);                       // wait for a second
      noTone(BUZZER);     // Stop sound...
      digitalWrite(LED, LOW);
      delay(100);
    }
  }
  guess.replace("@", "_"); // more intuitive for user to input text on "_"
  lcd.print(guess);
  guess.replace("_", "@");

  lcd.setCursor(0, 1);

  // Print hints
  lcd.print("?:");
  for (char car = 'A'; car <= 'Z'; ++car) {
    if (triedMasks[car] == 0) {
      // Never picked the letter
      continue;
    }

    if (toGuess.indexOf(car) == -1) {
      // Letter is not in the word
      continue;
    }

    for (int pos = 0; pos < 5; ++pos) {
      // Skip "greens"
      if (guessedMask & (1 << pos)) {
        continue;
      }
      if (toGuess[pos] == car) {
        // Is yellow
        lcd.print(car);
        break;
      }
    }
  }

}

void extractWord() {
  if (!cardError) {
    cardError = readCard();
  }

  if (cardError) {
    noWords = 5;
    long toGuessIndex = getRandom(noWords);
    toGuess = words[toGuessIndex];
    Serial.println("Word to guess: " + toGuess);
  }

  lcd.clear();
}

void resetGame() {
  guess = "@@@@@";
  wordCursor = 0;
  lastBlinked = 0;
  idGuess = 1;
  pressedReset = true;
  selectedLanguage = false;

  for (char car = 'A'; car <= 'Z'; ++car) {
    for (int pos = 0; pos < 5; ++pos) {
      triedMasks[car] =  0; // b00000
    }
  }
  guessedMask = 0; // guessed none of the letters

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("New game...");
  delay(1000);
  lcd.clear();
  digitalWrite(LED, LOW);
}

void lcdCursorBlink() {
  lcd.setCursor(wordCursor + 9, 0);
  // Turn off the blinking cursor:
  lcd.blink();
}

void checkReset() {
  reset.state = digitalRead(RESET);
  if (reset.state != reset.lastState) {
    if (reset.state == LOW) {
      resetGame();
    }
  }
  reset.lastState = reset.state;
}

// Returns whether game has ended;
bool tryGuess() {
  ++idGuess;

  if (guess == toGuess) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("You won!");

    playSong();

    delay(2000);
    resetGame();
    return true;
  } else if (idGuess == 7) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Word was " + toGuess);
    lcd.setCursor(0, 1);
    lcd.print("Game over!");
    delay(2000);
    resetGame();
    return true;
  }
  else {
    for (int pos = 0; pos < 5; ++pos) {
      if (guessedMask & (1 << pos)) {
        // Already know it is green
        continue;
      }
      if (toGuess[pos] == guess[pos]) {
        // Guessed a letter and its position
        guessedMask |= (1 << pos);
      }
      else {
        // Tried a letter at pos
        triedMasks[guess[pos]] |= (1 << pos);
      }
    }
  }
  return false;
}

void checkButton(Button& button) {
  button.state = digitalRead(button.pin);
  if (button.state != button.lastState) {
    if (button.state == LOW) {
      if (button.pin == DOWN) {
        guess[wordCursor] = guess[wordCursor] + 1 <= 'Z' ? guess[wordCursor] + 1 : 'A';
        lcd.print(guess[wordCursor]);
      }
      else if (button.pin == UP) {
        guess[wordCursor] = guess[wordCursor] - 1 >= 'A' ? guess[wordCursor] - 1 : 'Z';
        lcd.print(guess[wordCursor]);
      }
      else if (button.pin == LEFT) {
        if (selectedLanguage == false) {
          isEnglish = false;
          selectedLanguage = true;
          extractWord();
          lcdPrintGuess();
        }
        else {
          do {
            // Skip guessed letters ("greens")
            wordCursor = wordCursor - 1 >= 0 ? wordCursor - 1 : 4;
          } while (guessedMask & (1 << wordCursor));
        }
        lcd.setCursor(wordCursor, 0);
      }
      else if (button.pin == RIGHT) {
        if (selectedLanguage == false) {
          isEnglish = true;
          selectedLanguage = true;
          extractWord();
          lcdPrintGuess();
        }
        else {
          do {
            // Skip guessed letters
            wordCursor = wordCursor + 1 < 5 ? wordCursor + 1 : 0;
          } while (guessedMask & (1 << wordCursor));
        }
        lcd.setCursor(wordCursor, 0);
      }
      else if (button.pin == SELECT) {
        if (guess.indexOf("@") < 0) {
          // Complete word
          bool hasEnded = tryGuess();
          if (hasEnded) {
            return;
          }

          lcdPrintGuess();
          wordCursor = 0;
          while (guessedMask & (1 << wordCursor)) {
            // Skip guessed letters
            wordCursor = wordCursor + 1 < 5 ? wordCursor + 1 : 0;
          }

          lcd.setCursor(wordCursor, 0);


        }
      }
      else return;
    }
  }
  button.lastState = button.state;
}

void checkDown() {
  down.state = digitalRead(down.pin);
  if (down.state != down.lastState) {
    if (down.state == LOW) {
      guess[wordCursor] = ((guess[wordCursor] + 1) - '@' ) % 27 + '@';
      lcdPrintGuess();
    }
  }
  down.lastState = down.state;
}



bool processCard() {
  // Initialize the SD.
  if (!sd.begin(SD_CONFIG)) {
    sd.initErrorPrint(&Serial);
    return true;
  }
  return false;
}

void setup()
{
  Serial.begin(9600); // open the serial port at 9600 bps:

  // Wait for USB Serial
  while (!Serial) {
    yield();
  }
  cardError = processCard();

  pinMode(RESET, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(UP, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(SELECT, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  reset.pin = RESET;
  down.pin = DOWN;
  up.pin = UP;
  left.pin = LEFT;
  right.pin = RIGHT;
  select.pin = SELECT;

  analogWrite(BUZZER, 2); // volume

  lcd.init(); // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
}

void loop() {
  if (pressedReset == false) {
    lcd.setCursor(0, 0);
    lcd.print("Press start...");
    checkReset();
  }
  else if (selectedLanguage == false) {
    lcd.setCursor(0, 0);
    lcd.print("Left for RO");

    lcd.setCursor(0, 1);
    lcd.print("Right for EN");

    checkReset();
    checkButton(left);
    checkButton(right);
  }
  else {
    checkReset();
    checkButton(down);
    checkButton(up);
    checkButton(left);
    checkButton(right);
    checkButton(select);
    lcdCursorBlink();
  }

}
