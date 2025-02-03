#include "rheaApp.h"
#include "rheaAppEnumAndDefine.h"
#include "../rheaCommonLib/rheaUtils.h"
#include "../rheaCommonLib/rheaNetBufferView.h"
#include "../rheaCommonLib//rheaStaticBuffer.h"
#include "../SocketBridge/SocketBridgeFileTEnumAndDefine.h"

namespace rhea
{
	namespace app
	{
		NullLogger nullLogger;
	}
}

using namespace rhea;

/*****************************************************************
 * In input, [in_out_bufferLength] deve contenere la dimensione di[out_buffer]
 * Ritorna:
 * false in caso di problemi.In questo caso, [in_out_bufferLength] contiene la dimensione minima necessaria per[out_buffer]
 *
 * Se ritorna true, allora[out_buffer] è stato fillato con i dati necessari per inviare l'evento a SocketBridge.
 * [in_out_bufferLength] contiene il num di bytes messi in[out_buffer];
 */
bool priv_prepareCommandBuffer (socketbridge::eOpcode commandChar, u8 requestID, const void *optionalData, u16 lenOfOptionalData, u8 *out_buffer, u16 *in_out_bufferLength)
{
	const u16 bytesNeeded =

		1 +					//# 
		1 +					//char del comando
		1 +					//requestID 
		2 +					//lenOfOptionalData espressa come byte alto, byte basso
		lenOfOptionalData +	//[optionalData]
		2;					//cheksum espressa come byte alto, byte basso

	if (*in_out_bufferLength < bytesNeeded)
	{
		DBGBREAK;
		*in_out_bufferLength = bytesNeeded;
		return false;
	}

	//fillo il buffer
	u16 ct = 0;
	out_buffer[ct++] = 35; //'#'
	out_buffer[ct++] = (u8)commandChar;
	out_buffer[ct++] = requestID;
	out_buffer[ct++] = ((lenOfOptionalData & 0xFF00) >> 8);
	out_buffer[ct++] = (lenOfOptionalData & 0x00FF);

	if (lenOfOptionalData)
	{
		memcpy(&out_buffer[ct], optionalData, lenOfOptionalData);
		ct += lenOfOptionalData;
	}

	//checksum
	const u16 ck = rhea::utils::simpleChecksum16_calc(out_buffer, ct);
	out_buffer[ct++] = ((ck & 0xFF00) >> 8);
	out_buffer[ct++] = (ck & 0x00FF);

	*in_out_bufferLength = ct;
	return true;
}



//*****************************************************************
void priv_event_sendToSocketBridge (rhea::IProtocolChannell *ch, rhea::IProtocol *proto, const void *optionalData, u8 sizeoOfOptionalData)
{
	u8	sendBuffer[128]; //128 bytes dovrebbero essere più che suff per il 99% delle richieste che le app inviano a socketbridge
	u16 sizeOSendfBuffer = sizeof(sendBuffer);

	if (priv_prepareCommandBuffer(socketbridge::eOpcode::event_E, 0xff, optionalData, sizeoOfOptionalData, sendBuffer, &sizeOSendfBuffer))
	{
		proto->write(ch, sendBuffer, sizeOSendfBuffer, 1000);
		return;
	}

	sizeOSendfBuffer += 4;

	rhea::Allocator *allocator = rhea::getScrapAllocator();
	u8 *temp = (u8*)RHEAALLOC(allocator, sizeOSendfBuffer);
	priv_prepareCommandBuffer(socketbridge::eOpcode::event_E, 0xff, optionalData, sizeoOfOptionalData, temp, &sizeOSendfBuffer);
	proto->write(ch, temp, sizeOSendfBuffer, 1000);
	RHEAFREE(allocator, temp);
}

