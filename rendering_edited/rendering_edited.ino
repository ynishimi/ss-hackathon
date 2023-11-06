#include <OutputMixer.h>
#include <MemoryUtil.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/board/board.h>

#include <AudioOscillator.h>

#include <Adafruit_GFX.h>
#include "Adafruit_ILI9341.h"
#include <Fonts/FreeMonoBoldOblique24pt7b.h>

#define TFT_RST 8
#define TFT_DC  9
#define TFT_CS  10

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS ,TFT_DC ,TFT_RST);

Oscillator  *theOscillator;
OutputMixer *theMixer;
bool ErrEnd = false;
bool er;
uint16_t freq = 250;
float attack = 500;
float decay = 200;
float sustain = 20;
float release = 400;


const int pin0 = 4;
const int pin1 = 5;
const int pin2 = 6;
const int pin3 = 7;
int n0 = 0;
int n1 = 0;
int n2 = 0;
int n3 = 0;
bool button0 = false;
bool button1 = false;
bool button2 = false;
bool button3 = false;

bool refreshDisplay = false;
char buf[60];

unsigned long lastInputTime = 0;

static void menu()
{
  printf("loop() start\n");
}

static void show()
{
  printf("=====================================\n");
  printf("Freq: %f A: %f D: %f S: %f R: %f\n", freq, attack, sustain, decay, release);
  printf("=====================================\n");
}

static bool getFrame(AsPcmDataParam *pcm)
{
  const uint32_t sample = 480;
  /* Alloc MemHandle */
  if (pcm->mh.allocSeg(S0_REND_PCM_BUF_POOL, (sample * 2 * 2)) != ERR_OK) {
    return false;
  }
  theOscillator->exec((q15_t*)pcm->mh.getPa(), sample);

  /* Set PCM parameters */
  pcm->identifier = 0;
  pcm->callback = 0;
  pcm->bit_length = 16;
  pcm->size = sample * 2 * 2;
  pcm->sample = sample;
  pcm->is_end = false;
  pcm->is_valid = true;

  return true;
}

static bool active()
{
  for (int i = 0; i < 5; i++) {
    /* get PCM */
    AsPcmDataParam pcm_param;
    if (!getFrame(&pcm_param)) {
      break;
    }

    /* Send PCM */
    int err = theMixer->sendData(OutputMixer0,
                                 outmixer_send_callback,
                                 pcm_param);

    if (err != OUTPUTMIXER_ECODE_OK) {
      printf("OutputMixer send error: %d\n", err);
      return false;
    }
  }
  return true;
}

/**
   @brief Mixer done callback procedure

   @param [in] requester_dtq    MsgQueId type
   @param [in] reply_of         MsgType type
   @param [in,out] done_param   AsOutputMixDoneParam type pointer
*/
static void outputmixer_done_callback(MsgQueId requester_dtq,
                                      MsgType reply_of,
                                      AsOutputMixDoneParam *done_param)
{
  (void)done_param;
  printf(">> %x done from %x\n", reply_of, requester_dtq);
  return;
}

/**
   @brief Mixer data send callback procedure

   @param [in] identifier   Device identifier
   @param [in] is_end       For normal request give false, for stop request give true
*/
static void outmixer_send_callback(int32_t identifier, bool is_end)
{
  (void)identifier;
  (void)is_end;
  return;
}


static void attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    er = true;
  }
}

//一番上のボタンが押された時
void button0Pressed() {
  // attack=(float)analogRead(A0);
  // Serial.println(attack);
  theOscillator->set(0, attack, decay, sustain, release);
  theOscillator->set(1, attack, decay, sustain, release);
  theOscillator->set(0, freq);
  theOscillator->set(1, freq * 3);

  refreshDisplay = true;

  //演奏開始時刻
  // lastInputTime = millis();

  //再生開始なら、今後はボタンを離すことを監視
  attachInterrupt(digitalPinToInterrupt(pin0), button0Released, RISING);

};

