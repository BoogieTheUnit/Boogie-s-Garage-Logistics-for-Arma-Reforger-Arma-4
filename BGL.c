// 2025-BoogieTheUnit - "Boogie's Garage and Logistics"

/*!
    BGL_Component
    -------------------------
    Server-side garage/storage controller attached to a world entity (e.g., garage sign).
    Handles:
      - Persisting per-player vehicle data to the profile directory
      - Storing a nearby vehicle (with safety checks: radius, empty seats, no weapons)
      - Spawning a stored vehicle at a configured PointInfo
      - Managing vehicle keys (ID/CODE) and item inventories
      - Enforcing per-player vehicle limits

    Files:
      - Saved under $profile:BLG/<playerUid>.json

    Key Concepts:
      - Storage payload: BGL_PlayerGarageStorageEntity { player_uid, vehicles[] }
      - Vehicle entry: BGL_VehicleStorageEntity { prefab, key_id, key_code, items... }
      - Nearby queries: sphere search around the owning entity (for store) or spawn point (for load)
      - Key validation: Only vehicles matching a player's key(s) may be stored

    Assumptions:
      - Executed on server when used via ScriptedUserAction or controller RPCs.
      - The owner entity is active and can receive events (set in OnPostInit).
      - PointInfo (m_SpawnPosition) is optional; owner transform is fallback.

    Notes:
      - UI hints/notifications are dispatched via SCR_PlayerController.BGL_Server_RequestNotify.
*/

class BGL_ComponentClass: ScriptComponentClass {}

class BGL_Component : ScriptComponent
{
	// -----------------------------
	// Tunables / Attributes
	// -----------------------------

	[Attribute()]
	ref PointInfo m_SpawnPosition;           //!< Optional spawn transform anchor for LoadCar()

	[Attribute(defvalue: "10")]
	int m_iMaxVehiclesPerPlayer;             //!< Per-player storage cap

	[Attribute(defvalue: "10.0")]
	float m_fRadius;                          //!< Search radius (meters) for storing vehicles

	// -----------------------------
	// Internals
	// -----------------------------

	protected const string DATA_DIR = "$profile:BLG/"; //!< Save directory
	protected IEntity m_FoundEntity;                   //!< Temp: query result
	protected array<string> m_keyIds;                  //!< Temp: keys found in player inventory
	protected string m_foundKey;                       //!< Temp: matched key id used for store

	// =========================================================
	// Lifecycle
	// =========================================================

