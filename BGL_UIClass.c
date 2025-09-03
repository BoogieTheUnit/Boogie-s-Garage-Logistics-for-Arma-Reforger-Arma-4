/*!
    BGL_UIClass
    -------------------------
    A player-facing garage menu for browsing, previewing, spawning, and deleting
    stored vehicles. The menu is backed by a serialized player storage string
    (m_playerStorageString) which is expanded into a BGL_PlayerGarageStorageEntity.

    Responsibilities
    - Wire up UI widgets and button/input handlers
    - Populate a list of stored vehicles with friendly names
    - Preview the currently selected vehicle prefab
    - Request spawn/delete actions via SCR_PlayerController RPCs
    - Keep the local storage string in sync after deletions

    Assumptions
    - The root layout contains widgets with ids:
      "ListBoxVehicle", "VehicleNameText", "VehicleDetailsText",
      "ItemPreview0", "SpawnButton", "DeleteButton", "CloseButton".
    - m_playerStorageString is set before the menu opens (via SetPlayerStorage).
    - ItemPreviewManager is available from ChimeraWorld.

    Notes
    - Vehicle delete is local-side mutation + client save request.
    - Spawning is requested through the player controller with current selection index.
*/
class BGL_UIClass : MenuBase
{
	// --- UI widgets ---
	private SCR_ListBoxComponent m_vehList;               //!< Vehicle list UI
	private SCR_InputButtonComponent m_spawnBtn;          //!< Spawn button
	private SCR_InputButtonComponent m_deleteBtn;         //!< Delete button
	private SCR_InputButtonComponent m_closeBtn;          //!< Close button
	private TextWidget m_vehName;                         //!< Selected vehicle name text
	private TextWidget m_vehDetails;                      //!< (Reserved) details text
	private ItemPreviewWidget m_vehPreview;               //!< 3D preview widget
	private ItemPreviewManagerEntity m_ItemPreviewManager;//!< Preview manager

	// --- Context/state ---
	private BGL_Component m_bgl;          //!< Reference to owning/related component (for future use)
	private string m_playerUid;           //!< Active player's UID (used for spawn request)
	private string m_playerStorageString; //!< Packed storage payload for vehicles
	private int m_vehSelected;            //!< Current index in the list (-1 when none)

	// =========================================================
	// Public API
	// =========================================================

	/*!
	    Entry point you can call after creating the menu if you want an explicit kick-off.
	    Currently just repopulates the list from storage.
	*/
	void Main()
	{
		Populate();
	}

	// =========================================================
	// Internal helpers
	// =========================================================

	/*!
	    Resolve and cache all widget references from the menu root and bind handlers.
	    Must be called after the layout is instantiated (OnMenuOpen).
	*/
	private void SetupWidget()
	{
		Widget root = GetRootWidget();

		Widget vehList = root.FindAnyWidget("ListBoxVehicle");
		m_vehList = SCR_ListBoxComponent.Cast(vehList.FindHandler(SCR_ListBoxComponent));
		m_vehName = TextWidget.Cast(root.FindAnyWidget("VehicleNameText"));
		m_vehDetails = TextWidget.Cast(root.FindAnyWidget("VehicleDetailsText"));
		m_vehPreview = ItemPreviewWidget.Cast(root.FindAnyWidget("ItemPreview0"));
		m_spawnBtn = SCR_InputButtonComponent.GetInputButtonComponent("SpawnButton", root);
		m_deleteBtn = SCR_InputButtonComponent.GetInputButtonComponent("DeleteButton", root);
		m_closeBtn = SCR_InputButtonComponent.GetInputButtonComponent("CloseButton", root);

		SetupButtonHandlers();
	}

	/*!
	    Wire up button and list events to their callbacks.
	    Idempotent: safe to call once per open.
	*/
	private void SetupButtonHandlers()
	{
		m_spawnBtn.m_OnActivated.Insert(OnBtnSpawnPressed);
		m_deleteBtn.m_OnActivated.Insert(OnBtnDeletePressed);
		m_closeBtn.m_OnActivated.Insert(Close);
		m_vehList.m_OnChanged.Insert(OnSelectItemChanged);
	}

