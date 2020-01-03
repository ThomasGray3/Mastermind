#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

//-------------Defining constant variables -----------------------------

#ifndef    TRUE
#  define    TRUE    (1==1)
#  define    FALSE    (1==2)
#endif

#define    PAGE_SIZE        (4*1024)
#define    BLOCK_SIZE        (4*1024)

#define    INPUT         0
#define    OUTPUT         1

#define    LOW             0
#define    HIGH         1

//----------------LCD PINS(BCM)-----------------------------------------

#define STRB_PIN 24
#define RS_PIN   25
#define DATA0_PIN 23
#define DATA1_PIN 10
#define DATA2_PIN 27
#define DATA3_PIN 22

//---------------LED/BUTTON PINS(BCM)-----------------------------------

#define GREENLED_PIN 13
#define BLUELED_PIN 16
#define BUTTON_PIN 19

//--------------DEFINE CHARACTERS FOR LCD-------------------------------

static unsigned char newChar [8] = {
    0b11111,
    0b10001,
    0b10001,
    0b10101,
    0b11111,
    0b10001,
    0b10001,
    0b11111,
} ;

static unsigned char hawoNewChar [8] = {
    0b11111,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11111,
} ;

//------------DATA STRUCTURE THAT REPERESENTS THE LCD-------------------

struct lcdDataStruct {
    int bits, rows, cols ;
    int rsPin, strbPin ;
    int dataPins [8] ;
    int cx, cy ;
} ;

static int lcdControl ;

//-----------------------LCD SETUP FACTS--------------------------------

#define    LCD_CLEAR    0x01
#define    LCD_HOME    0x02
#define    LCD_ENTRY    0x04
#define    LCD_CTRL    0x08
#define    LCD_CDSHIFT    0x10
#define    LCD_FUNC    0x20
#define    LCD_CGRAM    0x40
#define    LCD_DGRAM    0x80

// Bits in the entry register

#define    LCD_ENTRY_SH        0x01
#define    LCD_ENTRY_ID        0x02

// Bits in the control register

#define    LCD_BLINK_CTRL        0x01
#define    LCD_CURSOR_CTRL        0x02
#define    LCD_DISPLAY_CTRL    0x04

// Bits in the function register

#define    LCD_FUNC_F    0x04
#define    LCD_FUNC_N    0x08
#define    LCD_FUNC_DL    0x10

#define    LCD_CDSHIFT_RL    0x04

//------------------------SETUP GPIO MASK-------------------------------

static volatile unsigned int gpiobase ;
static volatile uint32_t *gpio ;
#define    PI_GPIO_MASK    (0xFFFFFFC0)

//-------------------------DEFINE VARIABLES FOR LEDS--------------------

int fSelBLUE, shiftBLUE, fSelGREEN, shiftGREEN, fSelBUTTON, shiftBUTTON = 0;
int pin,  clrOff, setOff, off;
int  j = 0;
int k;
int CLR = 10;
int SET = 7;
int LEV = 13;
int theValue;

int secret[3];
int guess[3];
int counter=0;
time_t time(time_t *t);

//--------------------------\\GPSET(DigitalWrite) FOR LCD---------------


void pinMode (volatile uint32_t *gpio, int pin, int value){
    
    int fSel = pin / 10;
    int shiftTemp = pin % 10;
    int shift = shiftTemp*3;
    
    asm(/*Set PIN as output */
        "\tLDR R0, [%[gpio], %[fSel]] \n"
        "\tMOV R2, #0b111 \n"
        "\tMOV R3, %[shift] \n"
        "\tLSL R2, R3 \n"
        "\tBIC R0, R2 \n"
        "\tMOV R2, #0b001 \n"
        "\tMOV R3, %[shift] \n"
        "\tLSL R2, R3 \n"
        "\tORR R0, R2 \n"
        "\tSTR R0, [%[gpio], %[fSel]] \n"
        :
        : [gpio] "r" (gpio)
        , [fSel] "r" (fSel*4)
        , [shift] "r" (shift)
        : "r0", "r2", "r3", "cc" //registers
        );
    
}

