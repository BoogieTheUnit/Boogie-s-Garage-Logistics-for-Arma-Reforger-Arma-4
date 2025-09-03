/*!
    SCR_PlayerController (modded)
    -----------------------------
    Extended with BGL (Boogie’s Garage & Logistics) RPCs and helpers.  
    Provides client ↔ server communication for garage interactions:
      - Opening the garage UI
      - Requesting vehicle load
      - Saving updated storage
      - Sending custom notifications

    Key Flow:
      1) Player interacts with a garage sign → server calls BGL_OpenMenu()
      2) Storage is serialized and sent to the client
      3) Client UI opens (BGL_UIClass), showing available vehicles
      4) UI actions (spawn/delete) trigger client requests back to server
      5) Server executes storage or spawn logic in BGL_Component
      6) Notifications are sent back to the player

    Notes:
      - All RPCs use reliable channels for consistency.
      - Server-only and owner-only receivers ensure correct flow.
*/
modded class SCR_PlayerController
{
	//! Reference to the active BGL_Component for this session (garage logic).
	BGL_Component m_bgl;

	// =========================================================
	// Public entry points (called by server or client code)
	// =========================================================

	/*!
	    Request to open the garage menu for a player.

	    Flow:
	      - Server packs player storage
	      - Calls RPC to client with signId, playerUid, and storage data

	    \param signId   EntityID of the garage sign/terminal
	    \param bgl      Reference to the BGL component attached to the sign
	    \param playerUid Persistent UID of the player
	    \param storage  Player’s garage storage payload
	*/
	void BGL_OpenMenu(EntityID signId, BGL_Component bgl, string playerUid, BGL_PlayerGarageStorageEntity storage)
	{
		// Ensure only server executes
		if (!Replication.IsServer())
			return;

		m_bgl = bgl;

		storage.Pack();
		Rpc(BGL_RPC_OpenMenu, signId, playerUid, storage.AsString());
	}

	/*!
	    Client request → Server: Load (spawn) a stored car.
	    Sends playerUid + selection index.
	*/
	void BGL_Client_RequestLoadCar(string playerUid, int vehSelected)
	{
		Rpc(BGL_RPC_LoadCar, playerUid, vehSelected, GetPlayerId());
	}

	/*!
	    Client request → Server: Save the player’s garage storage.
	    Packs storage and sends to server for persistence.
	*/
	void BGL_Client_RequestSavePlayerStorage(BGL_PlayerGarageStorageEntity storage)
	{
		storage.Pack();
		Rpc(BGL_RPC_SavePlayerStorage, storage.AsString());
	}

	/*!
	    Server → Client: Send a notification popup.
	    Uses SCR_HintManagerComponent for display.
	*/
	void BGL_Server_RequestNotify(string title, string description)
	{
		Rpc(BGL_RPC_Notify, title, description);
	}

	// =========================================================
	// RPCs (client/server communication handlers)
	// =========================================================

	/*!
	    RPC: Client-side handler for opening the garage UI.
	    Called by server via BGL_OpenMenu().

	    Flow:
	      - Resolve the sign entity
	      - Open BGL UI menu (ChimeraMenuPreset.BGLMenu)
	      - Inject BGL_Component, playerUid, and storage string
	      - Populate the UI
	*/
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

	/*!
	    RPC: Server-side handler for loading a vehicle.
	    Calls into BGL_Component.LoadCar().

	    \param playerUid  Persistent UID
	    \param vehSelected Index of stored vehicle
	    \param playerId   Runtime player id
	*/
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BGL_RPC_LoadCar(string playerUid, int vehSelected, int playerId)
	{
		m_bgl.LoadCar(playerUid, vehSelected, playerId);
	}

	/*!
	    RPC: Server-side handler for saving player storage.
	    Expands storage string and saves via BGL_Component.

	    \param playerStorage Packed storage string (JSON)
	*/
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BGL_RPC_SavePlayerStorage(string playerStorage)
	{
		BGL_PlayerGarageStorageEntity storage = new BGL_PlayerGarageStorageEntity();
		storage.ExpandFromRAW(playerStorage);
		m_bgl.SavePlayerGarageData(storage);
	}

	/*!
	    RPC: Client-side handler for displaying a notification.
	    Uses hint manager to show a title + description popup.
	*/
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BGL_RPC_Notify(string title, string description)
	{
		SCR_HintManagerComponent.ShowCustomHint(description, title);
	}
}
