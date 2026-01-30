// Compatibility stub for IDF 5.3+ where WiFiClientSecure was removed
// ArduinoWebsockets library requires this header even though we don't use secure connections
#ifndef WIFICLIENTSECURE_H
#define WIFICLIENTSECURE_H

#include <WiFiClient.h>

// Stub implementation - FluidNC doesn't support secure websockets
// ArduinoWebsockets library includes this header but we use non-secure connections
class WiFiClientSecure : public WiFiClient {
public:
    WiFiClientSecure() : WiFiClient() {}
    WiFiClientSecure(int socket) : WiFiClient(socket) {}
    
    // No-op SSL methods - not used in practice
    void setCACert(const char* /*rootCA*/) {}
    void setCertificate(const char* /*clientCert*/) {}
    void setPrivateKey(const char* /*clientKey*/) {}
    bool loadCACert(Stream& /*stream*/, size_t /*size*/) { return false; }
    bool loadCertificate(Stream& /*stream*/, size_t /*size*/) { return false; }
    bool loadPrivateKey(Stream& /*stream*/, size_t /*size*/) { return false; }
    void setInsecure() {}
    void setPreSharedKey(const char* /*pskIdent*/, const char* /*psKey*/) {}
};

#endif // WIFICLIENTSECURE_H
