#include "SocketBridgeFileT.h"
#include "SocketBridgeServer.h"
#include "../rheaCommonLib/rheaUtils.h"

using namespace socketbridge;


//***************************************************
FileTransfer::FileTransfer()
{
	localAllocator = NULL;
	logger = NULL;
	bufferW = NULL;
}

//***************************************************
void FileTransfer::setup (rhea::Allocator *allocator, rhea::ISimpleLogger *loggerIN)
{
	localAllocator = allocator;
	logger = loggerIN;
	bufferW = (u8*)RHEAALLOC(localAllocator, SIZE_OF_BUFFERW);
	activeTransferList.setup(localAllocator, 32);
}

//***************************************************
void FileTransfer::unsetup()
{
	if (NULL == localAllocator)
		return;

	u32 n = activeTransferList.getNElem();
	for (u32 i = 0; i < n; i++)
		priv_freeResources(i);
	activeTransferList.unsetup();

	RHEAFREE(localAllocator, bufferW);
	localAllocator = NULL;
}


//***************************************************
void FileTransfer::priv_send (Server *server, const HSokServerClient &h, const u8 *what, u16 sizeofwhat)
{
	assert(SIZE_OF_BUFFERW >= 8 + sizeofwhat);
	u32 ct = 0;
	bufferW[ct++] = '#'; 
	bufferW[ct++] = 'f';
	bufferW[ct++] = 'T';
	bufferW[ct++] = 'r';
	bufferW[ct++] = ((sizeofwhat & 0xFF00) >> 8);
	bufferW[ct++] =  (sizeofwhat & 0x00FF);
	
	if (sizeofwhat)
	{
		memcpy(&bufferW[ct], what, sizeofwhat);
		ct += sizeofwhat;
	}

	const u16 ck = rhea::utils::simpleChecksum16_calc(bufferW, ct);
	bufferW[ct++] = ((ck & 0xFF00) >> 8);
	bufferW[ct++] = (ck & 0x00FF);

	server->sendTo(h, bufferW, ct);
}

//***************************************************
u32 FileTransfer::priv_generateUID() const
{
	u32 ret;
	bool bQuit = false;
	while (bQuit == false)
	{
		const u32 a = 1 + rhea::randomU32(254);
		const u32 b = 1 + rhea::randomU32(254);
		const u32 c = 1 + rhea::randomU32(254);
		const u32 d = 1 + rhea::randomU32(254);
		ret = (a << 24) | (b << 16) | (c << 8) | d;

		bQuit = true;
		for (u32 i = 0; i < activeTransferList.getNElem(); i++)
		{
			if (activeTransferList(i).smuFileTransfUID == ret)
			{
				bQuit = false;
				break;
			}
		}
	}
	return ret;
}

//***************************************************
u32 FileTransfer::priv_findActiveTransferBySMUUID(u32 smuFileTransfUID) const
{
	const u32 n = activeTransferList.getNElem();
	for (u32 i = 0; i < n; i++)
	{
		if (activeTransferList(i).smuFileTransfUID == smuFileTransfUID)
			return i;
	}
	return u32MAX;
}

//***************************************************
void FileTransfer::priv_freeResources(u32 i)
{
	if (NULL != activeTransferList[i].f)
	{
        rhea::fs::fileClose (activeTransferList[i].f);
		activeTransferList[i].f = NULL;
	}

	if (activeTransferList[i].isAppUploading)
	{
		//app stava uppando
	}
	else
	{
		//app stava downloadando
		sWhenAPPisDownloading *s = &activeTransferList[i].other.whenAPPisDownloading;
		if (NULL != s->sendBuffer)
		{
			RHEAFREE(localAllocator, s->sendBuffer);
			s->sendBuffer = NULL;
		}
	}
}

