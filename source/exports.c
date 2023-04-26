#include "emscripten.h"

#include "display.h"
#include "snes9x.h"
#include "cpuexec.h"
#include "apu.h"
#include "apu_blargg.h"
#include "soundux.h"
#include "memmap.h"
#include "gfx.h"
#include "cheats.h"

#include <stdio.h>
#include <sys/time.h>

int joyPadInput = 0;
bool runGameFlag = false;
unsigned char *rgba8ScreenBuffer = NULL;
float *f32soundBuffer = NULL;
int16_t *i16soundBuffer = NULL;
unsigned int sramDestSize;
unsigned char *sramDest;

EMSCRIPTEN_KEEPALIVE
void setJoypadInput(int32_t input){
    joyPadInput = input;
} 

uint32_t S9xReadJoypad(int32_t port){
    if(port == 0)return joyPadInput;//1Pのみ対応
    return 0;
}

bool S9xReadMousePosition(int32_t which1, int32_t* x, int32_t* y, uint32_t* buttons)
{
   (void) which1;
   (void) x;
   (void) y;
   (void) buttons;
   return false;
}

bool S9xReadSuperScopePosition(int32_t* x, int32_t* y, uint32_t* buttons)
{
   (void) x;
   (void) y;
   (void) buttons;
   return true;
}

EMSCRIPTEN_KEEPALIVE
unsigned char *my_malloc(unsigned int length){
    return (unsigned char*)calloc(length, sizeof(unsigned char));
}

EMSCRIPTEN_KEEPALIVE
void my_free(unsigned char *ptr){
    free(ptr);
}

void S9xSoundCallback(void){
    //S9xClearSamples();
}

static void init_sfc_setting(unsigned int sampleRate)
{
   memset(&Settings, 0, sizeof(Settings));
   Settings.JoystickEnabled = false;
   Settings.SoundPlaybackRate = sampleRate;
#ifdef USE_BLARGG_APU
   Settings.SoundInputRate = sampleRate;
#endif
   Settings.CyclesPercentage = 100;

   Settings.DisableSoundEcho = false;
   Settings.InterpolatedSound = true;
   Settings.APUEnabled = true;

   Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
   Settings.FrameTimePAL = 20000;
   Settings.FrameTimeNTSC = 16667;
   Settings.DisableMasterVolume = false;
   Settings.Mouse = true;
   Settings.SuperScope = true;
   Settings.MultiPlayer5 = true;
   Settings.ControllerOption = SNES_JOYPAD;
#ifdef USE_BLARGG_APU
   Settings.SoundSync = false;
#endif
   Settings.ApplyCheats = true;
   Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;
}

EMSCRIPTEN_KEEPALIVE
void startWithRom(unsigned char *rom, unsigned int romLength, unsigned int sampleRate){
    memset(&Settings, 0, sizeof(Settings));
    //TO DO:
    //Settingsの設定が不足かも
    /*Settings.MouseMaster = true;
    Settings.SuperScopeMaster = true;
    Settings.JustifierMaster = true;
    Settings.MultiPlayer5Master = true;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.SoundPlaybackRate = 32040;
    CPU.Flags = 0;*/
    init_sfc_setting(sampleRate);
    S9xInitMemory();
    S9xInitAPU();
    //S9xInitSound(64, 0);//64ミリ秒のバッファ
    S9xInitSound();
    //S9xSetSamplesAvailableCallback(S9xSoundCallback);
    S9xInitDisplay();
    S9xInitGFX();
    //コントローラー
    //TO DO:コントローラー関係
    //ROMロード
    LoadROMFromBuffer(rom, romLength);
    //Settings.StopEmulation = false;
    //グラフィック設定
    //GFX.Pitch = 512;
    //リセット
    CommonS9xReset();
    runGameFlag = true;
}

EMSCRIPTEN_KEEPALIVE
void mainLoop(){
    if(!runGameFlag)return;
    S9xMainLoop();//1フレーム分実行される?
    S9xUpdateScreen();
}

EMSCRIPTEN_KEEPALIVE
uint8_t *getScreenBuffer(void){
    if(!rgba8ScreenBuffer)rgba8ScreenBuffer = my_malloc(512 * 448 * 4);
    if(!runGameFlag || !GFX.Screen)return rgba8ScreenBuffer;
    for(unsigned int i = 0;i < 512 * 448;i++){
        unsigned short col;
        memcpy(&col, GFX.Screen + 2 * i, 2);
        unsigned char r = ((col >> 11) & 0x1F) << 3;
        unsigned char g = ((col >> 5) & 0x3F) << 2;
        unsigned char b = ((col >> 0) & 0x1F) << 3;
        rgba8ScreenBuffer[i * 4 + 0] = r;
        rgba8ScreenBuffer[i * 4 + 1] = g;
        rgba8ScreenBuffer[i * 4 + 2] = b;
        rgba8ScreenBuffer[i * 4 + 3] = 0xFF;
    }
    return rgba8ScreenBuffer;
}

EMSCRIPTEN_KEEPALIVE
float *getSoundBuffer(unsigned int size){
    if(!f32soundBuffer)f32soundBuffer = (float*)calloc(size * 2, sizeof(float));
    if(!i16soundBuffer)i16soundBuffer = (int16_t*)calloc(size * 2, sizeof(int16_t));
    S9xMixSamples(i16soundBuffer, size);
    for(unsigned int i = 0;i < 2;i++){
        //for(unsigned int j = 0;j < size;j++)f32soundBuffer[i * size + j] = landing_buffer[j * 2 + i] / (float)0x8000;
        for(unsigned int j = 0;j < size;j++)f32soundBuffer[i * size + j] = i16soundBuffer[j * 2 + i];
    }
    return f32soundBuffer;
}

EMSCRIPTEN_KEEPALIVE
void saveSramRequest(void){
    if(!runGameFlag)return;
    sramDestSize = (1 << Memory.SRAMSize) * 1024;
    sramDest = (unsigned char*)calloc(sramDestSize, sizeof(unsigned char));
    memcpy(sramDest, Memory.SRAM, sramDestSize);
}

EMSCRIPTEN_KEEPALIVE
unsigned int getSaveSramSize(void){
    if(!runGameFlag)return 0;
    return sramDestSize;
}

EMSCRIPTEN_KEEPALIVE
unsigned char *getSaveSram(void){
    if(!runGameFlag)return NULL;
    return sramDest;
}

EMSCRIPTEN_KEEPALIVE
void loadSram(unsigned int sramSize, unsigned char *sram){
    if(!runGameFlag)return;
    memcpy(Memory.SRAM, sram, sramSize);
    CommonS9xReset();
}