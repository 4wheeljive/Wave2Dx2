#include <Arduino.h>
#include <FastLED.h>
#include "fl/xymap.h"

#define HEIGHT 32   
#define WIDTH 48
#define NUM_LEDS ( WIDTH * HEIGHT )

#define NUM_SEGMENTS 3
#define NUM_LEDS_PER_SEGMENT 512

CRGB leds[NUM_LEDS];
uint16_t ledNum = 0;

#define DATA_PIN_1 2
#define DATA_PIN_2 3
#define DATA_PIN_3 4

#define BRIGHTNESS 50

using namespace fl;

bool screenTest = false;
bool blendLayers = true;

extern uint16_t loc2indProgByColBottomUp[48][32];
extern uint16_t loc2indProg[1536];

uint16_t myXYFunctionLED(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
//uint16_t myXYFunctionScreen(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

XYMap myXYmap = XYMap::constructWithUserFunction(WIDTH, HEIGHT, myXYFunctionLED);
//XYMap myXYmap = XYMap::constructWithUserFunction(WIDTH, HEIGHT, myXYFunctionScreen);
XYMap xyRect(WIDTH, HEIGHT, false);   // For the wave simulation (always rectangular grid)


//#define IS_SERPENTINE false
//XYMap xyRect = XYMap::constructRectangularGrid(WIDTH, HEIGHT);
//uint16_t myXYFuncScreen(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
//XYMap myXYmap(WIDTH, HEIGHT, IS_SERPENTINE);
//XYMap myXYrect = XYMap::constructWithUserFunction(WIDTH, HEIGHT, myXYFunction);
//XYMap myXYmap = XYMap::constructRectangularGrid(WIDTH, HEIGHT);


//************************************************************************************************************

#include "fl/math_macros.h"  
#include "fl/time_alpha.h"  
#include "fl/ui.h"         
#include "fx/2d/blend.h"    
#include "fx/2d/wave.h"     

UIButton button("Trigger");                              
UICheckbox autoTrigger("Auto Trigger", true);   
UISlider triggerSpeed("Trigger Speed", .5f, 0.0f, 1.0f, 0.01f);
UICheckbox easeModeSqrt("Ease Mode Sqrt", false);    
UISlider blurAmount("Global Blur Amount", 0, 0, 172, 1);  
UISlider blurPasses("Global Blur Passes", 1, 1, 10, 1);   
UISlider superSample("SuperSampleExponent", 1.f, 0.f, 3.f, 1.f); 
  
// Lower wave layer controls:
UISlider speedLower("Wave Lower: Speed", 0.26f, 0.0f, 1.0f);      
UISlider dampeningLower("Wave Lower: Dampening", 9.0f, 0.0f, 20.0f, 0.1f); 
UICheckbox halfDuplexLower("Wave Lower: Half Duplex", true);         
UISlider blurAmountLower("Wave Lower: Blur Amount", 0, 0, 172, 1);  
UISlider blurPassesLower("Wave Lower: Blur Passes", 1, 1, 10, 1);    

// Upper wave layer controls:
UISlider speedUpper("Wave Upper: Speed", 0.12f, 0.0f, 1.0f);     
UISlider dampeningUpper("Wave Upper: Dampening", 8.9f, 0.0f, 20.0f, 0.1f); 
UICheckbox halfDuplexUpper("Wave Upper: Half Duplex", true);     
UISlider blurAmountUpper("Wave Upper: Blur Amount", 95, 0, 172, 1); 
UISlider blurPassesUpper("Wave Upper: Blur Passes", 1, 1, 10, 1);   

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   
    32,  0,   0,   70,  
    128, 20,  57,  255, 
    255, 255, 255, 255 
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,   
    8,   128, 64,  64,  
    16,  255, 222, 222, 
    64,  255, 255, 255, 
    255, 255, 255, 255 
};

//************************************************************************************************************

WaveFx::Args CreateArgsLower() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;
    out.half_duplex = true;
    out.auto_updates = true;  
    out.speed = 0.18f; 
    out.dampening = 9.0f;
    out.crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal);  
    return out;
}  

WaveFx::Args CreateArgsUpper() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X; 
    out.half_duplex = true;  
    out.auto_updates = true; 
    out.speed = 0.25f;      
    out.dampening = 3.0f;     
    out.crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal); 
    return out;
}

WaveFx waveFxLower(myXYmap, CreateArgsLower());
WaveFx waveFxUpper(myXYmap, CreateArgsUpper()); 

Blend2d fxBlend(myXYmap);

//************************************************************************************************************

SuperSample getSuperSample() {
    switch (int(superSample)) {
    case 0:
        return SuperSample::SUPER_SAMPLE_NONE;
    case 1:
        return SuperSample::SUPER_SAMPLE_2X;
    case 2:
        return SuperSample::SUPER_SAMPLE_4X;
    case 3:
        return SuperSample::SUPER_SAMPLE_8X;
    default:
        return SuperSample::SUPER_SAMPLE_NONE;
    }
}


