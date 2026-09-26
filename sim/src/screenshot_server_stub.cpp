// The on-device screenshot web server isn't needed in the simulator - take
// screenshots of the window instead (F12 saves a PNG, see sim_main.cpp).

#include "network/screenshot_server.h"

void ScreenshotServer::init(DisplayDriver *) {}
void ScreenshotServer::handleClient() {}
bool ScreenshotServer::isConnected() { return false; }
String ScreenshotServer::getIPAddress() { return String(); }
