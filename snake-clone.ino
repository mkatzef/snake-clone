/* 
 * snake-clone.ino
 * 
 * A clone of the classic game "Snake" for an Arduino Nano. Controlled through
 * serial communication.
 * 
 * Written by Marc Katzef
 */

#include <stdlib.h>
#include <stdbool.h>

//Libraries for Dot Matrix
#include <SPI.h>
#include <DMD.h>
#include <TimerOne.h>

#define COLUMNS 32
#define ROWS 16

#define SCAN_PERIOD_MICROS 500
#define BRIGHTNESS_STEPS 2 // Number of screen scans per PWM period
#define BRIGHTNESS 1 // (Brightness %) * BRIGHTNESS_STEPS

// Controls, based on number pad
#define DIRECTION_CHAR_UP '8'
#define DIRECTION_CHAR_RIGHT '6'
#define DIRECTION_CHAR_DOWN '2'
#define DIRECTION_CHAR_LEFT '4'

#define DEFAULT_SNAKE_LENGTH 3
#define GAME_STEP_PERIOD_MILLIS 100

enum directions {DIR_DOWN=0, DIR_LEFT, DIR_UP, DIR_RIGHT};

DMD dmd(1, 1);

int brightnessCount = 0;   // Keeps track of brightness PWM phase

/* 
 * The basic component of a linked list storing locations on the display. Used to
 * represent each pixel of the snake, and the fly.
 */
typedef struct segment_s Segment;
struct segment_s {
  int row;
  int col;
  Segment* next;
};


/* 
 * A structure to represent the current state of the snake. Stores a pointer to the
 * first segment of the snake's body, the fly, and the snake's current direction.
 */
typedef struct snake_s Snake;
struct snake_s {
  Segment* tail; // The HEAD of a linked list... sorry.
  Segment* fly;
  int direction;
};


/* 
 * Scans the display, temporarily enabling each DMD pixel written HIGH. Also
 * implements a simple PWM-based brightness control scheme. Supplies additional
 * power to display for a number of cycles every period.
 * Should be called very frequently, perhaps through timer interrupt.
 */
void scanDmd() {
  dmd.scanDisplayBySPI();
  brightnessCount++;
  
  if(brightnessCount < BRIGHTNESS) {
    digitalWrite(9, HIGH);
  } else {
    digitalWrite(9, LOW);
  }
  
  if (brightnessCount >= BRIGHTNESS_STEPS) {
    brightnessCount = 0;
  }
}


/* 
 * Initializes required display and communication features.
 */
void setup(){
  Timer1.initialize(SCAN_PERIOD_MICROS);
  Timer1.attachInterrupt(scanDmd);
  dmd.clearScreen(true);

  Serial.begin(9600);
}


/* 
 * Initializes snake and fly randomly on display before entering infinite game
 * loop. In infinite loop, snake may be controlled by sending one of the expected
 * DIRECTION_CHAR_X symbols through serial.
 */
void loop(){
  dmd.clearScreen(true);
  
  Segment tempFly = {0, 0, 0};                      // Placeholder fly
  Snake snake = {0, &tempFly, rand() % 4};          // Create new snake using fly and random direction
  newFly(&snake);                                   // Put the placeholder fly in a random spot
  Segment tempTail = {tempFly.row, tempFly.col, 0}; // Save it as a separate segment
  snake.tail = &tempTail;                           // Use that segment as the first segment
  newFly(&snake);                                   // Get a new fly again
  
  updateBitmap(&snake);
  
  int initLengthCounter = DEFAULT_SNAKE_LENGTH; // Number of times snake must grow to reach default length
    
  while (1) {
    if (initLengthCounter > 0) {
      initLengthCounter--;
    }

    if (Serial.available()) {
      char inputDir = Serial.read();
      int newDirection = -1;
      
      switch(inputDir) {
      case DIRECTION_CHAR_LEFT:
        newDirection = DIR_LEFT;
        Serial.println("Left");
        break;
      case DIRECTION_CHAR_UP:
        newDirection = DIR_UP;
        Serial.println("Up");
        break;
      case DIRECTION_CHAR_RIGHT:
        newDirection = DIR_RIGHT;
        Serial.println("Right");
        break;
      case DIRECTION_CHAR_DOWN:
        newDirection = DIR_DOWN;
        Serial.println("Down");
        break;
      default:
        Serial.print("Unrecognized char: \"");
        Serial.print(inputDir);
        Serial.println('"');
      }
      
      if (newDirection != -1) {
        if ((snake.direction - newDirection) % 2 == 0) {
          Serial.println("Ignored");
        } else {
          snake.direction = newDirection;
        }
      }
    }
      
    int status = gameStep(&snake, initLengthCounter);
    
    updateBitmap(&snake);
    delay(GAME_STEP_PERIOD_MILLIS);
    
    if (status != 0) {
      break;
    }   
  }
}


