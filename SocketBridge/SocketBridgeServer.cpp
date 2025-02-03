#include "SocketBridge.h"
#include "SocketBridgeVersion.h"
#include "SocketBridgeServer.h"
#include "../CPUBridge/CPUBridge.h"
#include "../rheaCommonLib/rheaString.h"
#include "../rheaCommonLib/rheaAllocatorSimple.h"
#include "../rheaCommonLib/rheaLogTargetConsole.h"
#include "../rheaCommonLib/rheaUtils.h"
#include "../rheaCommonLib/rheaIniFile.h"
#include "CmdHandler_ajaxReq.h"
#include "CmdHandler_eventReq.h"

using namespace socketbridge;



//***************************************************
Server::Server()
{
    localAllocator = NULL;
    server = NULL;
    logger = &nullLogger;
	eventSeqNumber = 0;
	cpuBridgeVersion = 0x00;
	rhea::event::setInvalid (hSubscriberEvent);

	moduleAlipayChina.subscribed = false;
	moduleAlipayChina.isOnline = false;
	moduleAlipayChina.threadStarted = false;

}


//***************************************************
void Server::useLogger (rhea::ISimpleLogger *loggerIN)
{
    if (NULL==loggerIN)
        logger = &nullLogger;
    else
        logger = loggerIN;

    if (NULL != server)
        server->useLogger(loggerIN);
}

//***************************************************
bool Server::open (u16 SERVER_PORT, const HThreadMsgW &hCPUServiceChannelW, bool bDieWhenNoClientConnected)
{
    logger->log ("SocketBridgeServer::open\n");
    logger->incIndent();

	dieWhenNoClientConnected = 0;
	if (bDieWhenNoClientConnected)
		dieWhenNoClientConnected = 1;

    localAllocator = RHEANEW(rhea::getSysHeapAllocator(), rhea::AllocatorSimpleWithMemTrack) ("socketBridgeSrv");

    server = RHEANEW(localAllocator, rhea::ProtocolSocketServer)(8, localAllocator);
    server->useLogger(logger);

	//apro la socket TCP
	logger->log("opening TCP socket...");
    eSocketError err = server->start (SERVER_PORT);
    if (err != eSocketError::none)
    {
        logger->log("FAIL\n");
        logger->decIndent();
        RHEADELETE(localAllocator, server);
        return false;
    }
    logger->log("OK\n");


	//iscrizione alla cpu
	if (!hCPUServiceChannelW.isInvalid())
	{
		logger->log("subscribing to CPUBridge...");
		if (!priv_subsribeToCPU(hCPUServiceChannelW))
		{
			logger->log("FAIL\n");
			logger->decIndent();
			RHEADELETE(localAllocator, server);
			return false;
		}
		logger->log("OK\n");
	}


    logger->decIndent();

	SEND_BUFFER_SIZE = 4096;
	sendBuffer = (u8*)RHEAALLOC(localAllocator, SEND_BUFFER_SIZE);

    //elenco dei clienti identificati
    identifiedClientList.setup (localAllocator);

    //lista degli handler dei messaggi
    nextTimePurgeCmdHandlerList = 0;
    cmdHandlerList.setup (localAllocator);

    //linear buffer per la ricezione di msg dai client
    buffer.setup (localAllocator, 128);

    _nextHandlerID = 0;

	//gestore del file transfer
	fileTransfer.setup(localAllocator, logger);

	//lista delle connessioni ai DB
	nextTimePurgeDBList = 0;
	dbList.setup(localAllocator);
	rst.setup(localAllocator, 8);

	//task factory
	taskFactory = RHEANEW(localAllocator, TaskFactory)();
	runningTask.setup(localAllocator, 32);
    return true;
}

//***************************************************
void Server::close()
{
    if (NULL == localAllocator)
        return;

    if (NULL != server)
    {
		rst.unsetup();
		dbList.unsetup();
		fileTransfer.unsetup();

		if (dieWhenNoClientConnected == 2)
		{
			//chiedo a CPUBridge di morire
			rhea::thread::pushMsg (subscriber.hFromSubscriberToMeW, CPUBRIDGE_SUBSCRIBER_ASK_DIE, (u32)0, NULL, 0);
		}

		cpubridge::unsubscribe (subscriber);

        buffer.unsetup();
        cmdHandlerList.unsetup();
        identifiedClientList.unsetup ();

        server->close();
        RHEADELETE(localAllocator, server);

		RHEAFREE(localAllocator, sendBuffer);
		sendBuffer = NULL;

		RHEADELETE(localAllocator, taskFactory);
		for (u32 i = 0; i < runningTask.getNElem(); i++)
			TaskStatus::FREE(runningTask[i]);
		runningTask.unsetup();

    }

    RHEADELETE(rhea::getSysHeapAllocator(), localAllocator);
    logger->log ("SocketBridgeServer::closed\n");
}

/***************************************************
 *	Crea il thread per la gestione delle transazioni alipay cinesi.
 *	I dati per la connessione (IP, porta e via dicendo) sono nel file config.alipaychina.ini in current/ini
 */