void digitalWrite (volatile uint32_t *gpio, int pin, int value){
    
    if(value == 1){
        asm(
            "\tMOV R1, %[gpio] \n"
            "\tMOV R2, #0b1 \n"
            "\tLSL R2, %[pin] \n"
            "\tSTR R2, [R1, #28] \n"
            :
            : [gpio] "r" (gpio)
            , [pin] "r" (pin)
            : "r1", "r2", "cc" //registers
            );
    }else{
        asm(
            "\tMOV R1, %[gpio]\n"
            "\tMOV R2, #0b1\n"
            "\tLSL R2, %[pin]\n"
            "\tSTR R2, [R1, #40]\n"
            :
            : [gpio] "r" (gpio)
            , [pin] "r" (pin)
            : "r1", "r2", "cc" //registers
            );
    }
}


int buttonWrite(volatile uint32_t *gpio, int pin){
    int res;
    
    asm(/*Set PIN as output */
        "\tLDR R1, [%[gpio], #52]\n"
        "\tMOV R2, #0b1\n"
        "\tLSL R2, %[pin]\n"
        "\tAND R2, R1\n"
        "\tMOV %[r], R2\n"
        : [r] "=r" (res) //output
        : [gpio] "r" (gpio)
        , [pin] "r" (pin)
        : "r1", "r2", "cc" //registers
        );
    
    return res;
    
}
//--------------------------MAKE DELAY METHODS---------------------------

void delay (unsigned int howLong) {
    
    struct timespec sleeper, dummy ;
    
    sleeper.tv_sec  = (time_t)(howLong / 1000) ;
    sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;
    nanosleep (&sleeper, &dummy) ;
}

void delayMicroseconds (unsigned int howLong)
{
    struct timespec sleeper ;
    unsigned int uSecs = howLong % 1000000 ;
    unsigned int wSecs = howLong / 1000000 ;
    
    /**/ if (howLong ==   0)
        return ;
#if 0
    else if (howLong  < 100)
        delayMicrosecondsHard (howLong) ;
#endif
    else
    {
        sleeper.tv_sec  = wSecs ;
        sleeper.tv_nsec = (long)(uSecs * 1000L) ;
        nanosleep (&sleeper, NULL) ;
    }
}

//-----------------------STROBE LCD CREATE STRUCT-----------------------

void strobe (const struct lcdDataStruct *lcd)
{
    
    // Note timing changes for new version of delayMicroseconds ()
    digitalWrite (gpio, lcd->strbPin, 1) ; delayMicroseconds (50) ;
    digitalWrite (gpio, lcd->strbPin, 0) ; delayMicroseconds (50) ;
}


//------------------------LCD FUNCTIONS---------------------------------


void sendDataCmd (const struct lcdDataStruct *lcd, unsigned char data)
{
    register unsigned char myData = data ;
    unsigned char          i, d4 ;
    
    if (lcd->bits == 4)
    {
        d4 = (myData >> 4) & 0x0F;
        for (i = 0 ; i < 4 ; ++i)
        {
            digitalWrite (gpio, lcd->dataPins [i], (d4 & 1)) ;
            d4 >>= 1 ;
        }
        strobe (lcd) ;
        
        d4 = myData & 0x0F ;
        for (i = 0 ; i < 4 ; ++i)
        {
            digitalWrite (gpio, lcd->dataPins [i], (d4 & 1)) ;
            d4 >>= 1 ;
        }
    }
    else
    {
        for (i = 0 ; i < 8 ; ++i)
        {
            digitalWrite (gpio, lcd->dataPins [i], (myData & 1)) ;
            myData >>= 1 ;
        }
    }
    strobe (lcd) ;
}

/*
 * lcdPutCommand:
 *    Send a command byte to the display
 *********************************************************************************
 */

void lcdPutCommand (const struct lcdDataStruct *lcd, unsigned char command)
{
#ifdef DEBUG
    fprintf(stderr, "lcdPutCommand: digitalWrite(%d,%d) and sendDataCmd(%d,%d)\n", lcd->rsPin,   0, lcd, command);
#endif
    digitalWrite (gpio, lcd->rsPin,   0) ;
    sendDataCmd  (lcd, command) ;
    delay (2) ;
}

void lcdPut4Command (const struct lcdDataStruct *lcd, unsigned char command)
{
    register unsigned char myCommand = command ;
    register unsigned char i ;
    
    digitalWrite (gpio, lcd->rsPin,   0) ;
    
    for (i = 0 ; i < 4 ; ++i)
    {
        digitalWrite (gpio, lcd->dataPins [i], (myCommand & 1)) ;
        myCommand >>= 1 ;
    }
    strobe (lcd) ;
}

