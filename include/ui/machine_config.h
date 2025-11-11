#ifndef MACHINE_CONFIG_H
#define MACHINE_CONFIG_H

#include <Arduino.h>

#define MAX_MACHINES 4

enum ConnectionType {
    CONN_WIRED = 0,
    CONN_WIRELESS = 1
};

struct MachineConfig {
    char name[32];
    ConnectionType connection_type;
    char ssid[33];           // WiFi SSID (max 32 chars + null)
    char password[64];       // WiFi password (max 63 chars + null)
    char fluidnc_url[128];   // FluidNC URL (e.g., "192.168.1.100" or "fluidnc.local")
    uint16_t websocket_port; // WebSocket port (default 81)
    bool is_configured;      // Whether this slot has a valid machine
    
    // Jog control defaults
    float jog_xy_step;       // Default XY step size (mm)
    float jog_z_step;        // Default Z step size (mm)
    int jog_xy_feed;         // Default XY feed rate (mm/min)
    int jog_z_feed;          // Default Z feed rate (mm/min)
    int jog_max_xy_feed;     // Max XY feed for joystick (mm/min)
    int jog_max_z_feed;      // Max Z feed for joystick (mm/min)
    
    // Probe control defaults
    int probe_feed_rate;     // Default probe feed rate (mm/min)
    int probe_max_distance;  // Default max probe distance (mm)
    int probe_retract;       // Default retract distance (mm)
    float probe_thickness;   // Default probe thickness (mm, 1 decimal place)
    
    // Constructor with defaults
    MachineConfig() : connection_type(CONN_WIRELESS), websocket_port(81), is_configured(false),
                      jog_xy_step(10.0f), jog_z_step(1.0f), 
                      jog_xy_feed(3000), jog_z_feed(1000),
                      jog_max_xy_feed(3000), jog_max_z_feed(1000),
                      probe_feed_rate(100), probe_max_distance(10),
                      probe_retract(2), probe_thickness(0.0f) {
        name[0] = '\0';
        ssid[0] = '\0';
        password[0] = '\0';
        fluidnc_url[0] = '\0';
    }
};

class MachineConfigManager {
public:
    // Load all machines from Preferences (with caching)
    static void loadMachines(MachineConfig machines[MAX_MACHINES]);
    
    // Force reload from NVS (clears cache)
    static void reloadMachines();
    
    // Save all machines to Preferences
    static void saveMachines(const MachineConfig machines[MAX_MACHINES]);
    
    // Get a specific machine by index
    static bool getMachine(int index, MachineConfig &config);
    
    // Save a specific machine by index
    static bool saveMachine(int index, const MachineConfig &config);
    
    // Delete a machine (mark as unconfigured)
    static bool deleteMachine(int index);
    
    // Get the currently selected machine index
    static int getSelectedMachineIndex();
    
    // Set the currently selected machine index
    static void setSelectedMachineIndex(int index);
    
    // Get currently selected machine config
    static bool getSelectedMachine(MachineConfig &config);
    
    // Check if machines are configured
    static bool hasConfiguredMachines();
    
private:
    static MachineConfig cached_machines[MAX_MACHINES];
    static bool cache_valid;
};

#endif // MACHINE_CONFIG_H
