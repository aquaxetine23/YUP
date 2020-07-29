#include "client.h"

cl_clientfunc_t *g_pClient = NULL;
cl_enginefunc_t *g_pEngine = NULL;
engine_studio_api_t *g_pStudio = NULL;
StudioModelRenderer_t* g_pStudioModelRenderer = NULL;
playermove_t* pmove = NULL;
net_status_s Status;

CL_Move_t CL_Move_s = NULL;
PreS_DynamicSound_t PreS_DynamicSound_s = NULL;
cl_clientfunc_t g_Client;
cl_enginefunc_t g_Engine;
engine_studio_api_t g_Studio;
StudioModelRenderer_t g_StudioModelRenderer;

PColor24 Console_TextColor;
DWORD HudRedraw;

void HUD_Redraw(float time, int intermission)
{
	g_Client.HUD_Redraw(time, intermission);

	HudRedraw = GetTickCount();
	KzFameCount();
	Snapshot();
}

int HUD_Key_Event(int down, int keynum, const char* pszCurrentBinding)
{
	//preset keys bind
	bool keyrush = cvar.rage_active && keynum == cvar.route_rush_key;
	bool keyquick = cvar.misc_quick_change && keynum == cvar.misc_quick_change_key;
	bool keystrafe = cvar.kz_strafe && keynum == cvar.kz_strafe_key;
	bool keyfast = cvar.kz_fast_run && keynum == cvar.kz_fastrun_key;
	bool keygstrafe = cvar.kz_gstrafe && keynum == cvar.kz_gstrafe_key;
	bool keybhop = cvar.kz_bhop && keynum == cvar.kz_bhop_key;
	bool keyjump = cvar.kz_jump_bug && keynum == cvar.kz_jumpbug_key;
	bool keyrage = cvar.rage_active && !cvar.rage_auto_fire && keynum == cvar.rage_auto_fire_key;
	bool keylegit = !cvar.rage_active && cvar.legit[g_Local.weapon.m_iWeaponID].trigger_active && !cvar.legit[g_Local.weapon.m_iWeaponID].active && IsCurWeaponGun() && keynum == cvar.legit_trigger_key;
	bool keychat = cvar.gui_chat && keynum == cvar.gui_chat_key;
	bool keychatteam = cvar.gui_chat && keynum == cvar.gui_chat_key_team;
	bool keymenu = keynum == cvar.gui_key;

	//send keys to menu
	keysmenu[keynum] = down;

	//send chat message
	if (bInputActive && keynum == K_ENTER && down)
	{
		char SayText[255];
		if (iInputMode == 1)sprintf(SayText, "say \"%s\"", InputBuf);
		if (iInputMode == 2)sprintf(SayText, "say_team \"%s\"", InputBuf);
		g_Engine.pfnClientCmd(SayText);
		strcpy(InputBuf, "");
		bInputActive = false;
		for (unsigned int i = 0; i < 256; i++) ImGui::GetIO().KeysDown[i] = false;
		return false;
	}
	//return game bind if chat active
	if (bInputActive && CheckDraw())
		return false;

	//return game bind for chat bind
	if (keychat && down)
	{
		bInputActive = true, iInputMode = 1, SetKeyboardFocus = true;
		return false;
	}
	if (keychatteam && down)
	{
		bInputActive = true, iInputMode = 2, SetKeyboardFocus = true;
		return false;
	}
	
	//return game bind for menu bind
	if (keymenu && down)
	{
		bShowMenu = !bShowMenu;
		SaveCvar();
		return false;
	}

	//return game bind if menu active
	if (bShowMenu && CheckDraw())
		return false;

	//check if alive
	if (bAliveLocal())
	{
		//do function bind
		if (keyrush)
		{
			if (down)
				cvar.route_auto = 1, cvar.misc_wav_speed = 64;
			else
				cvar.route_auto = 0, cvar.misc_wav_speed = 1;
		}
		if (keyquick && down)
		{
			cvar.rage_active = !cvar.rage_active, SaveCvar(), ModeChangeDelay = GetTickCount();
			if (!cvar.rage_active)cvar.route_auto = 0, cvar.misc_wav_speed = 1, RageKeyStatus = false;
		}
		if (keystrafe)
			Strafe = down;
		if (keyfast)
			Fastrun = down;
		if (keygstrafe)
			Gstrafe = down;
		if (keybhop)
			Bhop = down;
		if (keyjump)
			Jumpbug = down;
		if (keyrage)
			RageKeyStatus = down;
		if (keylegit && down)
			TriggerKeyStatus = !TriggerKeyStatus;
		
		//return game bind for function bind
		if ((keystrafe || keyfast || keygstrafe || keybhop || keyjump || keyrage || keylegit || keyquick || keyrush) && down)
			return false;
	}
	
	return g_Client.HUD_Key_Event(down, keynum, pszCurrentBinding);
}

void CL_CreateMove(float frametime, struct usercmd_s* cmd, int active)
{
	g_Client.CL_CreateMove(frametime, cmd, active);

	AdjustSpeed(cvar.misc_wav_speed);
	UpdateAimbot();
	UpdateWeaponData();
	AimBot(cmd);
	ContinueFire(cmd);
	ItemPostFrame(cmd);
	Kz(frametime, cmd);
	NoRecoil(cmd);
	NoSpread(cmd);
	Route(cmd);
	AntiAim(cmd);
	FakeLag(frametime, cmd);
	CustomFOV();
}