//*****************************************************************
u16 priv_event_decode (const u8 *buffer, u16 nBytesInBuffer, app::sDecodedEventMsg *out)
{
	//come minimo devono esserci 8 byte
	if (nBytesInBuffer < 8)
		return 0;

	if (buffer[0] == '#' && buffer[1] == 'e' && buffer[2] == 'V' && buffer[3] == 'n')
	{
		out->eventType = (socketbridge::eEventType)buffer[4];
		out->seqNumber = buffer[5];
		out->payloadLen = buffer[6];
		out->payloadLen <<= 8;
		out->payloadLen |= buffer[7];

		if (out->payloadLen)
			out->payload = &buffer[8];
		else
			out->payload = NULL;
		return 8 + out->payloadLen;
	}
	
	return 0;
}


//*****************************************************************
u16 priv_RawFileTrans_decodeMsg(const u8 *buffer, u16 nBytesInBuffer, app::sDecodedFileTransfMsg *out)
{
	//come minimo devono esserci 8 byte
	if (nBytesInBuffer < 8)
		return 0;

	if (buffer[0] == '#' && buffer[1] == 'f' && buffer[2] == 'T' && buffer[3] == 'r')
	{
		out->payloadLen = buffer[4];
		out->payloadLen <<= 8;
		out->payloadLen |= buffer[5];

		u32 ct = 6;
		if (out->payloadLen)
		{
			out->payload = &buffer[6];
			ct += out->payloadLen;
		}
		else
			out->payload = NULL;
		
		const u16 ck = (((u16)buffer[ct]) << 8) | buffer[ct + 1];
		if (ck != rhea::utils::simpleChecksum16_calc(buffer, ct))
			return 0;

		return ct+2;
	}

	return 0;
}



//*****************************************************************
bool app::decodeSokBridgeMessage(const u8 *bufferIN, u16 nBytesInBufferIN, sDecodedMsg *out, u16 *out_nBytesConsumed)
{
	if (nBytesInBufferIN < 1)
		return false;

	//il primo char deve essere un #
	u16 i = 0;
	while (i < nBytesInBufferIN)
	{
		if (bufferIN[i] == '#')
		{
			const u8 *buffer = &bufferIN[i];
			u16 nBytesInBuffer = nBytesInBufferIN - i;
			*out_nBytesConsumed = i;

			//è un messaggio di tipo "evento" ?
			u16 ret = priv_event_decode (buffer, nBytesInBuffer, &out->data.asEvent);
			if (ret)
			{
				out->what = eDecodedMsgType::event;
				*out_nBytesConsumed += ret;
				return true;
			}

			//E' un messaggio di tipo "file transfer" ?
			ret = priv_RawFileTrans_decodeMsg (buffer, nBytesInBuffer, &out->data.asFileTransf);
			if (ret)
			{
				out->what = eDecodedMsgType::fileTransf;
				*out_nBytesConsumed += ret;
				return true;
			}
		}
		i++;
	}

	out->what = eDecodedMsgType::unknown;
	*out_nBytesConsumed = nBytesInBufferIN;
	return false;
}




//*****************************************************************
void app::send_requestIDCodeAfterConnection(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, const socketbridge::SokBridgeClientVer &version)
{
	u8	buffer[32];
	u16 sizeOfBuffer = sizeof(buffer);

	u8 optionalData[4];
	optionalData[0] = (u8)version.apiVersion;
	optionalData[1] = (u8)version.appType;
	optionalData[2] = (u8)version.unused2;
	optionalData[3] = (u8)version.unused3;

	if (priv_prepareCommandBuffer(socketbridge::eOpcode::request_idCode, 0xff, optionalData, sizeof(optionalData), buffer, &sizeOfBuffer))
	{
		proto->write(ch, buffer, sizeOfBuffer, 1000);
		return;
	}

	DBGBREAK;
}

