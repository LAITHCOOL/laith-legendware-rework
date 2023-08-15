// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "logs.h"

void eventlogs::paint_traverse()
{
	constexpr float showtime = 5.f;
	constexpr float animation_time = 0.2f;

	if (logs.empty())
		return;

	while (logs.size() > 10)
		logs.erase(logs.begin());

	static const auto easeOutQuad = [](float x) {
		return 1 - (1 - x) * (1 - x);
	};

	static const auto easeInQuad = [](float x) {
		return x * x;
	};

	for (int i = logs.size() - 1; i >= 0; i--)
	{
		float in_anim = math::clamp((util::epoch_time() - logs[i].log_time) / animation_time, 0.01f, 1.f);
		float out_anim = math::clamp(((util::epoch_time() - logs[i].log_time) - showtime) / animation_time, 0.f, FLT_MAX);

		logs[i].color.SetAlpha(in_anim * (1.f - out_anim) * 255.f);

		if (out_anim > 1.f)
			logs[i].color.SetAlpha(0.f);
		if (in_anim > 1.f)
			logs[i].color.SetAlpha(255.f);

		in_anim = easeInQuad(in_anim);
		out_anim = easeOutQuad(out_anim);

		if (logs[i].color.a() > 0.f) {
			const float x_position = in_anim * 5.f - out_anim * 5.f;
			const float y_position = 14.f * i;

			render::get().text(fonts[LOGS], x_position, y_position + 6.5f, Color(255, 255, 255, logs[i].color.a()), HFONT_CENTERED_NONE, logs[i].message.c_str());
		}
	}

	for (int i = logs.size() - 1; i >= 0; i--) {
		if (logs[i].color.a() <= 0.f) {
			logs.erase(logs.begin() + i);
			break;
		}
	}
}

