#include "spammers.h"
#include "../prediction/Networking.h"
void spammers::clan_tag()
{
    auto apply = [](const char* tag) -> void
    {
        using Fn = int(__fastcall*)(const char*, const char*);
        static auto fn = reinterpret_cast<Fn>(util::FindSignature(crypt_str("engine.dll"), crypt_str("53 56 57 8B DA 8B F9 FF 15")));

        fn(tag, tag);
    };

	static auto removed = false;

	if (!g_cfg.misc.clantag_spammer && !removed)
	{
		removed = true;
		apply(crypt_str(""));
		return;
	}

	if (g_cfg.misc.clantag_spammer)
	{
		static auto time = 1;

		auto ticks = TIME_TO_TICKS(g_Networking->flow_outgoing) + (float)m_globals()->m_tickcount; //-V807
		auto intervals = 0.4f / m_globals()->m_intervalpertick;

		auto main_time = (int)(ticks / intervals) % 11;

		if (main_time != time && m_clientstate()->iChokedCommands == 0)
		{
			auto tag = crypt_str("");

			switch (main_time)
			{
			case 0:
				tag = crypt_str(" ");
				break;
			case 1:
				tag = crypt_str("l ");
				break;
			case 2:
				tag = crypt_str("lu ");
				break;
			case 3:
				tag = crypt_str("lul ");
				break;
			case 4:
				tag = crypt_str("lulu ");
				break;
			case 5:
				tag = crypt_str("lul ");
				break;
			case 6:
				tag = crypt_str("lu ");
				break;
			case 7:
				tag = crypt_str("l");
				break;
			case 8:
				tag = crypt_str("");
				break;
			}

			apply(tag);
			time = main_time;
		}

		removed = false;
	}
}