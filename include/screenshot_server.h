#ifndef SCREENSHOT_SERVER_H
#define SCREENSHOT_SERVER_H

#include <Arduino.h>

// Forward declaration
class DisplayDriver;

class ScreenshotServer {
public:
    static void init(DisplayDriver* display_driver);
    static void handleClient();
    static bool isConnected();
    static String getIPAddress();
};

#endif // SCREENSHOT_SERVER_H