bool Server::priv_module_alipayChina_setup ()
{
	//Se sono gi� iscritto alla msgQ del thread di alipay, non devo fare altro
	if (moduleAlipayChina.subscribed)
	{
		logger->log ("SocketBridgeServer::alipayChina_setup() => a module is already instanced\n");
		return false;
	}

	//parametri di comunicazione di default
	const char	DEFAULT_cryptoKey[] = { "1648339973B547DC8DE3D60787079B3D" };
	const char	DEFAULT_machineID[] = { "C20190001" };
	const char	DEFAULT_serverIP[] = { "121.196.20.39" };
	const u16	DEFAULT_SERVER_PORT = 6019;

	moduleAlipayChina.serverPort = DEFAULT_SERVER_PORT;
	sprintf_s (moduleAlipayChina.serverIP, sizeof(moduleAlipayChina.serverIP), DEFAULT_serverIP);
	sprintf_s (moduleAlipayChina.machineID, sizeof(moduleAlipayChina.machineID), DEFAULT_machineID);
	sprintf_s (moduleAlipayChina.criptoKey, sizeof(moduleAlipayChina.criptoKey), DEFAULT_cryptoKey);
	

	//se esiste, apro e parso il file "current/ini/config.alipaychina.ini" per leggere i parametri di comunicazione
	{
		u8 filename[256];
		sprintf_s ((char*)filename, sizeof(filename), "%s/current/ini/config.alipaychina.ini", rhea::getPhysicalPathToAppFolder());
		rhea::IniFile config;
		config.setup (localAllocator);
		if (config.loadAndParse (filename))
		{
			rhea::utf8::String s;
			moduleAlipayChina.serverPort = (u16)config.getOrDefaultAsU32 ((const u8*)"alipayChina.serverPort", DEFAULT_SERVER_PORT);
			config.getOrDefault ((const u8*)"alipayChina.serverIP", (const u8*)DEFAULT_serverIP, (u8*)moduleAlipayChina.serverIP, sizeof(moduleAlipayChina.serverIP));
			config.getOrDefault ((const u8*)"alipayChina.machineID", (const u8*)DEFAULT_machineID, (u8*)moduleAlipayChina.machineID, sizeof(moduleAlipayChina.machineID));
			config.getOrDefault ((const u8*)"alipayChina.key", (const u8*)DEFAULT_cryptoKey, (u8*)moduleAlipayChina.criptoKey, sizeof(moduleAlipayChina.criptoKey));
		}
		config.unsetup();
	}


	//creazione thread
	if (!moduleAlipayChina.threadStarted)
	{
		moduleAlipayChina.threadStarted = rhea::AlipayChina::startThread (moduleAlipayChina.serverIP, moduleAlipayChina.serverPort, moduleAlipayChina.machineID, DEFAULT_cryptoKey, this->logger, &moduleAlipayChina.ctx);
		if (!moduleAlipayChina.threadStarted)
		{
			logger->log ("SocketBridgeServer::alipayChina_setup() => error starting the thread\n");
			return false;
		}

		//creo la msgQ tra me e il thread di alipay
		rhea::thread::createMsgQ (&moduleAlipayChina.hMsgQR, &moduleAlipayChina.hMsgQW);
	}

	//Provo ad iscrivermi alla coda di notifiche del thrad alipay
	rhea::AlipayChina::subscribe (moduleAlipayChina.ctx, moduleAlipayChina.hMsgQW);
	u64 timeToExitMSec = rhea::getTimeNowMSec() + 4000;
	do
	{
		//attendo risposta
		rhea::thread::sleepMSec(100);

		rhea::thread::sMsg msg;
		if (rhea::thread::popMsg(moduleAlipayChina.hMsgQR, &msg))
		{
			//ok, ci siamo
			if (msg.what == ALIPAYCHINA_SUBSCRIPTION_ANSWER_ACCEPTED)
			{
				OSEvent hEvent;
				rhea::thread::getMsgQEvent(moduleAlipayChina.hMsgQR, &hEvent);
				server->addOSEventToWaitList (hEvent, MSGQ_ALIPAY_CHINA);
				rhea::thread::deleteMsg(msg);
				moduleAlipayChina.subscribed = true;
				moduleAlipayChina.isOnline = false;
				logger->log ("SocketBridgeServer::alipayChina_setup() => subscribed OK\n");

				//chiedo lo stato di "Online" del servizio
				rhea::AlipayChina::ask_ONLINE_STATUS (moduleAlipayChina.ctx);
				return true;
			}
			rhea::thread::deleteMsg(msg);
		}
	} while (rhea::getTimeNowMSec() < timeToExitMSec);

	//il thread alypay non ha risposto...
	logger->log ("SocketBridgeServer::alipayChina_setup() => subscribed KO\n");
	return false;
}

//***************************************************
void Server::module_alipayChina_activate()
{
	if (moduleAlipayChina.subscribed)
		return;
	priv_module_alipayChina_setup();
}

//***************************************************
bool Server::module_alipayChina_askQR (const u8 *selectionName, u8 selectionNum, const char *selectionPrice, bool bForceJUG, u8 *out_urlForQRCode, u32 sizeOfOutURL UNUSED_PARAM)
{
	moduleAlipayChina.curSelRunning = 0;
	moduleAlipayChina.curSelPrice = 0;
	moduleAlipayChina.curSelForceJUG = bForceJUG;
	if (!moduleAlipayChina.subscribed || !moduleAlipayChina.isOnline)
		return false;

	

	//chiedo al server alipay di iniziare la transazione
	//Mi risponde con una serie di notifiche ALIPAYCHINA_NOTIFY_ORDER_STATUS che riportano
	//un codice di stato preso dalla enum AlipayChina::eOrderStatus
	rhea::AlipayChina::ask_startOrder (moduleAlipayChina.ctx, selectionName, selectionNum, selectionPrice);

	OSEvent hEvent;
	rhea::thread::getMsgQEvent(moduleAlipayChina.hMsgQR, &hEvent);
	const u64 timeToExitMSec = rhea::getTimeNowMSec() + 60000;
	while (rhea::getTimeNowMSec() < timeToExitMSec)
	{
		if (rhea::event::wait (hEvent, 5000))
		{
			rhea::thread::sMsg msg;
			while (rhea::thread::popMsg(moduleAlipayChina.hMsgQR, &msg))
			{
				switch (msg.what)
				{
				default:
					priv_onAlipayChinaNotification(msg);
					break;

				case ALIPAYCHINA_NOTIFY_ORDER_STATUS:
					switch ((rhea::AlipayChina::eOrderStatus)msg.paramU32)
					{
					case rhea::AlipayChina::eOrderStatus::waitingQR:
						//siamo ancora in attesa di una risposta
						break;

					case rhea::AlipayChina::eOrderStatus::pollingPaymentStatus:
						//ok, il thread alipay � in polling quindi vuol dire che ha ricevuto il qrcode.
						//Questo msg contiene anche l'url da visualizzare nel qrcode
                        memcpy (out_urlForQRCode, msg.buffer, msg.bufferSize);
						rhea::thread::deleteMsg(msg);
							
						//memorizzo la selezione da far partire. Da ora in poi sono in attesa di un messaggio dal thread alipay
						//che mi dica se l'utente ha pagato oppure abortito.
						//Se ha pagato, faccio partire la selezione, vedi priv_onAlipayChinaNotification()
						moduleAlipayChina.curSelRunning = selectionNum;
						{
							u8 ct = 0;
							u8 price[16];
							for (u8 i = 0; i < strlen(selectionPrice); i++)
							{
								char c = selectionPrice[i];
								if (c >= '0' && c <= '9')
									price[ct++] = c;
							}
							price[ct] = 0x00;
							moduleAlipayChina.curSelPrice = rhea::string::utf8::toU32(price);
						}
						return true;
						break;

					default:
						//DBGBREAK;
						rhea::thread::deleteMsg(msg);
						return false;
						break;
					}
					break;
				}
				rhea::thread::deleteMsg(msg);
			}			
		}
	}
	return false;
}

