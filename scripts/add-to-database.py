# Script that can insert or update records in the database.
# The script expects to read stations data from a JSON file with the following
# format:
# ```JSON
# [
#   {
#     "address": { "street": "123 rue de la rue", "postal_code": "01234", "city": "Laville", "country": "This is optional"},
#     "latitude": 45.454545,
#     "longitude": 8.080808,
#     "price": 0.999,
#     "price_date": "31/12/2025"
#   },
#   ...
# ]
# ```
# Note that:
# - The "country" field in "address" is optional.
# - "address" can be replaced with "address-compact", assuming the latter
#     contains a compact, string representation of the address, e.g.,
#     "123 rue de la rue, 01234, Laville, Nowhere".
# - The date must be in dd/mm/yyyy.

import argparse
from datetime import datetime
import json
import pathlib
import platformdirs
import sqlite3
from typing import Dict, List, Optional, Tuple


def main():
    parser = argparse.ArgumentParser(
        description="Import stations into a SQLite database."
    )
    parser.add_argument("json", help="Path to JSON file.")
    args = parser.parse_args()

    # Try to load the database.
    db_connection = open_database()
    if db_connection is None:
        print("Cannot find the database. Have you run 'create-database.py'?")
        return

    # Insert data from the JSON into the database.
    data = load_json(args.json)
    n_added, n_updated, n_ignored = update_database(db_connection, data)
    db_connection.close()

    # Print some feedback.
    print(
        f"Database updated from {len(data)} entries ({n_added} added, {n_updated} updated, {n_ignored} skipped)."
    )


def load_json(path: str) -> List[Dict]:
    """Load station list from a JSON file."""
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def open_database() -> Optional[sqlite3.Connection]:
    """Open the stations.db SQLite database."""
    db_path = pathlib.Path(
        platformdirs.user_data_dir("lpg_planner", appauthor=False, roaming=True)
    ).joinpath("stations.db")
    if not db_path.exists() or not db_path.is_file():
        return None
    return sqlite3.connect(db_path)


def update_database(
    db_connection: sqlite3.Connection, data: List[Dict]
) -> Tuple[int, int, int]:
    """Insert or update GPL prices."""
    cursor = db_connection.cursor()

    # Some statistics.
    n_added = 0
    n_updated = 0
    n_ignored = 0

    for station in data:
        # Get price and date.
        price = station["price"]
        date = station["price_date"]

        # Skip entries missing either price or date (or for which the price is
        # too low to be realistic).
        if price == "" or float(price) < 0.1 or date == "":
            n_ignored += 1
            continue

        # Obtain the address - which can be either given broken down in parts,
        # or as a single string.
        if "address-compact" in station:
            address = station["address-compact"]
        else:
            address_dict = station["address"]
            address = f"{address_dict['street']}, {address_dict['postal_code']}, {address_dict['city']}"
            if "country" in address_dict:
                address += f", {address_dict['country']}"

        # Obtain GPS coordinates and date.
        latitude = round(float(station["latitude"]), 6)
        longitude = round(float(station["longitude"]), 6)
        date = datetime.strptime(date, "%d/%m/%Y").strftime("%Y-%m-%d")

        # Try to find this station in the database.
        cursor.execute(
            "SELECT id, latitude, longitude, price_date FROM Stations WHERE latitude=? AND longitude=?",
            (latitude, longitude),
        )
        row = cursor.fetchone()

        if row is None:
            # If the station does not exist, create a new record.
            cursor.execute(
                """
                INSERT INTO Stations(latitude, longitude, fuel_price, price_date, address)
                VALUES (?, ?, ?, ?, ?)
                """,
                (latitude, longitude, price, date, address),
            )
            n_added += 1
        else:
            # If the station is in the database, check if it can be udpated (we
            # update only if the price is newer!).
            station_id, old_latitude, old_longitude, old_date = row

            if date <= old_date:
                n_ignored += 1
                continue

            # Update price and date of the station.
            cursor.execute(
                """
                UPDATE Stations
                SET fuel_price = ?, price_date = ?
                WHERE id = ?
                """,
                (price, date, station_id),
            )
            n_updated += 1

    # Apply all changes.
    db_connection.commit()

    return n_added, n_updated, n_ignored


if __name__ == "__main__":
    main()