//*****************************************************************
void app::send_identifyAfterConnection (rhea::IProtocolChannell *ch, rhea::IProtocol *proto, const socketbridge::SokBridgeClientVer &version, socketbridge::SokBridgeIDCode &idCode)
{
	u8	buffer[32];
	u16 sizeOfBuffer = sizeof(buffer);

	u8 optionalData[8];
	optionalData[0] = (u8)version.apiVersion;
	optionalData[1] = (u8)version.appType;
	optionalData[2] = (u8)version.unused2;
	optionalData[3] = (u8)version.unused3;
	optionalData[4] = (u8)idCode.data.buffer[0];
	optionalData[5] = (u8)idCode.data.buffer[1];
	optionalData[6] = (u8)idCode.data.buffer[2];
	optionalData[7] = (u8)idCode.data.buffer[3];

	if (priv_prepareCommandBuffer(socketbridge::eOpcode::identify_W, 0xff, optionalData, sizeof(optionalData), buffer, &sizeOfBuffer))
	{
		proto->write(ch, buffer, sizeOfBuffer, 1000);
		return;
	}

	DBGBREAK;
}



//*****************************************************************
bool app::handleInitialRegistrationToSocketBridge (rhea::ISimpleLogger *logIN, rhea::IProtocolChannell *ch, rhea::IProtocol *proto, rhea::LinearBuffer &bufferR, const socketbridge::SokBridgeClientVer &version, socketbridge::SokBridgeIDCode *out_idCode, u32 *out_SMUversion)
{
	rhea::ISimpleLogger *log = logIN;
	if (NULL == log)
		log = &nullLogger;


	log->log("handleInitialRegistrationToSocketBridge\n");
	log->incIndent();

	log->log("requesting new IDCode...\n");
	log->incIndent();
	{
		app::send_requestIDCodeAfterConnection(ch, proto, version);
		u16 nRead = proto->read(ch, 2000, bufferR);
		if (nRead >= rhea::protocol::RES_ERROR)
		{
			log->log("error reading from protocol. Err = %d\n", nRead);
			log->decIndent();
			log->decIndent();
			return false;
		}

		app::sDecodedMsg decoded;
		u8 *buffer = bufferR._getPointer(0);
		u16 nBytesConsumed = 0;
		u16 nUsed = rhea::app::decodeSokBridgeMessage(buffer, nRead, &decoded, &nBytesConsumed);
		if (nUsed > 0)
		{
			if (decoded.what != eDecodedMsgType::event)
			{
				log->log("invalid message rcv. Was expecting an event\n");
				log->decIndent();
				log->decIndent();
				return false;
			}

			app::sDecodedEventMsg *decEvent = &decoded.data.asEvent;
			if (decEvent->payloadLen != 6 || decEvent->eventType != socketbridge::eEventType::answer_to_idCodeRequest)
			{
				log->log("invalid message rcv. payload len is [%d], expect was 6. Eventtype is[%d], expected was %d\n", decEvent->payloadLen, decEvent->eventType, socketbridge::eEventType::answer_to_idCodeRequest);
				log->decIndent();
				log->decIndent();
				return false;
			}
			*out_SMUversion = ((u32)decEvent->payload[0]) << 8; //cpuBridge Version
			*out_SMUversion |= decEvent->payload[1]; //socketBridge Version

			out_idCode->data.buffer[0] = decEvent->payload[2];
			out_idCode->data.buffer[1] = decEvent->payload[3];
			out_idCode->data.buffer[2] = decEvent->payload[4];
			out_idCode->data.buffer[3] = decEvent->payload[5];
			log->log("rcv idCode [0x%08X]\n", out_idCode->data.asU32);
		}
	}
	log->decIndent();


	log->log("sending idCode...\n");
	log->incIndent();
	{
		app::send_identifyAfterConnection (ch, proto, version, *out_idCode);
		log->log("done\n");
	}
	log->decIndent();

	log->decIndent();
	return true;
}






/*****************************************************************
 * CurrentSelectionRunningStatus
 */
void app::CurrentSelectionRunningStatus::decodeAnswer (const sDecodedEventMsg &msg, cpubridge::eRunningSelStatus *out)
{
	assert(msg.eventType == socketbridge::eEventType::selectionRequestStatus);
	assert(msg.payloadLen >= 1);
	*out = (cpubridge::eRunningSelStatus)msg.payload[0];
}