	/*!
	    Component post-init.
	    - Ensures save directory exists
	    - Activates owner and hooks INIT
	*/
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		FileIO.MakeDirectory(DATA_DIR);

		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}

	// =========================================================
	// Public API
	// =========================================================

	/*!
	    Return all stored vehicles for a player.
	    \param playerUid Persistent player UID
	    \return Array of BGL_VehicleStorageEntity (may be empty)
	*/
	array<ref BGL_VehicleStorageEntity> GetStoredVeh(string playerUid)
	{
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
		return storage.vehicles;
	}

	/*!
	    Attempt to store the nearest eligible vehicle for the player.
	    Flow:
	      1) Read player's inventory keys → m_keyIds
	      2) Find nearest vehicle within m_fRadius that matches a key
	      3) Validate: no occupied seats, no weapons in vehicle storage, capacity not exceeded
	      4) Capture vehicle state (prefab, inventory → itemsMap, key id/code)
	      5) Save to player's storage file
	      6) Delete world vehicle and (optionally) remove matching key from player inventory

	    Notifications:
	      - "Your vehicle is not within the storage radius"
	      - "Weapons in compartment, please remove before storage."
	      - "Garage is full, can't store more vehicles."
	      - "Your vehicle has been stored."

	    \param playerUid Persistent player UID
	    \param playerId  Runtime player ID (for controller + notifications)
	    \return true if stored successfully, false otherwise
	*/
	bool Store(string playerUid, int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));

		// 1) Gather player-held car keys
		InventoryStorageManagerComponent playerInv = InventoryStorageManagerComponent.Cast(player.FindComponent(InventoryStorageManagerComponent));
		array<string> keyIds = {};
		array<IEntity> playerInvItems = {};
		playerInv.GetItems(playerInvItems);

		foreach (IEntity item : playerInvItems)
		{
			string prefabName = item.GetPrefabData().GetPrefabName();
			if (prefabName == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et")
			{
				Key_LockComponent keyLock = Key_LockComponent.Cast(item.FindComponent(Key_LockComponent));
				string keyId = keyLock.myID;
				if (keyId && keyId != "")
					keyIds.Insert(keyId);
			}
		}
		m_keyIds = keyIds;

		// 2) Find nearest keyed vehicle within radius
		IEntity veh = FindClosestVehicle(GetOwner(), m_fRadius);
		if (!veh)
		{
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle is not within the storage radius");
			return false;
		}

		// 3a) Ensure all seats/compartments are empty (no occupants)
		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(veh.FindComponent(SCR_BaseCompartmentManagerComponent));
		array<BaseCompartmentSlot> compartmentSlots = {};
		compartmentManager.GetCompartments(compartmentSlots);

		bool isOccuped = false;
		foreach (BaseCompartmentSlot slot : compartmentSlots)
		{
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

		// 3b) Ensure no weapons in vehicle storage
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(veh.FindComponent(InventoryStorageManagerComponent));
		array<IEntity> items = {};
		inventory.GetItems(items);

		ref map<string, int> itemsMap = new map<string, int>();
		bool haveWeapon = false;

		foreach (IEntity item : items)
		{
			if (haveWeapon) break;

			WeaponComponent wp = WeaponComponent.Cast(item.FindComponent(WeaponComponent));
			if (wp)
			{
				haveWeapon = true;
				break;
			}

			string itemName = item.GetPrefabData().GetPrefabName();
			if (itemsMap.Contains(itemName))
				itemsMap.Set(itemName, itemsMap.Get(itemName) + 1);
			else
				itemsMap.Insert(itemName, 1);
		}

		if (haveWeapon)
		{
			playerController.BGL_Server_RequestNotify("Garage", "Weapons in compartment, please remove before storage.");
			return false;
		}

		// 3c) Capacity check
		Key_LockComponent keyLock = Key_LockComponent.Cast(veh.FindComponent(Key_LockComponent));
		if (!CanStoreMoreVehicles(playerUid))
		{
			playerController.BGL_Server_RequestNotify("Garage", "Garage is full, can't store more vehicles.");
			return false;
		}

		// 4) Capture and append to storage
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

		// 5) Persist and clean up
		if (SavePlayerGarageData(storage))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(veh);
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle has been stored.");

			// Remove the specific matching key from player inventory (quality-of-life)
			foreach (IEntity item2 : playerInvItems)
			{
				string prefabName2 = item2.GetPrefabData().GetPrefabName();
				if (prefabName2 == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et")
				{
					Key_LockComponent keyLockItem = Key_LockComponent.Cast(item2.FindComponent(Key_LockComponent));
					string keyId2 = keyLockItem.myID;
					if (keyId2 && keyId2 != "" && keyId2 == m_foundKey)
						playerInv.TryDeleteItem(item2);
				}
			}
			return true;
		}

		return false;
	}

	/*!
	    Spawn a stored vehicle into the world and remove it from storage.

	    Flow:
	      - Validate indices and storage
	      - Compute spawn transform (PointInfo or owner transform)
	      - Ensure spawn area is clear
	      - Spawn prefab, clear any existing items, then rehydrate saved inventory
	      - Apply key ID/code to vehicle and a spawned CarKey item
	      - Save updated storage and notify

	    Notifications:
	      - "Can't spawn vehicle, area blocked."
	      - "Your vehicle has been removed from the garage."

	    \param playerUid    Persistent player UID
	    \param vehSelected  Index of the stored vehicle to spawn
	    \param playerId     Runtime player ID (for notifications)
	*/
	void LoadCar(string playerUid, int vehSelected, int playerId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
		if (!storage || storage.vehicles.IsEmpty())
			return;

		if (vehSelected < 0 || vehSelected >= storage.vehicles.Count())
			return;

		BGL_VehicleStorageEntity vehData = storage.vehicles[vehSelected];

		// Spawn transform
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

		// Area clear check
		IEntity haveAleardyveh = FindClosestVehicleForLoad(GetOwner().GetOrigin(), 1);
		if (haveAleardyveh)
		{
			playerController.BGL_Server_RequestNotify("Garage", "Can't spawn vehicle, area blocked.");
			return;
		}

		// Spawn prefab
		Resource vehResource = Resource.Load(vehData.prefab);
		IEntity veh = GetGame().SpawnEntityPrefab(vehResource, GetOwner().GetWorld(), params);

		if (veh)
		{
			// Remove from storage
			storage.vehicles.Remove(vehSelected);

			// Clear and rehydrate inventory
			InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(veh.FindComponent(InventoryStorageManagerComponent));
			array<IEntity> items = {};
			inventory.GetItems(items);
			foreach (IEntity item : items)
				inventory.TryDeleteItem(item);

			foreach (string resource, int count : vehData.ToMap())
			{
				for (int i; i < count; i++)
					inventory.TrySpawnPrefabToStorage(resource);
			}

			// Apply locks/keys
			Key_LockComponent keyLock = Key_LockComponent.Cast(veh.FindComponent(Key_LockComponent));
			keyLock.SetID(vehData.key_id, vehData.key_id);
			keyLock.SetCode(vehData.key_code);
			keyLock.SetLocked(false);

			// Give a key item and ensure it carries the same ID/code
			inventory.TrySpawnPrefabToStorage("{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et");
			items = {};
			inventory.GetItems(items);
			foreach (IEntity item2 : items)
			{
				string prefabName = item2.GetPrefabData().GetPrefabName();
				if (prefabName == "{CCFD8AA837B9611A}Prefabs/Items/CarKey/CarKey.et")
				{
					Key_LockComponent keyLockItem = Key_LockComponent.Cast(item2.FindComponent(Key_LockComponent));
					string keyId = keyLockItem.myID;
					if (!keyId)
					{
						keyLockItem.SetID(vehData.key_id, vehData.key_id);
						keyLockItem.SetCode(vehData.key_code);
					}
				}
			}

			// Persist and notify
			SavePlayerGarageData(storage);
			playerController.BGL_Server_RequestNotify("Garage", "Your vehicle has been removed from the garage.");
		}

		return;
	}

	/*!
	    Persist the player's storage to disk.
	    \return true on success
	*/
	bool SavePlayerGarageData(BGL_PlayerGarageStorageEntity storage)
	{
		if (!storage || !storage.player_uid)
			return false;

		string filePath = GetPlayerStoragePath(storage.player_uid);
		return storage.SaveToFile(filePath);
	}

	/*!
	    Snapshot minimal vehicle state (currently prefab path).
	    Extend here to capture fuel/health/paint/etc.

	    \param vehicle The world vehicle to capture
	    \return BGL_VehicleStorageEntity or null
	*/
	private BGL_VehicleStorageEntity CaptureVehicleState(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		BGL_VehicleStorageEntity data = new BGL_VehicleStorageEntity();
		data.prefab = vehicle.GetPrefabData().GetPrefabName();
		return data;
	}

	/*!
	    Build the absolute storage file path for a player.
	*/
	private string GetPlayerStoragePath(string playerUid)
	{
		return string.Format("%1%2.json", DATA_DIR, playerUid);
	}

	/*!
	    Load or initialize a player's storage payload.
	    - If file exists, attempts to load; returns null on load failure.
	    - If not, returns a new payload with player_uid set.

	    \return Storage entity (never null on first-time init)
	*/
	BGL_PlayerGarageStorageEntity LoadPlayerGarageData(string playerUid)
	{
		string filePath = GetPlayerStoragePath(playerUid);
		BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();

		if (FileIO.FileExists(filePath))
		{
			if (!storage.LoadFromFile(filePath))
				return null;
		}
		else
		{
			storage.player_uid = playerUid;
		}

		return storage;
	}

	/*!
	    Check if the player may store another vehicle (capacity gate).
	    \return true if count < m_iMaxVehiclesPerPlayer
	*/
	private bool CanStoreMoreVehicles(string playerUid)
	{
		BGL_PlayerGarageStorageEntity storage = LoadPlayerGarageData(playerUid);
		if (!storage)
			return true;

		return storage.vehicles.Count() < m_iMaxVehiclesPerPlayer;
	}

	// =========================================================
	// Spatial Queries (sphere scans)
	// =========================================================

	/*!
	    Candidate filter used by FindClosestVehicle.
	    Accepts entities that:
	      - Inherit BaseVehicle
	      - Have a prefab name
	      - Have a Key_LockComponent whose myID matches one of m_keyIds
	    On match: m_FoundEntity set and short-circuit (return false).

	    \return false to stop scan, true to continue
	*/
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
					return false; // stop
				}
			}
		}
		return true; // continue
	}

	/*!
	    Find the nearest keyed vehicle within radius of the owner entity.

	    \param entity       Anchor entity (usually the component owner)
	    \param searchRadius Radius in meters
	    \return The first matched vehicle entity or null
	*/
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

	/*!
	    Find any vehicle in a small radius of a position (used to ensure spawn area is clear).

	    \param position     World position to check
	    \param searchRadius Radius in meters
	    \return A found vehicle or null
	*/
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

	/*!
	    Candidate filter used by FindClosestVehicleForLoad.
	    Accepts any BaseVehicle with a prefab; first match wins.

	    \return false to stop scan, true to continue
	*/
	private bool AddEntityForLoad(IEntity entity)
	{
		if (entity && entity.IsInherited(BaseVehicle) && entity.GetPrefabData().GetPrefabName())
		{
			m_FoundEntity = entity;
			return false; // stop
		}
		return true; // continue
	}
}

// Register custom menu preset for the UI
modded enum ChimeraMenuPreset
{
	BGLMenu
}