//***************************************************
void FileTransfer::update(u64 timeNowMSec)
{
	u32 n = activeTransferList.getNElem();
	for (u32 i = 0; i < n; i++)
	{
		switch (activeTransferList(i).status)
		{
		case eTransferStatus::finished_OK:
			logger->log("Transfer [0x%08X] finished OK\n", activeTransferList(i).smuFileTransfUID);
			priv_freeResources(i);
			activeTransferList.removeAndSwapWithLast(i);
			--i;
			--n;
			break;

		case eTransferStatus::finished_KO:
			logger->log("Transfer [0x%08X] finished KO\n", activeTransferList(i).smuFileTransfUID);
			priv_freeResources(i);
			activeTransferList.removeAndSwapWithLast(i);
			--i;
			--n;
			break;

		case eTransferStatus::pending:
			if (timeNowMSec > activeTransferList(i).timeoutMSec)
			{
				activeTransferList[i].status = eTransferStatus::finished_KO;
				logger->log("Transfer [0x%08X] time out\n", activeTransferList(i).smuFileTransfUID);
			}
			break;
		}
	}
}

//***************************************************
void FileTransfer::handleMsg (Server *server, const HSokServerClient &h, socketbridge::sDecodedMessage &decoded, u64 timeNowMSec)
{
	if (decoded.payloadLen == 0)
	{
		DBGBREAK;
		return;
	}

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(decoded.payload, decoded.payloadLen, rhea::eEndianess::eBigEndian);

	u8 opcode;
	nbr.readU8(opcode);
	switch ((eFileTransferOpcode)opcode)
	{
	default:
		logger->log("ERR: FileTransfer::handleMsg() unkwnown opcode [0x%02X]\n", opcode);
		return;

	case eFileTransferOpcode::upload_request_fromApp:
		//qualcuno vuole iniziare un upload verso di me
		priv_on0x01 (server, h, nbr, timeNowMSec);
		break;

	case eFileTransferOpcode::upload_request_fromApp_packet:
		priv_on0x03(server, h, nbr, timeNowMSec);
		break;

	case eFileTransferOpcode::download_request_fromApp:
		priv_on0x51(server, h, nbr, timeNowMSec);
		break;

	case eFileTransferOpcode::download_request_fromApp_packet_answ:
		priv_on0x54(server, h, nbr, timeNowMSec);
		break;

	}
}

//**********************************************************************
void FileTransfer::priv_sendChunkOfPackets (Server *server, const HSokServerClient &h, sActiveUploadRequest *s, u16 nPacket)
{
	if (s->status != eTransferStatus::pending)
		return;
	if (s->isAppUploading != 0)
	{
		DBGBREAK;
		return;
	}

	if (s->packetSize > SIZE_OF_BUFFERW)
	{
		DBGBREAK;
		s->timeoutMSec = 0;
		return;
	}

	rhea::NetStaticBufferViewW nbw;
	u8 *serializedDataBuffer = s->other.whenAPPisDownloading.sendBuffer;
	nbw.setup (serializedDataBuffer, s->other.whenAPPisDownloading.sizeOfSendBuffer, rhea::eEndianess::eBigEndian);

	u32 iPacket = s->other.whenAPPisDownloading.nextPacketToSend;
	u32 fileOffset = iPacket * s->packetSize;
	u32 nBytesLeft = s->totalFileSizeInBytes - fileOffset;

	u8 chunkSeq = 1;
	fseek(s->f, fileOffset, SEEK_SET);
	while (nPacket-- && nBytesLeft)
	{
		u16 nByteToRead = s->packetSize;
		if (nBytesLeft < nByteToRead)
		{
			nByteToRead = (u16)nBytesLeft;
			nBytesLeft = 0;
		}
		else
			nBytesLeft -= nByteToRead;
		
		nbw.seek(0, rhea::eSeek::start);
		nbw.writeU8((u8)socketbridge::eFileTransferOpcode::download_request_fromApp_packet);
		nbw.writeU32(s->appFileTransfUID);
		nbw.writeU32(iPacket);
		nbw.writeU8(chunkSeq);
        rhea::fs::fileRead (s->f, &serializedDataBuffer[nbw.length()], nByteToRead);

		//logger->log("FT => sending packet [%d]\n", s->packetNum);
		priv_send (server, h, serializedDataBuffer, nbw.length() + nByteToRead);

		iPacket++;
		chunkSeq++;
	}
}

/***************************************************
 * qualcuno vuole iniziare un upload verso di me
 */