//************************************************************************************************************

void triggerRipple() {

    float perc = .15f;
  
    uint8_t min_x = perc * WIDTH;    
    uint8_t max_x = (1 - perc) * WIDTH; 
    uint8_t min_y = perc * HEIGHT;     
    uint8_t max_y = (1 - perc) * HEIGHT; 
   
    int x = random(min_x, max_x);
    int y = random(min_y, max_y);
    
    waveFxLower.setf(x, y, 1); 
    waveFxUpper.setf(x, y, 1);

}

//************************************************************************************************************

struct ui_state {
    bool button = false;   
};

ui_state ui() {
    
    U8EasingFunction easeMode = easeModeSqrt
                                    ? U8EasingFunction::WAVE_U8_MODE_SQRT
                                    : U8EasingFunction::WAVE_U8_MODE_LINEAR;
    
    waveFxLower.setSpeed(speedLower);             
    waveFxLower.setDampening(dampeningLower);      
    waveFxLower.setHalfDuplex(halfDuplexLower);    
    waveFxLower.setSuperSample(getSuperSample());  
    waveFxLower.setEasingMode(easeMode);           
    
    waveFxUpper.setSpeed(speedUpper);             
    waveFxUpper.setDampening(dampeningUpper);     
    waveFxUpper.setHalfDuplex(halfDuplexUpper);   
    waveFxUpper.setSuperSample(getSuperSample()); 
    waveFxUpper.setEasingMode(easeMode);      
   
    fxBlend.setGlobalBlurAmount(blurAmount);      
    fxBlend.setGlobalBlurPasses(blurPasses);     

    Blend2dParams lower_params = {
        .blur_amount = blurAmountLower,            
        .blur_passes = blurPassesLower,         
    };

    Blend2dParams upper_params = {
        .blur_amount = blurAmountUpper,        
        .blur_passes = blurPassesUpper,           
    };

    fxBlend.setParams(waveFxLower, lower_params);
    fxBlend.setParams(waveFxUpper, upper_params);

     ui_state state{
        .button = button,   
    };
    return state;
}

//************************************************************************************************************

void processAutoTrigger(uint32_t now) {
  
    static uint32_t nextTrigger = 0;
    
    uint32_t trigger_delta = nextTrigger - now;
    
    if (trigger_delta > 10000) {
        trigger_delta = 0;
    }
    
    if (autoTrigger) {
        if (now >= nextTrigger) {
            triggerRipple();
  
            float speed = 1.0f - triggerSpeed.value();
            uint32_t min_rand = 500 * speed; 
            uint32_t max_rand = 3000 * speed; 

            uint32_t min = MIN(min_rand, max_rand);
            uint32_t max = MAX(min_rand, max_rand);
            
            if (min == max) {
                max += 1;
            }
            
            nextTrigger = now + random(min, max);
        }
    }
}

//************************************************************************************************************

void setup() {
   
    if (screenTest) {
        auto screenmap = xyRect.toScreenMap();
        screenmap.setDiameter(.2);
        FastLED.addLeds<NEOPIXEL, DATA_PIN_1>(leds, NUM_LEDS).setScreenMap(screenmap);
    }
    else {

        FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds, 0, NUM_LEDS_PER_SEGMENT)
            .setCorrection(TypicalLEDStrip);

        FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds, NUM_LEDS_PER_SEGMENT, NUM_LEDS_PER_SEGMENT)
            .setCorrection(TypicalLEDStrip);
        
        FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds, NUM_LEDS_PER_SEGMENT * 2, NUM_LEDS_PER_SEGMENT)
            .setCorrection(TypicalLEDStrip);
        
        FastLED.setBrightness(BRIGHTNESS);
    }

    FastLED.clear();
    FastLED.show();

    if (blendLayers){
        fxBlend.add(waveFxLower);
        fxBlend.add(waveFxUpper);
    }

}

//************************************************************************************************************

void loop() {

    uint32_t now = millis();

    ui_state state = ui();
    
   // EVERY_N_MILLISECONDS (3000) {
    //    triggerRipple(); 
    //}
    
    processAutoTrigger(now);
    
    Fx::DrawContext ctx(now, leds);
    
    if (blendLayers){
        fxBlend.draw(ctx);
    }
    else{
        waveFxLower.draw(ctx);
    }
    
    FastLED.show();

}

//************************************************************************************************************


uint16_t myXYFunctionLED(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    width = WIDTH;
    height = HEIGHT;
    if (x >= width || y >= height) return 0;
    ledNum = loc2indProgByColBottomUp[x][y];
    return ledNum;
}


uint16_t myXYFunctionScreen(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    width = WIDTH;
    height = HEIGHT;
    if (x >= width || y >= height) return 0;
    uint16_t j = (y * WIDTH) + x;
    ledNum = loc2indProg[j];
    return ledNum;
}