/** Finds a free pixel by randomly drawing a tuple struct from those
 * that do not hold a segment of the given snake. This updates the fly
 * associated with the given snake. If it is successful, it returns 0,
 * otherwise 1. **/
int newFly(Snake* snake) {
  unsigned short covered[COLUMNS] = {0};
  
  Segment* focus = snake->tail;
  while (focus) {
    covered[focus->col] |= (1 << focus->row);
    focus = focus->next;
  }
  
  byte uncoveredCols[COLUMNS] = { 0 };

  byte counter = 0;
  for (byte col = 0; col < COLUMNS; col++) {
    if (covered[col] < 0xFFFF) {
      uncoveredCols[counter] = col;
      counter++;
    }
  }

  if (counter == 0) {
    return 1;
  }

  byte col = rand() % counter;
  byte column = uncoveredCols[col];
  
  counter = 0;
  byte uncoveredRows[ROWS] = {0};
  for (byte row = 0; row < ROWS; row++) {
    if (column & (1<<row)) {
      uncoveredRows[counter] = row;
      counter++;
    }
  }
  
  byte row = uncoveredRows[rand() % counter];
  
  snake->fly->row = row;
  snake->fly->col = col;
  
  return 0;
}


/** Identifies if the new head pixel is already occupied by a segment in
 * the linked list starting with tail. If it is, returns 1. If no
 * collision occurs, returns 0. **/
int collision(Segment* tail, Segment* head) {
  Segment* focus = tail;
  while (focus->next) { //check all but the head itself
    if ((focus->row == head->row) && (focus->col == head->col)) {
      return 1;
    }
    focus = focus->next;
  }
  return 0;
}


/** Updates the snake segment positions by shuffling the segment
 * coordinates toward the snake's tail, finds the new head position
 * according to the current direction. If the snake is starting or a fly
 * is eaten, the original end segment is recreated (malloc) and kept.
 * If a collision occurs or a new fly couldn't be created, returns 1.
 * Otherwise all is well and returns 0. 
 * Should update for nav_direction to never be -1, instead always the
 * current direction. **/
int gameStep(Snake* snake, bool starting) {
  //Save tail data in case of length increase
  int tail_row = snake->tail->row;
  int tail_col = snake->tail->col;
  
  //Shift snake segments
  Segment* focus_seg = snake->tail;
  Segment* next_seg = focus_seg->next;
  while (next_seg) {
    focus_seg->row = next_seg->row;
    focus_seg->col = next_seg->col;
    focus_seg = next_seg;
    next_seg = focus_seg->next;
  }
  Segment* head = focus_seg;
  
  //Step forward
  switch (snake->direction) {
    case 0:
    head->row = (head->row+ROWS-1) % ROWS;
    break; 
    
    case 1:
    head->col = (head->col+1) % COLUMNS;
    break;
    
    case 2:
    head->row = (head->row+1) % ROWS;
    break;
    
    case 3:
    head->col = (head->col+COLUMNS-1) % COLUMNS;
    break;
  }
  
  //Need to grow
  if (((head->row == snake->fly->row) && (head->col == snake->fly->col)) || starting) {
    
    Segment* old_tail = malloc(sizeof(Segment)); // WARNING: use of malloc without corresponding free anywhere.
    old_tail->row = tail_row;
    old_tail->col = tail_col;
    old_tail->next = snake->tail;
    snake->tail = old_tail;
    
    if (((head->row == snake->fly->row) && (head->col == snake->fly->col))) { //Ate a fly
      int status = newFly(snake);
      if (status != 0) {
        return 1;
      }
    }
  } else {
    dmd.writePixel(tail_col,tail_row,GRAPHICS_NORMAL,0);
  }
  
  //Check for collisions
  if (collision(snake->tail, head)) {
    return 1;
  }
  
  return 0;
}


/** Puts a 1 in every position in the bitmask where a snake segment or a
 * fly should be represented. **/
void updateBitmap(Snake* snake) {
  //for (int i = 0; i < COLUMNS; i++) {
  //  bitmap[i] = 0;
  //}
  
  Segment* focus_seg = snake->tail;
  do {
    //bitmap[focus_seg->col] |= 1 << focus_seg->row;
    
    dmd.writePixel(focus_seg->col,focus_seg->row,GRAPHICS_NORMAL,1);
    focus_seg = focus_seg->next;
  } while (focus_seg);
  //bitmap[snake->fly->col] |= 1 << snake->fly->row;
    dmd.writePixel(snake->fly->col,snake->fly->row,GRAPHICS_NORMAL,1);
}




