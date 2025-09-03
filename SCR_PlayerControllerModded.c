modded class SCR_PlayerController
{
	BGL_Component m_bgl;
	
    void BGL_OpenMenu(EntityID signId, BGL_Component bgl, string playerUid, BGL_PlayerGarageStorageEntity storage)
    {
        // Server side replication
        if (!Replication.IsServer())
            return;
		
		m_bgl = bgl;
		
		
		storage.Pack();
        
        Rpc(BGL_RPC_OpenMenu, signId, playerUid, storage.AsString());
    }
	
	
	void BGL_Client_RequestLoadCar(string playerUid, int vehSelected)
	{
		Rpc(BGL_RPC_LoadCar, playerUid, vehSelected, GetPlayerId());
	}
	
	void BGL_Client_RequestSavePlayerStorage(BGL_PlayerGarageStorageEntity storage)
	{
		storage.Pack();
		Rpc(BGL_RPC_SavePlayerStorage, storage.AsString());
	}
	
	void BGL_Server_RequestNotify(string title, string description)
	{
		Rpc(BGL_RPC_Notify, title, description);
	}
    
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void BGL_RPC_OpenMenu(EntityID signId, string playerUid, string storageString)
    {
		IEntity sign = GetGame().GetWorld().FindEntityByID(signId);
		BGL_UIClass bgl_uiclass = BGL_UIClass.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.BGLMenu));
		BGL_Component bgl = BGL_Component.Cast(sign.FindComponent(BGL_Component));
		
		bgl_uiclass.SetBGLComponent(bgl);
		bgl_uiclass.SetPlayerUid(playerUid);
		bgl_uiclass.SetPlayerStorage(storageString);
		bgl_uiclass.Main();
    }
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BGL_RPC_LoadCar(string playerUid, int vehSelected, int playerId)
	{
		m_bgl.LoadCar(playerUid, vehSelected, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BGL_RPC_SavePlayerStorage(string playerStorage)
	{
		BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();
		storage.ExpandFromRAW(playerStorage);
		m_bgl.SavePlayerGarageData(storage);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BGL_RPC_Notify(string title, string description)
	{
		SCR_HintManagerComponent.ShowCustomHint(description, title);
	}
}