//***************************************************
void Server::module_alipayChina_abort()
{
	if (moduleAlipayChina.subscribed && moduleAlipayChina.curSelRunning)
	{
		moduleAlipayChina.curSelRunning = 0;
		moduleAlipayChina.curSelPrice = 0;
		moduleAlipayChina.curSelForceJUG = false;
		rhea::AlipayChina::ask_abortOrder(moduleAlipayChina.ctx);
	}
}

//***************************************************
bool Server::module_getConnectionDetail (char *out_serverIP, u16 *out_serverPort, char *out_machineID, char *out_criptoKey) const
{
	if (!moduleAlipayChina.subscribed)
		return false;

	*out_serverPort = moduleAlipayChina.serverPort;
	strcpy (out_serverIP, moduleAlipayChina.serverIP);
	strcpy (out_machineID, moduleAlipayChina.machineID);
	strcpy (out_criptoKey, moduleAlipayChina.criptoKey);
	return true;	
}

//***************************************************
bool Server::priv_subsribeToCPU(const HThreadMsgW &hCPUServiceChannelW)
{
	assert(rhea::event::isInvalid(hSubscriberEvent));

	//creo una msgQ temporanea per ricevere da CPUBridge la risposta alla mia richiesta di iscrizione
	HThreadMsgR hMsgQR;
	HThreadMsgW hMsgQW;
	rhea::thread::createMsgQ (&hMsgQR, &hMsgQW);

	//invio la richiesta
	cpubridge::subscribe (hCPUServiceChannelW, hMsgQW, SOCKETBRIDGE_SUBSCRIBER_UID);

	//attendo risposta
	u64 timeToExitMSec = rhea::getTimeNowMSec() + 2000;
	do
	{
		rhea::thread::sleepMSec(50);

		rhea::thread::sMsg msg;
		if (rhea::thread::popMsg(hMsgQR, &msg))
		{
			//ok, ci siamo
			if (msg.what == CPUBRIDGE_SERVICECH_SUBSCRIPTION_ANSWER)
			{
				cpubridge::translate_SUBSCRIPTION_ANSWER (msg, &subscriber, &cpuBridgeVersion);
				rhea::thread::deleteMsg(msg);
				rhea::thread::getMsgQEvent(subscriber.hFromMeToSubscriberR, &hSubscriberEvent);
				server->addOSEventToWaitList (hSubscriberEvent, u32MAX);
				break;
			}

			rhea::thread::deleteMsg(msg);
		}
	} while (rhea::getTimeNowMSec() < timeToExitMSec);
	
	//delete della msgQ
	rhea::thread::deleteMsgQ (hMsgQR, hMsgQW);
	

	return rhea::event::isValid(hSubscriberEvent);
}

//*****************************************************************
bool Server::priv_extractOneMessage(u8 *bufferPt, u16 nBytesInBuffer, sDecodedMessage *out, u16 *out_nBytesConsumed) const
{
	//deve essere lungo almeno 7 byte e iniziare con #
	if (nBytesInBuffer < 7)
	{
		//scarta il messaggio perch� non pu� essere un msg valido
		DBGBREAK;
		*out_nBytesConsumed = nBytesInBuffer;
		return false;
	}

	if (bufferPt[0] != '#')
	{
		//msg non valido, avanzo fino a che non trovo un # oppure fine buffer
		for (u16 i = 1; i < nBytesInBuffer; i++)
		{
			if (bufferPt[i] == '#')
			{
				bool ret = priv_extractOneMessage (&bufferPt[i], (nBytesInBuffer - i), out, out_nBytesConsumed);
				*out_nBytesConsumed += i;
				return ret;
			}
		}

		//errore, non ho trovato un #, msg non valido
		DBGBREAK;
		*out_nBytesConsumed = nBytesInBuffer;
		return false;
	}


	out->opcode = (eOpcode)bufferPt[1];
	out->requestID = bufferPt[2];
	out->payloadLen = ((u16)bufferPt[3] * 256) + (u16)bufferPt[4];

	if (nBytesInBuffer < 7 + out->payloadLen)
	{
		//errore, non ci sono abbastanza byte nel buffer per l'intero msg
		DBGBREAK;
		*out_nBytesConsumed = nBytesInBuffer;
		return false;
	}

	out->payload = &bufferPt[5];

	//verifica della ck
	const u16 ck = ((u16)bufferPt[5 + out->payloadLen] * 256) + bufferPt[6 + out->payloadLen];
	if (ck != rhea::utils::simpleChecksum16_calc(bufferPt, 5 + out->payloadLen))
	{
		//il messaggio sembrava buono ma la ck non lo �, lo scarto.
		//Consumo il # e passo avanti
		bool ret = priv_extractOneMessage (&bufferPt[1], (nBytesInBuffer - 1), out, out_nBytesConsumed);
		*out_nBytesConsumed += 1;
		return ret;
	}


	*out_nBytesConsumed = (7 + out->payloadLen);
	return true;
}

