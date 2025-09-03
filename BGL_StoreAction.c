class BGL_StoreAction : ScriptedUserAction 
{

	
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
            return;
        // This entire interaction is only executing on the server
        PlayerManager manager = GetGame().GetPlayerManager();
        int playerId = manager.GetPlayerIdFromControlledEntity(pUserEntity);
        
		BackendApi api = GetGame().GetBackendApi();
	
		if (!api)
			return;
		
		string playerUid = api.GetPlayerIdentityId(playerId);
		
		BGL_Component bgl = BGL_Component.Cast(pOwnerEntity.FindComponent(BGL_Component));
		
		bgl.Store(playerUid, playerId);
	}
	
	 override bool CanBroadcastScript() { return false; }
};