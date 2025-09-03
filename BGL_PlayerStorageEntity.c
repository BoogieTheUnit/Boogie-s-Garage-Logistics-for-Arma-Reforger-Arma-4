class BGL_PlayerGarageStorageEntity : JsonApiStruct
{
	string player_uid;
	
	ref array<ref BGL_VehicleStorageEntity> vehicles;
	
	void BGL_PlayerGarageStorageEntity()
	{
		RegV("player_uid");
		RegV("vehicles");
		vehicles = new array<ref BGL_VehicleStorageEntity>();
	}
}