/****************************************************************
 * CurrentCredit
 */
void app::CurrentCredit::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::creditUpdated;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentCredit::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_creditoInFormatoStringa, u32 sizeOfOut)
{
	assert(msg.eventType == socketbridge::eEventType::creditUpdated);
	assert(msg.payloadLen >= 8);
	
	u32 n = msg.payloadLen;
	if (n > sizeOfOut - 1)
		n = sizeOfOut - 1;
	memcpy(out_creditoInFormatoStringa, msg.payload, n);
	out_creditoInFormatoStringa[n] = 0;

}


/****************************************************************
 * CurrentCPUMessage
 */
void app::CurrentCPUMessage::ask (rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::cpuMessage;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentCPUMessage::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_msgImportanceLevel, u16 *out_msgLenInBytes, u8 *out_messageInUTF8, u32 sizeOfOutMessage)
{
	assert(msg.eventType == socketbridge::eEventType::cpuMessage);
	assert(msg.payloadLen >= 2);
	*out_msgImportanceLevel = msg.payload[0];
	*out_msgLenInBytes = (((u16)msg.payload[1]) << 8) | msg.payload[2];

	memset(out_messageInUTF8, 0, sizeOfOutMessage);
	if (*out_msgLenInBytes > 0)
	{
		assert(msg.payloadLen == *out_msgLenInBytes + 3);
		const u8 *m = &msg.payload[3];

		u32 ct = 0;
		u32 i = 0;
		while (i < *out_msgLenInBytes)
		{
			u8 byte1 = m[i++];
			u8 byte2 = m[i++];

			out_messageInUTF8[ct++] = byte1;
			if (byte2 != 0x00)
				out_messageInUTF8[ct++] = byte2;
		}
	}
}



/****************************************************************
 * CurrentCPUStatus
 */
void app::CurrentCPUStatus::ask (rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::cpuStatus;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentCPUStatus::decodeAnswer (const sDecodedEventMsg &msg, cpubridge::eVMCState *out_VMCstate, u8 *out_VMCerrorCode, u8 *out_VMCerrorType, u16 *out_flag1)
{
	assert(msg.eventType == socketbridge::eEventType::cpuStatus);
	assert(msg.payloadLen >= 5);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

	u8 u;
	nbr.readU8(u);
	*out_VMCstate = (cpubridge::eVMCState)u;
	nbr.readU8(*out_VMCerrorCode);
	nbr.readU8(*out_VMCerrorType);
	nbr.readU16(*out_flag1);
}



/****************************************************************
 * ClientList
 */
void app::ClientList::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::reqClientList;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::ClientList::decodeAnswer (const sDecodedEventMsg &msg, rhea::Allocator *allocator,u16 *out_nClientConnected, u16 *out_nClientInfo, rhea::DateTime *out_dtCPUBridgeStarted, socketbridge::sIdentifiedClientInfo **out_list)
{
	assert(msg.eventType == socketbridge::eEventType::reqClientList);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

    u16 nClient = 0, nClientInfo = 0;
    u64 u = 0;
	nbr.readU16(nClient);
	nbr.readU16(nClientInfo);
	nbr.readU64(u);
	out_dtCPUBridgeStarted->setFromInternalRappresentation(u);

	if (nClient == 0)
	{
		*out_nClientConnected = *out_nClientInfo = 0;
		*out_list = NULL;
		return;
	}
	
	*out_nClientConnected = nClient;
	*out_nClientInfo = nClientInfo;
	*out_list = (socketbridge::sIdentifiedClientInfo*)RHEAALLOC(allocator, sizeof(socketbridge::sIdentifiedClientInfo) * nClientInfo);
	for (u32 i = 0; i < nClientInfo; i++)
	{
		socketbridge::sIdentifiedClientInfo *info = &(*out_list)[i];

		nbr.readU16(info->handle.index);
		nbr.readU32(info->currentWebSocketHandleAsU32);

		nbr.readU32(info->idCode.data.asU32);
		nbr.readU8(info->clientVer.apiVersion);
		nbr.readU8(info->clientVer.appType);
		nbr.readU8(info->clientVer.unused2);
		nbr.readU8(info->clientVer.unused3);

		nbr.readU64(info->timeCreatedMSec);
		nbr.readU64(info->lastTimeRcvMSec);
	}

}


/*****************************************************************
 * CurrentSelectionAvailability
 */
void app::CurrentSelectionAvailability::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::selectionAvailabilityUpdated;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentSelectionAvailability::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_numSel, u8 *out_selectionAvailability, u32 sizeOfSelecionAvailability)
{
	assert(msg.eventType == socketbridge::eEventType::selectionAvailabilityUpdated);
	assert(msg.payloadLen > 1);
	
	u8 n = *out_numSel = msg.payload[0];

	//serve 1 byte per ogni selezione
	if (n > sizeOfSelecionAvailability)
		n = sizeOfSelecionAvailability;
	for (u8 i = 0; i < n; i++)
		out_selectionAvailability[i] = msg.payload[1 + i];
}



