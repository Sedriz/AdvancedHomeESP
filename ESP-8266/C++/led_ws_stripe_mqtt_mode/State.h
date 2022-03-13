#include <FastLED.h>
#include <vector>
#include <iostream>

class State
{
private:
    
public:
    char mode;
    int speed;
    int brightness;
    std::vector<CRGB> colorList;
    std::vector<CRGB>::iterator it;
};

