import os
import sqlite3
from .config import DB_PATH, IMGS_PATH


def get_db_connection():
    """Establishes a connection to the SQLite database."""
    conn = sqlite3.connect(DB_PATH)
    return conn


def setup_filesystem():
    """Ensures the image directory and database table exist."""
    if not os.path.exists(IMGS_PATH):
        print(f"Image directory not found. Creating at: {IMGS_PATH}")
        os.makedirs(IMGS_PATH)

    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS captures (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                image_path TEXT NOT NULL,
                gps_lat REAL,
                gps_lon REAL
            )
        ''')
        conn.commit()
    except sqlite3.Error as e:
        print(f"Database setup error: {e}")
    finally:
        if conn:
            conn.close()
    print("Filesystem and database are ready.")


def db_insert_capture(timestamp, image_path):
    """Inserts a new capture record into the database."""
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO captures (timestamp, image_path) VALUES (?, ?)", (timestamp, image_path))
        conn.commit()
    except sqlite3.Error as e:
        print(f"Database insert error: {e}")
    finally:
        if conn:
            conn.close()
