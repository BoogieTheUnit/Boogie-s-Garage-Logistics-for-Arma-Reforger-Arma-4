/*!
    BGL_PlayerGarageStorageEntity
    -----------------------------
    Persistent JSON-serializable container for a player’s garage data.  
    This object is what gets saved to and loaded from disk for each player.

    Responsibilities
    - Hold a unique player UID (persistent identity).
    - Hold an array of stored vehicle entries (BGL_VehicleStorageEntity).
    - Register fields for automatic JSON serialization/deserialization via JsonApiStruct.

    Lifecycle
    - Constructed when loading an existing player file or when creating a new
      player’s storage entry.
    - Always initializes `vehicles` as a valid array (empty if no vehicles yet).

    File format (per player):
    {
      "player_uid": "<uid string>",
      "vehicles": [
         { ...vehicle storage entry... },
         { ... }
      ]
    }
*/
class BGL_PlayerGarageStorageEntity : JsonApiStruct
{
	//! Persistent player identifier (matches BackendApi identity id).
	string player_uid;

	//! Collection of this player’s stored vehicles.
	ref array<ref BGL_VehicleStorageEntity> vehicles;

	/*!
	    Constructor
	    - Registers members with JSON API (RegV).
	    - Ensures `vehicles` is allocated as an empty array.
	*/
	void BGL_PlayerGarageStorageEntity()
	{
		RegV("player_uid");
		RegV("vehicles");
		vehicles = new array<ref BGL_VehicleStorageEntity>();
	}
}