//*****************************************************************
bool Server::priv_encodeMessageOfTypeEvent (eEventType eventType, const void *optionalData, u16 lenOfOptionalData, u8 *out_buffer, u16 *in_out_bufferLength)
{
	//calcola la dimensione minima necessaria in byte per il buffer
	const u16 minSizeInBytes = 8 + lenOfOptionalData;

	if (*in_out_bufferLength < minSizeInBytes)
	{
		DBGBREAK;
		*in_out_bufferLength = minSizeInBytes;
		return false;
	}

	u16 n = 0;
	out_buffer[n++] = '#';
	out_buffer[n++] = 'e';
	out_buffer[n++] = 'V';
	out_buffer[n++] = 'n';
	out_buffer[n++] = (u8)eventType;
	out_buffer[n++] = eventSeqNumber++;

	out_buffer[n++] = ((lenOfOptionalData & 0xFF00) >> 8);
	out_buffer[n++] = (lenOfOptionalData & 0x00FF);
	if (lenOfOptionalData)
	{
		memcpy(&out_buffer[n], optionalData, lenOfOptionalData);
		n += lenOfOptionalData;
	}

	*in_out_bufferLength = n;
	return true;
}

//*****************************************************************
bool Server::priv_encodeMessageOfAjax(u8 requestID, const u8 *ajaxData, u16 lenOfAjaxData, u8 *out_buffer, u16 *in_out_bufferLength)
{
	//calcola la dimensione minima necessaria in byte per il buffer
	const u16 minSizeInBytes = 6 + lenOfAjaxData;

	if (*in_out_bufferLength < minSizeInBytes)
	{
		DBGBREAK;
		*in_out_bufferLength = minSizeInBytes;
		return false;
	}

	u16 n = 0;
	out_buffer[n++] = '#';
	out_buffer[n++] = 'A';
	out_buffer[n++] = 'J';
	out_buffer[n++] = requestID;
	out_buffer[n++] = 'j';
	out_buffer[n++] = 'a';

	if (lenOfAjaxData)
	{
		memcpy(&out_buffer[n], ajaxData, lenOfAjaxData);
		n += lenOfAjaxData;
	}

	*in_out_bufferLength = n;
	return true;
}


//*****************************************************************
void Server::sendTo(const HSokServerClient &h, const u8 *buffer, u32 nBytesToSend)
{
	server->client_writeBuffer(h, buffer, nBytesToSend);
}

//*****************************************************************
void Server::sendAjaxAnwer (const HSokServerClient &h, u8 requestID, const u8 *ajaxData, u16 lenOfAjaxData)
{
	u16 n = SEND_BUFFER_SIZE;
	if (priv_encodeMessageOfAjax (requestID, ajaxData, lenOfAjaxData, sendBuffer, &n))
		sendTo(h, sendBuffer, n);
	else
	{
		RHEAFREE(localAllocator, sendBuffer);
		while (SEND_BUFFER_SIZE < n)
			SEND_BUFFER_SIZE += 1024;
		
		sendBuffer = (u8*)RHEAALLOC(localAllocator, SEND_BUFFER_SIZE);
		sendAjaxAnwer(h, requestID, ajaxData, lenOfAjaxData);
	}
}

//*****************************************************************
void Server::sendEvent (const HSokServerClient &h, eEventType eventType, const void *optionalData, u16 lenOfOptionalData)
{
	u16 n = SEND_BUFFER_SIZE;
	if (priv_encodeMessageOfTypeEvent(eventType, optionalData, lenOfOptionalData, sendBuffer, &n))
		sendTo(h, sendBuffer, n);
	else
	{
		RHEAFREE(localAllocator, sendBuffer);
		while (SEND_BUFFER_SIZE < n)
			SEND_BUFFER_SIZE += 1024;

		sendBuffer = (u8*)RHEAALLOC(localAllocator, SEND_BUFFER_SIZE);
		sendEvent(h, eventType, optionalData, lenOfOptionalData);
	}
}

//***************************************************
bool Server::isMotherboard_D23() const
{
#ifdef PLATFORM_ROCKCHIP
	return true;
#else
	return false;
#endif
}

//***************************************************
void Server::formatSinglePrice(u16 price, u8 numDecimals, char *out, u16 sizeofOut) const
{
	const char DECIMAL_SEP = '.';
	rhea::string::format::currency (price, numDecimals, DECIMAL_SEP, out, sizeofOut);
}

//***************************************************
void Server::formatPriceList (const u16 *priceList, u16 nPricesInList, u8 numDecimals, char *out, u16 sizeofOut) const
{
	out[0] = 0x00;
	if (sizeofOut < 2)
		return;
	sizeofOut--;
	
	for (u16 i = 0; i < nPricesInList; i++)
	{
		char s[32];
		formatSinglePrice (priceList[i], numDecimals, s, sizeof(s));

		u8 len = 1 + (u8)strlen(s);
		if (sizeofOut < len)
			return;

		if (i > 0)
			strcat(out, "§"); //§ (ovvero 0xc2 0xa7) � l'UTF8 per il carattere �
		strcat(out, s);
		sizeofOut -= len;
	}
}