/*****************************************************************
 * ButtonProgPressed
 */
void app::ButtonProgPressed::decodeAnswer(const sDecodedEventMsg &msg)
{
	assert(msg.eventType == socketbridge::eEventType::btnProgPressed);
}

/*****************************************************************
 * namespace CurrentCPUInitParam
 */
void app::CurrentCPUInitParam::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::reqIniParam;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentCPUInitParam::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::sCPUParamIniziali *out)
{
	assert(msg.eventType == socketbridge::eEventType::reqIniParam);

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

	nbr.readBlob(out->CPU_version, sizeof(out->CPU_version));
	nbr.readU8(out->protocol_version);
	for (u8 i = 0; i < 48; i++)
		nbr.readU16(out->pricesAsInAnswerToCommandC[i]);
}

/*****************************************************************
 * namespace ReadDataAudit
 */
void app::ReadDataAudit::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::reqDataAudit;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::ReadDataAudit::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::eReadDataFileStatus *status, u16 *totKbSoFar, u16 *out_fileID)
{
	assert(msg.eventType == socketbridge::eEventType::reqDataAudit);
	assert(msg.payloadLen >= 5);

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

    u16 u = 0;
	nbr.readU16(u); *out_fileID = u;
	nbr.readU16(u); *totKbSoFar = u;

    u8 b = 0;
	nbr.readU8(b); *status = (cpubridge::eReadDataFileStatus)b;
}

/*****************************************************************
 * namespace ReadVMCDataFile
 */
void app::ReadVMCDataFile::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::reqVMCDataFile;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::ReadVMCDataFile::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::eReadDataFileStatus *status, u16 *totKbSoFar, u16 *out_fileID)
{
	assert(msg.eventType == socketbridge::eEventType::reqVMCDataFile);
	assert(msg.payloadLen >= 5);

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

    u16 u = 0;
	nbr.readU16(u); *out_fileID = u;
	nbr.readU16(u); *totKbSoFar = u;

    u8 b = 0;
	nbr.readU8(b); *status = (cpubridge::eReadDataFileStatus)b;
}

/*****************************************************************
 * namespace WriteLocalVMCDataFile
 */
void app::WriteLocalVMCDataFile::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, const char *localFilePathAndName)
{
	u8 optionalData[256];
	optionalData[0] = (u8)socketbridge::eEventType::reqWriteLocalVMCDataFile;
	u32 n = strlen(localFilePathAndName);
	memcpy(&optionalData[1], localFilePathAndName, n+1);
	priv_event_sendToSocketBridge(ch, proto, optionalData, n+2);
}