void FileTransfer::priv_on0x01 (Server *server, const HSokServerClient &h, rhea::NetStaticBufferViewR &nbr, u64 timeNowMSec)
{
	//decodifica e validazione dei dati ricevuti
	fileT::sData0x01 data;
	if (!data.decode(nbr))
	{
		DBGBREAK;
		return;
	}


	//preparo la risposta
	fileT::sData0x02 answ;
	answ.reason_refused = 0;
	answ.packetSizeInBytes = 0;
	answ.smuTransfUID = 0;
	answ.appTransfUID = data.appTransfUID;


	//ok... qualcuno vuole iniziare un upload verso di me
	//verifichiamo che il suo "usage" sia lecito
	FILE *fDest = NULL;
	if (strcasecmp(data.usage, "test") == 0)
	{
		char s[256];
		sprintf_s(s, sizeof(s), "%s/test.upload", rhea::getPhysicalPathToWritableFolder());
		fDest = fopen(s, "wb");
		if (NULL == fDest)
			answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
	}
	else if (strncmp(data.usage, "saveas:", 7) == 0)
	{
		//Qualcuno vuol uppare un file locale e metterelo in una cartella locale (serve quando per esempio da backoffice
		//quando l'utente vuole caricare una immagine. Dopo [saveas:] mi aspetto una valido pathname locale che � esattamente dove vado a scrivere il
		//file che ricevo
		fDest = fopen(&data.usage[7], "wb");
		if (NULL == fDest)
		{
			answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
			logger->log("ERR: FileTransfer::priv_on_Opcode_upload_request_fromApp() error opening file\n", data.usage);
		}
	}
	else if (strncmp(data.usage, "store:", 6) == 0)
	{
		//Qualcuno vuol uppare un file generico. Lo metto nella mia cartella temp
		char s[256];
		sprintf_s(s, sizeof(s), "%s/temp/%s", rhea::getPhysicalPathToAppFolder(), &data.usage[6]);
		fDest = fopen(s, "wb");
		if (NULL == fDest)
		{
			answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
			logger->log("ERR: FileTransfer::priv_on_Opcode_upload_request_fromApp() error opening file\n", data.usage);
		}
	}
	else 
	{
		answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
		logger->log("ERR: FileTransfer::priv_on_Opcode_upload_request_fromApp() invalid usage [%s]\n", data.usage);
	}



	if (NULL != fDest)
	{
		//genero un UID per il trasferimento
		//aggiungo il recordo alla lista dei trasferimenti attivi
		sActiveUploadRequest info;
		info.status = eTransferStatus::pending;
		info.isAppUploading = 1;
		info.f = fDest;
		info.smuFileTransfUID = priv_generateUID();
		info.appFileTransfUID = data.appTransfUID;
		info.timeoutMSec = timeNowMSec + 5000;
		info.nextTimeOutputANotificationMSec = 0;
		info.packetSize = PACKET_SIZE;
		info.numPacketInAChunk = NUM_PACKET_IN_A_CHUNK;
		info.totalFileSizeInBytes = data.fileSizeInBytes;

		info.other.whenAPPisUploading.nextTimeSendNACKMsec = 0;
		info.other.whenAPPisUploading.lastGoodPacket = u32MAX;
		info.other.whenAPPisUploading.numOfPacketToBeRcvInTotal = data.fileSizeInBytes / info.packetSize;
		if (info.other.whenAPPisUploading.numOfPacketToBeRcvInTotal * info.packetSize < data.fileSizeInBytes)
			info.other.whenAPPisUploading.numOfPacketToBeRcvInTotal++;
		

		activeTransferList.append(info);

		//preparo la risposta
		answ.reason_refused = 0;
		answ.smuTransfUID = info.smuFileTransfUID;
		answ.packetSizeInBytes = info.packetSize;
		answ.numPacketInAChunk = info.numPacketInAChunk;

		logger->log("FileTransfer => upload of [%s] accepted, smuUID[0x%08X] appUID[0x%08X]\n", data.usage, answ.smuTransfUID, answ.appTransfUID);
	}

	
	//spedisco
	u8	serializedDataBuffer[128];
	u16 n = answ.encode(serializedDataBuffer, sizeof(serializedDataBuffer));
	priv_send(server, h, serializedDataBuffer, n);
}