//***************************************************
void Server::run()
{
    assert (server != NULL);

//    priv_module_alipayChina_setup();

    bQuit = false;
    while (bQuit == false)
    {
        //la wait() si sveglia se � in arrivo una nuova connessione, se un client gi�  connesso invia dati oppure se la coda di msg con CPUBridge ha qualche msg pendente
        u8 nEvents = server->wait (5000);

        //elimino eventuali handler rimasti pendenti
        u64 timeNowMSec = rhea::getTimeNowMSec();
        if (timeNowMSec > nextTimePurgeCmdHandlerList)
        {
            cmdHandlerList.purge (localAllocator, timeNowMSec);
			identifiedClientList.purge(timeNowMSec);
			nextTimePurgeCmdHandlerList = timeNowMSec + 10000;


			//se � stato indicato l'apposito flag durante la open(), allora quando non ho + client connessi, muoio
			if (dieWhenNoClientConnected == 2)
			{
				if (identifiedClientList.getCount() == 0)
				{
					bQuit = true;
					continue;
				}
			}
        }

		if (timeNowMSec > nextTimePurgeDBList)
		{
			dbList.purge(timeNowMSec);
			nextTimePurgeDBList = timeNowMSec + 60000;
		}

		//aggiornamento degli eventuali file transfer in corso
		fileTransfer.update(timeNowMSec);

        //analizzo gli eventi ricevuti
        for (u8 i=0; i<nEvents; i++)
        {
            switch (server->getEventType(i))
            {
            default:
                logger->log ("SocketBridgeServer::run(), unknown event type: %d\n", server->getEventType(i));
                break;

			case rhea::ProtocolSocketServer::eEventType::ignore:
				break;

            case rhea::ProtocolSocketServer::eEventType::new_client_connected:
                {
                    //un client vuole connettersi. Accetto la socket in ingresso, ma mi riservo di chiudere la
                    //connessione se il prossimo msg che manda � != dal messaggio di identificazione.
                    //HSokServerClient h = server->getEventSrcAsClientHandle(i);
                    //printf ("server> new connection accepted (handle:0x%02X)\n", h.asU32());
                }
                break;

            case rhea::ProtocolSocketServer::eEventType::client_has_data_avail:
				//uno dei clienti gi�  connessi ha mandato qualcosa
                priv_onClientHasDataAvail (i, timeNowMSec);
                break;

            case rhea::ProtocolSocketServer::eEventType::osevent_fired:
                {
					//un OSEvent associato alla waitList � stato attivato.
					//Al momento la cosa significa solo che CPUBridge ha mandato un msg lungo la msgQ
					//In futuro, potrebbero esserci anche altri eventi da monitorare.
					const u32 fromWhom = server->getEventSrcUserParam(i);
                    rhea::thread::sMsg msg;

					switch (fromWhom)
					{
					case u32MAX:
						//E' arrivato un msg da CPUBridge
                        while (rhea::thread::popMsg(subscriber.hFromMeToSubscriberR, &msg))
                        {
                            priv_onCPUBridgeNotification(msg);
                            rhea::thread::deleteMsg(msg);
                        }
						break;

                    case Server::MSGQ_ALIPAY_CHINA:
						//e' arrivato un msg dal thread che gestire alipay china
						if (moduleAlipayChina.subscribed)
						{
							while (rhea::thread::popMsg(moduleAlipayChina.hMsgQR, &msg))
							{
								priv_onAlipayChinaNotification(msg);
								rhea::thread::deleteMsg(msg);
							}
						}
						break;

					default:
						logger->log("SocketBridgeServer => unkwnown event from waitList\n");
						break;
					}
                }
                break;
            }
        }
    }

    cmdHandlerList.purge (localAllocator, u64MAX);

	//module alipay china
	if (moduleAlipayChina.subscribed)
	{
		moduleAlipayChina.subscribed = 0;
		
		OSEvent hEvent;
		rhea::thread::getMsgQEvent (moduleAlipayChina.hMsgQR, &hEvent);
		server->removeOSEventFromWaitList (hEvent);
		rhea::AlipayChina::kill(moduleAlipayChina.ctx);
		rhea::thread::deleteMsgQ (moduleAlipayChina.hMsgQR, moduleAlipayChina.hMsgQW);
	}
}

/**************************************************************************
 * priv_getANewHandlerID ()
 *
 * Ritorna un ID univoco da assegnare agli handler dei messaggi
 */
u16 Server::priv_getANewHandlerID ()
{
    _nextHandlerID++;

    //i primi RESERVED_HANDLE_RANGE id sono riservati ad uso interno
    if (_nextHandlerID < RESERVED_HANDLE_RANGE)
        _nextHandlerID = RESERVED_HANDLE_RANGE;
    return _nextHandlerID;
}