//------------------------------LCD HOME--------------------------------

void lcdHome (struct lcdDataStruct *lcd)
{
#ifdef DEBUG
    fprintf(stderr, "lcdHome: lcdPutCommand(%d,%d)\n", lcd, LCD_HOME);
#endif
    lcdPutCommand (lcd, LCD_HOME) ;
    lcd->cx = lcd->cy = 0 ;
    delay (5) ;
}

//------------------------------LCD CLEAR-------------------------------

void lcdClear (struct lcdDataStruct *lcd)
{
#ifdef DEBUG
    fprintf(stderr, "lcdClear: lcdPutCommand(%d,%d) and lcdPutCommand(%d,%d)\n", lcd, LCD_CLEAR, lcd, LCD_HOME);
#endif
    lcdPutCommand (lcd, LCD_CLEAR) ;
    lcdPutCommand (lcd, LCD_HOME) ;
    lcd->cx = lcd->cy = 0 ;
    delay (5) ;
}

//-----------------------------LCD CURSOR POSITION----------------------

void lcdPosition (struct lcdDataStruct *lcd, int x, int y)
{
    
    if ((x > lcd->cols) || (x < 0))
        return ;
    if ((y > lcd->rows) || (y < 0))
        return ;
    
    lcdPutCommand (lcd, x + (LCD_DGRAM | (y>0 ? 0x40 : 0x00)  /* rowOff [y] */  )) ;
    
    lcd->cx = x ;
    lcd->cy = y ;
}



//----------------------LCD CURSOR AND DISPLAY ON/OFF-------------------

void lcdDisplay (struct lcdDataStruct *lcd, int state)
{
    if (state)
        lcdControl |=  LCD_DISPLAY_CTRL ;
    else
        lcdControl &= ~LCD_DISPLAY_CTRL ;
    
    lcdPutCommand (lcd, LCD_CTRL | lcdControl) ;
}

void lcdCursor (struct lcdDataStruct *lcd, int state)
{
    if (state)
        lcdControl |=  LCD_CURSOR_CTRL ;
    else
        lcdControl &= ~LCD_CURSOR_CTRL ;
    
    lcdPutCommand (lcd, LCD_CTRL | lcdControl) ;
}

void lcdCursorBlink (struct lcdDataStruct *lcd, int state)
{
    if (state)
        lcdControl |=  LCD_BLINK_CTRL ;
    else
        lcdControl &= ~LCD_BLINK_CTRL ;
    
    lcdPutCommand (lcd, LCD_CTRL | lcdControl) ;
}

//--------------------LCD PUT CHAR TO DISPLAY---------------------------

void lcdPutchar (struct lcdDataStruct *lcd, unsigned char data)
{
    digitalWrite (gpio, lcd->rsPin, 1) ;
    sendDataCmd  (lcd, data) ;
    
    if (++lcd->cx == lcd->cols)
    {
        lcd->cx = 0 ;
        if (++lcd->cy == lcd->rows)
            lcd->cy = 0 ;
        
        lcdPutCommand (lcd, lcd->cx + (LCD_DGRAM | (lcd->cy>0 ? 0x40 : 0x00)   /* rowOff [lcd->cy] */  )) ;
    }
}


//------------------------SEND STRING TO DISPLAY------------------------

void lcdPuts (struct lcdDataStruct *lcd, const char *string)
{
    while (*string)
        lcdPutchar (lcd, *string++) ;
}

//--------------------------------FAILURE MTHOD-------------------------


int failure (int fatal, const char *message, ...) {
    va_list argp ;
    char buffer [1024] ;
    
    if (!fatal) //  && wiringPiReturnCodes)
        return -1 ;
    
    va_start (argp, message) ;
    vsnprintf (buffer, 1023, message, argp) ;
    va_end (argp) ;
    
    fprintf (stderr, "%s", buffer) ;
    exit (EXIT_FAILURE) ;
    
    return 0 ;
}

//----------------------WAIT FOR ENTER METHOD---------------------------

void waitForEnter (void) {
    printf ("Press ENTER to play: ") ;
    (void)fgetc (stdin) ;
}


//---------------------------FLASH BLUE LED-----------------------------

