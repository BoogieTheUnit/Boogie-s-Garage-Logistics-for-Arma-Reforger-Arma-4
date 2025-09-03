class BGL_GetAction : ScriptedUserAction 
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
            return;
        // This entire interaction is only executing on the server
        PlayerManager manager = GetGame().GetPlayerManager();
        int playerId = manager.GetPlayerIdFromControlledEntity(pUserEntity);
        SCR_PlayerController playerController = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
        
		BackendApi api = GetGame().GetBackendApi();
	
		if (!api)
			return;
		
		string playerUid = api.GetPlayerIdentityId(playerId);
		
		BGL_Component bgl = BGL_Component.Cast(pOwnerEntity.FindComponent(BGL_Component));
		
        playerController.BGL_OpenMenu(pOwnerEntity.GetID(), bgl, playerUid, bgl.LoadPlayerGarageData(playerUid));
	}
	
	override bool CanBroadcastScript() { return false; }
};

