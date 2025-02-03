#include "CmdHandler_ajaxReqGetLastInstalledGUIFilename.h"
#include "../SocketBridge.h"
#include "../../rheaCommonLib/rheaJSONParser.h"
#include "../../rheaDB/SQLInterface.h"

using namespace socketbridge;

//***********************************************************
void CmdHandler_ajaxReqGetLastInstalledGUIFilename::handleRequestFromSocketBridge(socketbridge::Server *server, HSokServerClient &hClient, const u8 *params UNUSED_PARAM)
{
	u8			fullPath[256], fileName[128];
	u32			strLen;
	OSFileFind	h;

	strLen = sprintf_s((char*)fullPath, sizeof(fullPath), "%s/last_installed/gui", rhea::getPhysicalPathToAppFolder());
	rhea::fs::sanitizePathInPlace(fullPath, strLen);

	memset(fileName, 0, sizeof fileName);

	if (rhea::fs::findFirst(&h, fullPath, (const u8*)"*.rheagui"))
	{
		bool	exitLoop = false;

		do
		{
			if (!rhea::fs::findIsDirectory(h))
			{
				const u8 *folderName = rhea::fs::findGetFileName(h);
				if (folderName[0] != '.')
				{
					sprintf_s((char*)fileName, sizeof fileName, "%s", folderName);
					exitLoop = true;
				}
			}
		} while (false == exitLoop && rhea::fs::findNext(h));
		rhea::fs::findClose(h);
	}

	if (fileName[0] == 0x00)
	{
		fileName[0] = '?';
		fileName[1] = '?';
		fileName[2] = '?';
		fileName[3] = 0x00;
	}

	server->sendAjaxAnwer(hClient, ajaxRequestID, fileName, (u16)rhea::string::utf8::lengthInBytes(fileName));
}
