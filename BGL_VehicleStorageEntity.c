class BGL_VehicleInventorySlot : JsonApiStruct
{
	string prefab;
	int count;
	
	void BGL_VehicleInventorySlot()
	{
		RegV("prefab");
		RegV("count");
	}
}


class BGL_VehicleStorageEntity : JsonApiStruct
{
	string prefab;
	ref array<ref BGL_VehicleInventorySlot> inventory;
	string key_id;
	string key_code;
	
	void BGL_VehicleStorageEntity()
	{
		RegV("prefab");
		RegV("inventory");
		RegV("key_id");
		RegV("key_code");
		inventory = new array<ref BGL_VehicleInventorySlot>();
	}
	
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
	
	ref map<string, int> ToMap() 
	{
		ref map<string, int> itemsMap = new map<string, int>();
		
		foreach(BGL_VehicleInventorySlot slot : inventory)
		{
			itemsMap.Insert(slot.prefab, slot.count);
		}
		
		return itemsMap;
	}
}