/*!
    BGL_GetAction
    -------------------------
    A user action script that opens the Garage & Logistics UI for the player.  

    Responsibilities
    - Runs exclusively on the server (guarded by Replication.IsServer).
    - Resolves the acting player’s ID, controller, and persistent UID.
    - Retrieves the BGL_Component from the interacted entity.
    - Calls into SCR_PlayerController.BGL_OpenMenu() with all necessary context:
        • Owner entity ID (for tracking the garage sign/prefab)
        • BGL_Component reference
        • Player UID
        • Player’s garage storage data (loaded from file)

    Usage
    - Attach to a prefab (e.g., garage sign / terminal) to allow players to
      interact and open their garage UI.
    - Complements BGL_StoreAction (storing vehicles) by providing a retrieval UI.

    Notes
    - Menu actually opens client-side via RPC in BGL_OpenMenu.
    - CanBroadcastScript returns false to prevent client broadcast of this action.
*/
class BGL_GetAction : ScriptedUserAction
{
	/*!
	    Executed when the player performs this action.

	    Flow:
	      - Check server execution (Replication.IsServer).
	      - Resolve acting player ID and SCR_PlayerController.
	      - Obtain BackendApi and derive the player’s persistent UID.
	      - Locate BGL_Component on the interacted entity.
	      - Call playerController.BGL_OpenMenu(...) to open the garage UI
	        with context (signId, bgl reference, playerUid, loaded storage).

	    \param pOwnerEntity The entity owning this action (e.g., garage terminal).
	    \param pUserEntity  The entity controlled by the player performing the action.
	*/
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Ensure execution only runs on the server
		if (!Replication.IsServer())
            return;

		// Resolve player identity
		PlayerManager manager = GetGame().GetPlayerManager();
		int playerId = manager.GetPlayerIdFromControlledEntity(pUserEntity);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));

		// Resolve persistent UID
		BackendApi api = GetGame().GetBackendApi();
		if (!api)
			return;

		string playerUid = api.GetPlayerIdentityId(playerId);

		// Locate garage component
		BGL_Component bgl = BGL_Component.Cast(pOwnerEntity.FindComponent(BGL_Component));

		// Request UI open (menu is spawned client-side via controller RPC)
        playerController.BGL_OpenMenu(
			pOwnerEntity.GetID(),      // sign / terminal entity ID
			bgl,                       // garage logic component
			playerUid,                 // persistent player UID
			bgl.LoadPlayerGarageData(playerUid) // player storage payload
		);
	}

	/*!
	    Prevents this scripted action from being broadcast to clients.
	    Action logic runs exclusively on the server.
	    \return Always false.
	*/
	override bool CanBroadcastScript() { return false; }
};

