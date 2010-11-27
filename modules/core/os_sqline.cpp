/* OperServ core functions
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

class SQLineDelCallback : public NumberList
{
	CommandSource &source;
	unsigned Deleted;
 public:
	SQLineDelCallback(CommandSource &_source, const Anope::string &numlist) : NumberList(numlist, true), source(_source), Deleted(0)
	{
	}

	~SQLineDelCallback()
	{
		if (!Deleted)
			source.Reply(OPER_SQLINE_NO_MATCH);
		else if (Deleted == 1)
			source.Reply(OPER_SQLINE_DELETED_ONE);
		else
			source.Reply(OPER_SQLINE_DELETED_SEVERAL, Deleted);
	}

	void HandleNumber(unsigned Number)
	{
		if (!Number)
			return;

		XLine *x = SQLine->GetEntry(Number - 1);

		if (!x)
			return;

		++Deleted;
		DoDel(source, x - 1);
	}

	static void DoDel(CommandSource &source, XLine *x)
	{
		SQLine->DelXLine(x);
	}
};

class SQLineListCallback : public NumberList
{
 protected:
	CommandSource &source;
	bool SentHeader;
 public:
	SQLineListCallback(CommandSource &_source, const Anope::string &numlist) : NumberList(numlist, false), source(_source), SentHeader(false)
	{
	}

	~SQLineListCallback()
	{
		if (!SentHeader)
			source.Reply(OPER_SQLINE_NO_MATCH);
	}

	virtual void HandleNumber(unsigned Number)
	{
		if (!Number)
			return;

		XLine *x = SQLine->GetEntry(Number - 1);

		if (!x)
			return;

		if (!SentHeader)
		{
			SentHeader = true;
			source.Reply(OPER_SQLINE_LIST_HEADER);
		}

		DoList(source, x, Number - 1);
	}

	static void DoList(CommandSource &source, XLine *x, unsigned Number)
	{
		source.Reply(OPER_LIST_FORMAT, Number + 1, x->Mask.c_str(), x->Reason.c_str());
	}
};

class SQLineViewCallback : public SQLineListCallback
{
 public:
	SQLineViewCallback(CommandSource &_source, const Anope::string &numlist) : SQLineListCallback(_source, numlist)
	{
	}

	void HandleNumber(unsigned Number)
	{
		if (!Number)
			return;

		XLine *x = SQLine->GetEntry(Number - 1);

		if (!x)
			return;

		if (!SentHeader)
		{
			SentHeader = true;
			source.Reply(OPER_SQLINE_VIEW_HEADER);
		}

		DoList(source, x, Number);
	}

	static void DoList(CommandSource &source, XLine *x, unsigned Number)
	{
		Anope::string expirebuf = expire_left(source.u->Account(), x->Expires);
		source.Reply(OPER_VIEW_FORMAT, Number + 1, x->Mask.c_str(), x->By.c_str(), do_strftime(x->Created).c_str(), expirebuf.c_str(), x->Reason.c_str());
	}
};

class CommandOSSQLine : public Command
{
 private:
	CommandReturn DoAdd(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		unsigned last_param = 2;
		Anope::string expiry, mask;
		time_t expires;

		mask = params.size() > 1 ? params[1] : "";
		if (!mask.empty() && mask[0] == '+')
		{
			expiry = mask;
			mask = params.size() > 2 ? params[2] : "";
			last_param = 3;
		}

		expires = !expiry.empty() ? dotime(expiry) : Config->SQLineExpiry;
		/* If the expiry given does not contain a final letter, it's in days,
		 * said the doc. Ah well.
		 */
		if (!expiry.empty() && isdigit(expiry[expiry.length() - 1]))
			expires *= 86400;
		/* Do not allow less than a minute expiry time */
		if (expires && expires < 60)
		{
			source.Reply(BAD_EXPIRY_TIME);
			return MOD_CONT;
		}
		else if (expires > 0)
			expires += Anope::CurTime;

		if (params.size() <= last_param)
		{
			this->OnSyntaxError(source, "ADD");
			return MOD_CONT;
		}

		Anope::string reason = params[last_param];
		if (last_param == 2 && params.size() > 3)
			reason += " " + params[3];
		if (!mask.empty() && !reason.empty())
		{
			User *user = finduser(mask);
			if (user)
				mask = "*@" + user->host;
			unsigned int affected = 0;
			for (patricia_tree<User *>::const_iterator it = UserListByNick.begin(), it_end = UserListByNick.end(); it != it_end; ++it)
				if (Anope::Match((*it)->GetIdent() + "@" + (*it)->host, mask))
					++affected;
			float percent = static_cast<float>(affected) / static_cast<float>(UserListByNick.size()) * 100.0;

			if (percent > 95)
			{
				source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
				Log(LOG_ADMIN, u, this) << "tried to SQLine " << percent << "% of the network (" << affected << " users)";
				return MOD_CONT;
			}
			XLine *x = SQLine->Add(OperServ, u, mask, expires, reason);

			if (!x)
				return MOD_CONT;

			source.Reply(OPER_SQLINE_ADDED, mask.c_str());

			if (Config->WallOSSQLine)
			{
				Anope::string buf;

				if (!expires)
					buf = "does not expire";
				else
				{
					time_t wall_expiry = expires - Anope::CurTime;
					Anope::string s;

					if (wall_expiry >= 86400)
					{
						wall_expiry /= 86400;
						s = "day";
					}
					else if (wall_expiry >= 3600)
					{
						wall_expiry /= 3600;
						s = "hour";
					}
					else if (wall_expiry >= 60)
					{
						wall_expiry /= 60;
						s = "minute";
					}

					buf = "expires in " + stringify(wall_expiry) + " " + s + (wall_expiry == 1 ? "" : "s");
				}

				ircdproto->SendGlobops(OperServ, "%s added an SQLINE for %s (%s) [affects %i user(s) (%.2f%%)]", u->nick.c_str(), mask.c_str(), buf.c_str(), affected, percent);
			}

			if (readonly)
				source.Reply(READ_ONLY_MODE);

		}
		else
			this->OnSyntaxError(source, "ADD");

		return MOD_CONT;
	}

	CommandReturn DoDel(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		if (SQLine->GetList().empty())
		{
			source.Reply(OPER_SQLINE_LIST_EMPTY);
			return MOD_CONT;
		}

		const Anope::string &mask = params.size() > 1 ? params[1] : "";

		if (mask.empty())
		{
			this->OnSyntaxError(source, "DEL");
			return MOD_CONT;
		}

		if (!mask.empty() && isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			SQLineDelCallback list(source, mask);
			list.Process();
		}
		else
		{
			XLine *x = SQLine->HasEntry(mask);

			if (!x)
			{
				source.Reply(OPER_SQLINE_NOT_FOUND, mask.c_str());
				return MOD_CONT;
			}

			FOREACH_MOD(I_OnDelXLine, OnDelXLine(u, x, X_SQLINE));

			SQLineDelCallback::DoDel(source, x);
			source.Reply(OPER_SQLINE_DELETED, mask.c_str());
		}

		if (readonly)
			source.Reply(READ_ONLY_MODE);

		return MOD_CONT;
	}

	CommandReturn DoList(CommandSource &source, const std::vector<Anope::string> &params)
	{
		if (SQLine->GetList().empty())
		{
			source.Reply(OPER_SQLINE_LIST_EMPTY);
			return MOD_CONT;
		}

		Anope::string mask = params.size() > 1 ? params[1] : "";

		if (!mask.empty() && isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			SQLineListCallback list(source, mask);
			list.Process();
		}
		else
		{
			bool SentHeader = false;

			for (unsigned i = 0, end = SQLine->GetCount(); i < end; ++i)
			{
				XLine *x = SQLine->GetEntry(i);

				if (mask.empty() || mask.equals_ci(x->Mask) || Anope::Match(x->Mask, mask))
				{
					if (!SentHeader)
					{
						SentHeader = true;
						source.Reply(OPER_SQLINE_LIST_HEADER);
					}

					SQLineListCallback::DoList(source, x, i);
				}
			}

			if (!SentHeader)
				source.Reply(OPER_SQLINE_NO_MATCH);
			else
				source.Reply(END_OF_ANY_LIST, "SQLine");
		}

		return MOD_CONT;
	}

	CommandReturn DoView(CommandSource &source, const std::vector<Anope::string> &params)
	{
		if (SQLine->GetList().empty())
		{
			source.Reply(OPER_SQLINE_LIST_EMPTY);
			return MOD_CONT;
		}

		Anope::string mask = params.size() > 1 ? params[1] : "";

		if (!mask.empty() && isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			SQLineViewCallback list(source, mask);
			list.Process();
		}
		else
		{
			bool SentHeader = false;

			for (unsigned i = 0, end = SQLine->GetCount(); i < end; ++i)
			{
				XLine *x = SQLine->GetEntry(i);

				if (mask.empty() || mask.equals_ci(x->Mask) || Anope::Match(x->Mask, mask))
				{
					if (!SentHeader)
					{
						SentHeader = true;
						source.Reply(OPER_SQLINE_VIEW_HEADER);
					}

					SQLineViewCallback::DoList(source, x, i);
				}
			}

			if (!SentHeader)
				source.Reply(OPER_SQLINE_NO_MATCH);
		}

		return MOD_CONT;
	}

	CommandReturn DoClear(CommandSource &source)
	{
		User *u = source.u;
		FOREACH_MOD(I_OnDelXLine, OnDelXLine(u, NULL, X_SQLINE));
		SGLine->Clear();
		source.Reply(OPER_SQLINE_CLEAR);

		return MOD_CONT;
	}
 public:
	CommandOSSQLine() : Command("SQLINE", 1, 4, "operserv/sqline")
	{
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &cmd = params[0];

		if (cmd.equals_ci("ADD"))
			return this->DoAdd(source, params);
		else if (cmd.equals_ci("DEL"))
			return this->DoDel(source, params);
		else if (cmd.equals_ci("LIST"))
			return this->DoList(source, params);
		else if (cmd.equals_ci("VIEW"))
			return this->DoView(source, params);
		else if (cmd.equals_ci("CLEAR"))
			return this->DoClear(source);
		else
			this->OnSyntaxError(source, "");
		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		source.Reply(OPER_HELP_SQLINE);
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "SQLINE", OPER_SQLINE_SYNTAX);
	}

	void OnServHelp(CommandSource &source)
	{
		source.Reply(OPER_HELP_CMD_SQLINE);
	}
};

class OSSQLine : public Module
{
	CommandOSSQLine commandossqline;

 public:
	OSSQLine(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		if (!ircd->sqline)
			throw ModuleException("Your IRCd does not support QLines.");

		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(OperServ, &commandossqline);
	}
};

MODULE_INIT(OSSQLine)
