/* OperServ core functions
 *
 * (C) 2003-2009 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $Id$
 *
 */
/*************************************************************************/

#include "module.h"

class CommandOSJupe : public Command
{
 public:
	CommandOSJupe() : Command("JUPE", 1, 2, "operserv/jupe")
	{
	}

	CommandReturn Execute(User *u, const std::vector<ci::string> &params)
	{
		const char *jserver = params[0].c_str();
		const char *reason = params.size() > 1 ? params[1].c_str() : NULL;
		Server *server = findserver(servlist, jserver);

		if (!isValidHost(jserver, 3))
			notice_lang(Config.s_OperServ, u, OPER_JUPE_HOST_ERROR);
		else if (server && (server->HasFlag(SERVER_ISME) || server->HasFlag(SERVER_ISUPLINK)))
			notice_lang(Config.s_OperServ, u, OPER_JUPE_INVALID_SERVER);
		else {
			char rbuf[256];
			snprintf(rbuf, sizeof(rbuf), "Juped by %s%s%s", u->nick.c_str(), reason ? ": " : "", reason ? reason : "");
			if (findserver(servlist, jserver))
				ircdproto->SendSquit(jserver, rbuf);
			Server *juped_server = new_server(me_server, jserver, rbuf, SERVER_JUPED, ircd->ts6 ? ts6_sid_retrieve() : NULL);
			ircdproto->SendServer(juped_server);

			if (Config.WallOSJupe)
				ircdproto->SendGlobops(findbot(Config.s_OperServ), "\2%s\2 used JUPE on \2%s\2", u->nick.c_str(), jserver);
		}
		return MOD_CONT;
	}

	bool OnHelp(User *u, const ci::string &subcommand)
	{
		notice_help(Config.s_OperServ, u, OPER_HELP_JUPE);
		return true;
	}

	void OnSyntaxError(User *u, const ci::string &subcommand)
	{
		syntax_error(Config.s_OperServ, u, "JUPE", OPER_JUPE_SYNTAX);
	}
};

class OSJupe : public Module
{
 public:
	OSJupe(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetVersion("$Id$");
		this->SetType(CORE);

		this->AddCommand(OPERSERV, new CommandOSJupe());

		ModuleManager::Attach(I_OnOperServHelp, this);
	}
	void OnOperServHelp(User *u)
	{
		notice_lang(Config.s_OperServ, u, OPER_HELP_CMD_JUPE);
	}
};

MODULE_INIT(OSJupe)