//***************************************************
void FileTransfer::priv_on0x03 (Server *server, const HSokServerClient &h, rhea::NetStaticBufferViewR &nbr, u64 timeNowMSec)
{
    u32 smuFileTransfUID = 0, packetNumReceived = 0;
    u8	chunkSeq = 0;
	nbr.readU32(smuFileTransfUID);
	nbr.readU32(packetNumReceived);
	nbr.readU8(chunkSeq);

	//ho ricevuto un pacchetto dati, vediamo se lo riconosco
	u32 index = priv_findActiveTransferBySMUUID(smuFileTransfUID);
	if (u32MAX == index)
	{
		//sconosciuto...
		DBGBREAK;
		return;
	}

	sActiveUploadRequest *req = &activeTransferList.getElem(index);

	if (req->status != eTransferStatus::pending)
	{
		return;
	}

	//aggiorno il timeout per questo tranfert
	req->timeoutMSec = timeNowMSec + 15000;

	//vediamo se il pacchetto ricevuto � coerente
	u32 expectedPacketNum = req->other.whenAPPisUploading.lastGoodPacket + 1;

#ifdef _DEBUG
	//simulo un errore di trasferimento
	if (rhea::random01() < 0.001f)	
	{ 
		logger->log("FileTransfer => [0x%08X] Error simulation\n", req->smuFileTransfUID); 
		expectedPacketNum++; 
	}
#endif


	if (packetNumReceived == expectedPacketNum)
	{
		//bene, era quello buono
		
		
		//aggiorno le info sul trasferimento
		req->other.whenAPPisUploading.lastGoodPacket = packetNumReceived;

		//per non spammare messagi a video, ne butto fuori uno ogni 5 secondi
		if (timeNowMSec > req->nextTimeOutputANotificationMSec)
		{
			logger->log("FileTransfer => [0x%08X] packet [%d]/[%d]\n", req->smuFileTransfUID, 1 + req->other.whenAPPisUploading.lastGoodPacket, req->other.whenAPPisUploading.numOfPacketToBeRcvInTotal);
			req->nextTimeOutputANotificationMSec = timeNowMSec + 5000;
		}

		//era l'ultimo pacchetto?
		if (req->other.whenAPPisUploading.lastGoodPacket + 1 >= req->other.whenAPPisUploading.numOfPacketToBeRcvInTotal)
		{
			u32 lastPacketSize = req->totalFileSizeInBytes - req->other.whenAPPisUploading.lastGoodPacket * req->packetSize;
			nbr.readBlob(bufferW, lastPacketSize);
			fwrite(bufferW, lastPacketSize, 1, req->f);

			req->status = eTransferStatus::finished_OK;
            rhea::fs::fileClose(req->f);
			req->f = NULL;
		}
		else
		{
			nbr.readBlob(bufferW, req->packetSize);
			fwrite(bufferW, req->packetSize, 1, req->f);
		}

		//rispondo confermando la ricezione
		req->other.whenAPPisUploading.nextTimeSendNACKMsec = 0;
		if (chunkSeq == req->numPacketInAChunk || req->status == eTransferStatus::finished_OK)
		{
			fileT::sData0x04 answ;
			answ.appTransfUID = req->appFileTransfUID;
			answ.packetNumAccepted = req->other.whenAPPisUploading.lastGoodPacket;

			//spedisco
			u8	toSendBuffer[128];
			u16 sizeOfToSendBuffer = answ.encode(toSendBuffer, sizeof(toSendBuffer));
			priv_send(server, h, toSendBuffer, sizeOfToSendBuffer);
			//logger->log("FileTransfer => [0x%08X] send ACK for packet[%d]\n", req->smuFileTransfUID, req->lastGoodPacket + 1);
		}
	}
	else
	{
		//mi � arrivato un pacchetto che non era quello che mi aspettato (li voglio in ordine seqeuenziale).
		//Confermo la ricezione per evitare il timeout, ma come "packetNumAccepted" spedisco il mio "last good packet"
		//in modo che dall'altra parte ripartano a spedire pacchetti da dove dico io
		if (timeNowMSec >= req->other.whenAPPisUploading.nextTimeSendNACKMsec)
		{
			req->other.whenAPPisUploading.nextTimeSendNACKMsec = timeNowMSec + 2000; //sta cosa serve per evitare di mandare 1000 NACK di seguito visto che presumibilmente c'�
																					//un intero chunk di pacchetti in arrivo che sto per scartare
			chunkSeq = req->numPacketInAChunk;

			fileT::sData0x04 answ;
			answ.appTransfUID = req->appFileTransfUID;
			answ.packetNumAccepted = req->other.whenAPPisUploading.lastGoodPacket;

			//spedisco
			u8	toSendBuffer[128];
			u16 sizeOfToSendBuffer = answ.encode(toSendBuffer, sizeof(toSendBuffer));
			priv_send(server, h, toSendBuffer, sizeOfToSendBuffer);

			logger->log("FileTransfer => [0x%08X] send NACK for packet[%d]\n", req->smuFileTransfUID, req->other.whenAPPisUploading.lastGoodPacket + 1);
		}
	}
}



