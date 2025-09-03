//2025-BoogieTheUnit - "Boogie's Garage and Logistics" Garage System based on Elan-Life Garage

class BGL_ComponentClass: ScriptComponentClass {}
class BGL_Component : ScriptComponent
{
	[Attribute()]
    ref PointInfo m_SpawnPosition;
	
	[Attribute(defvalue: "10")]
	int m_iMaxVehiclesPerPlayer;
	
	[Attribute(defvalue: "10.0")]
    float m_fRadius;

	protected const string DATA_DIR = "$profile:BLG/";
    protected IEntity m_FoundEntity;
	protected array<string> m_keyIds;
	protected string m_foundKey;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		FileIO.MakeDirectory(DATA_DIR);
		
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
		
	}
	
	array<ref BGL_VehicleStorageEntity> GetStoredVeh(string playerUid)
	{
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
		return storage.vehicles;
	}
	
	bool Store(string playerUid, int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		
		InventoryStorageManagerComponent playerInv = InventoryStorageManagerComponent.Cast(player.FindComponent(InventoryStorageManagerComponent));
		
		array<string> keyIds = {};
		array<IEntity> playerInvItems = {};
		playerInv.GetItems(playerInvItems);
		
		foreach(IEntity item: playerInvItems)
		{
			string prefabName = item.GetPrefabData().GetPrefabName();
			
			if (prefabName == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et") {
				Key_LockComponent keyLock = Key_LockComponent.Cast(item.FindComponent(Key_LockComponent));
				string keyId = keyLock.myID;
				if (keyId && keyId != "")
				{
					keyIds.Insert(keyId);
				}
			}
		}
		
		m_keyIds = keyIds;
		
		IEntity veh = FindClosestVehicle(GetOwner(), m_fRadius);
	
		if (!veh) {
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle is not within the storage radius");
			return false;
		}
		
		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(veh.FindComponent(SCR_BaseCompartmentManagerComponent));
		
		array<BaseCompartmentSlot> compartmentSlots = {};
		
		compartmentManager.GetCompartments(compartmentSlots);
		
		bool isOccuped = false;
			
		foreach(BaseCompartmentSlot slot: compartmentSlots)
		{
			if (isOccuped) break;
			if (slot.IsOccupied())
			{
				isOccuped = true;
				break;
			}
		}
		
		if (isOccuped)
		{
			return false;
		}
		
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(veh.FindComponent(InventoryStorageManagerComponent));
		
		array<IEntity> items = {};
		
		inventory.GetItems(items);
		
		ref map<string, int> itemsMap = new map<string, int>();
		
		bool haveWeapon = false;
		
		foreach(IEntity item: items)
		{
			WeaponComponent wp = WeaponComponent.Cast(item.FindComponent(WeaponComponent));
			
			if (haveWeapon) break;
			
			if (wp)
			{
				haveWeapon = true;
				break;
			}
			
			string itemName = item.GetPrefabData().GetPrefabName();
		
			if (itemsMap.Contains(itemName))
			{
				int count = itemsMap.Get(itemName);
				count = count + 1;
				itemsMap.Set(itemName, count);
			} else {
				itemsMap.Insert(itemName, 1);
			}
		}
		
		if (haveWeapon)
		{
			playerController.BGL_Server_RequestNotify("Garage", "Weapons in compartment, please remove before storage.");
			return false;
		}
		
		Key_LockComponent keyLock = Key_LockComponent.Cast(veh.FindComponent(Key_LockComponent));
		
		if (!CanStoreMoreVehicles(playerUid))
        {
           	playerController.BGL_Server_RequestNotify("Garage", "Garage is full, can't store more vehicles.");
            return false;
        }

		
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
        if (!storage)
            return false;
		
		BGL_VehicleStorageEntity vehicleData = CaptureVehicleState(veh);
        if (!vehicleData)
            return false;
		
		vehicleData.FromMap(itemsMap);
		vehicleData.key_id = keyLock.myID;
		vehicleData.key_code = keyLock.myCode;
	
		storage.vehicles.Insert(vehicleData);
		
		if (SavePlayerGarageData(storage))
        {
			
           	SCR_EntityHelper.DeleteEntityAndChildren(veh);
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle has been stored.");
			foreach(IEntity item: playerInvItems)
			{
				string prefabName = item.GetPrefabData().GetPrefabName();
				
				if (prefabName == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et") {
					Key_LockComponent keyLockItem = Key_LockComponent.Cast(item.FindComponent(Key_LockComponent));
					string keyId = keyLockItem.myID;
					if (keyId && keyId != "" && keyId == m_foundKey)
					{
						playerInv.TryDeleteItem(item);
					}
				}
			}
            return true;
        }
		
		return false;
	}
	
	void LoadCar(string playerUid, int vehSelected, int playerId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
		if (!storage || storage.vehicles.IsEmpty())
			return;

		if (vehSelected < 0 || vehSelected >= storage.vehicles.Count())
            return;

		BGL_VehicleStorageEntity vehData = storage.vehicles[vehSelected];
		EntitySpawnParams params = EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
		
		if (m_SpawnPosition)
        {
            m_SpawnPosition.Init(GetOwner());
            m_SpawnPosition.GetModelTransform(params.Transform);
            params.Transform[3] = GetOwner().CoordToParent(params.Transform[3]);
        }
        else
        {
            GetOwner().GetTransform(params.Transform);
        }
		
		IEntity haveAleardyveh = FindClosestVehicleForLoad(GetOwner().GetOrigin(), 1);
		if (haveAleardyveh)
		{
			playerController.BGL_Server_RequestNotify("Garage", "Can't spawn vehicle, area blocked.");
			return;
		}
		
		
		Resource vehResource = Resource.Load(vehData.prefab);
        IEntity veh = GetGame().SpawnEntityPrefab(vehResource, GetOwner().GetWorld(), params);
		
		if (veh)
		{
			storage.vehicles.Remove(vehSelected);
			
			InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(veh.FindComponent(InventoryStorageManagerComponent));
			
			array<IEntity> items = {};
		
			inventory.GetItems(items);
		
			foreach(IEntity item : items)
			{
				inventory.TryDeleteItem(item);
			}
			
			foreach(string resource, int count : vehData.ToMap())
			{
				for (int i; i < count; i++)
				{
					inventory.TrySpawnPrefabToStorage(resource);
				}
			}
			
			Key_LockComponent keyLock = Key_LockComponent.Cast(veh.FindComponent(Key_LockComponent));
			keyLock.SetID(vehData.key_id, vehData.key_id);
			keyLock.SetCode(vehData.key_code);
			keyLock.SetLocked(false);
		
			inventory.TrySpawnPrefabToStorage("{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et");
			
			items = {};
			
			inventory.GetItems(items);
			
			foreach(IEntity item: items)
			{
				string prefabName = item.GetPrefabData().GetPrefabName();
				
				if (prefabName == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et") {
					Key_LockComponent keyLockItem = Key_LockComponent.Cast(item.FindComponent(Key_LockComponent));
					string keyId = keyLockItem.myID;
					if (!keyId)
					{
						keyLockItem.SetID(vehData.key_id, vehData.key_id);
						keyLockItem.SetCode(vehData.key_code);
					}
				}
			}
			
			SavePlayerGarageData(storage);
			
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle has been removed from the garage.");
		}
		
		return;
	}
	
	bool SavePlayerGarageData(BGL_PlayerGarageStorageEntity storage)
    {
        if (!storage || !storage.player_uid)
            return false;
		

        string filePath = GetPlayerStoragePath(storage.player_uid);
        return storage.SaveToFile(filePath);
    }

	
	private BGL_VehicleStorageEntity CaptureVehicleState(IEntity vehicle)
    {
        if (!vehicle)
            return null;

        BGL_VehicleStorageEntity data = new BGL_VehicleStorageEntity();

        data.prefab = vehicle.GetPrefabData().GetPrefabName();

        return data;
    }
	
	private string GetPlayerStoragePath(string playerUid)
    {
        return string.Format("%1%2.json", DATA_DIR, playerUid);
    }
	
	BGL_PlayerGarageStorageEntity LoadPlayerGarageData(string playerUid)
    {
        string filePath = GetPlayerStoragePath(playerUid);
        BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();

        if (FileIO.FileExists(filePath))
        {
            if (!storage.LoadFromFile(filePath))
            {
                return null;
            }
        }
        else
        {
            storage.player_uid = playerUid;
        }

        return storage;
    }
	
 	private bool CanStoreMoreVehicles(string playerUid)
    {
        BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
        if (!storage)
            return true;

        return storage.vehicles.Count() < m_iMaxVehiclesPerPlayer;
    }
	
	private bool AddEntity(IEntity entity)
    {
        if (entity && entity.IsInherited(BaseVehicle) && entity.GetPrefabData().GetPrefabName())
        {
			Key_LockComponent keyLock = Key_LockComponent.Cast(entity.FindComponent(Key_LockComponent));
			if (keyLock) 
			{
				string keyId = keyLock.myID;
				if (m_keyIds.Contains(keyId))
				{
					m_FoundEntity = entity;
					m_foundKey = keyId;
            		return false;
				}
			}
        }
        return true;
    }
	
	private IEntity FindClosestVehicle(IEntity entity, float searchRadius)
    {
        if (!entity)
            return null;
		
        m_FoundEntity = null;
        vector entityPosition = entity.GetOrigin();

        GetGame().GetWorld().QueryEntitiesBySphere(
            entityPosition,
            searchRadius,
            AddEntity,
            null,
            EQueryEntitiesFlags.DYNAMIC
        );

        return m_FoundEntity;
    }
	
	private IEntity FindClosestVehicleForLoad(vector position, float searchRadius)
    {
        m_FoundEntity = null;

        GetGame().GetWorld().QueryEntitiesBySphere(
            position,
            searchRadius,
            AddEntityForLoad,
            null,
            EQueryEntitiesFlags.DYNAMIC
        );

        return m_FoundEntity;
    }
//Don't be a dingus script theif - Love Boogie	
	private bool AddEntityForLoad(IEntity entity)
    {
        if (entity && entity.IsInherited(BaseVehicle) && entity.GetPrefabData().GetPrefabName())
        {
			m_FoundEntity = entity;
          	return false;
        }
        return true;
    }
}

modded enum ChimeraMenuPreset {
    BGLMenu
}
