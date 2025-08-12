import os

# --- PATHS & FOLDERS ---
# Resolves the project root to the 'srv' directory.
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
IMGS_FOLDER_NAME = 'imgs'
IMGS_PATH = os.path.join(ROOT_DIR, IMGS_FOLDER_NAME)
DB_PATH = os.path.join(ROOT_DIR, 'captures.db')

# --- WEB SERVER ---
FLASK_PORT = 5550

# --- BLE DEVICE & PROTOCOL ---
DEVICE_NAMES = ["T-Camera-BLE-Batch", "T-Camera-BLE"]

# UUIDs for BLE Service and Characteristics
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_CONFIG = "a31a6820-8437-4f55-8898-5226c04a29a3"

# Protocol Command Bytes
CMD_NEXT_CHUNK = b'N'
CMD_ACKNOWLEDGE = b'A'
CMD_READY = b'R'
