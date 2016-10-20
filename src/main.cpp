
// Access to the Arduino Libraries
#include <Arduino.h>
#include <stdarg.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

#define LEDS_PIN 6
#define LEDS_RETURN_PIN 2

String command = "";
boolean strComplet = false;
uint8_t delayt = 0;
uint16_t numLeds = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(numLeds, LEDS_PIN, NEO_GRB + NEO_KHZ800);

uint8_t minBrightness = 10;
uint8_t maxBrightness = 246;
uint8_t alpha = 4;

#define DEFAULT_DELAY 7
#define FLAG_DIRECTION_BIT 0
#define FLAG_FLICK_BIT 1
#define FLAG_DIRTY_BIT 2

const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

struct FlickLed
{
    union{
        uint32_t color;
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t w;
        };
    };

    uint8_t flags;

    //FlickLed() {color = strip.Color(120,0,0); flags = 0;}
};

#define FLAG_ALREADY_INITIALIZED 0
#define FLAG_RESET_REQUESTED 1


struct Configuration
{
    uint8_t flags;
    uint8_t delay;
    uint16_t numberOfLeds;
};

#define CONFIGURATION_ADDRESS 0
Configuration configuration;
#define LED_DATA_ADDRESS (CONFIGURATION_ADDRESS + sizeof(Configuration))
FlickLed* flickData;

uint8_t* ledData = NULL;

volatile bool ledBack = false;

void ledsDetected()
{
    ledBack = true;
}

//Bluethoot module: bc417

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
    // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
    // End of trinket special code
    Serial.begin(9600);
    command.reserve(20);
    randomSeed(analogRead(0));

    //Let's find number of leds installed
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(LEDS_RETURN_PIN), ledsDetected, RISING);

    strip.begin();
    strip.clear();

    while (!ledBack)
    {
        numLeds++;
        strip.updateLength(numLeds);
        for (uint16_t i = 0; i < numLeds; i++)
        {
            strip.setPixelColor(i, 0x10,0x10,0x10);
        }
        strip.show();
    }

    //Now he have the number of leds, set the final number to the strip.
    numLeds--;
    detachInterrupt(digitalPinToInterrupt(LEDS_RETURN_PIN));
    strip.updateLength(numLeds);
    strip.clear();
    strip.show();

    ledData = strip.getPixels();
    flickData = (FlickLed*) malloc(sizeof(FlickLed)*numLeds);
    memset(flickData, 0, sizeof(FlickLed)*numLeds);

    EEPROM.get(CONFIGURATION_ADDRESS, configuration);
    if (configuration.numberOfLeds == numLeds && (!(configuration.flags >> FLAG_RESET_REQUESTED & 1)) && (configuration.flags >> FLAG_ALREADY_INITIALIZED & 1))
    {
        //The leds have been already initialized, no reset is requested, no leds have  been added/removed, so reload leds from memmory
        for (uint16_t i = 0; i < numLeds; i++)
        {
            EEPROM.get(LED_DATA_ADDRESS + (sizeof(FlickLed)*i), flickData[i]);
            flickData[i].flags |= (1<<FLAG_DIRTY_BIT);
        }
    }
    else
    {
        //Reinitialize leds using random colors and place random leds doing "fire flickering"
        for (uint16_t i = 0; i < numLeds; i++)
        {
            flickData[i].color = strip.Color(150,0,0);
            flickData[i].flags = 7;
            uint8_t t = random(20);
            if (t>2)
            {
                flickData[i].flags &= ~(1 << FLAG_FLICK_BIT);
                flickData[i].color = strip.Color(random(150),random(100),random(120));
            }
        }

        //If we are in this else because a reset was rekested,
        //clear the flags.
        //The initialized flag is not set here, only when the presist (p) command is used.
        if (configuration.flags >> FLAG_RESET_REQUESTED & 1)
        {
            configuration.flags = 0;
            configuration.numberOfLeds = 0;
            configuration.delay = DEFAULT_DELAY;
            EEPROM.put(CONFIGURATION_ADDRESS, configuration);
        }
    }

    delayt = configuration.delay;
    Serial.println("OK");
}

void parseSetColor(String cmd)
{
    uint8_t index = cmd.substring(0,3).toInt();

    //Avoid commands with wrong indexes.
    if (index < numLeds)
    {
        uint8_t r = (uint8_t)(cmd.substring(3,6).toInt());
        uint8_t g = (uint8_t)(cmd.substring(6,9).toInt());
        uint8_t b = (uint8_t)(cmd.substring(9,cmd.length()).toInt());
        flickData[index].r = r;
        flickData[index].g = g;
        flickData[index].b = b;
        flickData[index].w = 0;
        flickData[index].flags |= 1 << FLAG_DIRTY_BIT;
    }
}

void parseSetFlick(String cmd)
{
    uint8_t index = cmd.substring(0,3).toInt();

    //Avoid commands with wrong indexes.
    if (index < numLeds)
    {
        uint8_t flick = (uint8_t)(cmd.substring(3,4).toInt());
        if (flick)
        {
            flickData[index].flags |= 1 << FLAG_FLICK_BIT;
        }
        else
        {
            flickData[index].flags &= ~(1 << FLAG_FLICK_BIT);
        }
    }
}

