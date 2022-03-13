#include <FastLED.h>
#include <vector>
#include <iostream>

class State
{   
public:
    int mode;
    int speed;
    int brightness;
    std::vector<CRGB> colorVector;
    std::vector<int> additionalNumberVector;
};