void app::WriteLocalVMCDataFile::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::eWriteDataFileStatus *status, u16 *totKbSoFar)
{
	assert(msg.eventType == socketbridge::eEventType::reqWriteLocalVMCDataFile);
	assert(msg.payloadLen >= 3);

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

    u16 u = 0;
	nbr.readU16(u); *totKbSoFar = u;

    u8 b = 0;
	nbr.readU8(b); *status = (cpubridge::eWriteDataFileStatus)b;
}



/*****************************************************************
 * namespace CurrentVMCDataFileTimestamp
 */
void app::CurrentVMCDataFileTimestamp::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::reqVMCDataFileTimestamp;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::CurrentVMCDataFileTimestamp::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::sCPUVMCDataFileTimeStamp *out)
{
	assert(msg.eventType == socketbridge::eEventType::reqVMCDataFileTimestamp);
	assert(msg.payloadLen >= out->getLenInBytes());
	out->readFromBuffer(msg.payload);
}


/*****************************************************************
 * namespace ExecuteSelection
 */
void app::ExecuteSelection::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 selNum)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::startSelection;
	optionalData[1] = selNum;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 2);
}


/*****************************************************************
 * namespace ExecuteProgramCmd
 */
void app::ExecuteProgramCmd::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, cpubridge::eCPUProgrammingCommand cmd, u8 param1, u8 param2, u8 param3, u8 param4)
{
	u8 optionalData[8];
	optionalData[0] = (u8)socketbridge::eEventType::cpuProgrammingCmd;
	optionalData[1] = (u8)cmd;
	optionalData[2] = param1;
	optionalData[3] = param2;
	optionalData[4] = param3;
	optionalData[5] = param4;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 6);
}

/*****************************************************************
 * namespace ExecuteProgramCmd
 */
void app::SanWashingStatus::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::cpuSanWashingStatus;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::SanWashingStatus::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_b0, u8 *out_b1, u8 *out_b2)
{
	assert(msg.eventType == socketbridge::eEventType::cpuSanWashingStatus);
	*out_b0 = msg.payload[0];
	*out_b1 = msg.payload[1];
	*out_b2 = msg.payload[2];
}

/*****************************************************************
 * namespace SendButton
 */
void app::SendButton::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 btnNum)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::cpuBtnPressed;
	optionalData[1] = btnNum;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 2);
}

/*****************************************************************
 * namespace WritePartialVMCDataFile
 */
void app::WritePartialVMCDataFile::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_blockWritten)
{
	assert(msg.eventType == socketbridge::eEventType::cpuWritePartialVMCDataFile);
	*out_blockWritten = msg.payload[0];
}

	
/*****************************************************************
 * namespace ExtendedConfigInfo
 */
void app::ExtendedConfigInfo::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::cpuExtendedConfigInfo;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::ExtendedConfigInfo::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::sExtendedCPUInfo *out)
{
	assert(msg.eventType == socketbridge::eEventType::cpuExtendedConfigInfo);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

	u8 u;
	nbr.readU8(out->msgVersion);
	nbr.readU8(u);	out->machineType = (cpubridge::eCPUMachineType)u;
	nbr.readU8(out->machineModel);
	nbr.readU8(out->isInduzione);
	nbr.readU8(u);	out->tipoGruppoCaffe = (cpubridge::eCPUGruppoCaffe)u;
}


/*****************************************************************
 * namespace SetDecounter
 */
void app::SetDecounter::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, cpubridge::eCPUProg_decounter which, u16 value)
{
	u8 optionalData[4];
	NetStaticBufferViewW nbw;
	nbw.setup(optionalData, sizeof(optionalData), rhea::eEndianess::eBigEndian);
	nbw.writeU8((u8)socketbridge::eEventType::setDecounter);
	nbw.writeU8((u8)which);
	nbw.writeU16(value);
	priv_event_sendToSocketBridge(ch, proto, optionalData, 4);
}