void HUD_PostRunCmd(struct local_state_s* from, struct local_state_s* to, struct usercmd_s* cmd, int runfuncs, double time, unsigned int random_seed)
{
	g_Client.HUD_PostRunCmd(from, to, cmd, runfuncs, time, random_seed);
	ItemPreFrame(from, to, cmd, runfuncs, time, random_seed);
}

void PreV_CalcRefdef(struct ref_params_s* pparams)
{
	g_Local.vPunchangle = pparams->punchangle;
	g_Local.vPrevForward = pparams->forward;
	g_Local.iPrevHealth = pparams->health;
	V_CalcRefdefRecoil(pparams);
}

void PostV_CalcRefdef(struct ref_params_s* pparams)
{
	g_Local.vPostForward = pparams->forward;
	g_Local.iPostHealth = pparams->health;
	ThirdPerson(pparams);
	if (pparams->nextView == 0)
	{
		for (unsigned int id = 1; id <= g_Engine.GetMaxClients(); id++)
		{
			g_Player[id].bGotHitboxPlayer = false;
			g_Player[id].bGotBonePlayer = false;
		}
	}
}

void V_CalcRefdef(struct ref_params_s* pparams)
{
	PreV_CalcRefdef(pparams);
	g_Client.V_CalcRefdef(pparams);
	PostV_CalcRefdef(pparams);
}

void bSendpacket(bool status)
{
	static bool bsendpacket_status = true;
	static DWORD NULLTIME = NULL;
	if (status && !bsendpacket_status)
	{
		bsendpacket_status = true;
		*(DWORD*)(c_Offset.dwSendPacketPointer) = c_Offset.dwSendPacketBackup;
	}
	if (!status && bsendpacket_status)
	{
		bsendpacket_status = false;
		*(DWORD*)(c_Offset.dwSendPacketPointer) = (DWORD)& NULLTIME;
	}
}

void CL_Move()
{
	bSendpacket(true);
	CL_Move_s();
}

void AdjustSpeed(double speed)
{
	static double LastSpeed = 1;
	if (speed != LastSpeed)
	{
		*(double*)c_Offset.dwSpeedPointer = (speed * 1000);

		LastSpeed = speed;
	}
}

void HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	PM_InitTextureTypes(ppmove);
	return g_Client.HUD_PlayerMoveInit(ppmove);
}

void HUD_ProcessPlayerState(struct entity_state_s* dst, const struct entity_state_s* src)
{
	if (cvar.bypass_valid_blockers)
	{
		for (unsigned int i = 0; i < 3; i++)
			src->mins[i] = i == 2 ? -36 : -16;
		for (unsigned int i = 0; i < 3; i++)
			src->maxs[i] = i == 2 ? 36 : 16;
	}
	g_Client.HUD_ProcessPlayerState(dst, src);
}

int HUD_GetHullBounds(int hullnum, float* mins, float* maxs)
{
	if (hullnum == 1)
		maxs[2] = 32.0f;
	return 1;
}

int CL_IsThirdPerson(void)
{
	if (cvar.visual_chase_cam && bAliveLocal() && CheckDrawEngine())
		return 1;
	return g_Client.CL_IsThirdPerson();
}

int HUD_AddEntity(int type, struct cl_entity_s* ent, const char* modelname)
{
	if (ent && ent->player && ent->index == pmove->player_index + 1 && bAliveLocal() && cvar.rage_active && cvar.visual_chase_cam && CheckDrawEngine())
	{
		SetAntiAimAngles(ent);
		g_Engine.CL_CreateVisibleEntity(ET_PLAYER, ent);
		return 0;
	}
	if (ent && strstr(ent->model->name, "player") && ent->index != pmove->player_index + 1 && !bAlive(ent->index) && cvar.rage_active && CheckDrawEngine() && bAliveLocal())
		return 0;
	return g_Client.HUD_AddEntity(type, ent, modelname);
}

void HUD_Frame(double time)
{
	g_Engine.pNetAPI->Status(&(Status));
	NoFlash();
	Lightmap();
	g_Client.HUD_Frame(time);
}

void HookClientFunctions()
{
	g_pClient->HUD_Frame = HUD_Frame;
	g_pClient->HUD_Redraw = HUD_Redraw;
	g_pClient->HUD_AddEntity = HUD_AddEntity;
	g_pClient->CL_CreateMove = CL_CreateMove;
	g_pClient->HUD_PlayerMoveInit = HUD_PlayerMoveInit;
	g_pClient->V_CalcRefdef = V_CalcRefdef;
	g_pClient->HUD_PostRunCmd = HUD_PostRunCmd;
	g_pClient->HUD_Key_Event = HUD_Key_Event;
	g_pClient->HUD_ProcessPlayerState = HUD_ProcessPlayerState;
	g_pClient->HUD_GetHullBounds = HUD_GetHullBounds;
	g_pClient->CL_IsThirdPerson = CL_IsThirdPerson;
}