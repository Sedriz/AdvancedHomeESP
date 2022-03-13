#include "Color.h"
#include <vector>
#include <iostream>

class State
{
private:
    
public:
    char mode;
    int speed;
    int brightness;
    std::vector<Color> colorList;
    std::vector<Color>::iterator it;
};

