/* NickServ core functions
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

class CommandNSLogout : public Command
{
 public:
	CommandNSLogout() : Command("LOGOUT", 0, 2)
	{
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		const Anope::string &nick = !params.empty() ? params[0] : "";
		const Anope::string &param = params.size() > 1 ? params[1] : "";

		User *u2;
		if (!u->Account()->IsServicesOper() && !nick.empty())
			this->OnSyntaxError(source, "");
		else if (!(u2 = (!nick.empty() ? finduser(nick) : u)))
			source.Reply(NICK_X_NOT_IN_USE, nick.c_str());
		else if (!nick.empty() && u2->Account() && !u2->Account()->IsServicesOper())
			source.Reply(NICK_LOGOUT_SERVICESADMIN, nick.c_str());
		else
		{
			if (!nick.empty() && !param.empty() && param.equals_ci("REVALIDATE"))
				validate_user(u2);

			u2->isSuperAdmin = 0; /* Dont let people logout and remain a SuperAdmin */
			Log(LOG_COMMAND, u, this) << "to logout " << u2->nick;

			/* Remove founder status from this user in all channels */
			if (!nick.empty())
				source.Reply(NICK_LOGOUT_X_SUCCEEDED, nick.c_str());
			else
				source.Reply(NICK_LOGOUT_SUCCEEDED);

			ircdproto->SendAccountLogout(u2, u2->Account());
			u2->RemoveMode(NickServ, UMODE_REGISTERED);
			ircdproto->SendUnregisteredNick(u2);

			u2->Logout();

			/* Send out an event */
			FOREACH_MOD(I_OnNickLogout, OnNickLogout(u2));
		}
		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		User *u = source.u;
		if (u->Account() && u->Account()->IsServicesOper())
			source.Reply(NICK_SERVADMIN_HELP_LOGOUT);
		else
			source.Reply(NICK_HELP_LOGOUT);

		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "LOGOUT", NICK_LOGOUT_SYNTAX);
	}

	void OnServHelp(CommandSource &source)
	{
		source.Reply(NICK_HELP_CMD_LOGOUT);
	}
};

class NSLogout : public Module
{
	CommandNSLogout commandnslogout;

 public:
	NSLogout(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(NickServ, &commandnslogout);
	}
};

MODULE_INIT(NSLogout)