void flashBlue(int times) {
    
    int i;
    for(i=0; i<times; i++){                                                      // loop to the desired amount
        delay(300);
        digitalWrite(gpio, BLUELED_PIN, 1) ;                                       // turn blue LED on
        delay(300);
        digitalWrite(gpio, BLUELED_PIN, 0) ;                                       // turn blue LED on
    }
}

//--------------------------GET BUTTON PRESSES--------------------------

int getButtonPress() {
    
    // declare variables
    int input = 0;
    int elapsed = 0;
    time_t start = 0;
    time_t end = 0;
    
    // detects button presses and performs calculations
    while(elapsed<3) {                                                            // constantly loops until the 3 second timout is reached
        if((BUTTON_PIN & 0xFFFFFFC0) == 0){
            if(buttonWrite(gpio, BUTTON_PIN) != 0){                                         // detects if the button has been pressed, checking if it does not equal 0
                if(buttonWrite(gpio, BUTTON_PIN) == 0){                              // detects when the button has been released
                    input++;                                                            // incremtents the button clicks variable
                    start = time(NULL);                                               // get the time of the button press
                    printf("BUTTON PRESSED------------------%d\n", input);            // output the number of presses for that digit in the secret, to see its registered
                }
            }else{                                                                    // if the button isn't being pressed
                end = time(NULL);                                                       // get the current time
                if(start != 0){                                                       // chekcs if there is a start time from the first press
                    elapsed = end-start;                                               // calculate the time between the last button press and the current time,
                }                                                                           // if the time elapsed is greater than 3, the method returns the number of presses
            }
        }
    }
    return input;                                                                 // return the number of presses
}

//----------------------------LED SHOW INPUT----------------------------

void flashLED(int input) {
    
    // flashes LED's to repeat the button press input
    int i=0;
    delay(200);
    digitalWrite(gpio, BLUELED_PIN, 1);                                           // turn blue LED on
    delay(400);
    digitalWrite(gpio, BLUELED_PIN, 0);                                           // turn blue LED off
    
    for(i=0; i<input; i++){                                                       // loops to the number of presses
        delay(300);
        digitalWrite(gpio, GREENLED_PIN, 1);                                        // turn green LED on
        delay(400);
        digitalWrite(gpio, GREENLED_PIN, 0);                                        // turn green LED off
    }
}

//------------------------WIN METHOD IF GUESS IS CORRECT------------------------

void win(struct lcdDataStruct *lcd, int guessNo) {
    
    
    guessNo = guessNo+1;
    char temp[10];
    char g[1];
    int i;
    
    digitalWrite(gpio, BLUELED_PIN, 1);                                              // turn blue LED on
    printf("YOU WON ON GUESS NUMBER %d\n", guessNo);                                 // print win message and number of guesses
    
    
    sprintf(g, "%d", guessNo);                                                    // change the int into a char to be printed on the LCD
    lcdClear(lcd);
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,"You Won on Guess");
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, g) ;
    delay(500);
    
    for(i=0; i<6; i++){                                                           // loop to flash green LED
        delay(400);
        digitalWrite(gpio, GREENLED_PIN, 1);                                        // turn green LED on
        delay(400);
        digitalWrite(gpio, GREENLED_PIN, 0);                                        // turn green LED off
    }
    
    delay(600);
    digitalWrite(gpio, BLUELED_PIN, 0);                     ;                        // turn blue LED off
    lcdClear(lcd) ;
    exit(0);
}

//----------------------COMPARE GUESS AND SECRET------------------------

