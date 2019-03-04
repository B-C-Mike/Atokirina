#include <SPI.h> // require Arduino
#include <nRF24L01.h>  // require https://github.com/nRF24/RF24
#include <RF24.h>
#include <printf.h>
#include <Adafruit_NeoPixel.h> // require https://github.com/adafruit/Adafruit_NeoPixel
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include "PL1167_nRF24.h" 
#include "MiLightRadio.h" // require https://github.com/henryk/openmili

#define CE_PIN 10
#define CSN_PIN 9
#define PIN 6
#define NUM_LEDS 10

RF24 radio(CE_PIN, CSN_PIN);
PL1167_nRF24 prf(radio);
MiLightRadio mlr(prf);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);

byte neopix_gamma[] = {
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

#define modes 4
uint8_t curr_mode = 2;
uint8_t curr_bucket = 0;
uint8_t settings[modes][20] = {
            // bank 0
        8, 0, 0, 0, // main (speed)
        0, 0, 0, 0, // 1 (R, Y)
        0, 0, 31, 0, // 2 (G, cW)
        0, 0, 0, 0, // 3 (B, WW)
        31, 0, 0, 0, // 4 (W, spare)
            // bank 1
        16, 0, 0, 0, // main (speed)
        31, 0x90, 0, 64, // 1 (power, color, white power, LED count)
        0, 0, 20, 64, // 2 (power, color, white power, LED count)
        8, 0, 0, 0, // 3 (power, color, white power, LED count)
        0, 0, 0, 0, // 4 (power, color, white power, LED count)
            // bank 2
        24, 0, 0, 0, // main (speed)
        31, 0x90, 0, 140, // 1 (power, color, white power, LED count)
        0, 0, 31, 30, // 2 (power, color, white power, LED count)
        0, 0, 0, 0, // 3 (power, color, white power, LED count)
        0, 0, 0, 0, // 4 (power, color, white power, LED count)
        
        };
//uint8_t key[2] = {0x42, 0x75}; // work only with one remote
//uint8_t key[2] = {0xF3, 0x68}; // work only with one remote
uint8_t key[2] = {0x00, 0x00}; // work with any remote
uint8_t animation_counter=0;

void setup()
{
  Serial.begin(115200);
  printf_begin();
  delay(100);
  Serial.println("# It's ready! ");
  mlr.begin();
  //strip.setBrightness(60);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  //[0]={0,0,3};

  
}


static int dupesPrinted = 0;
static uint8_t outgoingPacket[3];
static uint8_t outgoingPacketPos = 0;
static uint8_t nibble;
static enum {
  IDLE,
  HAVE_NIBBLE,
  COMPLETE,
} state;



void update_settings(void) { // decode radio remote commands
  printf("\n");
  uint8_t packet[7];
  size_t packet_length = sizeof(packet);
  mlr.read(packet, packet_length);
  
  for (int i = 0; i < packet_length; i++) {
    printf("%02X ", packet[i]);
  }
  if ( ((!key[0]) && (!key[1])) || ((key[0]==packet[1]) && (key[1]==packet[2])) ) {
    printf(" ok ");

    if (packet[5]==0x0B) {
      curr_mode++; 
      if (curr_mode>=modes) {curr_mode = 0;} //overflow
      printf("mode %02X ", curr_mode); 
    }
    if (packet[5]==0x0C) {
      curr_mode--; 
      if (curr_mode>=200) {curr_mode = modes-1;} //underflow
      printf("mode %02X ", curr_mode); 
    }

    if (packet[5] && packet[5]<=0x0A) { // ALL on/off buttons as bucket selector
      curr_bucket = (packet[5]-1)*2;
      printf("bucket %02X ", curr_bucket);
    }

    if (packet[5]==0x0E) { // brightness slider, encoded as top 5 bits, zero in the middle, negative scale, a bit offset too
      uint8_t input = -packet[4]-108; // this negative value is experimental, may need some tweaks. 
      input = input >>3;
      settings[curr_mode][curr_bucket] = input;
      printf("mode %02X, bucket %02X, value %02X ", curr_mode, curr_bucket, input);
    }
    if (packet[5]==0x0F) { // color wheel
      settings[curr_mode][curr_bucket+1] = packet[3];
      printf("mode %02X, bucket %02X, value %02X ", curr_mode, curr_bucket+1, packet[3]);
    }
    
  } else {
    printf(" wrong remote ");
  }
}

void write_settings(void){
  if (outgoingPacket[0]>= modes) { // mode have to be less than array size
    printf("\nERR mode have to be less than %02X \n", modes);
    return(0);
  }
  if (outgoingPacket[1]>= sizeof(settings[modes])) { // also buckets are limited
    printf("\nERR bucket have to be less than %02X \n", sizeof(settings[modes]));
    return(0);
  }
  
  curr_mode = outgoingPacket[0];
  curr_bucket = outgoingPacket[1];
  settings[curr_mode][curr_bucket] = outgoingPacket[2]; // note the brightness is in range 0-31 (hex 0x1f)
  printf("\n OK, saved! ");
}


void update_LED(void) {
  if(curr_mode==0) {

    uint8_t brightness, r, g, b, w;
    if (animation_counter<128) {
      brightness = animation_counter;
    } else {
      brightness = -animation_counter;
    }
    r =  (settings[0][4]*brightness) >>4 ; // 5b brightness * 7b counter / 4b = 12b/4b = 8b input value for pixels
    g =  (settings[0][8]*brightness) >>4 ; // controll via 4 ON buttons and power slider
    b =  (settings[0][12]*brightness) >>4 ;
    w =  (settings[0][16]*brightness) >>4 ;
    for (uint8_t i=0; i<8;i++) { strip.setPixelColor(i, strip.Color(r,g,b,w) ); } // ring LEDs
    brightness = 128-brightness;
    r =  (settings[0][6]*brightness) >>4 ; // 5b brightness * 7b counter / 4b = 12b/4b = 8b input value for pixels
    g =  (settings[0][10]*brightness) >>4 ; // controll via 4 OFF buttons and power slider
    b =  (settings[0][14]*brightness) >>4 ;
    w =  (settings[0][18]*brightness) >>4 ;
    strip.setPixelColor(8, strip.Color(r,g,b,w) ); // center/core LED(s)

    delay(settings[0][0]); // controll speed via main main ON and color wheel
	
  }

  if(curr_mode==1) {
    uint32_t c;
    uint8_t start1, end1, start2, end2, gap;
    strip.clear();
    
    start1 = animation_counter; 
    end1 = start1 + settings[1][7]; // 1 OFF ring
    gap = 127 -(settings[1][7]>>1) -(settings[1][11]>>1); // 2 OFF ring
    if (gap>127) { // overflow
      gap=0;
    }
    start2 = end1 + gap;
    end2 = start2 + settings[1][11]; // 2 OFF ring


    start1 = start1>>5; // convert 8b to 3b for LEDs index
    end1 = end1>>5; 
    if(start1>end1) { end1+=8; } // fix overflow problem
    start2 = start2>>5;
    end2 = end2>>5; 
    if(start2>end2) { end2+=8; }

    
    c=Wheel(settings[1][5],settings[1][6],settings[1][4]); 
    //       1 ON wheel     1 OFF slider   1 ON slider 
    for (uint8_t i=start1; i<end1;i++) { strip.setPixelColor(i%8, c ); } // ring LEDs

    c=Wheel(settings[1][9],settings[1][10],settings[1][8]); 
    //       2 ON wheel     2 OFF slider   2 ON slider 
    for (uint8_t i=start2; i<end2;i++) { strip.setPixelColor(i%8, c ); } // ring LEDs


    c=Wheel(settings[1][13],settings[1][14],settings[1][12]); 
    strip.setPixelColor(8, c ); // center/core LED(s)

    delay(settings[1][0]);
    
  }

  if(curr_mode==2) {
    uint32_t c;
    uint8_t start1, end1, start2, end2, gap, power;
    strip.clear();
    
    start1 = animation_counter; 
    end1 = start1 + settings[2][7]; // 1 OFF ring
    gap = 127 -(settings[2][7]>>1) -(settings[2][11]>>1); // 2 OFF ring
    if (gap>127) { // overflow
      gap=0;
    }
    start2 = end1 + gap;
    if (gap) {
      end2 = start2 + settings[2][11]; // 2 OFF ring
    } else { //to many LEDs, no gap 
      end2 = start1;
    }
    

    start1 = start1>>1; // convert 8b to 7b 
    end1 = end1>>1; 
    if(start1>end1) { end1+=128; } // fix overflow problem
    start2 = start2>>1;
    end2 = end2>>1; 
    if(start2>end2) { end2+=128; }


    // draw [1]
    if (end1>>4 == start1>>4) { //same/single pixel
      power = end1-start1;
      c=Wheel(settings[2][5],(settings[2][6]*power)>>4,(settings[2][4]*power)>>4); 
      strip.setPixelColor((start1>>4)%8, c );
    } else { // at least 2 pixels
      c=0; 
      if ((end2>>4)%8 == (start1>>4)%8) { // overlap with [2] tail
        power = (end2%16); // power for last pixel
        c=Wheel(settings[2][9],(settings[2][10]*power)>>4,(settings[2][8]*power)>>4); 
      }
      
      power = 16-(start1%16); // power for first pixel [1]
      c+=Wheel(settings[2][5],(settings[2][6]*power)>>4,(settings[2][4]*power)>>4); //please no overflow here
      strip.setPixelColor((start1>>4)%8, c );

      c=Wheel(settings[2][5],settings[2][6],settings[2][4]); //middle pixels [1]
      //       1 ON wheel     1 OFF slider   1 ON slider 
      for (uint8_t i=(start1>>4)+1; i<end1>>4;i++) { strip.setPixelColor(i%8, c ); } // ring LEDs

      power = (end1%16); // power for last pixel [1]
      c=Wheel(settings[2][5],(settings[2][6]*power)>>4,(settings[2][4]*power)>>4); 
      strip.setPixelColor((end1>>4)%8, c );
      
    }
    

    if (end2>>4 == start2>>4) { //same/single pixel
      uint8_t power = end2-start2;
      c=Wheel(settings[2][9],(settings[2][10]*power)>>4,(settings[2][8]*power)>>4); 
      strip.setPixelColor((start2>>4)%8, c );
    } else { // at least 2 pixels
      c=0; 
      if ((end1>>4)%8 == (start2>>4)%8) {
        power = (end1%16); // power for last pixel [1]
        c=Wheel(settings[2][5],(settings[2][6]*power)>>4,(settings[2][4]*power)>>4); 
      }

      power = 16-(start2%16); // power for first pixel [2]
      c+=Wheel(settings[2][9],(settings[2][10]*power)>>4,(settings[2][8]*power)>>4); //please no overflow here
      strip.setPixelColor((start2>>4)%8, c );

      c=Wheel(settings[2][9],settings[2][10],settings[2][8]); //middle pixels [2]
      //       2 ON wheel     2 OFF slider   2 ON slider 
      for (uint8_t i=(start2>>4)+1; i<end2>>4;i++) { strip.setPixelColor(i%8, c ); } // ring LEDs

      power = (end2%16); // power for last pixel [2]
      c=Wheel(settings[2][9],(settings[2][10]*power)>>4,(settings[2][8]*power)>>4); 
      if ((end2>>4)%8 != (start1>>4)%8) { //skip if overlap with first pixel [1]
        strip.setPixelColor((end2>>4)%8, c );
      }
      
    }
    
    c=Wheel(settings[2][13],settings[2][14],settings[2][12]); 
    strip.setPixelColor(8, c ); // center/core LED(s)

    delay(settings[2][0]);
    
  }




  strip.show();
  animation_counter++;
  //delay(10);
}


uint8_t timer =0;
void loop()
{
  if (mlr.available()) {
    update_settings();
  }

  timer++;
  if (timer==0) {
    mlr.begin(); // a little hack - reinitialize nRF24 because it like to die from time to time
  }

  int dupesReceived = mlr.dupesReceived();
  for (; dupesPrinted < dupesReceived; dupesPrinted++) { // just for visualization on serial terminal 
    printf(".");
  }

  while (Serial.available()) {
    char inChar = (char)Serial.read();
    uint8_t val = 0;
    bool have_val = true;

    if (inChar >= '0' && inChar <= '9') {
      val = inChar - '0';
    } else if (inChar >= 'a' && inChar <= 'f') {
      val = inChar - 'a' + 10;
    } else if (inChar >= 'A' && inChar <= 'F') {
      val = inChar - 'A' + 10;
    } else {
      have_val = false;
    }

    if (have_val) {
      switch (state) {
        case IDLE:
          nibble = val;
          state = HAVE_NIBBLE;
          break;
        case HAVE_NIBBLE:
          if (outgoingPacketPos < sizeof(outgoingPacket)) {
            outgoingPacket[outgoingPacketPos++] = (nibble << 4) | (val);
          } else {
            Serial.println("# Error: outgoing packet buffer full/packet too long");
          }
          if (outgoingPacketPos >= sizeof(outgoingPacket)) {
            state = COMPLETE;
          } else {
            state = IDLE;
          }
          break;
        case COMPLETE:
          Serial.println("# Error: outgoing packet complete. Press enter to send.");
          break;
      }
    } else {
      switch (inChar) {
        case ' ':
        case '\n':
        case '\r':
        case '.':
          if (state == COMPLETE) {
            printf("\n");
            for (int i = 0; i < sizeof(outgoingPacket); i++) {
              printf("-%02X-", outgoingPacket[i]);
            }
            write_settings();
            printf("\n");
          }
          if(inChar != ' ') {
            outgoingPacketPos = 0;
            state = IDLE;
          }
          break;
      }
    }
  }
  update_LED();
}



uint32_t Wheel(uint8_t WheelPos, uint8_t white, uint8_t brightness) { // surprisingly, Adafruit color wheel match with MiLight standard
  WheelPos = 255 - WheelPos; // this function aslo blends in white component
  if(WheelPos < 85) {
    return strip.Color(((255-WheelPos*3)*brightness)>>5, 0, ((WheelPos * 3)*brightness)>>5, (white)<<3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color( 0, ((WheelPos * 3)*brightness)>>5, ((255 - WheelPos * 3)*brightness)>>5, (white)<<3);
  }
  WheelPos -= 170;
  return strip.Color(((WheelPos * 3)*brightness)>>5, ((255 - WheelPos * 3)*brightness)>>5, 0, (white)<<3);
}
