#include "display.hpp"
#include "app_config.h"    // for screen dimensions and pins

// Initialize the global display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool initDisplay() {
    // Initialize the display hardware
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        // If OLED init fails, return false (we'll continue without display)
        return false;
    }
    return true;
}
