/* ChanServ core functions
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"
#include "hashcomp.h"

class CommandCSList : public Command
{
public:
	CommandCSList() : Command("LIST", 1, 2)
	{
		this->SetFlag(CFLAG_STRIP_CHANNEL);
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		Anope::string pattern = params[0];
		unsigned nchans;
		char buf[BUFSIZE];
		bool is_servadmin = u->Account()->HasCommand("chanserv/list");
		int count = 0, from = 0, to = 0;
		bool forbidden = false, suspended = false, channoexpire = false;

		if (Config->CSListOpersOnly && !is_oper(u))
		{
			source.Reply(ACCESS_DENIED);
			return MOD_STOP;
		}

		if (pattern[0] == '#')
		{
			Anope::string tmp = myStrGetToken(pattern.substr(1), '-', 0); /* Read FROM out */
			if (tmp.empty())
			{
				source.Reply(LIST_INCORRECT_RANGE);
				source.Reply(CS_LIST_INCORRECT_RANGE);
				return MOD_CONT;
			}
			if (!tmp.is_number_only())
			{
				source.Reply(LIST_INCORRECT_RANGE);
				source.Reply(CS_LIST_INCORRECT_RANGE);
				return MOD_CONT;
			}
			from = convertTo<int>(tmp);
			tmp = myStrGetTokenRemainder(pattern, '-', 1);  /* Read TO out */
			if (tmp.empty())
			{
				source.Reply(LIST_INCORRECT_RANGE);
				source.Reply(CS_LIST_INCORRECT_RANGE);
				return MOD_CONT;
			}
			if (!tmp.is_number_only())
			{
				source.Reply(LIST_INCORRECT_RANGE);
				source.Reply(CS_LIST_INCORRECT_RANGE);
				return MOD_CONT;
			}
			to = convertTo<int>(tmp);
			pattern = "*";
		}

		nchans = 0;

		if (is_servadmin && params.size() > 1)
		{
			Anope::string keyword;
			spacesepstream keywords(params[1]);
			while (keywords.GetToken(keyword))
			{
				if (keyword.equals_ci("FORBIDDEN"))
					forbidden = true;
				if (keyword.equals_ci("SUSPENDED"))
					suspended = true;
				if (keyword.equals_ci("NOEXPIRE"))
					channoexpire = true;
			}
		}

		Anope::string spattern = "#" + pattern;

		source.Reply(NICK_LIST_HEADER, pattern.c_str());

		for (registered_channel_map::const_iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; ++it)
		{
			ChannelInfo *ci = it->second;

			if (!is_servadmin && (ci->HasFlag(CI_PRIVATE) || ci->HasFlag(CI_FORBIDDEN) || ci->HasFlag(CI_SUSPENDED)))
				continue;
			if (forbidden && !ci->HasFlag(CI_FORBIDDEN))
				continue;
			else if (suspended && !ci->HasFlag(CI_SUSPENDED))
				continue;
			else if (channoexpire && !ci->HasFlag(CI_NO_EXPIRE))
				continue;

			if (pattern.equals_ci(ci->name) || ci->name.equals_ci(spattern) || Anope::Match(ci->name, pattern) || Anope::Match(ci->name, spattern))
			{
				if (((count + 1 >= from && count + 1 <= to) || (!from && !to)) && ++nchans <= Config->CSListMax)
				{
					char noexpire_char = ' ';
					if (is_servadmin && (ci->HasFlag(CI_NO_EXPIRE)))
						noexpire_char = '!';

					if (ci->HasFlag(CI_FORBIDDEN))
						snprintf(buf, sizeof(buf), "%-20s  [Forbidden]", ci->name.c_str());
					else if (ci->HasFlag(CI_SUSPENDED))
						snprintf(buf, sizeof(buf), "%-20s  [Suspended]", ci->name.c_str());
					else
						snprintf(buf, sizeof(buf), "%-20s  %s", ci->name.c_str(), !ci->desc.empty() ? ci->desc.c_str() : "");

					source.Reply("  %c%s", noexpire_char, buf);
				}
				++count;
			}
		}

		source.Reply(CHAN_LIST_END, nchans > Config->CSListMax ? Config->CSListMax : nchans, nchans);
		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		source.Reply(CHAN_HELP_LIST);
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "LIST", NICK_LIST_SYNTAX);
	}

	void OnServHelp(CommandSource &source)
	{
		source.Reply(CHAN_HELP_CMD_LIST);
	}
};

class CSList : public Module
{
	CommandCSList commandcslist;

 public:
	CSList(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(ChanServ, &commandcslist);
	}
};

MODULE_INIT(CSList)