void compare(struct lcdDataStruct *lcd, int guessNo){
    
    // declare variables
    int i;
    int j;
    int exact =0;
    int approx = 0;
    int temp[3];
    char e[1];
    char a[1];
    
    if(guessNo>1){
        lcdClear(lcd) ;
    }
    
    
    /* store the secret in a temporary array, this allows us to calculate the
     approx numbers by marking the visited pairs */
    for(i=0; i<3; i++){
        temp[i] = secret[i];
    }
    
    // exact number calculation
    for(i=0; i<3; i++) {                                                          // loop 3 times, as that is the length of the array
        if(temp[i] == guess[i]){                                                    // compare the elements in the same idexes
            exact++;                                                                    // increment exact variable if they match
            temp[i] = 10;                                                             // mark index as something else if they match to avoid clashing
            guess[i] = 15;                                                            // mark index as something else if they match to avoid clashing
        }
    }
    
    // approx number calculation
    for(i=0; i<3; i++) {                                                          // loop 3 times for each number in the secret
        for(j=0; j<3; j++) {                                                        // loop 3 times for each number in the temporary guess
            if(guess[i] == temp[j]){                                                  // compare the two numbers
                temp[j]=9;                                                          // mark index as something else if they match to avoid clashing
                guess[i] = 18;                                                     // mark index as something else if they match to avoid clashing
                approx++;                                                           // increment approx variable
            }
        }
    }
    
    // turn integers to chars for printing on LCD
    sprintf(e, "%d", exact);
    sprintf(a, "%d", approx);
    
    printf("ANS: %d %d\n", exact, approx);
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Exact");
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, "Approx");                            // print exact and approx numbers
    lcdPosition (lcd, 8, 0) ; lcdPuts (lcd, e) ;
    lcdPosition (lcd, 9, 1) ; lcdPuts (lcd, a) ;
    delay(2000);
    
    
    
    // if the exact number is 3, the game is won. Call the win method
    if(exact == 3){
        lcdClear(lcd) ;
        win(lcd, guessNo);
    }
    
    // if the user has had 3 guesses, game over!
    if(guessNo == 2 && exact != 3){
        delay(1000);
        lcdClear(lcd) ;
        printf("GAME OVER, YOU FAILED TO SOLVE THE CODE!\n");
        lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,"GAME OVER") ;
        lcdPosition (lcd, 0, 1) ; lcdPuts (lcd,"YOU FAILED") ;
        delay(3000);
        lcdClear(lcd) ;
        exit(0);
    }
}



//----------------------MAIN---METHOD-----------------------------------