//**************************************************************************
void Server::priv_handleIdentification (const HSokServerClient &h, const sIdentifiedClientInfo *identifiedClient, socketbridge::sDecodedMessage &decoded)
{
	if (decoded.opcode == socketbridge::eOpcode::request_idCode && decoded.payloadLen == 4)
	{
		//il client mi sta chiedendo di inviargli un idCode univoco che lui user�  da ora in avanti per indentificarsi.
		//Mi sta anche gi�  comunicando la sua clientVer, che rimarr�  immutata di qui in avanti
		SokBridgeClientVer clientVer;
		clientVer.zero();
		clientVer.apiVersion = decoded.payload[0];
		clientVer.appType = decoded.payload[1];
		clientVer.unused2 = decoded.payload[2];
		clientVer.unused3 = decoded.payload[3];

		//Creo un nuovo record in idClientList e genero un idCode univoco
		SokBridgeIDCode	idCode;
		HSokBridgeClient identifiedCLientHanle = identifiedClientList.newEntry(rhea::getTimeNowMSec(), &idCode);
		identifiedClientList.updateClientVerInfo(identifiedCLientHanle, clientVer);

		logger->log("connection [0x%08X] is requesting new idCode. Sending [0x%08X]\n", h.asU32(), idCode.data.asU32);

		//rispodo al client inviandogli il suo idCode
        //u32 version = (((u32)cpuBridgeVersion) << 8) | SOCKETBRIDGE_VERSION;
		u8 toSend[8] = { cpuBridgeVersion, SOCKETBRIDGE_VERSION, idCode.data.buffer[0], idCode.data.buffer[1], idCode.data.buffer[2], idCode.data.buffer[3], 0 ,0 };
		sendEvent(h, eEventType::answer_to_idCodeRequest, toSend, 6);
		return;
	}
	else if (decoded.opcode == socketbridge::eOpcode::identify_W && decoded.payloadLen == 8)
	{
		assert(decoded.opcode == socketbridge::eOpcode::identify_W);

		SokBridgeClientVer clientVer;
		clientVer.zero();
		clientVer.apiVersion = decoded.payload[0];
		clientVer.appType = decoded.payload[1];
		clientVer.unused2 = decoded.payload[2];
		clientVer.unused3 = decoded.payload[3];

		SokBridgeIDCode	idCode;
		idCode.data.buffer[0] = decoded.payload[4];
		idCode.data.buffer[1] = decoded.payload[5];
		idCode.data.buffer[2] = decoded.payload[6];
		idCode.data.buffer[3] = decoded.payload[7];

		//ho ricevuto un idCode e una clientVer; questa combinazione deve esistere di gi�  nella mia lista di identifiedClient, altrimenti chiudo connession
		identifiedClient = identifiedClientList.isKnownIDCode(idCode);
		if (NULL == identifiedClient)
		{
			logger->log("refusing connection [0x%08X] because its idCode is not valid [idCode:0x%08X]\n", h.asU32(), idCode.data.asU32);
			server->client_sendClose(h);
			return;
		}

		if (!identifiedClientList.compareClientVerInfo(identifiedClient->handle, clientVer))
		{
			logger->log("refusing connection [0x%08X] because its clientVer is not equal to previous ver\n", h.asU32());
			server->client_sendClose(h);
			return;
		}

		//ok, se siamo arrivati qui vuol dire che il client mi ha inviato un idCode corretto e una clientVer corretta.
		//[identifiedClient] � quindi un valido pt. Bindo la socket al client e ritorno
		logger->log("client [0x%08X]: reconnected with connection [0x%08X] \n", idCode.data.asU32, h.asU32());
		if (!identifiedClientList.bindSocket(identifiedClient->handle, h, rhea::getTimeNowMSec()))
		{
			logger->log("warn, socket is already bind\n");
			identifiedClientList.unbindSocket(identifiedClient->handle, rhea::getTimeNowMSec());
			identifiedClientList.bindSocket(identifiedClient->handle, h, rhea::getTimeNowMSec());
		}

		//gli mando l'attuale messaggio di CPU (uso la stessa procedura che userei se fosse stato davvero il client a chiedermelo)
		CmdHandler_eventReq *handler = CmdHandler_eventReqFactory::spawnFromSocketClientEventType(localAllocator, identifiedClient->handle, eEventType::cpuMessage, priv_getANewHandlerID(), 10000);
		if (NULL != handler)
		{
			cmdHandlerList.add(handler);
			handler->passDownRequestToCPUBridge(subscriber, NULL, 0);
		}
		return;
	}
	else
	{
		logger->log("ERR: killing connection [0x%08X] because it's not identified [rcvd opcode=%d]\n", h.asU32(), decoded.opcode);
		server->client_sendClose(h);

		/*logger->log("reqID[%d], payloadLen[%d]\n", decoded.requestID, decoded.payloadLen);
		for (u16 i = 0; i < decoded.payloadLen; i++)
			logger->log("%c ", (char)decoded.payload[i]);
		logger->log("\n");
		*/		
		return;
	}
}



/**************************************************************************
 * onClientHasDataAvail
 *
 * Un client gi�  collegato ha inviato dei dati lungo la socket
 */
void Server::priv_onClientHasDataAvail(u8 iEvent, u64 timeNowMSec)
{
	HSokServerClient h = server->getEventSrcAsClientHandle(iEvent);
	const sIdentifiedClientInfo	*identifiedClient = identifiedClientList.isKwnownSocket(h, timeNowMSec);

	i32 nBytesLetti = server->client_read(h, buffer);
	if (nBytesLetti == -1)
	{
		logger->log("connection [0x%08X] closed\n", h.asU32());

		//cerco il client che aveva questa socket e, se esiste, la unbindo.
		//Il client rimane registrato nella lista degli identificati, ma non ha una socket associata
		if (identifiedClient)
		{
			identifiedClientList.unbindSocket(identifiedClient->handle, rhea::getTimeNowMSec());
			if (dieWhenNoClientConnected == 1)
				dieWhenNoClientConnected = 2;
		}
		return;
	}

	if (nBytesLetti < 1)
	{
		logger->log("connection [0x%08X] not enought data [%d bytes]\n", h.asU32(), nBytesLetti);
		return;
	}

	//ho ricevuto un messaggio, proviamo a decodificarlo
	socketbridge::sDecodedMessage decoded;
	u16 nBytesConsumed;
	u8 *bufferPt = buffer._getPointer(0);
	while (priv_extractOneMessage(bufferPt, (u16)nBytesLetti, &decoded, &nBytesConsumed))
	{
		priv_onClientHasDataAvail2 (timeNowMSec, h, identifiedClient, decoded);

		assert(nBytesLetti >= nBytesConsumed);
		nBytesLetti -= nBytesConsumed;
		if (nBytesLetti == 0)
			return;
		bufferPt += nBytesConsumed;

		if (NULL == identifiedClient)
			identifiedClient = identifiedClientList.isKwnownSocket(h, timeNowMSec);
	}
}