	/*!
	    Populate the vehicle list from the player's storage.
	    - If no vehicles are stored, adds a single "No stored vehicles" row.
	*/
	private void Populate()
	{
		m_vehList.Clear();

		array<ref BGL_VehicleStorageEntity> vehs = GetVehicles();
		if (!vehs || vehs.IsEmpty())
		{
			m_vehList.AddItem("No stored vehicles");
			return;
		}

		foreach (BGL_VehicleStorageEntity veh : vehs)
		{
			string label = GetFriendlyVehicleName(veh.prefab);
			m_vehList.AddItem(label);
		}
	}

	/*!
	    Convert a prefab path to a short, friendly name.

	    Example:
	      "Some/Path/MyVehicle.et" -> "MyVehicle"

	    \param prefabPath Full prefab path (with extension)
	    \return Name between last slash and the final dot. Falls back to full path if parsing fails.
	*/
	private string GetFriendlyVehicleName(string prefabPath)
	{
		int lastSlash = prefabPath.LastIndexOf("/");
		int dot = prefabPath.LastIndexOf(".");
		if (lastSlash >= 0 && dot > lastSlash)
			return prefabPath.Substring(lastSlash + 1, dot - lastSlash - 1);
		return prefabPath;
	}

	/*!
	    Update the 3D preview widget with the selected vehicle.

	    \param veh Vehicle storage entry holding the prefab path to preview.
	    \pre m_ItemPreviewManager and m_vehPreview are valid.
	*/
	private void UpdateVehPreview(BGL_VehicleStorageEntity veh)
	{
		m_ItemPreviewManager.SetPreviewItemFromPrefab(m_vehPreview, veh.prefab);
	}

	/*!
	    Update the UI vehicle name text from the selected vehicle.

	    \param veh Vehicle storage entry.
	*/
	private void UpdateVehName(BGL_VehicleStorageEntity veh)
	{
		m_vehName.SetText(GetFriendlyVehicleName(veh.prefab));
	}

	/*!
	    Delete the currently selected vehicle from the player's storage.

	    Flow:
	    - Validate selection and storage
	    - Remove entry from storage array
	    - Ask the player controller to persist (BGL_Client_RequestSavePlayerStorage)
	    - Re-pack the local storage string for future reads

	    Side effects:
	    - Mutates both in-memory storage and m_playerStorageString
	    - Calls into player controller to save

	    No-op if there is no valid selection or storage is empty.
	*/
	private void DeleteCar()
	{
		BGL_PlayerGarageStorageEntity storage = GetPlayerStorage();
		if (!storage || storage.vehicles.IsEmpty())
			return;

		if (m_vehSelected < 0 || m_vehSelected >= storage.vehicles.Count())
			return;

		storage.vehicles.Remove(m_vehSelected);

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		playerController.BGL_Client_RequestSavePlayerStorage(storage);

		storage.Pack();
		m_playerStorageString = storage.AsString();
		return;
	}

	/*!
	    Deserialize the local storage string into a structured storage entity.

	    \return BGL_PlayerGarageStorageEntity expanded from m_playerStorageString.
	    \note Returns a new instance each call; callers should cache if needed.
	*/
	private BGL_PlayerGarageStorageEntity GetPlayerStorage()
	{
		BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();
		storage.ExpandFromRAW(m_playerStorageString);
		return storage;
	}

	/*!
	    Convenience accessor for the current list of stored vehicles.

	    \return Array of vehicle entries (may be null if storage failed to expand).
	*/
	private array<ref BGL_VehicleStorageEntity> GetVehicles()
	{
		BGL_PlayerGarageStorageEntity storage = GetPlayerStorage();
		return storage.vehicles;
	}

