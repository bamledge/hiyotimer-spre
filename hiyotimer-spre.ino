//
//  name          : hiyotimer.ino
//  date/author   : 2025/08/09 Takeshi
//  update/author : 2024/08/10 Takeshi
//

//#if (SUBCORE != 1)
//#error "Core selection is wrong!!"
//#endif

#include "spreLGFXLib.hpp"
#include "spreTouchLib.hpp"
#include <MP.h>
#include <SDHCI.h>
#include <Audio.h>

unsigned long start, now, interval, milliseconds, seconds, minutes, lastTap;
bool measStarted, measEnded, initialized;
const unsigned long TAP_GUARD_TIME_MS = 200;

SDClass theSD;
AudioClass *theAudio;
const char *initSoundFileName = "/news.mp3";
const char *startSoundFileName = "/gong.mp3";
const char *endSoundFileName = "/jaja-n.mp3";

File mp3File;
bool playing = false;

unsigned long probestart, probeend;

void preparePlayer() {
  theAudio->setReadyMode();
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);
  err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);
  if (err != AUDIOLIB_ECODE_OK) {
    printf("Player0 initialize error\n");
    //exit(1);
  }
}

void startPlayMp3File(const char *path) {
  if (playing) {
    theAudio->stopPlayer(AudioClass::Player0);
    if (!mp3File) {
      mp3File.close();
    }
    playing = false;
  }
  mp3File = theSD.open(path);
  if (!mp3File) {
    printf("File open error: %s\n", path);
    return;
  }
  preparePlayer();

  int err = theAudio->writeFrames(AudioClass::Player0, mp3File);
  if (err != AUDIOLIB_ECODE_OK && err != AUDIOLIB_ECODE_FILEEND) {
      printf("Initial writeFrames error: %d\n", err);
      mp3File.close();
      playing = false;
      return;
  }
  printf("Opened %s\n", path);
  theAudio->startPlayer(AudioClass::Player0);

  playing = true;
}

void processPlayMp3() {
  if (!playing) return;

  probestart = millis();
  int err = theAudio->writeFrames(AudioClass::Player0, mp3File);
  if (err == AUDIOLIB_ECODE_FILEEND) {
    printf("Main player File End!\n");
    theAudio->stopPlayer(AudioClass::Player0);
    mp3File.close();
    playing = false;
  } else if (err) {
    printf("Main player error code: %d\n", err);
    theAudio->stopPlayer(AudioClass::Player0);
    mp3File.close();
    playing = false;
  }
  probeend = millis();Serial.printf("time=%d\n", probeend-probestart);
}

//===================================
// setup
//===================================
void setup(void) {
  Serial.begin(115200);
  setupLGFX(DEPTH_16BIT, ROT90);
  setupTouch(_w, _h, ROT90, false);
  int rtn = 0;
  uint32_t sz = _w * _h;

  spr.setColorDepth(DEPTH_8BIT);
  if (!spr.createSprite(_w, _h)) {
    Serial.printf("ERROR: malloc error (tmpspr:%dByte)\n", sz);
    rtn = -1;
  }

  spr.setFont(&fonts::FreeMonoBold24pt7b);

  // Initialize SD for audio
  while (!theSD.begin()) {
    Serial.println("Insert SD card.");
  }
  // start audio system
  theAudio = AudioClass::getInstance();
  theAudio->begin();
  theAudio->setVolume(-300);
  puts("initialization Audio Library");

  startPlayMp3File(initSoundFileName);

  measStarted = false;
  measEnded = false;
  initialized = true;
}

//===================================
// loop
//===================================
void loop(void) {
  spr.fillSprite(TFT_BLACK);
  spr.setCursor(0,0);spr.setTextDatum(top_center);
  spr.setTextColor(TFT_WHITE);spr.setTextSize(1);spr.printf("HIYO timer\n\n");
  //spr.setTextColor(TFT_WHITE);spr.println("    ひよタイマー");

  now = millis();
  interval = now - start;
  if (measStarted && !measEnded) {
    minutes = interval / 60000;
    seconds = (interval % 60000) / 1000;
    milliseconds = interval % 1000;
  }
  int tx, ty, tz;
  if (isTouch(&tx, &ty, &tz)) {
    if (now - lastTap >= TAP_GUARD_TIME_MS) {
      if (measStarted) {
        // Stop measurement
        measEnded = true;
        measStarted = false;
        startPlayMp3File(endSoundFileName);
      } else {
        if (initialized) {
        // Start measurement
          start = millis();
          measStarted = true;
          measEnded = false;
          initialized = false;
          startPlayMp3File(startSoundFileName);
        } else {
          // Reset measurement
          minutes = 0;
          seconds = 0;
          milliseconds = 0;
          measStarted = false;
          measEnded = false;
          initialized = true;
          startPlayMp3File(initSoundFileName);
        }
      }
    }
    lastTap = millis();
  }
  if (measStarted) {
    spr.setTextColor(TFT_YELLOW);
  } else if (measEnded) {
    spr.setTextColor(TFT_MAGENTA);
  } else {
    spr.setTextColor(TFT_SKYBLUE);
  }
  spr.setTextSize(1.2);
  spr.printf("%02d:%02d.%03d\n", minutes, seconds, milliseconds);
  if (measEnded) {
    spr.printf("~~~~~~~~~\n");
  }
  spr.pushSprite(&lcd, 0, 0);
  // 再生進行
  processPlayMp3();
}