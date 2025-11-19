# Script that creates a new database for the LpgPlanner app.

import pathlib
import platformdirs
import sqlite3


def main():
    """Create a database for LpgPlanner.

    The datbase will be stored in AppData/Roaming/lpg_planner/stations.db and
    it will contains the tables Stations and Distances.
    """
    # Make sure the target directory exists.
    db_dir = pathlib.Path(
        platformdirs.user_data_dir("lpg_planner", appauthor=False, roaming=True)
    )
    db_dir.mkdir(parents=True, exist_ok=True)

    # Open the database.
    db_connection = sqlite3.connect(db_dir.joinpath("stations.db"))
    cursor = db_connection.cursor()

    # Make sure the required tables are present in the database.
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS Stations(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            latitude REAL,
            longitude REAL,
            fuel_price REAL,
            price_date TEXT,
            address TEXT,
            UNIQUE(latitude, longitude)
        );
        """
    )
    cursor.execute(
        f"""
        CREATE TABLE IF NOT EXISTS Distances(
            from_id INTEGER,
            to_id INTEGER,
            distance REAL,
            UNIQUE(from_id, to_id),
            FOREIGN KEY(from_id) REFERENCES Stations(id),
            FOREIGN KEY(to_id) REFERENCES Stations(id)
        );
        """
    )

    # Apply the changes.
    db_connection.commit()


if __name__ == "__main__":
    main()
