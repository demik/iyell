/*
 *  irc.h
 *  
 *
 *  Created by Michel DEPEIGE on 12/10/07.
 *  Copyright (c) 2007 Michel DEPEIGE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 */

#ifndef IRC_H
#define IRC_H

/* IRC reply codes */
#define RPL_TRACELINK		"200"
#define RPL_TRACECONNECTING	"201"
#define RPL_TRACEHANDSHAKE	"202"
#define RPL_TRACEUNKNOWN	"203"
#define RPL_TRACEOPERATOR	"204"
#define RPL_TRACEUSER		"205"
#define RPL_TRACESERVER		"206"
#define RPL_TRACENEWTYPE	"208"
#define RPL_STATSLINKINFO	"211"
#define RPL_STATSCOMMANDS	"212"
#define RPL_STATSCLINE		"213"
#define RPL_STATSNLINE		"214"
#define RPL_STATSILINE		"215"
#define RPL_STATSKLINE		"216"
#define RPL_STATSYLINE		"218"
#define RPL_ENDOFSTATS		"219"
#define RPL_UMODEIS		"221"
#define RPL_STATSLLINE		"241"
#define RPL_STATSUPTIME		"242"
#define RPL_STATSOLINE		"243"
#define RPL_STATSHLINE		"244"
#define RPL_LUSERCLIENT		"251"
#define RPL_LUSEROP		"252"
#define RPL_LUSERUNKNOWN	"253"
#define RPL_LUSERCHANNELS	"254"
#define RPL_LUSERME		"255"
#define RPL_ADMINME		"256"
#define RPL_ADMINLOC1		"257"
#define RPL_ADMINLOC2		"258"
#define RPL_ADMINEMAIL		"259"
#define RPL_TRACELOG		"261"
#define RPL_NONE		"300"
#define RPL_AWAY		"301"
#define RPL_USERHOST		"302"
#define RPL_ISON		"303"
#define RPL_UNAWAY		"305"
#define RPL_NOWAWAY		"306"
#define RPL_WHOISUSER		"311"
#define RPL_WHOISSERVER		"312"
#define RPL_WHOISOPERATOR	"313"
#define RPL_WHOWASUSER		"314"
#define RPL_ENDOFWHO		"315"
#define RPL_WHOISIDLE		"317"
#define RPL_ENDOFWHOIS		"318"
#define RPL_WHOISCHANNELS	"319"
#define RPL_LISTSTART		"321"
#define RPL_LIST		"322"
#define RPL_LISTEND		"323"
#define RPL_CHANNELMODEIS	"324"
#define RPL_NOTOPIC		"331"
#define RPL_TOPIC		"332"
#define RPL_INVITING		"341"
#define RPL_SUMMONING		"342"
#define RPL_VERSION		"351"
#define RPL_WHOREPLY		"352"
#define RPL_NAMREPLY		"353"
#define RPL_LINKS		"364"
#define RPL_ENDOFLINKS		"365"
#define RPL_ENDOFNAMES		"366"
#define RPL_BANLIST		"367"
#define RPL_ENDOFBANLIST	"368"
#define RPL_ENDOFWHOWAS		"369"
#define RPL_INFO		"371"
#define RPL_MOTD		"372"
#define RPL_ENDOFINFO		"374"
#define RPL_MOTDSTART		"375"
#define RPL_ENDOFMOTD		"376"
#define RPL_YOUREOPER		"381"
#define RPL_REHASHING		"382"
#define RPL_TIME		"391"
#define RPL_USERSSTART		"392"
#define RPL_USERS		"393"
#define RPL_ENDOFUSERS		"394"
#define RPL_NOUSERS		"395"

/* IRC error codes */
#define ERR_NOSUCHNICK		"401"
#define ERR_NOSUCHSERVER	"402"
#define ERR_NOSUCHCHANNEL	"403"
#define ERR_CANNOTSENDTOCHAN	"404"
#define ERR_TOOMANYCHANNELS	"405"
#define ERR_WASNOSUCHNICK	"406"
#define ERR_TOOMANYTARGETS	"407"
#define ERR_NOORIGIN		"409"
#define ERR_NORECIPIENT		"411"
#define ERR_NOTEXTTOSEND	"412"
#define ERR_NOTOPLEVEL		"413"
#define ERR_WILDTOPLEVEL	"414"
#define ERR_UNKNOWNCOMMAND	"421"
#define ERR_NOMOTD		"422"
#define ERR_NOADMININFO		"423"
#define ERR_FILEERROR		"424"
#define ERR_NONICKNAMEGIVEN	"431"
#define ERR_ERRONEUSNICKNAME	"432"
#define ERR_NICKNAMEINUSE	"433"
#define ERR_NICKCOLLISION	"436"
#define ERR_USERNOTINCHANNEL	"441"
#define ERR_NOTONCHANNEL	"442"
#define ERR_USERONCHANNEL	"443"
#define ERR_NOLOGIN		"444"
#define ERR_SUMMONDISABLED	"445"
#define ERR_USERSDISABLED	"446"
#define ERR_NOTREGISTERED	"451"
#define ERR_NEEDMOREPARAMS	"461"
#define ERR_ALREADYREGISTRED	"462"
#define ERR_NOPERMFORHOST	"463"
#define ERR_PASSWDMISMATCH	"464"
#define ERR_YOUREBANNEDCREEP	"465"
#define ERR_KEYSET		"467"
#define ERR_CHANNELISFULL	"471"
#define ERR_UNKNOWNMODE		"472"
#define ERR_INVITEONLYCHAN	"473"
#define ERR_BANNEDFROMCHAN	"474"
#define ERR_BADCHANNELKEY	"475"
#define ERR_NOPRIVILEGES	"481"
#define ERR_CHANOPRIVSNEEDED	"482"
#define ERR_CANTKILLSERVER	"483"
#define ERR_NOOPERHOST		"491"
#define ERR_UMODEUNKNOWNFLAG	"501"
#define ERR_USERSDONTMATCH	"502"

/* prototypes */
void	irc_cmd_join(struct evbuffer *out, char *channel);
void	irc_cmd_nick(struct evbuffer *out, char *nick);
void	irc_check_invite(struct evbuffer *out, char *str);
void	irc_check_kick(struct evbuffer *out, char *str);
void	irc_cmd_notice(struct evbuffer *out, char *dest, char *message);
void	irc_cmd_part(struct evbuffer *out, char *str);
void	irc_cmd_pass(struct evbuffer *out);
void	irc_cmd_ping(struct evbuffer *out, char *target);
void	irc_cmd_pong(struct evbuffer *out, char *str);
void	irc_cmd_privmsg(struct evbuffer *out, char *dest, char *message);
void	irc_cmd_quit(struct evbuffer *out, char *message);
void	irc_cmd_user(struct evbuffer *out, char *user);
void	irc_got_banned(struct evbuffer *out, char *str);
void	irc_parse(struct evbuffer *out, struct evbuffer *in);
void	irc_send_join(struct evbuffer *out, char *channel);
void	irc_send_helo(struct evbuffer *out);
void	irc_send_raw(struct evbuffer *out, char *str);
void	irc_send_raw_vprintf(struct evbuffer *out, char *fmt, ...);
void	irc_send_sync(struct evbuffer *out, char *str);
void	irc_switch(struct evbuffer *out, char *str);
void	irc_wait_invite(struct evbuffer *out, char *str);
void	irc_wait_key(struct evbuffer *out, char *str);

/* structures */
typedef struct s_message {
	const char	*cmd;
	void		(*fp)(struct evbuffer *out, char *str);
}		t_message;

#endif
