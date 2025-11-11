#include "ui/machine_config.h"
#include "config.h"
#include <Preferences.h>
#include <Arduino.h>

// Static cache members
MachineConfig MachineConfigManager::cached_machines[MAX_MACHINES];
bool MachineConfigManager::cache_valid = false;

void MachineConfigManager::loadMachines(MachineConfig machines[MAX_MACHINES]) {
    // If cache is valid, copy from cache instead of reading NVS
    if (cache_valid) {
        Serial.println("MachineConfigManager: Using cached machines");
        memcpy(machines, cached_machines, sizeof(MachineConfig) * MAX_MACHINES);
        return;
    }
    
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true); // Read-only
    
    Serial.println("MachineConfigManager: Loading machines from NVS...");
    
    for (int i = 0; i < MAX_MACHINES; i++) {
        String prefix = "m" + String(i) + "_";
        
        machines[i].is_configured = prefs.getBool((prefix + "cfg").c_str(), false);
        
        Serial.printf("  Slot %d: is_configured = %d\n", i, machines[i].is_configured);
        
        if (machines[i].is_configured) {
            prefs.getString((prefix + "name").c_str(), machines[i].name, sizeof(machines[i].name));
            machines[i].connection_type = (ConnectionType)prefs.getUChar((prefix + "type").c_str(), CONN_WIRELESS);
            prefs.getString((prefix + "ssid").c_str(), machines[i].ssid, sizeof(machines[i].ssid));
            prefs.getString((prefix + "pwd").c_str(), machines[i].password, sizeof(machines[i].password));
            prefs.getString((prefix + "url").c_str(), machines[i].fluidnc_url, sizeof(machines[i].fluidnc_url));
            machines[i].websocket_port = prefs.getUShort((prefix + "port").c_str(), 81);
            
            Serial.printf("    Name: %s, URL: %s:%d\n", machines[i].name, machines[i].fluidnc_url, machines[i].websocket_port);
            
            // Load jog settings (with defaults if not present)
            machines[i].jog_xy_step = prefs.getFloat((prefix + "jxy_st").c_str(), 10.0f);
            machines[i].jog_z_step = prefs.getFloat((prefix + "jz_st").c_str(), 1.0f);
            machines[i].jog_xy_feed = prefs.getInt((prefix + "jxy_fd").c_str(), 3000);
            machines[i].jog_z_feed = prefs.getInt((prefix + "jz_fd").c_str(), 1000);
            machines[i].jog_max_xy_feed = prefs.getInt((prefix + "jxy_mx").c_str(), 3000);
            machines[i].jog_max_z_feed = prefs.getInt((prefix + "jz_mx").c_str(), 1000);
            
            // Load probe settings (with defaults if not present)
            machines[i].probe_feed_rate = prefs.getInt((prefix + "p_feed").c_str(), 100);
            machines[i].probe_max_distance = prefs.getInt((prefix + "p_dist").c_str(), 10);
            machines[i].probe_retract = prefs.getInt((prefix + "p_ret").c_str(), 2);
            machines[i].probe_thickness = prefs.getFloat((prefix + "p_thick").c_str(), 0.0f);
        }
    }
    
    prefs.end();
    Serial.println("MachineConfigManager: Load complete");
    
    // Cache the loaded data
    memcpy(cached_machines, machines, sizeof(MachineConfig) * MAX_MACHINES);
    cache_valid = true;
}

void MachineConfigManager::reloadMachines() {
    Serial.println("MachineConfigManager: Clearing cache, forcing reload");
    cache_valid = false;
}

void MachineConfigManager::saveMachines(const MachineConfig machines[MAX_MACHINES]) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false); // Read-write
    
    for (int i = 0; i < MAX_MACHINES; i++) {
        String prefix = "m" + String(i) + "_";
        
        prefs.putBool((prefix + "cfg").c_str(), machines[i].is_configured);
        
        if (machines[i].is_configured) {
            prefs.putString((prefix + "name").c_str(), machines[i].name);
            prefs.putUChar((prefix + "type").c_str(), (uint8_t)machines[i].connection_type);
            prefs.putString((prefix + "ssid").c_str(), machines[i].ssid);
            prefs.putString((prefix + "pwd").c_str(), machines[i].password);
            prefs.putString((prefix + "url").c_str(), machines[i].fluidnc_url);
            prefs.putUShort((prefix + "port").c_str(), machines[i].websocket_port);
            
            // Save jog settings
            prefs.putFloat((prefix + "jxy_st").c_str(), machines[i].jog_xy_step);
            prefs.putFloat((prefix + "jz_st").c_str(), machines[i].jog_z_step);
            prefs.putInt((prefix + "jxy_fd").c_str(), machines[i].jog_xy_feed);
            prefs.putInt((prefix + "jz_fd").c_str(), machines[i].jog_z_feed);
            prefs.putInt((prefix + "jxy_mx").c_str(), machines[i].jog_max_xy_feed);
            prefs.putInt((prefix + "jz_mx").c_str(), machines[i].jog_max_z_feed);
            
            // Save probe settings
            prefs.putInt((prefix + "p_feed").c_str(), machines[i].probe_feed_rate);
            prefs.putInt((prefix + "p_dist").c_str(), machines[i].probe_max_distance);
            prefs.putInt((prefix + "p_ret").c_str(), machines[i].probe_retract);
            prefs.putFloat((prefix + "p_thick").c_str(), machines[i].probe_thickness);
        }
    }
    
    prefs.end();
    
    // Invalidate cache since machines were modified
    cache_valid = false;
}

bool MachineConfigManager::getMachine(int index, MachineConfig &config) {
    if (index < 0 || index >= MAX_MACHINES) return false;
    
    MachineConfig machines[MAX_MACHINES];
    loadMachines(machines);
    
    if (machines[index].is_configured) {
        config = machines[index];
        return true;
    }
    return false;
}

bool MachineConfigManager::saveMachine(int index, const MachineConfig &config) {
    if (index < 0 || index >= MAX_MACHINES) return false;
    
    MachineConfig machines[MAX_MACHINES];
    loadMachines(machines);
    
    machines[index] = config;
    machines[index].is_configured = true;
    
    saveMachines(machines);
    return true;
}

bool MachineConfigManager::deleteMachine(int index) {
    if (index < 0 || index >= MAX_MACHINES) return false;
    
    MachineConfig machines[MAX_MACHINES];
    loadMachines(machines);
    
    machines[index] = MachineConfig(); // Reset to defaults
    machines[index].is_configured = false;
    
    saveMachines(machines);
    return true;
}

int MachineConfigManager::getSelectedMachineIndex() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    int index = prefs.getInt("sel_machine", -1);
    prefs.end();
    return index;
}

void MachineConfigManager::setSelectedMachineIndex(int index) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putInt("sel_machine", index);
    prefs.end();
}

bool MachineConfigManager::getSelectedMachine(MachineConfig &config) {
    int index = getSelectedMachineIndex();
    if (index >= 0 && index < MAX_MACHINES) {
        return getMachine(index, config);
    }
    return false;
}

bool MachineConfigManager::hasConfiguredMachines() {
    MachineConfig machines[MAX_MACHINES];
    loadMachines(machines);
    
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].is_configured) {
            return true;
        }
    }
    return false;
}