int main (int argc, char *argv[]) {
    
    //-------------------initialise variables-------------------------------
    
    int i ;
    struct lcdDataStruct *lcd ;
    int bits, rows, cols ;
    unsigned char func ;
    struct tm *t ;
    int   fd ;
    char buf [32] ;
    
    //-------hard-coded: 16x2 display, using a 4-bit connection-------------
    
    bits = 4;
    cols = 16;
    rows = 2;
    
    printf ("Raspberry Pi LCD driver, for a %dx%d display (%d-bit wiring)\n",cols,rows,bits) ;
    
    if (geteuid () != 0)
        fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;
    
    //----------------------------------------------------------------------
    
    // constants for RPi2
    gpiobase = 0x3F200000 ;
    
    //---------------------MEMORY MAPPING MASTER STUFF----------------------
    
    if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
        return failure (FALSE, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;
    
    //---------------------------GPIO MEMORY MAP----------------------------
    
    gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
    if ((int32_t)gpio == -1)
        return failure (FALSE, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;
    
    //---------------------------LCD MEMORY MAP-----------------------------
    
    lcd = (struct lcdDataStruct *)malloc (sizeof (struct lcdDataStruct)) ;
    if (lcd == NULL)
        return -1 ;
    
    //------------------DEFINE VARIABLES FOR LCD(HARD WIRED)----------------
    
    lcd->rsPin   = RS_PIN ;
    lcd->strbPin = STRB_PIN ;
    lcd->bits    = 4 ;
    lcd->rows    = rows ;                                                         // # of rows on the display
    lcd->cols    = cols ;                                                         // # of cols on the display
    lcd->cx      = 0 ;                                                            // x-pos of cursor
    lcd->cy      = 0 ;                                                            // y-pos of curosr
    
    lcd->dataPins [0] = DATA0_PIN ;
    lcd->dataPins [1] = DATA1_PIN ;
    lcd->dataPins [2] = DATA2_PIN ;
    lcd->dataPins [3] = DATA3_PIN ;
    
    
    digitalWrite (gpio, lcd->rsPin,   0) ; pinMode (gpio, lcd->rsPin,   OUTPUT) ;
    digitalWrite (gpio, lcd->strbPin, 0) ; pinMode (gpio, lcd->strbPin, OUTPUT) ;
    
    for (i = 0 ; i < bits ; ++i)
    {
        digitalWrite (gpio, lcd->dataPins [i], 0) ;
        pinMode      (gpio, lcd->dataPins [i], OUTPUT) ;
    }
    delay (35) ; // mS
    
    //--------------------------MORE LCD STUFF------------------------------
    
    if (bits == 4)
    {
        func = LCD_FUNC | LCD_FUNC_DL ;                                                // Set 8-bit mode 3 times
        lcdPut4Command (lcd, func >> 4) ; delay (35) ;
        lcdPut4Command (lcd, func >> 4) ; delay (35) ;
        lcdPut4Command (lcd, func >> 4) ; delay (35) ;
        func = LCD_FUNC ;                                                            // 4th set: 4-bit mode
        lcdPut4Command (lcd, func >> 4) ; delay (35) ;
        lcd->bits = 4 ;
    }
    else
    {
        failure(TRUE, "setup: only 4-bit connection supported\n");
        func = LCD_FUNC | LCD_FUNC_DL ;
        lcdPutCommand  (lcd, func     ) ; delay (35) ;
        lcdPutCommand  (lcd, func     ) ; delay (35) ;
        lcdPutCommand  (lcd, func     ) ; delay (35) ;
    }
    
    if (lcd->rows > 1)
    {
        func |= LCD_FUNC_N ;
        lcdPutCommand (lcd, func) ; delay (35) ;
    }
    
    // Rest of the initialisation sequence
    lcdDisplay     (lcd, TRUE) ;
    lcdCursor      (lcd, FALSE) ;
    lcdCursorBlink (lcd, FALSE) ;
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,"            ");                         // writing to the LCD so the clear isn't clearing nothing - this was giving us issues before
    lcdClear       (lcd) ;
    
    lcdPutCommand (lcd, LCD_ENTRY   | LCD_ENTRY_ID) ;                             // set entry mode to increment address counter after write
    lcdPutCommand (lcd, LCD_CDSHIFT | LCD_CDSHIFT_RL) ;                           // set display shift to right-to-left
    
    
    //-------------------SETS LEDS TO OUTPUT--------------------------------
    
    pinMode(gpio, GREENLED_PIN, OUTPUT) ;
    pinMode(gpio, BLUELED_PIN, OUTPUT) ;
    
    //-------------------SETS BUTTON TO INPUT-------------------------------
    
    pinMode(gpio, BUTTON_PIN, INPUT) ;
    
    // --------------------WELCOME TEXT AND LEDS-------------------------------------------------
    
    lcdClear(lcd) ;
    fprintf(stderr,"Welcome to Mastermind !\n");
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd, "Welcome to") ;
    lcdPosition (lcd, 0, 1) ; lcdPuts (lcd, "Mastermind!") ;
    
    // turn the LED's on
    digitalWrite(gpio, GREENLED_PIN, 1);
    digitalWrite(gpio, BLUELED_PIN, 1);
    
    
    //------------------WHEN ENTER PRESSED TURN LEDS/LCD OFF----------------
    waitForEnter();
    lcdPosition (lcd, 0, 0) ; lcdPuts (lcd,"            ");
    lcdClear(lcd) ;
    digitalWrite(gpio, GREENLED_PIN, 0);
    digitalWrite(gpio, BLUELED_PIN, 0);
    
    //---------------------USING BUTTON TO TURN GET VALUE-------------------
    
    // obtain the secret from the user
    for(i=0; i<3; i++) {                                                          // loop 3 times for each number in the secret=
        secret[i] = getButtonPress();                                               // call the button method and store the number in the array
        if(secret[i]>3) {                                                           // set the number to 3 if the user enters anything greater
            secret[i] = 3;
        }
        flashLED(secret[i]);                                                        // flash the LED's in response to the input
    }
    
    flashBlue(2);                                                                 // flash the blue LED twice to signal the start of the guessing round
    
    printf("SECRET: %d %d %d\n", secret[0], secret[1], secret[2]);                // print the secret
    printf("===================================\n");
    
    // obtain the users guesses
    int j;
    for(j=0; j<3; j++) {                                                          // loops 3 times as that is the maximum amount of guesses
        for(i=0; i<3; i++) {                                                        // nested loop as each guess has 3 numbers
            guess[i] = getButtonPress();                                              // call the button method and store the number in the array
            if(guess[i]>3) {                                                          // set the number to 3 if the user enters anything greater
                guess[i] = 3;
            }
            flashBlue(1);                                                             // flash blue LED once to signal the end of the guess
        }
        printf("GUESS: %d %d %d\n", guess[0], guess[1], guess[2]);                  // print the users guess
        compare(lcd, j);                                                            // call the compare method to check the guess
        flashBlue(3);                                                                  // flash the blue LED 3 times to signal the next guess
    }
}
