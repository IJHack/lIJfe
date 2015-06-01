
#include "LedControl.h"
#include "EEPROM.h"

/*
GAME OF LIFE
For an 8x8 LED matrix

Connect digital IO pins 2-13 and analog pins 0-3 to LED matrix (through current limiting resistors)
Analog pin 4 (see RANDOMIZER_ANALOG_PIN) is used to select the randomization mode.
  - connect to 5V to randomize and generate a new rand seed
  - connect to 3.3V to use the last randomized set (USE_EEPROM must be #defined)
  - connect to 0V to use the hard coded starting state
Analog pin 5 (see UNCONNECTED_ANALOG_PIN) is left unconnected and used to add a bit of true randomness to seed the RNG
 */
const int NUMROWS = 8;
const int NUMCOLS = 8;
const int MICROS = 100;

const int EEPROM_ADDRESS_1 = 34;
const int EEPROM_ADDRESS_2 = 35;

const int RANDOMIZER_ANALOG_PIN = 4;
const int UNCONNECTED_ANALOG_PIN = 5;

const int BAUD_RATE = 9600;

/**
 * Conditional compilation directives
 */
//#define USE_EEPROM 1                // uncomment this line to enable eeprom storage

boolean gameBoard[NUMROWS][NUMCOLS] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 1, 1, 0, 0, 0, 0 },
  { 0, 1, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 }
};

boolean newGameBoard[NUMROWS][NUMCOLS] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
};

/////////////////////////////////

byte gameboard[] = {
  B11000011,
  B11000011,
  B00000000,
  B00000011,
  B11000011,
  B11100111,
  B01111110,
  B00111100
};

static const int DATA_PIN = 20;
static const int CLK_PIN  = 5;
static const int CS_PIN   = 21;

LedControl lc=LedControl(DATA_PIN, CLK_PIN, CS_PIN, 1);

void setSprite(byte *sprite) {
  for (int r = 0; r < 8; r++) {
    lc.setColumn(0, 7 - r, sprite[r]);
  }
}


void setup() {
  // The MAX72XX is in power-saving mode on startup,
  // we have to do a wakeup call
  //  pinMode(POTPIN, INPUT);
  randomSeed(analogRead(5));
  Serial.begin(BAUD_RATE);
  Serial.println("\nBegin setup()");
#ifdef USE_EEPROM
  Serial.println("EEPROM code enabled");
#else
  Serial.println("EEPROM code disabled");
#endif // defined USE_EEPROM
  lc.shutdown(0, false);
  // Set the brightness to a medium values
  lc.setIntensity(0, 5);
  // and clear the display
  lc.clearDisplay(0);
  randomSeed(analogRead(0));

  lc.setIntensity(0, 15);
  setSprite(gameboard);
  delay(500);
  lc.clearDisplay(0);
  
  setUpInitialBoard();
  Serial.println("End setup()\n");
}


void loop() {
  long time = millis();

  // Display the current game board for approx. 250ms
  do {
    displayGameBoard();
  }
  while (millis() < time + 250);

  // Calculate the next iteration
  calculateNewGameBoard();
  swapGameBoards();
};

/**
 * Does some randomizing of the initial board state.
 */
void setUpInitialBoard() {
  // Generate a new seed for the RNG
  int seed = analogRead(UNCONNECTED_ANALOG_PIN);

  // Look at how the randomizer pin is connected.
  // If it's pulled high, then generate and store a new seed.
  // If it's middle (3.3v) then read the seed from EEPROM (if that code is enabled with USE_EEPROM)
  pinMode(RANDOMIZER_ANALOG_PIN, INPUT);
  int randomizerPinValue = analogRead(RANDOMIZER_ANALOG_PIN);
  if (randomizerPinValue > 900) {  // connected to +5V
#ifdef USE_EEPROM
    // Generate and store a new random seed
    Serial.println("Generating new randseed...");
    Serial.print("Storing ");
    Serial.print(seed, DEC);
    EEPROM.write(EEPROM_ADDRESS_1, lowByte(seed));
    EEPROM.write(EEPROM_ADDRESS_2, highByte(seed));
    Serial.print("... done\n");
#endif // defined USE_EEPROM

    Serial.print("Seeding RNG with ");
    Serial.print(seed, DEC);
    Serial.print("\n");

    randomSeed(seed);
    perturbInitialGameBoard();
  } else if (randomizerPinValue > 300) {  // connected to +3.3V
    // Retrieve random seed from EEPROM
#ifdef USE_EEPROM
    Serial.println("Retrieving randseed...");
    int hi = EEPROM.read(EEPROM_ADDRESS_2);
    int lo = EEPROM.read(EEPROM_ADDRESS_1);
    seed = (hi << 8 ) | lo;
    Serial.print("Read ");
    Serial.print(seed, DEC);
    Serial.print("\n");
#endif // defined USE_EEPROM

    Serial.print("Seeding RNG with ");
    Serial.print(seed, DEC);
    Serial.print("\n");

    randomSeed(seed);
    perturbInitialGameBoard();
  } else {
    Serial.println("Using basic board.");
  }
}