void Server::priv_onClientHasDataAvail2(u64 timeNowMSec, HSokServerClient &h, const sIdentifiedClientInfo *identifiedClient, socketbridge::sDecodedMessage &decoded)
{
	//se la socket in questione non � stata ancora bindata ad un client, allora il cliente mi deve per
	//forza aver mandato un messaggio di tipo [eOpcode::request_idCode] oppure [eOpcode::identify_W], altrimenti killo la connessione.
	//Se invece la socket � gi�  bindata ad un identified-client, nessun problema, passo ad analizzare il messaggio perch� il client � stato gi�  identificato ad 
	//un giro precedente
	if (NULL == identifiedClient)
	{
		priv_handleIdentification(h, identifiedClient, decoded);
		return;
	}


	//ok, vediamo che cosa mi sta chiedendo il client
	//logger->log ("client [0x%08X]: rcv command [%c]\n", identifiedClient->idCode.data.asU32, decoded.opcode);
	switch (decoded.opcode)
	{
	default:
		logger->log("unkown command [%c]\n", decoded.opcode);
		return;

	case socketbridge::eOpcode::event_E:
		/*ho ricevuto una richiesta di tipo "scatena un evento"
		Istanzio un "handler" appropriato che possa gestire la richiesta. Se quest'handler esiste, allora ci sono 2 possibiit� :
			1- la richiesta deve essere passata a CPUBridge (per es mi stanno chiedendo un aggiornamento sullo stati di disp delle selezioni)
			2- la richiesta la gestisce direttamente this (per es mi stanno chiedendo una lista dei client connessi).
		Caso 1:
			aggiungo l'handle alla lista degli handler attivi e chiamo il suo metodo passDownRequestToCPUBridge() che, a sua volta, appende una richiesta alla msgQ
			verso CPUBridge in modo che CPUBridge possa intercettare la richiesta e rispondere sulla stessa msgQ.
			La risposta di CPUBridge (che prevede tra i vari parametri anche l'handlerID), viene gestita dallo stesso handler che ho istanziato qui che, sostanzialmente, spedisce indietro
			al client il risultato appena ottenuto da CPUBridge.
			Il giro quindi �:
				client chiede qualcosa a SocketBridge
				SocketBridge istanzia un handler che a sua volta gira la richiesta a CPUBridge lungo la msgQ dedicata
				CPUBridge riceve la richiesta e risponde a SocketBridge lungo la stessa msgQ
				SocketBridge recupera la risposta di CPUBridge e la passa allo stesso handler che inizialmente si era preoccupato di girare la richiesta a CPUBridge
				L'handler in questione fa le sue elaborazione e spedisce indietro il risultato al client

		Caso 2:
			chiamo direttamente il metodo handleRequestFromSocketBridge() dell'handler che risponde al cliente, e poi distruggo l'istanza di handler.
		*/
		if (decoded.payloadLen >= 1)
		{
			socketbridge::eEventType evType = (socketbridge::eEventType)decoded.payload[0];
			CmdHandler_eventReq *handler = CmdHandler_eventReqFactory::spawnFromSocketClientEventType(localAllocator, identifiedClient->handle, evType, priv_getANewHandlerID(), 10000);
			if (NULL != handler)
			{
				if (handler->needToPassDownToCPUBridge())
				{
					cmdHandlerList.add(handler);
					handler->passDownRequestToCPUBridge(subscriber, decoded.payload, decoded.payloadLen);
				}
				else
				{
					handler->handleRequestFromSocketBridge(this, h);
					RHEADELETE(localAllocator, handler);
				}
			}
		}
		break;

	case socketbridge::eOpcode::ajax_A:
		/*	ho ricevuto una richiesta di tipo "ajax"
			Funziona allo stesso modo di cui sopra
		*/
		{
			const u8 *params = NULL;
			CmdHandler_ajaxReq *handler = CmdHandler_ajaxReqFactory::spawn(localAllocator, identifiedClient->handle, decoded.requestID, decoded.payload, decoded.payloadLen, priv_getANewHandlerID(), 10000, &params);
			if (NULL != handler)
			{
				if (handler->needToPassDownToCPUBridge())
				{
					cmdHandlerList.add(handler);
					handler->passDownRequestToCPUBridge(subscriber, params);
				}
				else
				{
					handler->handleRequestFromSocketBridge(this, h, params);
					RHEADELETE(localAllocator, handler);
				}
			}
		}
		break;

	case socketbridge::eOpcode::fileTransfer:
		/*	messaggio di gestione dei file transfer */
		fileTransfer.handleMsg(this, h, decoded, timeNowMSec);
		break;
	}
}


/**************************************************************************
 * priv_onCPUBridgeNotification
 *
 * E' arrivato un messaggio da parte di CUPBrdige sulla msgQ dedicata (ottenuta durante la subscribe di this a CPUBridge).
 */
void Server::priv_onCPUBridgeNotification (rhea::thread::sMsg &msg)
{
	const u16 handlerID = (msg.paramU32 & 0x0000FFFF);
    //const u8 *data = (const u8*)msg.buffer;

	if (handlerID == 0)
	{
		//in queso caso, CPUBridge ha mandato una notifica di sua spontanea volont� , non � una risposta ad una mia specifica richiesta.
		//Questo vuol dire che devo diffondere la notifica a tutti i miei client connessi
		rhea::Allocator *allocator = rhea::getScrapAllocator();

		const u16 notifyID = (u16)msg.what;
		for (u32 i = 0; i < identifiedClientList.getCount(); i++)
		{
			HSokBridgeClient identifiedClientHandle;
			if (identifiedClientList.getHandleByIndex(i, &identifiedClientHandle))
			{
				HSokServerClient h;
				if (identifiedClientList.isBoundToSocket(identifiedClientHandle, &h))
				{
					//BRUTTO allocare ogni volta... forse al posto di handleAnswer() meglio splittare in prepare / send / free in modo da chiamare 1 volta sola prepare, n volte send e 1 free
					CmdHandler_eventReq *handler = CmdHandler_eventReqFactory::spawnFromCPUBridgeEventID(allocator, identifiedClientHandle, notifyID, 0, 10000);
					if (NULL != handler)
					{
						handler->onCPUBridgeNotification(this, h, msg);
						RHEADELETE(allocator, handler);
					}
				}
			}
		}
	}
	else
	{
		//in questo caso, CPUBridge ha risposto ad richiesta iniziata da me tramite un handler che io stesso ho istanziato (vedi priv_onClientHasDataAvail).
		//Recupero l'handler appropriato e gestisco la risposta.
		CmdHandler *handler = cmdHandlerList.findByHandlerID (handlerID);
		if (NULL != handler)
		{
			const HSokBridgeClient identifiedClientHandle = handler->getIdentifiedClientHandle();
			HSokServerClient h;
			if (identifiedClientList.isBoundToSocket(identifiedClientHandle, &h))
			{
				handler->onCPUBridgeNotification(this, h, msg);
				cmdHandlerList.removeAndDeleteByID(localAllocator, handlerID);
			}
			else
			{
				//segno che questo client ha un messaggio pendente, bisogna spedirglielo alla sua riconnessione
				//identifiedClientList.markAsHavPendingRequest(identifiedClientHandle, true);
			}
		}
	}
}