void parseDelay(String cmd)
{
    delayt = (uint8_t)(cmd.substring(0,3).toInt());
}

void serial_printf(char* fmt, ...)
{
    char buff[15];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, 15, fmt, args);
    va_end(args);
    Serial.print(buff);
}

/**
 * @brief parseCommand
 * Supported commands:
 * s -> sets led color:
 * snnnrrrgggbbb where nnn is the led number,  rrr is the  red color, ggg is the green color and bbb is the blue color
 * f -> sets led flick mode
 * fnnnA where nnn is the led number, A is the flick mode: 1 for on, 0 for off
 * d -> sets the delay between frames.
 * dnnn where nnn is the new delay (default is 7)
 * r -> request a reset (deletes the saved led information)
 * p -> persists the led color and flick information.
 * n -> query actual number of leds.
 * q -> send actual led information. Format:
 * nnnrrrgggbbbf nnn: Led number rrrgggbbb color in rgb. f: Flick. 1 on, 0 off
 * All led info will be separated by ":" between leds: <led1>:<led2>:<led3> ...
 * t -> query configured delay
 */
void parseCommand()
{
    command.trim();
    if (command.startsWith("s"))
    {
        Serial.println("OK");
        parseSetColor(command.substring(1,command.length()));
    } else if (command.startsWith("f"))
    {
        Serial.println("OK");
        parseSetFlick(command.substring(1,command.length()));
    }
    else if (command.startsWith("d"))
    {
        Serial.println("OK");
        parseDelay(command.substring(1,command.length()));
    }
    else if (command.startsWith("r"))
    {
        Serial.println("OK");
        configuration.flags |= (1<<FLAG_RESET_REQUESTED);
        EEPROM.put(CONFIGURATION_ADDRESS, configuration);
    }
    else if (command.startsWith("p"))
    {
        Serial.println("OK");
        for (uint16_t i = 0; i< numLeds; i++)
        {
            EEPROM.put(LED_DATA_ADDRESS + (sizeof(FlickLed)*i), flickData[i]);
        }
        configuration.flags |= (1<<FLAG_ALREADY_INITIALIZED);
        configuration.numberOfLeds = numLeds;
        configuration.delay = delayt;
        EEPROM.put(CONFIGURATION_ADDRESS, configuration);
    }
    else if (command.startsWith("n"))
    {
        Serial.println(numLeds, DEC);
    }
    else if (command.startsWith("t"))
    {
        Serial.println(delayt, DEC);
    }
    else if (command.startsWith("q"))
    {
        for (uint16_t i = 0; i < numLeds; i++)
        {
            uint8_t flick = 0;
            if (flickData[i].flags >> FLAG_FLICK_BIT & 1)
            {
                flick = 1;
            }
            serial_printf((char*)"%03d%03d%03d%03d%01d:", i, flickData[i].r,
                          flickData[i].g, flickData[i].b, flick);
        }
        Serial.println("");
    }
}

void loop() {
    uint8_t *p = ledData;
    uint8_t r,g,b,current;

    for (uint16_t i = 0; i < numLeds; i++)
    {
        r = flickData[i].r;
        g = flickData[i].g;
        b = flickData[i].b;
        current = flickData[i].w;

        if (flickData[i].flags >> FLAG_FLICK_BIT & 1)
        {
            int flip = random(32);
            uint8_t myAlpha = random(3,9);
            if (flip > 25)
            {
                flickData[i].flags ^= 1 << FLAG_DIRECTION_BIT;
            }
            if (flickData[i].flags >> FLAG_DIRECTION_BIT & 1)
            {
                current += myAlpha;
            }
            else
            {
                current -=myAlpha;
            }

            if (current < minBrightness)
            {
                current = minBrightness;
                flickData[i].flags |= 1 << FLAG_DIRECTION_BIT;
            }

            if (current > maxBrightness)
            {
                current = maxBrightness;
                flickData[i].flags &= ~(1 << FLAG_DIRECTION_BIT);
            }

            p[1] = (r * current) >> 8;
            p[0] = (g * current) >> 8;
            p[2] = (b * current) >> 8;
            //p[1] = (r * 255) >> 8;
            //p[0] = (g * 10) >> 8;
            //p[2] = (b * 10) >> 8;

            flickData[i].w = current;
            //        Serial.println("f"+String(i));
        }

        if (flickData[i].flags >> FLAG_DIRTY_BIT & 1)
        {
            p[1] = pgm_read_byte(&gamma8[r]);
            p[0] = pgm_read_byte(&gamma8[g]);
            p[2] = pgm_read_byte(&gamma8[b]);
            //        Serial.println(String(i));
            flickData[i].flags &= ~(1 << FLAG_DIRTY_BIT);
        }
        p +=3;
    }

    strip.show();
    delay(delayt);
    randomSeed(analogRead(0));

    if (strComplet)
    {
        parseCommand();
        strComplet = false;
        command = "";
    }

}

void serialEventRun()
{
    while (Serial.available())
    {
        char c = (char)Serial.read();
        command += c;

        if (c == '\n')
        {
            strComplet = true;
        }
    }
}


int main()
{
    // Initialize Arduino Librairies
    init();

    setup();

    for (;;) {
        loop();
        serialEventRun();
    }

    return 0;

}


