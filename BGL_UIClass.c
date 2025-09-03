class BGL_UIClass : MenuBase
{
	
	private SCR_ListBoxComponent m_vehList;
	private SCR_InputButtonComponent m_spawnBtn;
	private SCR_InputButtonComponent m_deleteBtn;
	private SCR_InputButtonComponent m_closeBtn;
	private TextWidget m_vehName;
	private TextWidget m_vehDetails;
	private ItemPreviewWidget m_vehPreview;
	private ItemPreviewManagerEntity m_ItemPreviewManager;
	
	private BGL_Component m_bgl;
	private string m_playerUid;
	
	private string m_playerStorageString;
	
	private int m_vehSelected;
	
	// Public Methods
	void Main()
	{
		Populate();
	}
	
	// Methods
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
	
	private void SetupButtonHandlers()
	{
		m_spawnBtn.m_OnActivated.Insert(OnBtnSpawnPressed);
		m_deleteBtn.m_OnActivated.Insert(OnBtnDeletePressed);
		m_closeBtn.m_OnActivated.Insert(Close);
		m_vehList.m_OnChanged.Insert(OnSelectItemChanged);
	}
	
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
	
	private string GetFriendlyVehicleName(string prefabPath)
    {

        int lastSlash = prefabPath.LastIndexOf("/");
        int dot = prefabPath.LastIndexOf(".");
        if (lastSlash >= 0 && dot > lastSlash)
            return prefabPath.Substring(lastSlash + 1, dot - lastSlash - 1);
        return prefabPath;
    }
	
	private void UpdateVehPreview(BGL_VehicleStorageEntity veh)
	{
		m_ItemPreviewManager.SetPreviewItemFromPrefab(m_vehPreview, veh.prefab);
	}
	
	private void UpdateVehName(BGL_VehicleStorageEntity veh)
	{
		m_vehName.SetText(GetFriendlyVehicleName(veh.prefab));
	}
	
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
	
	private BGL_PlayerGarageStorageEntity GetPlayerStorage()
	{
		BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();
		storage.ExpandFromRAW(m_playerStorageString);
		return storage;
	}
	
	private array<ref BGL_VehicleStorageEntity> GetVehicles()
	{
		BGL_PlayerGarageStorageEntity storage = GetPlayerStorage();
		return storage.vehicles;
	}
	
	// Events
	
    override void OnMenuInit()
    {
        super.OnMenuInit();
		
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, Close);
        GetGame().GetInputManager().AddActionListener("MenuEscape", EActionTrigger.DOWN, Close);
		
		GetGame().GetInputManager().AddActionListener("MenuSelectHold", EActionTrigger.DOWN, OnBtnSpawnPressed);
		GetGame().GetInputManager().AddActionListener("MenuCalibrateMotionControl", EActionTrigger.DOWN, OnBtnDeletePressed);
		
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world)
		{
			m_ItemPreviewManager = world.GetItemPreviewManager();
		}
    }

    override void OnMenuOpen()
    {
        super.OnMenuOpen();
		
		SetupWidget();
    }
	
	override void OnMenuClose()
	{
		super.OnMenuClose();

        GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, Close);
        GetGame().GetInputManager().RemoveActionListener("MenuEscape", EActionTrigger.DOWN, Close);
		GetGame().GetInputManager().RemoveActionListener("MenuSelectHold", EActionTrigger.DOWN, OnBtnSpawnPressed);
		GetGame().GetInputManager().RemoveActionListener("MenuCalibrateMotionControl", EActionTrigger.DOWN, OnBtnDeletePressed);
	}
	
	// Custom Events
	private void OnBtnSpawnPressed()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		playerController.BGL_Client_RequestLoadCar(m_playerUid, m_vehSelected);
		Close();
	}
	
	private void OnBtnDeletePressed()
	{
		DeleteCar();
		Populate();
	}
	
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
	
	// Setters
	
	void SetBGLComponent(BGL_Component bgl)
	{
		m_bgl = bgl;
	}
	
	void SetPlayerUid(string playerUid)
	{
		m_playerUid = playerUid;
	}
	
	void SetPlayerStorage(string storageString)
	{
		m_playerStorageString = storageString;
	}
	
}