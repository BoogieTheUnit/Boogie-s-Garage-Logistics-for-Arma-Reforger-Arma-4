/*!
    BGL_StoreAction
    -------------------------
    A user action script that triggers storing a vehicle (or other entity)
    into the player’s garage/storage system.  

    Responsibilities
    - Executes only on the server side.
    - Resolves the player performing the action via PlayerManager and BackendApi.
    - Retrieves the owning BGL_Component from the interacted entity.
    - Delegates to BGL_Component.Store() with the player’s UID and ID.

    Notes
    - This action is tied to a prefab/entity with a BGL_Component.
    - Intended to be invoked through ScriptedUserAction framework (usable interaction).
    - Cannot be broadcast to other clients (CanBroadcastScript = false).
*/
class BGL_StoreAction : ScriptedUserAction
{
	/*!
	    Called when the player performs this action.

	    Flow:
	    - Verify running on server (Replication.IsServer check).
	    - Resolve playerId from the controlled entity.
	    - Query BackendApi to get a persistent UID for this player.
	    - Find the BGL_Component on the owner entity.
	    - Call BGL_Component.Store(playerUid, playerId).

	    \param pOwnerEntity The entity that owns this user action (expected to contain BGL_Component).
	    \param pUserEntity  The entity that initiated the action (the player’s controlled entity).
	*/
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Ensure server-side execution only
		if (!Replication.IsServer())
            return;

		// Resolve the acting player's ID
        PlayerManager manager = GetGame().GetPlayerManager();
        int playerId = manager.GetPlayerIdFromControlledEntity(pUserEntity);

		// Backend API needed to get a UID
		BackendApi api = GetGame().GetBackendApi();
		if (!api)
			return;

		// Get persistent UID for the player
		string playerUid = api.GetPlayerIdentityId(playerId);

		// Find the BGL component on the interacted entity
		BGL_Component bgl = BGL_Component.Cast(pOwnerEntity.FindComponent(BGL_Component));
		if (!bgl)
			return;

		// Store the vehicle/item for this player
		bgl.Store(playerUid, playerId);
	}

	/*!
	    Disables broadcasting this user action to clients.
	    This ensures the action only runs on the server.

	    \return Always false.
	*/
	override bool CanBroadcastScript() { return false; }
};