void app::SetDecounter::decodeAnswer(const sDecodedEventMsg &msg, cpubridge::eCPUProg_decounter *out_which, u16 *out_value)
{
	assert(msg.eventType == socketbridge::eEventType::setDecounter);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

    u8 u = 0;
	nbr.readU8(u);	
	*out_which  = (cpubridge::eCPUProg_decounter)u;
	
    u16 uu = 0;
	nbr.readU16(uu);
	*out_value = uu;
}


/*****************************************************************
 * namespace GetAllDecounters
 */
void app::GetAllDecounters::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto)
{
	u8 optionalData[2];
	optionalData[0] = (u8)socketbridge::eEventType::getAllDecounters;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 1);
}

void app::GetAllDecounters::decodeAnswer (const sDecodedEventMsg &msg, u16 *out_arrayDi14valori)
{
	assert(msg.eventType == socketbridge::eEventType::getAllDecounters);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

	for (u8 i = 0; i < 14; i++)
		nbr.readU16(out_arrayDi14valori[i]);
}


/*****************************************************************
 * namespace SetMotoreMacina
 */
void app::SetMotoreMacina::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 macina_1o2, cpubridge::eCPUProg_macinaMove m)
{
	u8 optionalData[4];
	optionalData[0] = (u8)socketbridge::eEventType::setMotoreMacina;
	optionalData[1] = macina_1o2;
	optionalData[2] = (u8)m;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 3);
}
void app::SetMotoreMacina::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_macina_1o2, cpubridge::eCPUProg_macinaMove *out_m)
{
	assert(msg.eventType == socketbridge::eEventType::setMotoreMacina);

	*out_macina_1o2 = msg.payload[0];
	*out_m = (cpubridge::eCPUProg_macinaMove)msg.payload[1];
}

/*****************************************************************
 * namespace GetAperturaVGrind
 */
void app::GetAperturaVGrind::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 macina_1o2)
{
	u8 optionalData[4];
    optionalData[0] = (u8)socketbridge::eEventType::getAperturaVGrind;
	optionalData[1] = macina_1o2;
	priv_event_sendToSocketBridge(ch, proto, optionalData, 2);
}
void app::GetAperturaVGrind::decodeAnswer(const sDecodedEventMsg &msg, u8 *out_macina_1o2, u16 *out_pos)
{
    assert(msg.eventType == socketbridge::eEventType::getAperturaVGrind);

	//NetBufferView per poter leggere i dati in maniera "indian indipendent"
	NetStaticBufferViewR nbr;
	nbr.setup(msg.payload, msg.payloadLen, rhea::eEndianess::eBigEndian);

	nbr.readU16(*out_pos);
	nbr.readU8(*out_macina_1o2);
}


/*****************************************************************
 * namespace SetAperturaVGrind
 */
void app::SetAperturaVGrind::ask(rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 macina_1o2, u16 target)
{
	u8 optionalData[4];
	NetStaticBufferViewW nbw;
	nbw.setup(optionalData, sizeof(optionalData), rhea::eEndianess::eBigEndian);
    nbw.writeU8((u8)socketbridge::eEventType::setAperturaVGrind);
	nbw.writeU8((u8)macina_1o2);
	nbw.writeU16(target);
	priv_event_sendToSocketBridge(ch, proto, optionalData, 4);
}

/*****************************************************************
 * RawFileTrans
 */
 //*****************************************************************
void app::RawFileTrans::sendToSocketBridge (rhea::IProtocolChannell *ch, rhea::IProtocol *proto, u8 *sendBuffer, u32 sizeOfSendBufferIN, const void *optionalData, u16 sizeoOfOptionalData)
{
	u16 sizeOSendfBuffer = sizeOfSendBufferIN;

	if (priv_prepareCommandBuffer(socketbridge::eOpcode::fileTransfer, 0xfe, optionalData, sizeoOfOptionalData, sendBuffer, &sizeOSendfBuffer))
	{
		proto->write(ch, sendBuffer, sizeOSendfBuffer, 1000);
		return;
	}

	DBGBREAK;
}