//**************************************************************************
u16 Server::DB_getOrCreateHandle(const u8 *utf8_fullFilePathAndName)
{
	return dbList.getOrCreateDBHandle(rhea::getTimeNowMSec(), utf8_fullFilePathAndName);
}

//**************************************************************************
const rhea::SQLRst*	Server::DB_q(u16 dbHandle, const u8 *utf8_sql)
{
	if (dbList.q(dbHandle, rhea::getTimeNowMSec(), utf8_sql, &rst))
		return &rst;
	return NULL;
}

//**************************************************************************
bool Server::DB_exec (u16 dbHandle, const u8 *utf8_sql)
{
	return dbList.exec(dbHandle, rhea::getTimeNowMSec(), utf8_sql);
}

//**************************************************************************
void Server::DB_closeByPath(const u8 *utf8_fullFilePathAndName)
{
	dbList.closeDBByPath(utf8_fullFilePathAndName);
}

//**************************************************************************
void Server::DB_closeByHandle(u16 dbHandle)
{
	dbList.closeDBByHandle(dbHandle);
}

//**************************************************************************
bool Server::taskSpawnAndRun (const char *taskName, const u8 *params, u32 *out_taskID)
{
	TaskStatus *s = taskFactory->spawnAndRunTask(localAllocator, taskName, params);
	if (NULL == s)
	{
		*out_taskID = 0;
		return false;
	}

	*out_taskID = s->getUID();
	runningTask.append(s);
	return true;
}

//**************************************************************************
bool Server::taskGetStatusAndMesssage (u32 taskID, TaskStatus::eStatus *out_status, char *out_msg, u32 sizeofmsg)
{
	u32 n = runningTask.getNElem();
	for (u32 i = 0; i < n; i++)
	{
		if (runningTask(i)->getUID() == taskID)
		{
			runningTask[i]->getStatusAndMesssage(out_status, out_msg, sizeofmsg);
			if (*out_status == TaskStatus::eStatus::finished)
			{
				TaskStatus::FREE(runningTask[i]);
				runningTask.removeAndSwapWithLast(i);
			}
			return true;
		}
	}
	return false;
}

//**************************************************************************
void Server::priv_onAlipayChinaNotification (rhea::thread::sMsg &msg)
{
	switch (msg.what)
	{
	case ALIPAYCHINA_NOTIFY_ONLINE_STATUS_CHANGED:
		//il modulo AlipayChina che mantiene la connessione col server cinese ha inviato questa
		//notifica per far sapere che il suo stato di connessione � variato
		{
			bool bNotifyAll = false;
			if (msg.paramU32 == 0)
			{
				if (moduleAlipayChina.isOnline)
				{
					//il modulo � andato offline
					moduleAlipayChina.isOnline = false;
					logger->log ("SocketBridgeServer::priv_onAlipayChinaNotification() => offline\n");
					bNotifyAll = true;
				}
			}
			else
			{
				if (!moduleAlipayChina.isOnline)
				{
					//il modulo � andato online
					moduleAlipayChina.isOnline = true;
					logger->log ("SocketBridgeServer::priv_onAlipayChinaNotification() => online\n");
					bNotifyAll = true;
				}
			}

			//notifico i clienti che lo stato della connessione � variato
			if (bNotifyAll)
			{
				for (u32 i = 0; i < identifiedClientList.getCount(); i++)
				{
					HSokBridgeClient hClientHandle;
					if (identifiedClientList.getHandleByIndex(i, &hClientHandle))
					{
						HSokServerClient hServerHandle;
						if (identifiedClientList.isBoundToSocket (hClientHandle, &hServerHandle))
						{
							CmdHandler_eventReq *handler = CmdHandler_eventReqFactory::spawnFromSocketClientEventType(localAllocator, hClientHandle, eEventType::AlipayChina_onlineStatusChanged, priv_getANewHandlerID(), 10000);
							if (NULL != handler)
							{
								handler->handleRequestFromSocketBridge (this, hServerHandle);
								RHEADELETE(localAllocator, handler);
							}
						}
					}
				}
			}
		}
		break;

	case ALIPAYCHINA_NOTIFY_ORDER_STATUS:
		switch ((rhea::AlipayChina::eOrderStatus)msg.paramU32)
		{
		case rhea::AlipayChina::eOrderStatus::paymentOK:
			//L'utente ha pagato, possiamo procedere con la selezione
			if (moduleAlipayChina.curSelRunning)
			{
				cpubridge::ask_CPU_START_SELECTION_WITH_PAYMENT_ALREADY_HANDLED (subscriber, moduleAlipayChina.curSelRunning, moduleAlipayChina.curSelPrice, cpubridge::eGPUPaymentType::alipayChina, moduleAlipayChina.curSelForceJUG);
				moduleAlipayChina.curSelRunning = 0;
				moduleAlipayChina.curSelPrice = 0;
				moduleAlipayChina.curSelForceJUG = false;
				rhea::AlipayChina::ask_endOrder (moduleAlipayChina.ctx, true);
			}
			break;

		case rhea::AlipayChina::eOrderStatus::paymentTimeout:
			//L'utente non ha pagato, selezione terminata
			moduleAlipayChina.curSelRunning = 0;
			moduleAlipayChina.curSelPrice = 0;
			break;

		default:
			break;
		}
		break;


	default:
		logger->log ("SocketBridgeServer::priv_onAlipayChinaNotification() => unknown notification [%d]\n", msg.what);
		break;
	}
}