/**
 * Makes a small number of random changes to the game board
 */
void perturbInitialGameBoard() {
  int numChanges = random(20,100);
  for (int i=0; i<numChanges; i++) {
    int row = random(0, NUMROWS);
    int col = random(0, NUMCOLS);
    gameBoard[row][col] = !gameBoard[row][col]; // toggle the led in this position
  }
}

/**
 * Loops over all game board positions, and briefly turns on any LEDs for "on" positions.
 */
void displayGameBoard() {
  
  for (int x = 0 ; x < NUMROWS; x++) {
    byte b = 0;
    for (int y = 0 ; y < NUMCOLS ; y++) {
      b = b << 1;
        if (gameBoard[x,y]) {
          b |= 1;
        }
    }
    gameboard[x] = b;
  }
  
  setSprite(gameboard);
}

/**
 * Turns on the specified LED for a v. short period of time
 */
void pulseLed(byte row, byte col) {
  setLedOn(row, col);
  delayMicroseconds(MICROS);
  setLedOff(row, col);
}

/**
 * Counts the number of active cells surrounding the specified cell.
 * Cells outside the board are considered "off"
 * Returns a number in the range of 0 <= n < 9
 */
byte countNeighbors(byte row, byte col) {
  byte count = 0;
  for (char rowDelta=-1; rowDelta<=1; rowDelta++) {
    for (char colDelta=-1; colDelta<=1; colDelta++) {
      // skip the center cell
      if (!(colDelta == 0 && rowDelta == 0)) {
        if (isCellAlive(rowDelta+row, colDelta+col)) {
          count++;
        }
      }
    }
  }
  return count;
}

/**
 * Returns whether or not the specified cell is on.
 * If the cell specified is outside the game board, returns false.
 */
boolean isCellAlive(char row, char col) {
  if (row < 0 || col < 0 || row >= NUMROWS || col >= NUMCOLS) {
    return false;
  }

  return (gameBoard[row][col] == 1);
}

/**
 * Encodes the core rules of Conway's Game Of Life, and generates the next iteration of the board.
 * Rules taken from wikipedia.
 */
void calculateNewGameBoard() {
  for (byte row=0; row<NUMROWS; row++) {
    for (byte col=0; col<NUMCOLS; col++) {
      byte numNeighbors = countNeighbors(row, col);

      if (gameBoard[row][col] && numNeighbors < 2) {
        // Any live cell with fewer than two live neighbours dies, as if caused by under-population.
        newGameBoard[row][col] = false;
      } else if (gameBoard[row][col] && (numNeighbors == 2 || numNeighbors == 3)) {
        // Any live cell with two or three live neighbours lives on to the next generation.
        newGameBoard[row][col] = true;
      } else if (gameBoard[row][col] && numNeighbors > 3) {
        // Any live cell with more than three live neighbours dies, as if by overcrowding.
        newGameBoard[row][col] = false;
      } else if (!gameBoard[row][col] && numNeighbors == 3) {
        // Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.
        newGameBoard[row][col] = true;
      } else {
        // All other cells will remain off
        newGameBoard[row][col] = false;
      }
    }
  }
}

/**
 * Copies the data from the new game board into the current game board array
 */
void swapGameBoards() {
  for (byte row=0; row<NUMROWS; row++) {
    for (byte col=0; col<NUMCOLS; col++) {
      gameBoard[row][col] = newGameBoard[row][col];
    }
  }
}


void setLedOn(byte row, byte col) {
  lc.setLed(0, row, col, HIGH);
}
void setLedOff(byte row, byte col) {
  lc.setLed(0, row, col, LOW);
}