/***************************************************
 * qualcuno ha richiesto il download di qualcosa
 */
void FileTransfer::priv_on0x51 (Server *server, const HSokServerClient &h, rhea::NetStaticBufferViewR &nbr, u64 timeNowMSec)
{
	//decodifica e validazione dei dati ricevuti
	fileT::sData0x51 data;
	if (!data.decode(nbr))
	{
		DBGBREAK;
		return;
	}

	//preparo la risposta
	fileT::sData0x52 answ;
	answ.reason_refused = 0;
	answ.packetSizeInBytes = 0;
	answ.smuTransfUID = 0;
	answ.appTransfUID = data.appTransfUID;
	answ.fileSize = 0;

	//ok... qualcuno vuole downloadare qualcosa da me
	//verifichiamo che il suo "what" sia lecito
	char s[256];
	s[0] = 0x00;
	if (strcasecmp(data.what, "test") == 0)
	{
		sprintf_s(s, sizeof(s), "%s/test.upload", rhea::getPhysicalPathToWritableFolder());
	}
	else if (strncasecmp(data.what, "audit", 5) == 0)
	{
		//si vuole scaricare il file app/temp/dataAudit%d.txt
		sprintf_s(s, sizeof(s), "%s/temp/dataAudit%s.txt", rhea::getPhysicalPathToAppFolder(), &data.what[5]);
	}
	else if (strncasecmp(data.what, "packaudit", 9) == 0)
	{
		//si vuole scaricare il file app/temp/packedDataAudit%d.dat
		//Questo file e' automaticamente generato durante il download del dataAudit dalla CPU, vedi CPUBridgeServer::priv_downloadDataAudit
		sprintf_s(s, sizeof(s), "%s/temp/packedDataAudit%s.dat", rhea::getPhysicalPathToAppFolder(), &data.what[9]);
	}
	else if (strncasecmp(data.what, "da3", 3) == 0)
	{
		if (strlen(data.what) == 3)
		{
			sprintf_s(s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
		}
		else
		{
            //si vuole scaricare il file app/temp/vmcDataFile%d.da3 (vedi CPUBridgeServer::priv_downloadVMCDataFile)
			sprintf_s(s, sizeof(s), "%s/temp/vmcDataFile%s.da3", rhea::getPhysicalPathToAppFolder(), &data.what[3]);
		}
	}
	else if (strncasecmp(data.what, "guiinfo", 7) == 0)
	{
		//informazioni relative alla GUI utili al menuPROG
		sprintf_s(s, sizeof(s), "%s/current/gui/web/config/lang/menuProg.txt", rhea::getPhysicalPathToAppFolder());
	}
	else if (strncasecmp(data.what, "file:", 5) == 0)
	{
		//la sintassi prevista per usage in questo caso �: "file:fullFilePathAndName"
		sprintf_s(s, sizeof(s), "%s", &data.what[5]);
		rhea::fs::sanitizePathInPlace((u8*)s);
	}
	else
	{
		answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
		logger->log("ERR: FileTransfer::priv_on_Opcode_upload_request_fromApp() invalid usage [%s]\n");
	}


	if (s[0] != 0x00)
	{
		FILE *fSRC = NULL;

		fSRC = fopen(s, "rb");
		if (NULL == fSRC)
		{
			answ.reason_refused = (u8)eFileTransferFailReason::smuErrorOpeningFile;
			logger->log("ERR: FileTransfer::priv_on_Opcode_upload_request_fromApp() error opening file [%s]\n", s);
		}
		else
		{
			const u64 UN_MEGA = 1024 * 1024;
			const u64 fsize = rhea::fs::filesize(fSRC);
			u32 filesize = 0;
			if (fsize == 0 || fsize > 256 * UN_MEGA)
			{
                rhea::fs::fileClose(fSRC);
				fSRC = NULL;
				answ.reason_refused = (u8)eFileTransferFailReason::smuFileTooBigOrEmpty;
			}
			else
				filesize = (u32)fsize;

			//genero un UID per il trasferimento
			//aggiungo il recordo alla lista dei trasferimenti attivi
			sActiveUploadRequest info;
			info.status = eTransferStatus::pending;
			info.isAppUploading = 0;
			info.f = fSRC;
			info.smuFileTransfUID = priv_generateUID();
			info.appFileTransfUID = data.appTransfUID;
			info.timeoutMSec = timeNowMSec + 5000;
			info.nextTimeOutputANotificationMSec = 0;
			info.totalFileSizeInBytes = filesize;
			info.packetSize = PACKET_SIZE;
			info.numPacketInAChunk = NUM_PACKET_IN_A_CHUNK;

			info.other.whenAPPisDownloading.sizeOfSendBuffer = info.packetSize + 32;
			info.other.whenAPPisDownloading.sendBuffer = (u8*)RHEAALLOC(localAllocator, info.other.whenAPPisDownloading.sizeOfSendBuffer);

			//calcolo il num totale di pacchetti che si dovranno inviare
			info.other.whenAPPisDownloading.numOfPacketToBeSentInTotal = info.totalFileSizeInBytes / info.packetSize;
			if (info.other.whenAPPisDownloading.numOfPacketToBeSentInTotal * info.packetSize < info.totalFileSizeInBytes)
				info.other.whenAPPisDownloading.numOfPacketToBeSentInTotal++;


			activeTransferList.append(info);

			//preparo la risposta
			answ.reason_refused = 0;
			answ.smuTransfUID = info.smuFileTransfUID;
			answ.packetSizeInBytes = info.packetSize;
			answ.numPacketInAChunk = info.numPacketInAChunk;
			answ.fileSize = info.totalFileSizeInBytes;

			logger->log("FileTransfer => download of [%s] accepted, smuUID[0x%08X] appUID[0x%08X]\n", data.what, answ.smuTransfUID, answ.appTransfUID);
		}
	}

	//spedisco
	u8	serializedDataBuffer[128];
	u16 n = answ.encode(serializedDataBuffer, sizeof(serializedDataBuffer));
	priv_send(server, h, serializedDataBuffer, n);
	
}

//***************************************************
void FileTransfer::priv_on0x54(Server *server, const HSokServerClient &h, rhea::NetStaticBufferViewR &nbr, u64 timeNowMSec)
{
	//decodifica e validazione dei dati ricevuti
	fileT::sData0x54 data;
	if (!data.decode(nbr))
	{
		DBGBREAK;
		return;
	}

	//ho ricevuto un pacchetto dati, vediamo se lo riconosco
	u32 index = priv_findActiveTransferBySMUUID(data.smuFileTransfUID);
	if (u32MAX == index)
	{
		//sconosciuto...
		DBGBREAK;
		return;
	}

	//app ha confermato la ricezione di un chunk
	sActiveUploadRequest *req = &activeTransferList.getElem(index);
	if (req->status != eTransferStatus::pending)
		return;

	//aggiorno il timeout per questo tranfert
	req->timeoutMSec = timeNowMSec + 15000;
	req->other.whenAPPisDownloading.nextPacketToSend = data.packetNumAccepted + 1;

	//era l'ultimo ?
	if (req->other.whenAPPisDownloading.nextPacketToSend >= req->other.whenAPPisDownloading.numOfPacketToBeSentInTotal)
	{
		//ho finito!!
		req->status = eTransferStatus::finished_OK;
		req->timeoutMSec = 0;
	}
	else
		priv_sendChunkOfPackets(server, h, req, req->numPacketInAChunk);
}