	// =========================================================
	// Menu lifecycle (MenuBase overrides)
	// =========================================================

	/*!
	    Called when the menu is initialized (layout created, before open).
	    - Binds global input actions (back/escape, spawn/delete)
	    - Caches ItemPreviewManager for preview widget usage
	*/
	override void OnMenuInit()
	{
		super.OnMenuInit();

		// Close shortcuts
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, Close);
		GetGame().GetInputManager().AddActionListener("MenuEscape", EActionTrigger.DOWN, Close);

		// Action shortcuts
		GetGame().GetInputManager().AddActionListener("MenuSelectHold", EActionTrigger.DOWN, OnBtnSpawnPressed);
		GetGame().GetInputManager().AddActionListener("MenuCalibrateMotionControl", EActionTrigger.DOWN, OnBtnDeletePressed);

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world)
		{
			m_ItemPreviewManager = world.GetItemPreviewManager();
		}
	}

	/*!
	    Called when the menu becomes visible.
	    - Resolves widgets
	    - Hooks up per-widget handlers
	    - (Does not auto-populate; call Main() or Populate() as needed)
	*/
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		SetupWidget();
	}

	/*!
	    Called when the menu is closed.
	    - Unbinds global input actions to avoid leaks/duplication.
	*/
	override void OnMenuClose()
	{
		super.OnMenuClose();

		GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, Close);
		GetGame().GetInputManager().RemoveActionListener("MenuEscape", EActionTrigger.DOWN, Close);
		GetGame().GetInputManager().RemoveActionListener("MenuSelectHold", EActionTrigger.DOWN, OnBtnSpawnPressed);
		GetGame().GetInputManager().RemoveActionListener("MenuCalibrateMotionControl", EActionTrigger.DOWN, OnBtnDeletePressed);
	}

	// =========================================================
	// UI events / callbacks
	// =========================================================

	/*!
	    Spawn button callback.
	    - Issues a client request to load the selected car (index = m_vehSelected)
	    - Closes the menu afterwards

	    Preconditions:
	    - m_playerUid must be set
	    - m_vehSelected should reference a valid vehicle (validated server-side)
	*/
	private void OnBtnSpawnPressed()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		playerController.BGL_Client_RequestLoadCar(m_playerUid, m_vehSelected);
		Close();
	}

	/*!
	    Delete button callback.
	    - Deletes the current selection from storage
	    - Repopulates the list to reflect changes
	*/
	private void OnBtnDeletePressed()
	{
		DeleteCar();
		Populate();
	}

	/*!
	    List selection changed callback.
	    - Updates m_vehSelected
	    - Updates preview widget and name label to match the new selection
	    - No-ops for invalid indices or empty vehicle lists
	*/
	private void OnSelectItemChanged()
	{
		m_vehSelected = m_vehList.GetSelectedItem();
		if (m_vehSelected < 0)
			return;

		array<ref BGL_VehicleStorageEntity> vehs = GetVehicles();
		if (!vehs || m_vehSelected >= vehs.Count())
		 return;

		BGL_VehicleStorageEntity currentVeh = vehs[m_vehSelected];
		UpdateVehPreview(currentVeh);
		UpdateVehName(currentVeh);
	}

	// =========================================================
	// Setters (must be called by the opener before use)
	// =========================================================

	/*!
	    Inject the owning/related BGL component (optional).
	*/
	void SetBGLComponent(BGL_Component bgl)
	{
		m_bgl = bgl;
	}

	/*!
	    Set the player UID used when requesting spawns.
	    \param playerUid The authenticated player's UID.
	*/
	void SetPlayerUid(string playerUid)
	{
		m_playerUid = playerUid;
	}

	/*!
	    Provide the packed storage string for this player.
	    Must be called before Populate()/preview usage so storage can expand.
	    \param storageString Serialized storage payload.
	*/
	void SetPlayerStorage(string storageString)
	{
		m_playerStorageString = storageString;
	}
}
