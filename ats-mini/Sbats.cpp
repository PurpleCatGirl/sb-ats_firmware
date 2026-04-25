#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Themes.h"
#include "Draw.h"

// newkirks recommended 200ms delay
#define SBATS_POLL_TIME    200 // Tuning status polling interval (msecs)

static uint32_t sbatsTime;
static uint32_t sbatsPress;
static uint16_t sbatsCurFreq;
static uint16_t sbatsMinFreq;
static uint16_t sbatsMaxFreq;
static uint16_t sbatsStep;
static bool     sbatsRev = false;

static void sbatsInit(uint16_t freq) {
  const Band *band = getCurrentBand();
  const Step *step = getCurrentStep();
  sbatsCurFreq      = freq;                // start from wherever we are on the dial
  sbatsMinFreq      = band->minimumFreq;   // current band lower edge
  sbatsMaxFreq      = band->maximumFreq;   // current band upper edge
  sbatsStep         = step->step;          // current step size
  sbatsTime         = millis();            // reset the timer
  sbatsPress        = 0;                   // clear any button-pressing
  if(band->bandName == "VHF") {            // override FM band limits for north america
    sbatsMinFreq    = 8710;                // no stations below 87.1 MHz
    sbatsMaxFreq    = 10790;               // no stations above 107.9 MHz
  } else if(band->bandName == "MW1") {     // override AM band limits for north america
    sbatsMinFreq    = 500;                 // no stations below 500 kHz
  }
  // clear parts of the screen  (screen size 320 x 170)
  spr.fillRect(90, 0, 240, 15, TH.bg);     // above band/mode, right of the sidebar
  spr.fillRect(90, 90, 240, 80, TH.bg);    // under frequency, right of sidebar
  spr.fillRect(0, 115, 320, 55, TH.bg);    // bottom area, full width
  spr.pushSprite(0, 0);
  delay(10);
}

static bool sbatsMainLoop() {
  // EXPERIMENTAL - white noise on SBATS_FUZZ_PIN
  digitalWrite(SBATS_FUZZ_PIN, random(2));
  // check for button activity
  if(sbatsPress) {                                   // encoder previously pressed
    if(digitalRead(ENCODER_PUSH_BUTTON)==HIGH) {     // encoder released
      uint32_t delta = millis() - sbatsPress;
      if(delta > 375) {                              // longish press
        sbatsRev = !sbatsRev;                        // reverse direction
      } else if(delta > 30) {                        // click
        currentCmd = CMD_NONE;                       // cancel sb-ats mode
      }
      sbatsPress=0;                                  // clear press
      delay(10);                                     // debounce
    }
  } else if(digitalRead(ENCODER_PUSH_BUTTON)==LOW) { // encoder pressed
    sbatsPress = millis();                           // set encoder pressed
    delay(10);                                       // debounce
  }
  // EXPERIMENTAL use encoder rotation to change scan direction (jank af but it works)
  if((digitalRead(ENCODER_PIN_A)==HIGH) && (digitalRead(ENCODER_PIN_B)==LOW)) {
    sbatsRev=false;                                  // scan forwards
    delay(25);                                       // debounce
  } else if((digitalRead(ENCODER_PIN_A)==LOW) && (digitalRead(ENCODER_PIN_B)==HIGH)) {
    sbatsRev=true;                                   // scan backwards
    delay(25);                                       // debounce
  }
  // wait for the timer to reach the specified delay
  if(millis() - sbatsTime < SBATS_POLL_TIME) return(true);
  if(currentCmd != CMD_SBATS) return(false);         // abort if sb-ats inactive
  rx.getStatus(0, 0);
  if(!rx.getTuneCompleteTriggered()) {               // only proceed if its done tuning
    sbatsTime = millis();                            // reset the timer
    return(true);
  }
  if(rx.getCurrentFrequency() == sbatsCurFreq) {     // pick new freq when ready
    if(sbatsRev) {
      sbatsCurFreq -= sbatsStep;
      if(sbatsCurFreq < sbatsMinFreq) sbatsCurFreq = sbatsMaxFreq;
    } else {
      sbatsCurFreq += sbatsStep;
      if(sbatsCurFreq > sbatsMaxFreq) sbatsCurFreq = sbatsMinFreq;
    }
  }
  rx.setFrequency(sbatsCurFreq);                     // set tuner to new frequency
  spr.fillRect(100, 35, 152, 52, TH.bg);             // update display for new frequency
  spr.fillRect(0, 115, 320, 55, TH.bg);              // and adjust the scale at the bottom
  drawFrequency(sbatsCurFreq, FREQ_OFFSET_X, FREQ_OFFSET_Y, FUNIT_OFFSET_X, FUNIT_OFFSET_Y, 100);
  drawScale(sbatsCurFreq);
  spr.pushSprite(0, 0);                              // push screen updates to screen
  sbatsTime = millis();                              // reset the timer
  return(true);
}

// Run sb-ats
void sbatsRun(uint16_t originalFreq) {
  // initialize SB-ATS
  digitalWrite(SBATS_FUZZ_PIN, LOW);
  sbatsInit(originalFreq);
  // run the main loop until it returns false
  while(sbatsMainLoop()) delay(5);
  digitalWrite(SBATS_FUZZ_PIN, LOW);
  // Restore current frequency
  rx.setFrequency(originalFreq);
  currentCmd=CMD_NONE;
  drawScreen();
}
