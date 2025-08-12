import asyncio
import threading
import time

from app.database_handler import setup_filesystem
from app.web_routes import run_flask_app
from app.ble_handler import ble_communication_task

if __name__ == '__main__':
    # Set up the database and image directories first.
    setup_filesystem()

    # Start the Flask web server in a separate thread.
    threading.Thread(target=run_flask_app, daemon=True).start()

    # Allow the server to initialize.
    time.sleep(1)

    try:
        print("Starting BLE communication task...")
        # Start the main asynchronous BLE communication task.
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    except Exception as e:
        print(f"Fatal error in BLE task: {e}")
