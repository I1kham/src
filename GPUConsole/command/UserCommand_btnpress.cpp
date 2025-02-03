#include "UserCommand_btnpress.h"

//**********************************************************
const char*	UserCommand_btnpress::getExplain() const
{ 
	return "btnpress [btnnum]\nbtnnum >= 0 && btnnum <=48";
}

//**********************************************************
void UserCommand_btnpress::handle(const char *command, rhea::IProtocolChannell *ch, rhea::IProtocol *proto, WinTerminal *log, rhea::app::FileTransfer *ftransf) const
{
	rhea::string::utf8::Iter src, temp;
	src.setup((const u8*)command);

	const rhea::UTF8Char cSpace(" ");
	if (!rhea::string::utf8::advanceUntil(src, &cSpace, 1))
	{
		log->log("syntax error, expecting parameter [btnnum]\n");
		return;
	}
	rhea::string::utf8::toNextValidChar(src);

	i32 btnNum = 0;
	if (!rhea::string::utf8::extractInteger(src, &btnNum))
	{
		log->log("syntax error, expecting integer parameter [btnnum]\n");
		return;
	}

	if (btnNum >= 0 && btnNum <= 48)
	{
		rhea::app::SendButton::ask(ch, proto, (u8)btnNum);
		log->log("request sent\n");
	}
	else
		log->log("syntax error, invalid [btnnum]. Good values are between 0 and 48\n");
}