# This module holds the shared application state to be accessed
# by both the web server and the BLE handler.

# --- SHARED STATE ---
server_state = {
    "status": "Initializing...",
    "storage_usage": 0,
    "settings": {"frequency": 30, "threshold": 80}
}

# --- BLE COMMUNICATION GLOBALS ---
data_queue = None
device_found_event = None
found_device = None
pending_config_command = None
