//Libraries for Dot Matrix
#include <SPI.h>
#include <DMD.h>
#include <TimerOne.h>
#include <stdlib.h>
#include <stdbool.h>


DMD dmd(1, 1);

#define SPEAKERPIN 4
#define BUTTON1PIN 2
#define BUTTON2PIN 3
#define COLUMNS 32
#define ROWS 16
#define BSTEPS 7

int brightness=3;   //display brightness
int bcount=0;   //variable used to keep track of pwm phase

/** Stores the state of the screen. **/
//unsigned short bitmap[COLUMNS] = {0};

/** Used for every pixel of the snake, and the fly **/
typedef struct segment_s Segment;
struct segment_s {
  int row;
  int col;
  Segment* next;
};


/** Holds a pointer to the first segment of its body, the fly, and its
 * current direction. **/
typedef struct snek_s Snake;
struct snek_s {
  Segment* tail; //The HEAD of a linked list... sorry.
  Segment* fly;
  int direction;
};


/** Used in the search for a free pixel, could instead use a single byte
 * to represent a pixel of the display, but this is a bit more
 * readable. **/
typedef struct tuple_s Tuple;
struct tuple_s {
  int row;
  int col;
};



void ScanDMD(){       //also implements a basic pwm brightness control
  dmd.scanDisplayBySPI();
  bcount=bcount+brightness;
  if(bcount<BSTEPS){digitalWrite(9,LOW);}
  bcount=bcount%BSTEPS;
}

void setup(){
  Timer1.initialize( 500 );           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
  Timer1.attachInterrupt( ScanDMD );   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()
  dmd.clearScreen( true );     //clear/init the DMD pixels held in RAM
  pinMode(BUTTON1PIN,INPUT_PULLUP);
  pinMode(BUTTON2PIN,INPUT_PULLUP);
  pinMode(SPEAKERPIN,OUTPUT);
  digitalWrite(SPEAKERPIN,LOW);    //speaker off
  Serial.begin(9600);
}


void loop(){
  dmd.clearScreen( true );
  
  Serial.println("1");delay(50);
  Segment sneks_fly = {0, 0, 0}; //Placeholder
  Serial.println("2");delay(50);
  Snake snek = {0, &sneks_fly, rand()%4}; //Uses placeholder, and random direction
  Serial.println("3");delay(50);
  new_fly(&snek); //Puts the fly in a random spot
  Serial.println("4");delay(50);
  Segment temp = {sneks_fly.row, sneks_fly.col, 0}; //Save it as a separate segment
  Serial.println("5");delay(50);
  snek.tail = &temp; //Uses that segment as the first segment
  Serial.println("6");delay(50);
  new_fly(&snek); //Gets a new fly for real.
  Serial.println("7");delay(50);
  
  update_bitmap(&snek);
  Serial.println("8");delay(50);
  //updateDisplay();
  delay(250);
  
  int start_length = 3; //Start length in segments
  byte dir = 0;
    
  while (1) {
    if (start_length) {
      start_length--;
    }
    
    int trial = Serial.read() - '0';
    if (trial == 4) {
      dir = 1;
      Serial.println("Left");
    } else if (trial == 8) {
      dir = 2;
      Serial.println("Up");
    } else if (trial == 6) {
      dir = 3;
      Serial.println("Right");
    } else if (trial == 2) {
      dir = 0;
      Serial.println("Down");
    }
      
    int status = game_step(&snek, dir, start_length);
    
    update_bitmap(&snek);
    //updateDisplay();
    delay(100);
    
    if (status != 0) {
      break;
    }   
  }
}






/** Finds a free pixel by randomly drawing a tuple struct from those
 * that do not hold a segment of the given snake. This updates the fly
 * associated with the given snake. If it is successful, it returns 0,
 * otherwise 1. **/
int new_fly(Snake* snek) {
  unsigned short covered[COLUMNS] = {0};
  
  Segment* focus = snek->tail;
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
  
  snek->fly->row = row;
  snek->fly->col = col;
  
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
int game_step(Snake* snek, int nav_direction, bool starting) {
  //Save tail data in case of length increase
  int tail_row = snek->tail->row;
  int tail_col = snek->tail->col;
  
  //Shift snake segments
  Segment* focus_seg = snek->tail;
  Segment* next_seg = focus_seg->next;
  while (next_seg) {
    focus_seg->row = next_seg->row;
    focus_seg->col = next_seg->col;
    focus_seg = next_seg;
    next_seg = focus_seg->next;
  }
  Segment* head = focus_seg;
  
  //Change direction - probably doesn't need to be in this function... (before instead)
  if (nav_direction != -1) { //Used to use only one button, so either pressed or not... Update mentioned in function documentation
    if ((snek->direction - nav_direction) % 2 != 0) {
      snek->direction = nav_direction;
    }
  }
  
  //Step forward
  switch (snek->direction) {
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
  if (((head->row == snek->fly->row) && (head->col == snek->fly->col)) || starting) {
    
    Segment* old_tail = malloc(sizeof(Segment)); 
    old_tail->row = tail_row;
    old_tail->col = tail_col;
    old_tail->next = snek->tail;
    snek->tail = old_tail;
    
    if (((head->row == snek->fly->row) && (head->col == snek->fly->col))) { //Ate a fly
      int status = new_fly(snek);
      if (status != 0) {
        return 1;
      }
    }
  } else {
    dmd.writePixel(tail_col,tail_row,GRAPHICS_NORMAL,0);
  }
  
  //Check for collisions
  if (collision(snek->tail, head)) {
    return 1;
  }
  
  return 0;
}


/** Puts a 1 in every position in the bitmask where a snake segment or a
 * fly should be represented. **/
void update_bitmap(Snake* snek) {
  //for (int i = 0; i < COLUMNS; i++) {
  //  bitmap[i] = 0;
  //}
  
  Segment* focus_seg = snek->tail;
  do {
    //bitmap[focus_seg->col] |= 1 << focus_seg->row;
    
    dmd.writePixel(focus_seg->col,focus_seg->row,GRAPHICS_NORMAL,1);
    focus_seg = focus_seg->next;
  } while (focus_seg);
  //bitmap[snek->fly->col] |= 1 << snek->fly->row;
    dmd.writePixel(snek->fly->col,snek->fly->row,GRAPHICS_NORMAL,1);
}