void button0Released() {
  theOscillator->set(0, 0);  
  theOscillator->set(1, 0);
  //再生終了なら、今後はボタンを押すことを監視
  attachInterrupt(digitalPinToInterrupt(pin0), button0Pressed, FALLING);
  
}
void button1Pressed() {
  attack += 100;
};
void button2Pressed() {
  attack -= 100;
};
void button3Pressed() {
};

void setup()
{
  Serial.begin(115200);
  printf("setup() start\n");

  //液晶をスタート
  tft.begin();
  tft.setRotation(3);
  pinMode(pin0, INPUT_PULLUP);
  pinMode(pin1, INPUT_PULLUP);
  pinMode(pin2, INPUT_PULLUP);
  pinMode(pin3, INPUT_PULLUP);

  //ボタンの割り込みを監視
  attachInterrupt(digitalPinToInterrupt(pin0), button0Pressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(pin1), button1Pressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(pin2), button2Pressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(pin3), button3Pressed, FALLING);
  
  /* Initialize memory pools and message libs */
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_PLAYER);

  /* Create Objects */
  theOscillator = new Oscillator();

//  if(!theOscillator->begin(SinWave, 2)){
//  if(!theOscillator->begin(RectWave, 2)){
  if(!theOscillator->begin(SawWave, 2)){
      puts("begin error!");
      exit(1);
  }

  /* Start audio system */
  theMixer  = OutputMixer::getInstance();
  theMixer->activateBaseband();

  /* Create Objects */
  theMixer->create(attention_cb);

  /* Set rendering clock */
  theMixer->setRenderingClkMode(OUTPUTMIXER_RNDCLK_NORMAL);

  /* Activate Mixer Object.
     Set output device to speaker with 2nd argument.
     If you want to change the output device to I2S,
     specify "I2SOutputDevice" as an argument.
  */
  theMixer->activate(OutputMixer0, HPOutputDevice, outputmixer_done_callback);

  usleep(100 * 1000);

  /* Set main volume */
  theMixer->setVolume(-160, 0, 0);
  // theMixer->setVolume(0, 0, 0);

  /* Unmute */
  board_external_amp_mute_control(false);

  printf("setup() complete\n");
  menu();


  er = theOscillator->set(0, 0);
  er = theOscillator->set(1, 0);

  /* attack = 1000,decay = 700, sustain = 30, release = 300 */
  er = theOscillator->set(0, attack, decay, sustain, release);
  er = theOscillator->set(1, attack, decay, sustain, release);

  er = theOscillator->lfo(0, 4, 2);
  er = theOscillator->lfo(1, 4, 2);

  //表示... メモリ足りない？
  tft.fillScreen(0);
  tft.setTextSize(4);
  // tft.setFont( &FreeMonoBoldOblique24pt7b );
  tft.println("Sound\nto");
  tft.setTextColor(ILI9341_RED);
  tft.println("Spice");
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Synthesizer:)");
  // tft.println("Play");
  // tft.println();
  // tft.println("^");
  // tft.println("|");
  // tft.println("|");
  // tft.println("v");


  if(!er){
    puts("init error!");
    exit(1);
  }

}

#define C_NOTE  262
#define D_NOTE  294
#define E_NOTE  330
#define F_NOTE  349
#define G_NOTE  392

void loop()
{
  // attack = analogReadMap(A0, 0, 5000);
  // decay = analogReadMap(A1, 0, 5000);
  // sustain = analogReadMap(A2, 0, 100);
  // release = analogReadMap(A3, 0, 5000);

  Serial.println(attack);
  Serial.println(decay);
  Serial.println(sustain);
  Serial.println(release);
  Serial.println("------");

  freq = random(200, 500);

  // freq = analogRead(A0);
  // Serial.println(freq);

  // if (millis() - lastInputTime >= 3000) {
  //   // 3秒後に停止
  //   theOscillator->set(0, 0);  
  //   theOscillator->set(1, 0);
  // }

  if(!er){
    puts("set error!");
    goto stop_rendering;
  }
  active();

  usleep(1 * 1000);

  return;

stop_rendering:
  printf("Exit rendering\n");
  exit(1);
}