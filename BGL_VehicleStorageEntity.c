/*!
    BGL_VehicleInventorySlot
    ------------------------
    Represents a single inventory entry for a stored vehicle.  
    Each slot maps a prefab path → count (number of that item stored).

    Responsibilities
    - Provide serializable structure for vehicle inventory items.
    - Register fields with JsonApiStruct for save/load.

    Example JSON fragment:
    {
      "prefab": "{...}Prefabs/Items/Fuel/FuelCan.et",
      "count": 2
    }
*/
class BGL_VehicleInventorySlot : JsonApiStruct
{
	//! Prefab path of the item stored (unique identifier).
	string prefab;

	//! Quantity of this item stored.
	int count;

	/*!
	    Constructor
	    - Registers members for JSON serialization/deserialization.
	*/
	void BGL_VehicleInventorySlot()
	{
		RegV("prefab");
		RegV("count");
	}
}


/*!
    BGL_VehicleStorageEntity
    ------------------------
    Represents the full saved state of a single vehicle in the player’s garage.  
    Includes:
      - Prefab path of the vehicle
      - Vehicle’s inventory (serialized as array of slots)
      - Key lock metadata (id and code)

    Responsibilities
    - Provide serializable structure for vehicles inside player garage data.
    - Manage conversion between in-memory map<string,int> and JSON array slots.
    - Store key metadata for secure ownership transfer.

    Example JSON fragment:
    {
      "prefab": "{...}Prefabs/Vehicles/Car/MyCar.et",
      "inventory": [
        { "prefab": "{...}Prefabs/Items/Fuel/FuelCan.et", "count": 2 },
        { "prefab": "{...}Prefabs/Items/Toolkit/Toolkit.et", "count": 1 }
      ],
      "key_id": "1234-5678-90",
      "key_code": "ABCD"
    }
*/
class BGL_VehicleStorageEntity : JsonApiStruct
{
	//! Prefab path for the stored vehicle.
	string prefab;

	//! Inventory contents at time of storage (as slots).
	ref array<ref BGL_VehicleInventorySlot> inventory;

	//! Unique identifier for the vehicle’s key (used to match ownership).
	string key_id;

	//! Associated code for the vehicle’s key (PIN/lock code).
	string key_code;

	/*!
	    Constructor
	    - Registers members with JSON API.
	    - Ensures `inventory` is always initialized.
	*/
	void BGL_VehicleStorageEntity()
	{
		RegV("prefab");
		RegV("inventory");
		RegV("key_id");
		RegV("key_code");
		inventory = new array<ref BGL_VehicleInventorySlot>();
	}

	/*!
	    Populate inventory slots from a map<string,int>.

	    \param inv Map of prefab → count pairs (built from in-world inventory).
	*/
	void FromMap(map<string, int> inv)
	{
		for (int i; i < inv.Count(); i++)
		{
			BGL_VehicleInventorySlot slot = new BGL_VehicleInventorySlot();
			slot.prefab = inv.GetKey(i);
			slot.count = inv.GetElement(i);
			inventory.Insert(slot);
		}
	}

	/*!
	    Convert inventory slots back into a map<string,int>.

	    \return Map of prefab → count pairs (for respawning into a vehicle).
	*/
	ref map<string, int> ToMap() 
	{
		ref map<string, int> itemsMap = new map<string, int>();

		foreach (BGL_VehicleInventorySlot slot : inventory)
		{
			itemsMap.Insert(slot.prefab, slot.count);
		}

		return itemsMap;
	}
}


}