void eventlogs::events(IGameEvent* event)
{
	static auto get_hitgroup_name = [](int hitgroup) -> std::string
	{
		switch (hitgroup)
		{
		case HITGROUP_HEAD:
			return crypt_str("head");
		case HITGROUP_CHEST:
			return crypt_str("chest");
		case HITGROUP_STOMACH:
			return crypt_str("stomach");
		case HITGROUP_LEFTARM:
			return crypt_str("left arm");
		case HITGROUP_RIGHTARM:
			return crypt_str("right arm");
		case HITGROUP_LEFTLEG:
			return crypt_str("left leg");
		case HITGROUP_RIGHTLEG:
			return crypt_str("right leg");
		default:
			return crypt_str("generic");
		}
	};

	if (g_cfg.misc.events_to_log[EVENTLOG_HIT] && !strcmp(event->GetName(), crypt_str("player_hurt")))
	{
		auto userid = event->GetInt(crypt_str("userid")), attacker = event->GetInt(crypt_str("attacker"));

		if (!userid || !attacker)
			return;

		auto userid_id = m_engine()->GetPlayerForUserID(userid), attacker_id = m_engine()->GetPlayerForUserID(attacker); //-V807

		player_info_t userid_info, attacker_info;

		if (!m_engine()->GetPlayerInfo(userid_id, &userid_info))
			return;

		if (!m_engine()->GetPlayerInfo(attacker_id, &attacker_info))
			return;

		auto m_victim = static_cast<player_t*>(m_entitylist()->GetClientEntity(userid_id));

		std::stringstream ss;

		if (attacker_id == m_engine()->GetLocalPlayer() && userid_id != m_engine()->GetLocalPlayer())
		{
			ss << crypt_str("hit ") << userid_info.szName << crypt_str(" in the ") << get_hitgroup_name(event->GetInt(crypt_str("hitgroup"))) << crypt_str(" for ") << event->GetInt(crypt_str("dmg_health"));
			ss << crypt_str(" damage (") << event->GetInt(crypt_str("health")) << crypt_str(" health remaining).");

			add(ss.str());
		}
		else if (userid_id == m_engine()->GetLocalPlayer() && attacker_id != m_engine()->GetLocalPlayer())
		{
			ss << crypt_str("take ") << event->GetInt(crypt_str("dmg_health")) << crypt_str(" damage from ") << attacker_info.szName << crypt_str(" in the ") << get_hitgroup_name(event->GetInt(crypt_str("hitgroup")));
			ss << crypt_str(" (") << event->GetInt(crypt_str("health")) << crypt_str(" health remaining).");

			add(ss.str());
		}
	}

	if (g_cfg.misc.events_to_log[EVENTLOG_ITEM_PURCHASES] && !strcmp(event->GetName(), crypt_str("item_purchase")))
	{
		auto userid = event->GetInt(crypt_str("userid"));

		if (!userid)
			return;

		auto userid_id = m_engine()->GetPlayerForUserID(userid);

		player_info_t userid_info;

		if (!m_engine()->GetPlayerInfo(userid_id, &userid_info))
			return;

		auto m_player = static_cast<player_t*>(m_entitylist()->GetClientEntity(userid_id));

		if (!g_ctx.local() || !m_player)
			return;

		if (g_ctx.local() == m_player)
			g_ctx.globals.should_buy = 0;

		if (m_player->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
			return;

		std::string weapon = event->GetString(crypt_str("weapon"));

		std::stringstream ss;
		ss << userid_info.szName << crypt_str(" bought ") << weapon << crypt_str(".");

		add(ss.str());
	}

	if (g_cfg.misc.events_to_log[EVENTLOG_BOMB] && !strcmp(event->GetName(), crypt_str("bomb_beginplant")))
	{
		auto userid = event->GetInt(crypt_str("userid"));

		if (!userid)
			return;

		auto userid_id = m_engine()->GetPlayerForUserID(userid);

		player_info_t userid_info;

		if (!m_engine()->GetPlayerInfo(userid_id, &userid_info))
			return;

		auto m_player = static_cast<player_t*>(m_entitylist()->GetClientEntity(userid_id));

		if (!m_player)
			return;

		std::stringstream ss;
		ss << userid_info.szName << crypt_str(" has began planting the bomb.");

		add(ss.str());
	}

	if (g_cfg.misc.events_to_log[EVENTLOG_BOMB] && !strcmp(event->GetName(), crypt_str("bomb_begindefuse")))
	{
		auto userid = event->GetInt(crypt_str("userid"));

		if (!userid)
			return;

		auto userid_id = m_engine()->GetPlayerForUserID(userid);

		player_info_t userid_info;

		if (!m_engine()->GetPlayerInfo(userid_id, &userid_info))
			return;

		auto m_player = static_cast<player_t*>(m_entitylist()->GetClientEntity(userid_id));

		if (!m_player)
			return;

		std::stringstream ss;
		ss << userid_info.szName << crypt_str(" has began defusing the bomb ") << (event->GetBool(crypt_str("haskit")) ? crypt_str("with defuse kit.") : crypt_str("without defuse kit."));

		add(ss.str());
	}
}

void eventlogs::add(std::string text, bool full_display, Color color)
{
	logs.emplace_front(loginfo_t(util::epoch_time(), text, color));

	if (!full_display)
		return;

	if (g_cfg.misc.log_output[EVENTLOG_OUTPUT_CONSOLE])
	{
		last_log = true;

		m_cvar()->ConsoleColorPrintf(color, crypt_str("[ laith-legendware ] ")); //-V807

		m_cvar()->ConsoleColorPrintf(color, text.c_str());
		m_cvar()->ConsolePrintf(crypt_str("\n"));
	}

	if (g_cfg.misc.log_output[EVENTLOG_OUTPUT_CHAT])
	{
		static CHudChat* chat = nullptr;
		if (!chat)
			chat = util::FindHudElement <CHudChat>(crypt_str("CHudChat"));

		auto log = crypt_str("[ \flaith-legendware \001] ") + text;
		chat->chat_print(log.c_str());
	}
}