--[[
	global settings
--]]

-- server information
server = "irc.pouet.foo"
port = "7000"
ssl = "no"
-- pass = "secret"
throttling = "no"

-- IRC settings
nick = "iYell"
quit_message = "bye"
--sync_message = "hello there"
sync_notice = "yes"
--username = "irc"
--realname = "No life around here"
--perform = "PRIVMSG ChanServ :IDENTIFY test"

-- channels to join
channels = { "#test" }

-- listener settings
udp_listener = "yes"
udp_port = "25879"
udp_key = "secret"

syslog_listener = "yes"
syslog_port = "5140"
--syslog_show_host = "yes"
--syslog_silent_drop = "yes"

unix_listener = "no"
unix_path = "/tmp/iyell.fifo"

-- privileges 
--allow_direct = "no"
--allow_private_cmd = "no"

-- logging
syslog = "yes"

-- Misc 
--hooks = "yes"
--color = "yes"

--[[
	commands
--]]

cmd = {
	ping = "false",
	stats = "false",
	timestamp = "false",
	uptime = "false"
}

--[[
	forward
--]]

-- types and where forward
forward = {
	info = { "#chan", "#otherchan" },
	warn = { "#alert" }
}

--[[
	repeat
--]]

transmit = {
	--info = "[::1]:4242"
}
