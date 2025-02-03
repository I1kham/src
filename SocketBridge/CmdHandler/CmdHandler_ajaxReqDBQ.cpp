#include "CmdHandler_ajaxReqDBQ.h"
#include "../SocketBridge.h"
#include "../../rheaCommonLib/rheaJSONParser.h"
#include "../../rheaDB/SQLInterface.h"


using namespace socketbridge;


struct sInput
{
	u16		dbHandle;
	u8		sql[4096];
};

//***********************************************************
bool ajaxReqDBQ_jsonTrapFunction(const u8 *fieldName, const u8 *fieldValue, void *userValue)
{
	sInput *input = (sInput*)userValue;

	if (strcasecmp((const char*)fieldName, "h") == 0)
	{
		const u32 h = rhea::string::utf8::toU32(fieldValue);
		if (h > 0 && h < 0xffff)
			input->dbHandle = (u16)h;
		else
			input->dbHandle = 0;
	}
	else if (strcasecmp((const char*)fieldName, "sql") == 0)
	{
		assert(rhea::string::utf8::lengthInBytes(fieldValue) < sizeof(input->sql));
		rhea::string::utf8::copyStr (input->sql, sizeof(input->sql) - 1, fieldValue);
		return false;
	}

	return true;
}

//***********************************************************
void CmdHandler_ajaxReqDBQ::handleRequestFromSocketBridge(socketbridge::Server *server, HSokServerClient &hClient, const u8 *params)
{
	const rhea::SQLRst *rst = NULL;
	sInput data;

	if (rhea::json::parse(params, ajaxReqDBQ_jsonTrapFunction, &data))
		rst = server->DB_q(data.dbHandle, data.sql);
	

	if (NULL == rst)
	{
		char resp[64];
		sprintf(resp, "{\"data\":\"KO\"}");
		server->sendAjaxAnwer(hClient, ajaxRequestID, (const u8*)resp, (u16)strlen(resp));
	}
	else
	{
		rhea::Allocator *allocator = rhea::getScrapAllocator();

		const u32 sizeOfBuffer = rst->blob_calcMemoryNeeded();
		u8 *buffer = (u8*)RHEAALLOC(allocator, sizeOfBuffer);
		const u32 nBytesInBlob = rst->blob_copyToString(buffer, sizeOfBuffer);
		
		server->sendAjaxAnwer (hClient, ajaxRequestID, buffer, nBytesInBlob);

		RHEAFREE(allocator, buffer);
	}
}
