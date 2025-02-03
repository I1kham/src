#include "CPUBridgeServer.h"
#include "CPUBridge.h"
#include "CPUChannelCom.h"
#include "CPUBridgeVersion.h"
#include "../rheaCommonLib/rheaBit.h"
#include "../rheaCommonLib/rheaUtils.h"
#include "../rheaCommonLib/rheaMemory.h"
#include "../rheaCommonLib/rheaAllocatorSimple.h"
#include "../rheaCommonLib/rheaLogTargetConsole.h"
#include "EVADTSParser.h"
#include "rhFSProtocol.h"
#include "SECOOs.h"

using namespace cpubridge;






//***************************************************
Server::Server()
{
	localAllocator = NULL;
	chToCPU = NULL;
	server = NULL;
	rsProto = NULL;
	logger = &nullLogger;

	memset(utf16_CPUMasterVersionString, 0, sizeof(utf16_CPUMasterVersionString));

	memset (&runningSel.params, 0, sizeof(runningSel.params));
	runningSel.params.how = eStartSelectionMode_invalid;
	runningSel.sub = NULL;
	runningSel.stopSelectionWasRequested = 0;
	runningSel.status = eRunningSelStatus::finished_OK;

	showCPUStringModelAndVersionUntil_msec = 0;

	memset (&priceHolding, 0, sizeof(priceHolding));
	milkerType = eCPUMilkerType::none;
	quickMenuPinCode = 0;
	
	memset (&screenMsgOverride, 0, sizeof(screenMsgOverride));

	SelectionsEnableLoad();	// caricamento delle abilitazione delle selezioni
}

//***************************************************
void Server::useLogger(rhea::ISimpleLogger *loggerIN)
{
	if (NULL == loggerIN)
		logger = &nullLogger;
	else
		logger = loggerIN;
}

//***************************************************
bool Server::start (CPUChannel *chToCPU_IN, const HThreadMsgR hServiceChR_IN)
{
	chToCPU = chToCPU_IN;
	hServiceChR = hServiceChR_IN;
	logger->log ("CPUBridgeServer::open\n");
	logger->incIndent();

	localAllocator = RHEANEW(rhea::getSysHeapAllocator(), rhea::AllocatorSimpleWithMemTrack) ("cpuBrigeServer");

	//apro la socket TCP
    server = RHEANEW(localAllocator, rhea::ProtocolSocketServer)(8, localAllocator);
    server->useLogger(logger);
	logger->log("opening TCP socket...");
    eSocketError err = server->start (RSPROTO_TCP_PORT);
    if (err != eSocketError::none)
    {
        logger->log("FAIL\n");
        logger->decIndent();
        RHEADELETE(localAllocator, server);
		server = NULL;
        return false;
    }
    logger->log("OK\n");

	
	//recupero l'OSEvent della msgQ e lo aggiunto alla mia wait-list	
	OSEvent	hMsgQEvent;
	rhea::thread::getMsgQEvent (hServiceChR, &hMsgQEvent);
	//waitList.addEvent (hMsgQEvent, u32MAX);
	server->addOSEventToWaitList (hMsgQEvent, u32MAX);

	subscriberList.setup(localAllocator, 8);
	
	//apro il canale di comunicazione con la CPU fisica
	bool ret = chToCPU->isOpen();
	if (ret)	
		logger->log("Started OK\n");
	else
		logger->log("Started FAIL\n");
	logger->decIndent();

	//init language
	lang_init(&language);
	lang_open(&language, "GB");


    //linear buffer per la ricezione di msg dai client
    bufferTCPRead.setup (localAllocator, 128);
	return ret;
}

//***************************************************
void Server::close()
{
	logger->log("CPUBridgeServer::close\n");

	if (NULL == localAllocator)
		return;

	if (NULL != rsProto)
	{
		RHEADELETE(localAllocator, rsProto);
		rsProto = NULL;
	}

	//rimuove l'OSEvent della msgQ dalla mia wait-list	
	OSEvent	hMsgQEvent;
	rhea::thread::getMsgQEvent(hServiceChR, &hMsgQEvent);
	//waitList.removeEvent(hMsgQEvent);
	server->removeOSEventFromWaitList(hMsgQEvent);

	//notifica i subscriber
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
		notify_CPUBRIDGE_DYING(subscriberList(i)->q);
	rhea::thread::sleepMSec(500);
	
	//rimuove i subscriber
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
		priv_deleteSubscriber(subscriberList.getElem(i), false);
	subscriberList.unsetup();

    server->close();
    RHEADELETE(localAllocator, server);
	bufferTCPRead.unsetup();

	RHEADELETE (rhea::getSysHeapAllocator(), localAllocator);
}

//***************************************************
void Server::run()
{
	priv_enterState_compatibilityCheck();

	while (stato.get() != sStato::eStato::quit)
	{
        switch (stato.get())
		{
		case sStato::eStato::compatibilityCheck:
			priv_handleState_compatibilityCheck();
			break;

		case sStato::eStato::CPUNotSupported:
			priv_handleState_CPUNotSupported();
			break;

		case sStato::eStato::DA3_sync:
			priv_handleState_DA3Sync();
			break;

		default:
        case sStato::eStato::comError:
			priv_handleState_comError();
			break;

        case sStato::eStato::normal:
			priv_handleState_normal();
			break;

        case sStato::eStato::selection:
			priv_handleState_selection();
			break;

        case sStato::eStato::programmazione:
			priv_handleState_programmazione();
            break;

		case sStato::eStato::regolazioneAperturaMacina:
			priv_handleState_regolazioneAperturaMacina();
			break;

		case sStato::eStato::telemetry:
			priv_handleState_telemetry();
			break;

		case sStato::eStato::grinderSpeedTest:
			priv_handleState_grinderSpeedTest();
			break;

		case sStato::eStato::downloadPriceHoldingPriceList:
			priv_handleState_downloadPriceHoldingPriceList();
			break;
		}
	}

	logger->log("Quitting due to CPUBRIDGE_SUBSCRIBER_ASK_DIE...\n");
	for (u8 i = 0; i < 10; i++)
		priv_handleMsgQueues(rhea::getTimeNowMSec(), 50);

	while (1)
	{
		if (!priv_handleMsgQueues(rhea::getTimeNowMSec(), 500))
			break;
	}
}

//***************************************************
void Server::priv_deleteSubscriber(sSubscription *sub, bool bAlsoRemoveFromSubsriberList)
{
	//waitList.removeEvent(sub->hEvent);
	server->removeOSEventFromWaitList (sub->hEvent);
	rhea::thread::deleteMsgQ(sub->q.hFromMeToSubscriberR, sub->q.hFromMeToSubscriberW);
	rhea::thread::deleteMsgQ(sub->q.hFromSubscriberToMeR, sub->q.hFromSubscriberToMeW);
	if (bAlsoRemoveFromSubsriberList)
		subscriberList.findAndRemove(sub);
	localAllocator->dealloc(sub);
}

/***************************************************
 * gestisce tutti i msg in ingresso provenienti dal canale di "servizio" o dai subsriber
 */
bool Server::priv_handleMsgQueues(u64 timeNowMSec UNUSED_PARAM, u32 timeOutMSec)
{
    //se ho delle azioni da schedulare...
    actionScheduler.run (this);


	//vediamo se ho dei messaggi in coda
	//const u8 nEvent = waitList.wait (timeOutMSec);
	const u8 nEvent = server->wait (timeOutMSec);

	for (u8 i = 0; i < nEvent; i++)
	{
		//switch (waitList.getEventOrigin(i))
		switch (server->getEventType(i))
		{
		default:
			DBGBREAK;
			break;

		//case OSWaitableGrp::eEventOrigin::osevent:
		case rhea::ProtocolSocketServer::eEventType::osevent_fired:
			//siamo nel caso di eventi generati da una delle msgQ thread safe
			
			//if (waitList.getEventUserParamAsU32(i) == 0x0001)
			if (server->getEventSrcUserParam(i) == 0x0001)
			{
				//evento generato dalla msgQ di uno dei miei subscriber
				for (u32 i2 = 0; i2 < subscriberList.getNElem(); i2++)
				{
					//if (rhea::event::compare(subscriberList(i2)->hEvent, waitList.getEventSrcAsOSEvent(i)))
					if (rhea::event::compare(subscriberList(i2)->hEvent, *server->getEventSrcAsOSEvent(i)))
					{
						priv_handleMsgFromSingleSubscriber(subscriberList.getElem(i2));
						break;
					}
				}
			}
			//else if (waitList.getEventUserParamAsU32(i) == u32MAX)
			else if (server->getEventSrcUserParam(i) == u32MAX)
			{
				//evento generato dalla msg Q di servizio
				priv_handleMsgFromServiceMsgQ();
			}
			break;

		case rhea::ProtocolSocketServer::eEventType::new_client_connected:
			//siamo nel caso in cui qulcuno si � connesso via socket
			break;

		case rhea::ProtocolSocketServer::eEventType::client_has_data_avail:
			//siamo nel caso di eventi generati da una socket di un client connesso
			{
				HSokServerClient hClient = server->getEventSrcAsClientHandle(i);
				priv_handleEventFromSocket (hClient);
			}
			break;

		}
	}

	return (nEvent > 0);
}

/***************************************************
 * Uno dei client ha inviato il msg di identificazione, che � obbligatorio in quanto 
 * io altrimenti rifiuto tutti i suoi futuri messaggio e chiudo la connessione.
 * Decodifico il messaggio e verifico se � il caso di accettare la sua richiesta
 */
bool Server::priv_onMessageIdentifyRcv (HSokServerClient hClient, const rhFSx::proto::sDecodedMsg &ask)
{
	if (ask.payloadLen < 7)
	{
		logger->log ("Refused, invalid payload len\n");
		server->client_sendClose (hClient);
		return false;
	}

	sIdentifiedTCPClient cl;
	cl.hClient = hClient;
	cl.appType = static_cast<rhFSx::proto::eApplicationType>(rhea::utils::bufferReadU32 (ask.payload));
	cl.verMajor = ask.payload[4];
	cl.verMinor = ask.payload[5];
	cl.verBuild = ask.payload[6];
	cl.customValueOnIdentify = 0;

	if (ask.payloadLen >= 11)
		cl.customValueOnIdentify = rhea::utils::bufferReadU32 (&ask.payload[7]);


	//verifichiamo che l'appType sia valido
	bool bAccepted = false;
	switch (cl.appType)
	{
	default:
		logger->log ("Refused, invalid applicationTypeUID [0x%08X]\n", cl.appType);
		break;

	case rhFSx::proto::eApplicationType::RSPROTO:
		//eventuale verifica della versione, per accetazione o rifiuto
		//...
		if (NULL == rsProto)
		{
			rsProto = RHEANEW(localAllocator, RSProto)(server, hClient);
			bAccepted = true;
		}
		break;
	}

	if (!bAccepted)
	{
		logger->log ("App actively refused, app type = 0x%08X, ver %d.%d.%d, custom=0x%08X\n", cl.appType, cl.verMajor, cl.verMinor, cl.verBuild, cl.customValueOnIdentify);
		server->client_sendClose (hClient);
		return false;
	}

	logger->log ("Accepted, app type = 0x%08X, ver %d.%d.%d, custom=0x%08X\n", cl.appType, cl.verMajor, cl.verMinor, cl.verBuild, cl.customValueOnIdentify);

	//rispondo
	u8 bufferW[8];
	u16 nByteToSend = rhFSx::proto::encodeMsg (static_cast<u8>(rhFSx::proto::eAsk::allApp_identifyAnsw), ask.userValue, NULL, 0, bufferW, sizeof(bufferW));
	const u32 nWritten = server->client_writeBuffer (hClient, bufferW, nByteToSend);
	if (u32MAX == nWritten)
	{
		priv_onTCPClientDisconnected (hClient);
		return false;
	}

	rsProto->onSMUStateChanged (cpuStatus, logger);
	return true;
}

//***************************************************
void Server::priv_onTCPClientDisconnected (HSokServerClient hClient)
{
	if (NULL == rsProto)
		return;
	if (!rsProto->isItMe(hClient))
		return;

	//rimuovo il client dalla lista dei client identificati
	logger->log ("Server::priv_onTCPClientDisconnected()");
	logger->incIndent();
	RHEADELETE(localAllocator, rsProto);
	rsProto = NULL;
	logger->decIndent();
}


//***************************************************
void Server::priv_handleEventFromSocket (HSokServerClient hClient)
{
	while (1)
	{
		i32 nBytesLetti = server->client_read (hClient, bufferTCPRead);
		if (nBytesLetti == -1)
		{
			logger->log("connection [0x%08X] closed\n", hClient.asU32());
			priv_onTCPClientDisconnected(hClient);
			return;
		}

		if (nBytesLetti < 1)
		{
			//logger->log("connection [0x%08X] not enought data [%d bytes]\n", h.asU32(), nBytesLetti);
			return;
		}

		//decodifico il messaggio
		rhFSx::proto::sDecodedMsg ask;
		if (!rhFSx::proto::decodeMsg (bufferTCPRead._getPointer(0), nBytesLetti, &ask))
		{
			//non dovrebbe mai succedere perch� se arrivo qui vuol dire che ho ricevuto un messaggio completo 
			DBGBREAK;
			logger->log("Server::priv_handleEventFromSocket() => connection [0x%08X] not enought data [%d]\n", hClient.asU32(), nBytesLetti);
			return;
		}

		switch (static_cast<rhFSx::proto::eAsk>(ask.what))
		{
		default:
			DBGBREAK;
			logger->log("Server::priv_handleEventFromSocket() => invalid msg.what [0x%02X]\n", ask.what);
			break;

		case rhFSx::proto::eAsk::allApp_ping:
			{
				u8 bufferW[8];
				const u16 nByteToSend = rhFSx::proto::encodeMsg (static_cast<u8>(rhFSx::proto::eAsk::allApp_pong), ask.userValue, NULL, 0, bufferW, sizeof(bufferW));
				const u32 nWritten = server->client_writeBuffer (hClient, bufferW, nByteToSend);
				if (u32MAX == nWritten)
					priv_onTCPClientDisconnected (hClient);
			}
			break;

		case rhFSx::proto::eAsk::allApp_identify:
			{
				logger->log ("incoming IDENTIFY msg...\n");
				logger->incIndent();
				priv_onMessageIdentifyRcv (hClient, ask);
				logger->decIndent();
			}
			break;

		case rhFSx::proto::eAsk::rsproto_service_cmd:
		case rhFSx::proto::eAsk::rsproto_variabile:
		case rhFSx::proto::eAsk::rsproto_file:
			if (NULL != rsProto)
				rsProto->onMessageRCV (this, ask.what, ask.userValue, ask.payload, ask.payloadLen, logger);
			break;
		}
	}
}

//***************************************************
Server::sSubscription* Server::priv_newSubscription (u16 subscriberUID)
{
	//creo le msgQ
	sSubscription *sub = RHEAALLOCSTRUCT(localAllocator, sSubscription);
	rhea::thread::createMsgQ (&sub->q.hFromMeToSubscriberR, &sub->q.hFromMeToSubscriberW);
	rhea::thread::createMsgQ (&sub->q.hFromSubscriberToMeR, &sub->q.hFromSubscriberToMeW);
	sub->q.subscriberUID = subscriberUID;
	subscriberList.append (sub);

	//aggiungo la msqQ alla mia waitList
	rhea::thread::getMsgQEvent(sub->q.hFromSubscriberToMeR, &sub->hEvent);
	//waitList.addEvent (sub->hEvent, 0x0001);
	server->addOSEventToWaitList (sub->hEvent, 0x0001);

	return sub;
}

//***************************************************
void Server::scheduleAction_rebootASAP()
{
    actionScheduler.scheduleReboot (5000, 10000);
    logger->log ("scheduleAction_rebootASAP\n");
}

/***************************************************
* Attende un minimo di [n] minuti dopodiche inizia a provare a fare un rebootASAP
* Se durante la fase di attesa viene schedulato un nuovo relaxedReboot, allora il timer dell'attesa riparte da zero
*/
void Server::scheduleAction_relaxedReboot()
{
    //const u32 RELAXED_PERIOD_MSec = 5 * 60 * 1000;	//5 minuti
    const u32 RELAXED_PERIOD_MSec = 15 * 1000;	//15 secondi
    actionScheduler.scheduleReboot (RELAXED_PERIOD_MSec, 10000);
    logger->log ("scheduleAction_relaxedReboot\n");
}

//***************************************************
eActionResult Server::priv_runAction_rebootASAP()
{
    //gli unici stati validi per un reboot ASAP sono lo stato di idle oppure uno stato di errore
    //Se siamo in uno degli altri stati, allora vuol dire che in questo momento GPU/CPU stanno facendo qualcosa e non
    //� il caso di fare il reboot
    if (stato.get() == sStato::eStato::normal || stato.get() == sStato::eStato::CPUNotSupported || stato.get() == sStato::eStato::comError)
    {
        rhea::reboot();
        return eActionResult::finished;
    }

    return eActionResult::needToBeRescheduled;
}

//***************************************************
void Server::scheduleAction_downloadEVADTSAndAnswerToRSProto()
{
    if (!actionScheduler.exists (ActionScheduler::eAction::downloadEVADTSandAnswerToRSProto))
    {
        logger->log ("scheduleAction_downloadEVADTSAndAnswerToRSProto\n");
        actionScheduler.scheduleAction (ActionScheduler::eAction::downloadEVADTSandAnswerToRSProto, 0, 10000);
    }
}
eActionResult Server::priv_runAction_downloadEVADTSAndAnswerToRSProto()
{
    logger->log ("priv_runAction_downloadEVADTSAndAnswerToRSProto:: begin\n");
    //se non c'� il modulo RSProto collegato, non dovrei nemmeno arrivare qui. In ogni caso, abortisco l'operazione
    if (NULL == rsProto)
        return eActionResult::finished;

    //se non ci sono le condizioni per iniziare il download, termino (quest'action verr� rischedulata automaticamente)
    if (!priv_downloadDataAudit_canStartADownload())
    {
        logger->log ("priv_runAction_downloadEVADTSAndAnswerToRSProto:: needToBeRescheduled\n");
        return eActionResult::needToBeRescheduled;
    }

    //avvio la procedura di download da CPU. In caso di successo, il file scaricato si trova in
    // temp/dataAudit[fileID].txt
    u16 fileID;
    if (eReadDataFileStatus::finishedOK == priv_downloadDataAudit (NULL, u16MAX, false, &fileID))
    {
        //a termine dell'operazione, devo comunico a RSProto il nome del file in modo che lui a sua volta possa inviarlo al cloud
        u8 fullFilePathAndName[512];
        rhea::string::utf8::spf (fullFilePathAndName, sizeof(fullFilePathAndName), "%s/temp/dataAudit%d.txt", rhea::getPhysicalPathToAppFolder(), fileID);
        rsProto->sendFileAnswer (RSProto::FILE_EVA_DTS, 0, fullFilePathAndName);
        logger->log ("priv_runAction_downloadEVADTSAndAnswerToRSProto:: got audit file [%s]\n", fullFilePathAndName);
        return eActionResult::finished;
    }
    else
    {
        //mi faccio rischedulare e ci riprovo fra un po'
        logger->log ("priv_runAction_downloadEVADTSAndAnswerToRSProto:: failed, needToBeRescheduled\n");
        return eActionResult::needToBeRescheduled;
    }
}


//***************************************************
void Server::priv_handleMsgFromServiceMsgQ()
{
	rhea::thread::sMsg msg;
	while (rhea::thread::popMsg (hServiceChR, &msg))
	{
		switch (msg.what)
		{
		case CPUBRIDGE_SERVICECH_SUBSCRIPTION_REQUEST:
			//qualcuno vuole iscriversi ai miei eventi
			{
				logger->log("CPUBridgeServer => new SUBSCRIPTION_REQUEST\n");
				logger->incIndent();

				const u16 subscriberUID = rhea::utils::bufferReadU16 ((const u8*)msg.buffer);
				//creo la subscription
				sSubscription *sub = priv_newSubscription(subscriberUID);

				//rispondo al thread richiedente
				HThreadMsgW hToThreadW;
				hToThreadW.initFromU32 (msg.paramU32);
				rhea::thread::pushMsg (hToThreadW, CPUBRIDGE_SERVICECH_SUBSCRIPTION_ANSWER, (u32)CPUBRIDGE_VERSION, &sub->q, sizeof(sSubscriber));

                logger->log("OK\n");
				logger->decIndent();
			}
			break;

		default:
			logger->log("CPUBridgeServer::priv_handleMsgFromServiceMsgQ() => unknown request\n");
			break;
		}
		rhea::thread::deleteMsg(msg);
	}
}

//***************************************************
bool Server::priv_sendAndWaitAnswerFromCPU (const u8 *bufferToSend, u16 nBytesToSend, u8 *out_answer, u16 *in_out_sizeOfAnswer, u64 timeoutRCVMsec)
{
	//invio msg a CPU
	bool ret = chToCPU->sendAndWaitAnswer(bufferToSend, nBytesToSend, out_answer, in_out_sizeOfAnswer, logger, timeoutRCVMsec);
	return ret;
}

//***************************************************
bool Server::priv_setCPUSelectionParam (u8 selNum1ToN, eSelectionParam whichParam, u16 paramValue)
{
	u8 bufferW[32];
	const u16 nBytesToSend = cpubridge::buildMsg_setSelectionParam (selNum1ToN, whichParam, paramValue, bufferW, sizeof(bufferW));
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
		return true;
	return false;
}

//***************************************************
bool Server::priv_getCPUSelectionParam (u8 selNum1ToN, eSelectionParam whichParam, u16 *out_paramValue)
{
	u8 bufferW[32];
	const u16 nBytesToSend = cpubridge::buildMsg_getSelectionParam (selNum1ToN, whichParam, bufferW, sizeof(bufferW));
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
	{
		//rcv: # P [len] 0x34 [selNum] [paramID] [value16 LSB-MSB] [ck]
		*out_paramValue = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[6]);
		return true;
	}
	else
	{
		*out_paramValue = 0;
		return false;
	}
}

/***************************************************
 * chiede a CPU tutti i decount, 10 di prodotto + water_filter, coffee_brewer, coffee_ground, blocking_counter, maintenance
 * [out_decounter15] punta ad un array di almeno 15 u32
 */
bool Server::priv_askCPUAllDecounters (u32 *out_decounter15, u32 sizeof_out_decounter15)
{
	if (sizeof_out_decounter15 < sizeof(u32) * 15)
	{
		DBGBREAK;
		return false;
	}
	
	u8 bufferW[64];
	const u16 nBytesToSend = cpubridge::buildMsg_getAllDecounterValues(bufferW, sizeof(bufferW));
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
		return false;
	
	for (u8 i = 0; i < 14; i++)
		out_decounter15[i] = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[4 + i * 2]);

	//il decounter [14] � passato su 3 byte invece che i normali 2 e non esisteva prima del 2021/05/10
	out_decounter15[14] = 0;
	if (sizeOfAnswerBuffer > 33)
	{
		u8 ct = 32;
		out_decounter15[14] = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[ct]);
		ct += 2;
		out_decounter15[14] |= (((u32)answerBuffer[ct++]) << 16);
	}
	return true;
}

/***************************************************
 * Uno dei miei subscriber mi ha inviato una richiesta
 */
void Server::priv_handleMsgFromSingleSubscriber (sSubscription *sub)
{
	rhea::thread::sMsg msg;
	while (rhea::thread::popMsg (sub->q.hFromSubscriberToMeR, &msg))
	{
		const u16 handlerID = (u16)msg.paramU32;

		//questo � un comando di comodo per simulare "uno qualunque" dei comandi P.
		//In realt�, � necessario gestire qui caso per caso
		if (msg.what == CPUBRIDGE_SUBSCRIBER_ASK_CPU_PROGRAMMING_CMD)
		{
			if (priv_handleProgrammingMessage(sub, handlerID, msg))
				return;

			eCPUProgrammingCommand cmd;
			const u8 *optionalData;
			cpubridge::translate_CPU_PROGRAMMING_CMD(msg, &cmd, &optionalData);
			switch (cmd)
			{
			case eCPUProgrammingCommand::getTimeNextLavaggioCappuccinatore:
				msg.what = CPUBRIDGE_SUBSCRIBER_ASK_TIME_NEXT_LAVSAN_CAPPUCC;
				break;

			default:
				return;
			}
		}

		switch (msg.what)
		{
		default:
			logger->log("CPUBridgeServer::priv_handleMsgFromSubscriber() => invalid msg.what [0x%02X]\n", msg.what);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_DIE:
			stato.set(sStato::eStato::quit);
			break;

		case CPUBRIDGE_SERVICECH_UNSUBSCRIPTION_REQUEST:
			rhea::thread::deleteMsg(msg);
			while (rhea::thread::popMsg (sub->q.hFromSubscriberToMeR, &msg))
				rhea::thread::deleteMsg(msg);
			priv_deleteSubscriber(sub, true);
			return;
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_START_SELECTION:
		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_START_SELECTION_AND_FORCE_JUG:
		{
			sStartSelectionParams params;
			params.how = eStartSelectionMode_default;
			params.asDefault.selNum = 0;
			params.asDefault.bForceJUG = false;

			if (msg.what == CPUBRIDGE_SUBSCRIBER_ASK_CPU_START_SELECTION)
			{
				translate_CPU_START_SELECTION (msg, &params.asDefault.selNum);
			}
			else
			{
				translate_CPU_START_SELECTION_AND_FORCE_JUG (msg, &params.asDefault.selNum);
				params.asDefault.bForceJUG = true;

				//come ulteriore misura di sicurezza, mi accertto che la selezione in questione sia davvero "juggabile"
				if (this->jugRepetitions[params.asDefault.selNum-1] < 2)
					params.asDefault.bForceJUG = false;
			}
			
			//GB 2021-01-14
			//inizialmente l'evento "stato selezione in corso" veniva comunicato solo al "sub" che aveva fatto la richiesta di "inizio selezione".
			//Adesso invece l'evento "stato selezione in corso" viene comunicato a tutti i subscriber attivi. In questo modo se lo "start selezione" arriva da un subsriuber, anche tutti gli altri diventano
			//consapevoli del fatto che c'� una selezione in corso e possono prendere le loro decisioni
			//
			//priv_enterState_selection(params, sub);
			priv_enterState_selection(params, NULL);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_START_SELECTION_WITH_PAYMENT_ALREADY_HANDLED:
			{
				sStartSelectionParams params;
				params.how = eStartSelectionMode_alreadyPaid;
				params.asAlreadyPaid.selNum = 0;
				params.asAlreadyPaid.price = 0;
				params.asAlreadyPaid.paymentType = eGPUPaymentType::invalid;
				params.asAlreadyPaid.bForceJUG = false;

				translate_CPU_START_SELECTION_WITH_PAYMENT_ALREADY_HANDLED(msg, &params.asAlreadyPaid.selNum, &params.asAlreadyPaid.price, &params.asAlreadyPaid.paymentType, &params.asAlreadyPaid.bForceJUG);

				params.asAlreadyPaid.paymentMode = ePaymentMode::normal;
				if ((cpuStatus.flag1 & sCPUStatus::FLAG1_IS_FREEVEND) != 0)
					params.asAlreadyPaid.paymentMode = ePaymentMode::freevend;
				else if ((cpuStatus.flag1 & sCPUStatus::FLAG1_IS_TESTVEND) != 0)
					params.asAlreadyPaid.paymentMode = ePaymentMode::testvend;

				//in caso di "forceJUG", come ulteriore misura di sicurezza, mi accertto che la selezione in questione sia davvero "juggabile"
				if (params.asAlreadyPaid.bForceJUG)
				{
					if (this->jugRepetitions[params.asAlreadyPaid.selNum - 1] < 2)
						params.asAlreadyPaid.bForceJUG = false;
				}


				//GB 2021-01-14
				//inizialmente l'evento "stato selezione in corso" veniva comunicato solo al "sub" che aveva fatto la richiesta di "inizio selezione".
				//Adesso invece l'evento "stato selezione in corso" viene comunicato a tutti i subscriber attivi. In questo modo se lo "start selezione" arriva da un subsriuber, anche tutti gli altri diventano
				//consapevoli del fatto che c'� una selezione in corso e possono prendere le loro decisioni
				//
				//priv_enterState_selection(params, sub);
				priv_enterState_selection(params, NULL);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_DA3_SYNC:
			priv_enterState_DA3Sync();
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_STOP_SELECTION:
			if (stato.get() == sStato::eStato::selection)
				runningSel.stopSelectionWasRequested = 1;
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_SEND_BUTTON_NUM:
		{
			//Ho optato per inviare subito il comando B invece che schedularlo al prossimo giro
			keepOnSendingThisButtonNum = 0;
			u8 btnToSend = 0;
			translate_CPU_SEND_BUTTON(msg, &btnToSend);

			u8 bufferW[32];
			const u16 nBytesToSend = cpubridge::buildMsg_checkStatus_B(btnToSend, lang_getErrorCode(&language), false, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
				priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_KEEP_SENDING_BUTTON_NUM:
		{
			//Da ora in poi, nel comando B invio sempre quest bntNum fino a che non
			//ricevo un analogo msg con btnNum = 0
			translate_CPU_KEEP_SENDING_BUTTON_NUM(msg, &keepOnSendingThisButtonNum);

			u8 bufferW[32];
			const u16 nBytesToSend = cpubridge::buildMsg_checkStatus_B(keepOnSendingThisButtonNum, lang_getErrorCode(&language), false, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
				priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);
		}
		break;


		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_RUNNING_SEL_STATUS:
			priv_notify_CPU_RUNNING_SEL_STATUS(sub, handlerID, runningSel.status);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_FULLSTATE:
			notify_CPU_FULLSTATE(sub->q, handlerID, logger, &cpuStatus);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_INI_PARAM:
		{
			u8 bufferW[32];
			const u8 nBytesToSend = cpubridge::buildMsg_initialParam_C(2, 0, 0, bufferW, sizeof(bufferW));

			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
			{
				priv_parseAnswer_initialParam(answerBuffer, sizeOfAnswerBuffer);
				notify_CPU_INI_PARAM(sub->q, handlerID, logger, &cpuParamIniziali);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_SEL_AVAIL:
			notify_CPU_SEL_AVAIL_CHANGED(sub->q, handlerID, logger, &cpuStatus.selAvailability);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_SEL_PRICES:
        case CPUBRIDGE_SUBSCRIBER_ASK_CPU_SINGLE_SEL_PRICE:
		{
			u8 bufferW[32];
			const u8 nBytesToSend = cpubridge::buildMsg_initialParam_C(2, 0, 0, bufferW, sizeof(bufferW));

			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
			{
				priv_parseAnswer_initialParam(answerBuffer, sizeOfAnswerBuffer);
				const u8 numPrices = NUM_MAX_SELECTIONS;

				if (msg.what == CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_SEL_PRICES)
				{
					if (!priceHolding.isPriceHolding)
						notify_CPU_SEL_PRICES_CHANGED(sub->q, handlerID, logger, numPrices, cpu_numDecimalsForPrices, cpuParamIniziali.pricesAsInAnswerToCommandC);
					else
					{
						u16 prices[NUM_MAX_SELECTIONS + 2];
						for (u8 selNum = 0; selNum < numPrices; selNum++)
						{
							u16 index = cpuParamIniziali.pricesAsInAnswerToCommandC[selNum];
                            if (index > 100)
                                prices[selNum] = 0xffff;
                            else
                                prices[selNum] = cpuParamIniziali.pricesAsInPriceHolding[index];
						}

						notify_CPU_SEL_PRICES_CHANGED(sub->q, handlerID, logger, numPrices, cpu_numDecimalsForPrices, prices);
					}
				}
                else
                {
					//singolo prezzo
                    u8 selNum = 0;
                    translate_CPU_QUERY_SINGLE_SEL_PRICE (msg, &selNum);
                    selNum--;
                    if (selNum < 48)
                    {
						u16 priceToSend = cpuParamIniziali.pricesAsInAnswerToCommandC[selNum];
						if (priceHolding.isPriceHolding)
                        {
                            const u16 index = cpuParamIniziali.pricesAsInAnswerToCommandC[selNum];
                            if (index > 100)
                                priceToSend = 0xffff;
                            else
                                priceToSend = cpuParamIniziali.pricesAsInPriceHolding[index];
                        }

                        rhea::string::format::currency (priceToSend, cpu_numDecimalsForPrices, '.', (char*)bufferW, sizeof(bufferW));
                        notify_CPU_SINGLE_SEL_PRICE (sub->q, handlerID, logger, selNum+1, bufferW);
                    }
                }

			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_LCD_MESSAGE:
			notify_CPU_NEW_LCD_MESSAGE(sub->q, handlerID, logger, &cpuStatus.LCDMsg);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_CURRENT_CREDIT:
			notify_CPU_CREDIT_CHANGED(sub->q, handlerID, logger, cpuStatus.userCurrentCredit, sizeof(cpuStatus.userCurrentCredit));
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_STATE:
			notify_CPU_STATE_CHANGED(sub->q, handlerID, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_QUERY_CUR_SEL_RUNNING:
			if (cpuStatus.VMCstate == eVMCState::PREPARAZIONE_BEVANDA)
				notify_CPU_CUR_SEL_RUNNING(sub->q, handlerID, logger, this->runningSel.getSelNum());
			else
				notify_CPU_CUR_SEL_RUNNING(sub->q, handlerID, logger, 0);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_READ_DATA_AUDIT:
			{
				bool bIncludeBufferDataInNotify = false;
				translate_READ_DATA_AUDIT (msg, &bIncludeBufferDataInNotify);
				if (priv_downloadDataAudit_canStartADownload())
					priv_downloadDataAudit(&sub->q, handlerID, bIncludeBufferDataInNotify, NULL);
				else
					//rifiuto la richiesta perch� non sono in uno stato valido per la lettura del data audit
					notify_READ_DATA_AUDIT_PROGRESS(sub->q, handlerID, logger, eReadDataFileStatus::finishedKO_cantStart_invalidState, 0, 0, NULL, 0);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_READ_VMCDATAFILE:
			if (stato.get() == sStato::eStato::normal)
				priv_downloadVMCDataFile(&sub->q, handlerID);
			else
				//rifiuto la richiesta perch� non sono in uno stato valido per la lettura del file
				notify_READ_VMCDATAFILE_PROGRESS(sub->q, handlerID, logger, eReadDataFileStatus::finishedKO_cantStart_invalidState, 0, 0);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_WRITE_VMCDATAFILE:
		{
			u8 srcFullFileNameAndPath[512];
			translate_WRITE_VMCDATAFILE(msg, srcFullFileNameAndPath, sizeof(srcFullFileNameAndPath));
            if (stato.get() == sStato::eStato::normal || stato.get() == sStato::eStato::CPUNotSupported)
				priv_uploadVMCDataFile(&sub->q, handlerID, srcFullFileNameAndPath);
			else
				//rifiuto la richiesta perch� non sono in uno stato valido per la scrittura del file
				notify_WRITE_VMCDATAFILE_PROGRESS(sub->q, handlerID, logger, eWriteDataFileStatus::finishedKO_cantStart_invalidState, 0);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_VMCDATAFILE_TIMESTAMP:
		{
			sCPUVMCDataFileTimeStamp ts;
			if (!priv_askVMCDataFileTimeStampAndWaitAnswer(&ts, 2000))
				ts.setInvalid();
			notify_CPU_VMCDATAFILE_TIMESTAMP(sub->q, handlerID, logger, ts);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_MILKER_VER:
		{
			u8 bufferW[64];
			const u8 nBytesToSend = buildMsg_getMilkerVer(bufferW, sizeof(bufferW));

			//invio richiesta a CPU
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1500))
			{
				//la CPU risponde con [#] [M] [len] [ASCII 1] [..] [ASCII n] [ck]
				answerBuffer[sizeOfAnswerBuffer - 1] = 0;
				notify_CPU_MILKER_VER(sub->q, handlerID, logger, (const char*)&answerBuffer[3]);
			}
		}
		break;


		case CPUBRIDGE_SUBSCRIBER_ASK_WRITE_CPUFW:
		{
			u8 srcFullFileNameAndPath[512];
			translate_WRITE_CPUFW(msg, srcFullFileNameAndPath, sizeof(srcFullFileNameAndPath));
			priv_uploadCPUFW(&sub->q, handlerID, srcFullFileNameAndPath);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_WRITE_PARTIAL_VMCDATAFILE:
		{
			u8 block[VMCDATAFILE_BLOCK_SIZE_IN_BYTE];
			u8 blocco_n_di = 0;
			u8 tot_num_blocchi = 0;
			u8 blockNumOffset = 0;
			translate_PARTIAL_WRITE_VMCDATAFILE(msg, block, &blocco_n_di, &tot_num_blocchi, &blockNumOffset);

			u8 bufferW[80];
			const u16 nBytesToSend = cpubridge::buildMsg_writePartialVMCDataFile(block, blocco_n_di, tot_num_blocchi, blockNumOffset, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 5000))
			{
				//ok, la CPU ha ricevuto il blocco. A questo punto aggiorno anche il mio da3 file locale
				priv_updateLocalDA3(block, blockNumOffset);

				if (blocco_n_di >= tot_num_blocchi)
				{
					//chiedo alla CPU il nuovo timestamp del file ricevuto e lo salvo localmente
					sCPUVMCDataFileTimeStamp vmcDataFileTimeStamp;
					u8 nRetry = 20;
					while (nRetry--)
					{
						if (priv_askVMCDataFileTimeStampAndWaitAnswer(&vmcDataFileTimeStamp, 2000))
						{
							cpubridge::saveVMCDataFileTimeStamp(vmcDataFileTimeStamp);
							break;
						}
					}
				}

				//aggiorno alcuni dati che conservo anche localmente
				priv_retreiveSomeDataFromLocalDA3();

				//notifico il client
				notify_WRITE_PARTIAL_VMCDATAFILE(sub->q, handlerID, logger, blockNumOffset);
			}
			else
			{
				//la CPU non ha validato il blocco
				notify_WRITE_PARTIAL_VMCDATAFILE(sub->q, handlerID, logger, 0xff);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_DECOUNTER:
		{
			eCPUProg_decounter which = eCPUProg_decounter::unknown;
			u16 valore = 0;
			cpubridge::translate_CPU_SET_DECOUNTER(msg, &which, &valore);

			u8 bufferW[32];
			const u16 nBytesToSend = cpubridge::buildMsg_setDecounter(which, valore, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
			{
				which = (eCPUProg_decounter)answerBuffer[4];
				valore = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5]);
				notify_CPU_DECOUNTER_SET(sub->q, handlerID, logger, which, valore);
			}
			else
				notify_CPU_DECOUNTER_SET(sub->q, handlerID, logger, eCPUProg_decounter::error, 0);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_ALL_DECOUNTER_VALUES:
		{
			u32 decounters[15];
			if (priv_askCPUAllDecounters (decounters, sizeof(decounters)))
				notify_CPU_ALL_DECOUNTER_VALUES(sub->q, handlerID, logger, decounters, sizeof(decounters));
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_EXTENDED_CONFIG_INFO:
		{
			sExtendedCPUInfo info;
			if (priv_prepareSendMsgAndParseAnswer_getExtendedCOnfgInfo_c(&info))
				notify_EXTENDED_CONFIG_INFO(sub->q, handlerID, logger, &info);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_ATTIVAZIONE_MOTORE:
		{
			u8 motore_1_10, durata_dSec, numRipetizioni, pausaTraRipetizioni_dSec;
			cpubridge::translate_CPU_ATTIVAZIONE_MOTORE(msg, &motore_1_10, &durata_dSec, &numRipetizioni, &pausaTraRipetizioni_dSec);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_attivazioneMotore(motore_1_10, durata_dSec, numRipetizioni, pausaTraRipetizioni_dSec, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
				notify_ATTIVAZIONE_MOTORE(sub->q, handlerID, logger, answerBuffer[4], answerBuffer[5], answerBuffer[6], answerBuffer[7]);
			else
				notify_ATTIVAZIONE_MOTORE(sub->q, handlerID, logger, 0xff, 0, 0, 0);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CALCOLA_IMPULSI_GRUPPO:
		{
			u8 macina_1to4 = 0;
			u16 totalePesata_dgram = 0;
			cpubridge::translate_CPU_CALCOLA_IMPULSI_GRUPPO_AA (msg, &macina_1to4, &totalePesata_dgram);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_calcolaImpulsiGruppo_AA (macina_1to4, totalePesata_dgram, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
				notify_CALCOLA_IMPULSI_GRUPPO_STARTED(sub->q, handlerID, macina_1to4, logger);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_STATO_CALCOLO_IMPULSI_GRUPPO:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getStatoCalcoloImpulsiGruppo (bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				u8 stato = answerBuffer[4];
				u16 valore = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5]);
				notify_STATO_CALCOLO_IMPULSI_GRUPPO(sub->q, handlerID, logger, stato, valore);
			}
			else
				notify_STATO_CALCOLO_IMPULSI_GRUPPO(sub->q, handlerID, logger, 0, 0);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_FATTORE_CALIB_MOTORE:
		{
			eCPUProg_motor motore;
			u16 valore;
			cpubridge::translate_CPU_SET_FATTORE_CALIB_MOTORE(msg, &motore, &valore);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_setFattoreCalibMotore(motore, valore, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				eCPUProg_motor motore = (eCPUProg_motor)answerBuffer[4];
				u16 valore = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5]);
				notify_SET_FATTORE_CALIB_MOTORE(sub->q, handlerID, logger, motore, valore);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_STATO_GRUPPO:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getStatoGruppo(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				eCPUProg_statoGruppo stato;
				switch (answerBuffer[4])
				{
				case 0:		stato = eCPUProg_statoGruppo::nonAttaccato; break;
				default:	stato = eCPUProg_statoGruppo::attaccato; break;
				}
				notify_STATO_GRUPPO(sub->q, handlerID, logger, stato);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_TIME:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getTime(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				const u8 hh = answerBuffer[4];
				const u8 mm = answerBuffer[5];
				const u8 ss = answerBuffer[6];
				if (hh<24 && mm<60 && ss<60)
					notify_GET_TIME(sub->q, handlerID, logger, hh, mm, ss);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_DATE:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getDate(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				const u16 yy = 2000 + answerBuffer[4];
				const u8 mm = answerBuffer[5];
				const u8 dd = answerBuffer[6];
				if ((mm>=1 && mm<=12) && (dd>=1 && dd<=31) && (yy>=2017 && yy<=2050))
					notify_GET_DATE(sub->q, handlerID, logger, yy, mm, dd);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_DATE:
		{
			u16 year = 0;
			u8 month = 0;
			u8 day = 0;
			cpubridge::translate_CPU_SET_DATE(msg, &year, &month, &day);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_setDate(bufferW, sizeof(bufferW), year, month, day);
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				const u16 y = 2000 + answerBuffer[4];
				const u8 m = answerBuffer[5];
				const u8 d = answerBuffer[6];
				notify_SET_DATE(sub->q, handlerID, logger, y, m, d);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_TIME:
		{
			u8 hh = 0;
			u8 mm = 0;
			u8 ss = 0;
			cpubridge::translate_CPU_SET_TIME(msg, &hh, &mm, &ss);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_setTime(bufferW, sizeof(bufferW), hh, mm, ss);
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				const u8 hh = answerBuffer[4];
				const u8 mm = answerBuffer[5];
				const u8 ss = answerBuffer[6];
				notify_SET_TIME(sub->q, handlerID, logger, hh, mm, ss);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_POSIZIONE_MACINA:
		{
			u8 macina_1to4 = 0;
			cpubridge::translate_CPU_GET_POSIZIONE_MACINA_AA(msg, &macina_1to4);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getPosizioneMacina_AA(bufferW, sizeof(bufferW), macina_1to4);
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				macina_1to4 = answerBuffer[4] - 10;
				u16 pos = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5]);
				notify_CPU_POSIZIONE_MACINA(sub->q, handlerID, logger, macina_1to4, pos);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_MOTORE_MACINA:
		{
			u8 macina_1to4 = 0;
			eCPUProg_macinaMove m;
			cpubridge::translate_CPU_SET_MOTORE_MACINA_AA (msg, &macina_1to4, &m);

			if (priv_sendAndHandleSetMotoreMacina(macina_1to4, m))
			{
				macina_1to4 = (answerBuffer[4] - 11);
				notify_CPU_MOTORE_MACINA(sub->q, handlerID, logger, macina_1to4, (eCPUProg_macinaMove)answerBuffer[5]);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_POSIZIONE_MACINA:
		{
			u8 macina_1to4 = 0;
			u16 target = 0;
			cpubridge::translate_CPU_SET_POSIZIONE_MACINA_AA(msg, &macina_1to4, &target);
			priv_enterState_regolazioneAperturaMacina (macina_1to4, target);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_TEST_SELEZIONE:
		{
			u8 selNum = 0;
			eCPUProg_testSelectionDevice m;
			cpubridge::translate_CPU_TEST_SELECTION(msg, &selNum, &m);
			
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_testSelection(bufferW, sizeof(bufferW), selNum, m);
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
			{
				selNum = answerBuffer[4];
				m = (eCPUProg_testSelectionDevice)answerBuffer[5];
				notify_CPU_TEST_SELECTION(sub->q, handlerID, logger, selNum, m);
			}
			else
				notify_CPU_TEST_SELECTION(sub->q, handlerID, logger, 0xff, eCPUProg_testSelectionDevice::unknown);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_NOMI_LINGUE_CPU:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getNomiLingueCPU(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
			{
				u16 strLingua1UTF16[33];
				u16 strLingua2UTF16[33];
				memset(strLingua1UTF16, 0, sizeof(strLingua1UTF16));
				memset(strLingua2UTF16, 0, sizeof(strLingua2UTF16));

				u8 z = 5;
				if (answerBuffer[4] == 0x01)
				{
					//caso unicode
					for (u8 i = 0; i < 32;i++)
					{
						strLingua1UTF16[i] = (u16)answerBuffer[z] + (u16)answerBuffer[z + 1] * 256;
						z += 2;
					}
					for (u8 i = 0; i < 32;i++)
					{
						strLingua2UTF16[i] = (u16)answerBuffer[z] + (u16)answerBuffer[z + 1] * 256;
						z += 2;
					}
				}
				else
				{
					for (u8 i = 0; i < 32;i++)
						strLingua1UTF16[i] = (u16)answerBuffer[z++];
					for (u8 i = 0; i < 32;i++)
						strLingua2UTF16[i] = (u16)answerBuffer[z++];
				}
				
				notify_NOMI_LINGE_CPU(sub->q, handlerID, logger, strLingua1UTF16, strLingua2UTF16);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_DISINSTALLAZIONE:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_disintallazione(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_RICARICA_FASCIA_ORARIA_FV:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_ricaricaFasciaOrariaFreevend(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_EVA_RESET_PARTIALDATA:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_EVAresetPartial(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
					notify_EVA_RESET_PARTIALDATA(sub->q, handlerID, logger, true);
				else
					notify_EVA_RESET_PARTIALDATA(sub->q, handlerID, logger, false);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_EVA_RESET_TOTALS:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_EVAresetTotals(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
					notify_CPU_EVA_RESET_TOTALS(sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_VOLT_AND_TEMP:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_getVoltAndTemp(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					const u8 tCamera = answerBuffer[4];
					const u8 tBollitore = answerBuffer[5];
					const u8 tCappuccinatore = answerBuffer[6];
					const u16 voltaggio = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[7]);
					notify_GET_VOLT_AND_TEMP(sub->q, handlerID, logger, tCamera, tBollitore, tCappuccinatore, voltaggio);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_OFF_REPORT:
			{
				u8 indexNum = 0;
				cpubridge::translate_CPU_GET_OFF_REPORT(msg, &indexNum);

				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_getCPUOFFReportDetails(bufferW, sizeof(bufferW), indexNum);
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
				{
					const u8 lastIndexNum = answerBuffer[5];
					const u8 numOffs = (u8) ((sizeOfAnswerBuffer - 7) / 8);
					sCPUOffSingleEvent offs[32];
					u16 ct = 6;
					for (u8 i = 0; i < numOffs; i++)
					{
                        offs[i].codice = answerBuffer[ct++];
                        const u8 tipo = answerBuffer[ct++];
						if ((tipo >= 'a' && tipo <= 'z') || (tipo >= 'A' && tipo <= 'Z') || (tipo >= '0' && tipo <= '9'))
							offs[i].tipo = tipo;
						else
							offs[i].tipo = ' ';
                        offs[i].ora = answerBuffer[ct++];
                        offs[i].minuto = answerBuffer[ct++];
                        offs[i].giorno = answerBuffer[ct++];
                        offs[i].mese = answerBuffer[ct++];
                        offs[i].anno = answerBuffer[ct++];
                        offs[i].stato = answerBuffer[ct++];
					}

                    notify_GET_OFF_REPORT(sub->q, handlerID, logger, indexNum, lastIndexNum, offs, numOffs);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_LAST_FLUX_INFORMATION:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_getLastFluxInformation(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					const u16 lastFlux = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[4]);
					const u16 lastGrinderPos = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[6]);
					notify_GET_LAST_FLUX_INFORMATION(sub->q, handlerID, logger, lastFlux, lastGrinderPos);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SHOW_STR_VERSION_AND_MODEL:
		case CPUBRIDGE_SUBSCRIBER_ASK_GET_CPU_STR_VERSION_AND_MODEL:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_getCPUStringVersionAndModel(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					if (sizeOfAnswerBuffer > 30)
					{
						memset(utf16_CPUMasterVersionString, 0, sizeof(utf16_CPUMasterVersionString));
						if (answerBuffer[4] == 1)
						{
							//CPU ha mandato una stringa in unicode
							memcpy(utf16_CPUMasterVersionString, &answerBuffer[5], 64);
						}
						else
						{
							//CPU ha mandato una stringa in ascii
							for (u8 i = 0; i < 32; i++)
								utf16_CPUMasterVersionString[i] = (u16)answerBuffer[5 + i];
						}

						rhea::string::utf16::rtrim(utf16_CPUMasterVersionString);

						if (msg.what == CPUBRIDGE_SUBSCRIBER_ASK_SHOW_STR_VERSION_AND_MODEL)
						{
							//per i prossimi 7 secondi, nella risposta al comando B inserir� di default il messaggio con la versione e modello
							showCPUStringModelAndVersionUntil_msec = rhea::getTimeNowMSec() + 7000;
						}
						else
						{
							notify_CPU_STRING_VERSION_AND_MODEL(sub->q, handlerID, logger, utf16_CPUMasterVersionString);
						}
					}
				}
			}
			break;

		
		case CPUBRIDGE_SUBSCRIBER_ASK_START_MODEM_TEST:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_startModemTest(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
					notify_CPU_START_MODEM_TEST(sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_TIME_NEXT_LAVSAN_CAPPUCC:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getTimeNextLavaggioSanCappuccinatore(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
			{
				if (sizeOfAnswerBuffer == 7)
				{
					const u8 hh = answerBuffer[4];
					const u8 mm = answerBuffer[5];
					if (hh < 24 && mm < 60)
						notify_GET_TIME_NEXT_LAVSAN_CAPPUCCINATORE(sub->q, handlerID, logger, hh, mm);
				}
			}
		}
		break;


		case CPUBRIDGE_SUBSCRIBER_ASK_START_TEST_ASSORBIMENTO_GRUPPO:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_startTestAssorbimentoGruppo(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				notify_START_TEST_ASSORBIMENTO_GRUPPO(sub->q, handlerID, logger);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_QUERY_TEST_ASSORBIMENTO_GRUPPO:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getStatoTestAssorbimentoGruppo(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
			{
				//[fase] [esito] [report1up LSB MSB]  [report1down LSB MSB] ... [report6up LSB MSB] [report6down LSB MSB]
				const u8 fase = answerBuffer[4];
				const u8 esito = answerBuffer[5];
				u16 results[12];
				for (u8 i=0; i<12; i++)
					results[i] = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[6+2*i]);
				notify_GET_STATUS_TEST_ASSORBIMENTO_GRUPPO(sub->q, handlerID, logger, fase, esito, results);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_START_TEST_ASSORBIMENTO_MOTORIDUTTORE:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_startTestAssorbimentoMotoriduttore(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				notify_START_TEST_ASSORBIMENTO_MOTORIDUTTORE(sub->q, handlerID, logger);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_QUERY_TEST_ASSORBIMENTO_MOTORIDUTTORE:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getStatoTestAssorbimentoMotoriduttore(bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
			{
				//[fase] [esito] [report_up LSB MSB]  [report_down LSB MSB]
				const u8 fase = answerBuffer[4];
				const u8 esito = answerBuffer[5];
				const u16 reportUP = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[6]);
				const u16 reportDOWN = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[8]);
				notify_STATUS_TEST_ASSORBIMENTO_MOTORIDUTTORE(sub->q, handlerID, logger, fase, esito, reportUP, reportDOWN);
			}
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_START_GRINDER_SPEED_TEST:
			{
				u8 macina_1to4 = 0;
				u8 tempoDiMacinaSec = 0;
				cpubridge::translate_CPU_START_GRINDER_SPEED_TEST_AA (msg, &macina_1to4, &tempoDiMacinaSec);
				if (priv_enterState_grinderSpeedTest_AA (macina_1to4, tempoDiMacinaSec))
					notify_CPU_START_GRINDER_SPEED_TEST (sub->q, handlerID, logger, true);
				else
					notify_CPU_START_GRINDER_SPEED_TEST (sub->q, handlerID, logger, false);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_LAST_GRINDER_SPEED:
			notify_CPU_GET_LAST_GRINDER_SPEED (sub->q, handlerID, logger, grinderSpeedTest.lastCalculatedGrinderSpeed);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_VALIDATE_QUICK_MENU_PINCODE:
			{
				u16 pinCode = 0;
				translate_CPU_VALIDATE_QUICK_MENU_PINCODE (msg, &pinCode);
                //if (this->quickMenuPinCode == pinCode || pinCode == CPU_BYPASS_MENU_PROG_PIN_CODE)
				if (this->quickMenuPinCode == pinCode)
					notify_CPU_VALIDATE_QUICK_MENU_PINCODE(sub->q, handlerID, logger, true);
				else
					notify_CPU_VALIDATE_QUICK_MENU_PINCODE(sub->q, handlerID, logger, false);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_ACTIVATE_BUZZER:
		{
			u8 numRepeat = 0;
			u8 beepLen_dSec = 0;
			u8 pausaTraUnBeepELAltro_dSec = 0;
			translate_CPU_ACTIVATE_BUZZER(msg, &numRepeat, &beepLen_dSec, &pausaTraUnBeepELAltro_dSec);

			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_activateCPUBuzzer(numRepeat, beepLen_dSec, pausaTraUnBeepELAltro_dSec, bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_BUZZER_STATUS:
			{
				bool bBuzzerBusy = false;
				u8 bufferW[128];
				const u16 nBytesToSend = cpubridge::buildMsg_getBuzzerStatus(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					//# P [len] [0x28] [status] [ck]
					if (answerBuffer[4] != 0)
						bBuzzerBusy = true;
				}
				notify_CPU_BUZZER_STATUS (sub->q, handlerID, logger, bBuzzerBusy);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_STOP_JUG:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_stopJug(bufferW, sizeof(bufferW));
			
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				notify_CPU_STOP_JUG(sub->q, handlerID, logger, true);
			else
				notify_CPU_STOP_JUG(sub->q, handlerID, logger, false);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_JUG_CURRENT_REPETITION:
		{
			u8 bufferW[16];
			const u16 nBytesToSend = cpubridge::buildMsg_getJugCurrentRepetition(bufferW, sizeof(bufferW));

            u8 nOf = 0;
            u8 m = 0;
            u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
            if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
            {
				//# P [len] [0x28] [n] [m] [ck]
				nOf = answerBuffer[4];
				m = answerBuffer[5];
			}
			notify_CPU_GET_JUG_CURRENT_REPETITION(sub->q, handlerID, logger, nOf, m);
		}
		break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_IS_QUICK_MENU_PINCODE_SET:
			if (this->quickMenuPinCode != 0)
				notify_CPU_IS_QUICK_MENU_PINCODE_SET(sub->q, handlerID, logger, true);
			else
				notify_CPU_IS_QUICK_MENU_PINCODE_SET(sub->q, handlerID, logger, false);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_CPU_SELECTION_NAME_UTF16_LSB_MSB:
			{
				u8 selNum = 0;
				cpubridge::translate_CPU_GET_CPU_SELECTION_NAME_UTF16_LSB_MSB(msg, &selNum);

				u8 bufferW[128];
				const u16 nBytesToSend = cpubridge::buildMsg_richiestaNomeSelezioneDiCPU_d(selNum, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					//# d [len] [numSel] [isUnicode] [msgLenInByte] [msg�..] [ck]
					selNum = answerBuffer[3];
					const u8 isUnicode = answerBuffer[4];
					const u8 msgLenInByte = answerBuffer[5];

					if (isUnicode)
					{
						//CPU mi ha inviato il nome in formato UTF16 LSB-MSB, quindi � gi� formattato come desidero
						const u16 *selName = (const u16*)&answerBuffer[6];
						answerBuffer[6 + msgLenInByte] = 0;
						answerBuffer[7 + msgLenInByte] = 0;
						notify_CPU_GET_CPU_SELECTION_NAME_UTF16_LSB_MSB (sub->q, handlerID, logger, selNum, selName);
					}
					else
					{
						//CPU mi ha inviato il nome in formato ASCII.
						//Io lo devo spedire come UTF16 LSB MSB
						u32 ct = 0;
						for (u8 i = 0; i < msgLenInByte; i++)
						{
							bufferW[ct++] = answerBuffer[6 + i];
							bufferW[ct++] = 0x00;
						}
						bufferW[ct++] = 0x00;
						bufferW[ct++] = 0x00;

						const u16 *selName = (const u16*)bufferW;
						notify_CPU_GET_CPU_SELECTION_NAME_UTF16_LSB_MSB (sub->q, handlerID, logger, selNum, selName);
					}
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_GET_PRICEHOLDING_PRICELIST:
			{
				u8 firstPrice = 0;
				u8 numPrices = 0;
                cpubridge::translate_CPU_GET_PRICEHOLDING_PRICELIST (msg, &firstPrice, &numPrices);
	
				u8 bufferW[128];
				const u16 nBytesToSend = cpubridge::buildMsg_requestPriceHoldingPriceList(firstPrice, numPrices, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					firstPrice = answerBuffer[3];
					numPrices = answerBuffer[4];
					u16 prices[64];
					for (u8 i = 0; i < numPrices; i++)
						prices[i] = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5 +i*2]);

					notify_CPU_GET_PRICEHOLDING_PRICELIST (sub->q, handlerID, logger, firstPrice, numPrices, prices);
				}				
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_GET_MILKER_TYPE:
			notify_MILKER_TYPE(sub->q, handlerID, logger, milkerType);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_JUG_REPETITONS:
			{
				u8	len = sizeof(jugRepetitions) / sizeof jugRepetitions[0];
				u8	buffer[sizeof(jugRepetitions) / sizeof jugRepetitions[0]];
				memcpy(buffer, jugRepetitions, sizeof jugRepetitions);

				notify_CPU_GET_JUG_REPETITIONS(sub->q, handlerID, logger, len, buffer);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_CUPSENSOR_LIVE_VALUE:
			{
				u8 bufferW[32];
				const u16 nBytesToSend = cpubridge::buildMsg_getCupSensorLiveValue(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					const u16 value = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[4]);
					notify_CPU_GET_CUPSENSOR_LIVE_VALUE (sub->q, handlerID, logger, value);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_RUN_CAFFE_CORTESIA:
			{
				u8 macinaDa1a4 = 0;
				cpubridge::translate_CPU_RUN_CAFFE_CORTESIA (msg, &macinaDa1a4);

				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_Programming (eCPUProgrammingCommand::caffeCortesia, &macinaDa1a4, 1, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
					notify_CPU_RUN_CAFFE_CORTESIA (sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_QUERY_ID_101:
			cpubridge::notify_CPU_QUERY_ID101 (sub->q, handlerID, logger, id101);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_END_OF_GRINDER_CLEANING_PROC:
			{
                u8 grinder1toN;
                translate_END_OF_GRINDER_CLEANING_PROCEDURE (msg, &grinder1toN);

				u8 bufferW[16];
                const u16 nBytesToSend = cpubridge::buildMsg_notifyEndOfGrinderCleaningProcedure (grinder1toN, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000);
				notify_END_OF_GRINDER_CLEANING_PROCEDURE (sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_BROWSER_URL_CHANGE:
			{
                //A seguito della creazione delle macchine rhTT1 che non lavorano pi� con il browser incorporato nelle QT ma lavorano con un browser
                //esterno, questa notifica nasce per soddisfare l'esigenza di capire quando il browser vuole "uscire" dalla GUI utente per andare in GUI
                //programmazione e viceversa. Prima questa cosa veniva automaticamente intercettata dalla GPU Qt in quando il browser incorporato inviava
                //una notifica al codice c++ (vedi progetto GPU, mainwindow.cpp fn on_webView_urlChanged).
                //Nel caso di macchine rhTT con browser esterno, bisognava trovare un modo per simulare lo stesso comportamente e quindi ho creato questo
                //messaggio che va inoltrato al "subscriber GPU" per notificarlo del cambio di URL del browser
				char url[200];
				translate_CPU_BROWSER_URL_CHANGE (msg, url, sizeof(url));

                notify_CPU_BROWSER_URL_CHANGE (sub->q, handlerID, logger, url);

                //notifico tutti i sub
				for (u32 i = 0; i < subscriberList.getNElem(); i++)
                {
                    if (&sub->q != &subscriberList(i)->q)
                        notify_CPU_BROWSER_URL_CHANGE (subscriberList(i)->q, 0, logger, url);
                }
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SCIVOLO_BREWMATIC:
			{
				u8 perc0_100;
				translate_CPU_ATTIVAZIONE_SCIVOLO_BREWMATIC (msg, &perc0_100);

				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_scivoloBrewmatic (perc0_100, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000);
				notify_CPU_ATTIVAZIONE_SCIVOLO_BREWMATIC (sub->q, handlerID, logger, perc0_100);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_MSG_FROM_LANGUAGE_TABLE:
			{
				u8 tableID;
				u8 msgRowNum;
				u8 language1or2;
				translate_MSG_FROM_LANGUAGE_TABLE (msg, &tableID, &msgRowNum, &language1or2);

				u8 bufferW[256];
				const u16 nBytesToSend = cpubridge::buildMsg_askMessageFromLanguageTable (tableID, msgRowNum, language1or2, bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
				{
					//rcv: # P [len] 0x31 [tabellaID] [rigaNum] [lingua1o2] [isUnicode] [msg...] [ck]"
					msgRowNum = answerBuffer[5];
					const u8 isUnicode = answerBuffer[7];
					const u8 msgLenInByte = answerBuffer[2] - 9;

					memset (bufferW, 0, sizeof(bufferW));
					if (isUnicode)
					{
						//CPU risponde con una stringa in formato utf16 LSB MSB
						//La converto in utf16 MSB_LSB e poi in utf8
						u16 strUTF16Message[256];
						memset(strUTF16Message, 0, sizeof(strUTF16Message));

						u8 ct = 8;
						for (u8 i = 0; i < msgLenInByte/2; i++)
						{
							strUTF16Message[i] = (u16)answerBuffer[ct] + (u16)answerBuffer[ct + 1] * 256;
							ct += 2;
						}

						
						rhea::string::strUTF16toUTF8 (strUTF16Message, bufferW, sizeof(bufferW));
					}
					else
					{
						//CPU ha risposto con una stringa in ASCII
						u16 strUTF16Message[256];
						memset(strUTF16Message, 0, sizeof(strUTF16Message));
                        answerBuffer[answerBuffer[2] - 1] = 0x00;
						rhea::string::strANSItoUTF16 (reinterpret_cast<const char*>(&answerBuffer[8]), strUTF16Message, sizeof(strUTF16Message));

						//memcpy (bufferW, &answerBuffer[8], msgLenInByte);
						rhea::string::strUTF16toUTF8 (strUTF16Message, bufferW, sizeof(bufferW));
					}

					//Notifico il sub
					notify_MSG_FROM_LANGUAGE_TABLE (sub->q, handlerID, logger, tableID, msgRowNum, bufferW);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_CPU_RESTART:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_restart_U (bufferW, sizeof(bufferW));
				chToCPU->sendOnlyAndDoNotWait(bufferW, nBytesToSend, logger);
				notify_CPU_RESTART (sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_MACHINE_LOCK_STATUS:
			{
				cpubridge::eLockStatus lockStatus;
				translate_SET_MACHINE_LOCK_STATUS (msg, &lockStatus);
				priv_setLockStatus (lockStatus);
				notify_MACHINE_LOCK (sub->q, handlerID, logger, priv_getLockStatus());
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_MACHINE_LOCK_STATUS:
			notify_MACHINE_LOCK (sub->q, handlerID, logger, priv_getLockStatus());
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SELECTION_ENABLE:
			{
				u8 errorCode = 0;
				if (eLockStatus::locked == priv_getLockStatus())
					errorCode = 0xFE;
				else
				{
					u8 selNum1toN;
					bool bEnable;
					translate_SELECTION_ENABLE_DISABLE (msg, &selNum1toN, &bEnable);
					SelectionEnable (selNum1toN, bEnable);
				}
				notify_SELECTION_ENABLE_DISABLE (sub->q, handlerID, logger, errorCode);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_OVERWRITE_CPU_MESSAGE_ON_SCREEN:
			{
				u8 errorCode = 0;
				if (eLockStatus::locked == priv_getLockStatus())
					errorCode = 0xFE;
				else
				{
					const u8 *utf8msg;
					u8 timeSec;
					translate_OVERWRITE_CPU_MESSAGE_ON_SCREEN (msg, &utf8msg, &timeSec);
					memset (&screenMsgOverride, 0, sizeof(screenMsgOverride));
					screenMsgOverride.timeToStopShowingMSec = rhea::getTimeNowMSec() + ((u32)timeSec) * 1000;
					rhea::string::strUTF8toUTF16 (utf8msg, screenMsgOverride.utf16Msg, sizeof(screenMsgOverride.utf16Msg));
				}
				notify_OVERWRITE_CPU_MESSAGE_ON_SCREEN (sub->q, handlerID, logger, errorCode);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SET_SELECTION_PARAMU16:
			{
				u8 selNumDa1aN;
				eSelectionParam whichParam;
				u16 paramValue;
				translate_SET_SELECTION_PARAMU16 (msg, &selNumDa1aN, &whichParam, &paramValue);
				u8 errorCode = 0;
				if (eLockStatus::locked == priv_getLockStatus())
					errorCode = 0xFE;
				else
				{
					if (!priv_setCPUSelectionParam (selNumDa1aN, whichParam, paramValue))
						errorCode = 1;
				}
				notify_SET_SELECTION_PARAMU16 (sub->q, handlerID, logger, selNumDa1aN, whichParam, errorCode, paramValue);
			}
			break;

        case CPUBRIDGE_SUBSCRIBER_ASK_SCHEDULE_ACTION_RELAXED_REBOOT:
            this->scheduleAction_relaxedReboot();
            break;

		case CPUBRIDGE_SUBSCRIBER_ASK_GET_SELECTION_PARAMU16:
			{
				u8 selNumDa1aN;
				eSelectionParam whichParam;
				translate_GET_SELECTION_PARAMU16 (msg, &selNumDa1aN, &whichParam);

				u16 paramValue = 0;
				u8 errorCode = 0;
				if (eLockStatus::locked == priv_getLockStatus())
					errorCode = 0xFE;
				else
				{
					if (!priv_getCPUSelectionParam (selNumDa1aN, whichParam, &paramValue))
						errorCode = 1;
				}
				notify_GET_SELECTION_PARAMU16 (sub->q, handlerID, logger, selNumDa1aN, whichParam, errorCode, paramValue);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SNACK_ENTER_PROG:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_Snack_enterProg_0x04(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
				{
					//rcv: # Y [len] 0x04 [esito] [ck]
					bool result = false;
					if (answerBuffer[4] == 0x01)
						result = true;
					notify_SNACK_ENTER_PROG(sub->q, handlerID, logger, result);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SNACK_EXIT_PROG:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_Snack_exitProg_0x05(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
				{
					//rcv: # Y [len] 0x05 [esito] [ck]
					bool result = false;
					if (answerBuffer[4] == 0x01)
						result = true;
					notify_SNACK_EXIT_PROG(sub->q, handlerID, logger, result);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_SNACK_GET_STATUS:
			{
				u8 bufferW[16];
				const u16 nBytesToSend = cpubridge::buildMsg_Snack_status_0x03(bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
				{
					//rcv: # Y [len] 0x03 [stato] [stato_sel_1-8]... [stato_sel_41-48] [ck]
					bool result = false;
					u8 stato_sel_1_48[6];

					if (answerBuffer[4] == 0x01)
					{
						result = true;
						memcpy (stato_sel_1_48, &answerBuffer[5], 6);
					}
					else
						memset (stato_sel_1_48, 0, sizeof(stato_sel_1_48));

					notify_SNACK_GET_STATUS (sub->q, handlerID, logger, result, stato_sel_1_48);
				}
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_NETWORK_SETTINGS:
			{
				sNetworkSettings info;
				priv_retreiveNetworkSettings (&info);
				notify_NETWORK_SETTINGS (sub->q, handlerID, logger, &info);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_MODEM_LTE_ENABLE:
			{
				bool bEnable;
				cpubridge::translate_MODEM_LTE_ENABLE (msg, &bEnable);
				secoos::modemLTE::enable (bEnable);
				notify_MODEM_LTE_ENABLE (sub->q, handlerID, logger, bEnable);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_WIFI_SET_MODE_HOTSPOT:
			secoos::wifi::setModeHotspot ();
			notify_WIFI_SET_MODE_HOTSPOT (sub->q, handlerID, logger);
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_WIFI_SET_MODE_CONNECTTO:
			{
				const u8 *ssid = NULL;
				const u8 *pwd = NULL;
				cpubridge::translate_WIFI_SET_MODE_CONNECTTO (msg, &ssid, &pwd);
				secoos::wifi::setModeConnectTo (ssid, pwd);
				notify_WIFI_SET_MODE_CONNECTTO (sub->q, handlerID, logger);
			}
			break;

		case CPUBRIDGE_SUBSCRIBER_ASK_WIFI_GET_SSID_LIST:
			{
				u32 nSSID = 0;
				u8 *ssidList = secoos::wifi::getListOfAvailableSSID (rhea::getScrapAllocator(), &nSSID);
				if (NULL == ssidList)
					notify_WIFI_GET_SSID_LIST (sub->q, handlerID, logger, 0, NULL);
				else
				{
					notify_WIFI_GET_SSID_LIST (sub->q, handlerID, logger, static_cast<u8>(nSSID), ssidList);
					RHEAFREE(rhea::getScrapAllocator(), ssidList);
				}
			}
			break;
		} //switch (msg.what)

		rhea::thread::deleteMsg(msg);
	} //while
}

//**********************************************
void Server::priv_getLockStatusFilename (u8 *out_filePathAndName, u32 sizeOfOutFilePathAndName) const
{
	rhea::string::utf8::spf (out_filePathAndName,
							sizeOfOutFilePathAndName,
							"%s/current/lockStatus.rhea",
							rhea::getPhysicalPathToAppFolder());
}

//**********************************************
void Server::priv_writeLockStatus (eLockStatus statusIN) const
{
	u8 s[512];

	priv_getLockStatusFilename (s, sizeof(s));

	const u8 status = static_cast<u8>(statusIN);
	FILE *f = rhea::fs::fileOpenForWriteBinary(s);
	rhea::fs::fileWrite (f, &status, 1);
	rhea::fs::fileClose(f);	
}

//**********************************************
eLockStatus Server::priv_getLockStatus() const
{
	u8 s[512];
	priv_getLockStatusFilename (s, sizeof(s));

	if (rhea::fs::fileExists(s))
	{
		u8 status = 0;
		FILE *f = rhea::fs::fileOpenForReadBinary(s);
		rhea::fs::fileRead(f, &status, 1);
		rhea::fs::fileClose(f);

		return static_cast<eLockStatus>(status);
	}

	return eLockStatus::unlocked;
}

//**********************************************
void Server::priv_setLockStatus (eLockStatus lockMode)
{
	if (lockMode == priv_getLockStatus())
		return;
	priv_writeLockStatus (lockMode);
}

//**********************************************
bool Server::priv_prepareSendMsgAndParseAnswer_getExtendedCOnfgInfo_c(sExtendedCPUInfo *out)
{
	u8 bufferW[16];
	const u16 nBytesToSend = cpubridge::buildMsg_getExtendedConfigInfo(bufferW, sizeof(bufferW));
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1000))
		return false;


	//risposta attesa:	# c [len] [msg_ver] [machine_type] [machine_model] [bollitore_induzione] [brewer_type] [numDecimali] [ck]

	//parsing della risposta
	out->msgVersion = answerBuffer[3];

	//� necessario che la versione del msg sia almeno 2, altrimenti fingo che la CPU non mi abbia nemmeno risposto
	if (out->msgVersion < 2)
		return false;

	//machine type
	switch (answerBuffer[4])
	{
	default: 
		out->machineType = eCPUMachineType::unknown; 
		break;

	case 0x00: 
		out->machineType = eCPUMachineType::instant; 
		break;

	case 0x01:
        out->machineType = eCPUMachineType::espresso1;
        break;
        
	case 0x02:
		out->machineType = eCPUMachineType::espresso2;
		break;
	}
	out->machineModel = answerBuffer[5];
	out->isInduzione = answerBuffer[6];
	
	//Tipo gruppo caff�
	out->tipoGruppoCaffe = eCPUGruppoCaffe::Variflex;
	if (out->msgVersion >= 2)
	{
		if (answerBuffer[2] > 8)
		{
			switch (answerBuffer[7])
			{
			case 'V':	out->tipoGruppoCaffe = eCPUGruppoCaffe::Variflex; break;
			case 'M':	out->tipoGruppoCaffe = eCPUGruppoCaffe::Micro; break;
			default:	out->tipoGruppoCaffe = eCPUGruppoCaffe::None; break;
			}
		}
	}

	//numero cifre decimali nei prezzi
	//In realt�, ignoro questa informazione in quanto la prendo direttamente dal DA3, vedi this->cpu_numDecimalsForPrices
	//if (out->msgVersion >= 3)
	//	out->numDecimaliNeiPrezzi = answerBuffer[8];

	return true;
}

//**********************************************
void Server::priv_updateLocalDA3 (const u8 *blockOf64Bytes, u8 blockNum) const
{
	u8 s[256];
	sprintf_s((char*)s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
	
	rhea::Allocator *allocator = rhea::getScrapAllocator();
	u32 fileSize = 0;
	u8 *buffer = rhea::fs::fileCopyInMemory(s, allocator, &fileSize);
	if (NULL == buffer)
		return;

	const u32 blockPosition = VMCDATAFILE_BLOCK_SIZE_IN_BYTE * blockNum;
	if (fileSize >= blockPosition + VMCDATAFILE_BLOCK_SIZE_IN_BYTE)
	{
		memcpy(&buffer[blockPosition], blockOf64Bytes, VMCDATAFILE_BLOCK_SIZE_IN_BYTE);

		const u32 SIZE = 1024;
		FILE *f = rhea::fs::fileOpenForWriteBinary(s);
		u32 ct = 0;
		while (fileSize >= SIZE)
		{
			fwrite(&buffer[ct], SIZE, 1, f);
			ct += SIZE;
			fileSize -= SIZE;
		}
		if (fileSize)
			fwrite(&buffer[ct], fileSize, 1, f);
        rhea::fs::fileClose(f);
	}
	RHEAFREE(allocator, buffer);

    //salvo data e ora dell'ultima modifica
    rhea::DateTime dt;
    dt.setNow();
    sprintf_s((char*)s, sizeof(s), "%s/last_installed/da3/dateUM.bin", rhea::getPhysicalPathToAppFolder());
    FILE *f = rhea::fs::fileOpenForWriteBinary(s);
    u64 u = dt.getInternalRappresentation();
    fwrite (&u, sizeof(u64), 1, f);
    rhea::fs::fileClose(f);
}

//**********************************************
bool Server::priv_handleProgrammingMessage (sSubscription *sub, u16 handlerID, const rhea::thread::sMsg &msg)
{
	eCPUProgrammingCommand cmd;
	const u8 *optionalData;
	cpubridge::translate_CPU_PROGRAMMING_CMD (msg, &cmd, &optionalData);


	u8 bufferW[32];
	u8 nBytesToSend = 0;
	
	switch (cmd)
	{
	default:
		return false;
		break;

	case eCPUProgrammingCommand::enterProg:
		//il comando "vai in modalit� prog" lo mando solo se la CPU non � gi� in modalit� PROG
		if (cpuStatus.VMCstate != eVMCState::PROGRAMMAZIONE)
			nBytesToSend = cpubridge::buildMsg_Programming(cmd, NULL, 0, bufferW, sizeof(bufferW));
		break;


	case eCPUProgrammingCommand::querySanWashingStatus:
		nBytesToSend = cpubridge::buildMsg_Programming (cmd, NULL, 0, bufferW, sizeof(bufferW));
		break;

	case eCPUProgrammingCommand::cleaning:
		//in [optionalData] c'� un byte che indica il tipo di lavaggio
		nBytesToSend = cpubridge::buildMsg_Programming(cmd, optionalData, 1, bufferW, sizeof(bufferW));
		break;
	}


	if (nBytesToSend)
	{
		u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
		if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 1500))
		{
			logger->log("ERR sending P[%d] command to CPU\n", cmd);
		}
		else
		{
			//in generale, non mi interessa la risposta della CPU ai comandi prog, a parte per alcune eccezioni.
			//La cpu risponde sempre con:
			// [#] [P] [len] [subcommand] [optional_data] [ck]
			switch ((eCPUProgrammingCommand)answerBuffer[3])
			{
            default:
                break;

			case eCPUProgrammingCommand::querySanWashingStatus:
				//la CPU risponde con 3 bytes che indicano:
				//	b0 => fase del lavaggio
				//	b1 => se != da 0 allora vuol dire che la CPU � in attesa della pressione del tasto b-esimo
				//	b2 => come sopra (in pratica la CPU pu� essere in attesa della pressione del tasto b1 oppure del tasto b2
				//In ogni caso, io ignoro queste cose, mi limito a notificare i miei client
				if (NULL != sub && answerBuffer[2] >= 8)
				{
					if (NULL != sub)
					{
						//a partire da 2021-02-04, sono stati aggiunti 8 byte in coda a quanto gi� esistente
						u8 buffer8[8];
						memset (buffer8, 0, sizeof(buffer8));
						if (answerBuffer[2] >= 16)
							memcpy (buffer8, &answerBuffer[7], 8);
						notify_SAN_WASHING_STATUS(sub->q, handlerID, logger, answerBuffer[4], answerBuffer[5], answerBuffer[6], buffer8);
					}
				}
				break;
			}
		}
	}
	return true;
}

//**********************************************
u8 Server::priv_2DigitHexToInt(const u8 *buffer, u32 index) const
{
	u32 ret = 0;
	rhea::string::ansi::hexToInt((const char*)&buffer[index], &ret, 2);
	return (u8)ret;
}


/**********************************************
 * copiato da codice originale e lievemente ripulito, sorry.
 *	Ritorna 0 se tutto OK
 */
bool Server::priv_WriteByteMasterNext (u8 dato_8, bool isLastFlag, u8 *out_bufferW, u32 &in_out_bufferCT)
{
	if (!isLastFlag)
	{
		out_bufferW[in_out_bufferCT++] = dato_8;
		if (in_out_bufferCT != CPUFW_BLOCK_SIZE)
			return true;
	}

	chToCPU->sendOnlyAndDoNotWait(out_bufferW, in_out_bufferCT, logger);

	u8 checksum_counter = 0;
	for (u32 i = 0; i < in_out_bufferCT; i++)
		checksum_counter += out_bufferW[i];

	if (isLastFlag) 
		return true;

	if (!chToCPU->waitForASpecificChar(checksum_counter, 190))
		return false;

	in_out_bufferCT = 0;
	return true;
}


/***************************************************
 * Dato un file mhx localmente accessibile, lo usa per sovrascrivere il FW della CPU
 * Periodicamente emette un notify_WRITE_CPUFW_PROGRESS() con lo stato dell'upload.
 * Al termine dell'upload, se tutto � andato bene, il mhx sorgente viene copiato pari pari (compreso il suo nome originale) in
 *	app/last_installed/cpu/nomeFileSrc.mhx
 */
eWriteCPUFWFileStatus Server::priv_uploadCPUFW(cpubridge::sSubscriber *subscriber, u16 handlerID, const u8 *srcFullFileNameAndPath)
{
	u8 cpuFWFileNameOnly[256];
	u8 tempFilePathAndName[512];
	rhea::fs::extractFileNameWithExt(srcFullFileNameAndPath, cpuFWFileNameOnly, sizeof(cpuFWFileNameOnly));

	if (rhea::string::utf8::areEqualWithLen (srcFullFileNameAndPath, (const u8*)"APP:/temp/", false, 10))
	{
		//il file si suppone gi� esistere nella mia cartella temp
		sprintf_s((char*)tempFilePathAndName, sizeof(tempFilePathAndName), "%s/temp/%s", rhea::getPhysicalPathToAppFolder(), cpuFWFileNameOnly);
	}
	else
	{
		//copio il file nella mia directory locale temp
		sprintf_s((char*)tempFilePathAndName, sizeof(tempFilePathAndName), "%s/temp/%s", rhea::getPhysicalPathToAppFolder(), cpuFWFileNameOnly);

		if (!rhea::string::utf8::areEqual(srcFullFileNameAndPath, tempFilePathAndName, true))
		{
			if (!rhea::fs::fileCopy(srcFullFileNameAndPath, tempFilePathAndName))
			{
				if (NULL != subscriber)
					notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_unableToCopyFile, 0);
				return eWriteCPUFWFileStatus::finishedKO_unableToCopyFile;
			}
		}
	}


	if (!rhea::fs::fileExists(tempFilePathAndName))
	{
		if (NULL != subscriber)
			notify_WRITE_CPUFW_PROGRESS (*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_unableToOpenLocalFile, 0);
		return eWriteCPUFWFileStatus::finishedKO_unableToOpenLocalFile;
	}

	//invalido il timestamp del mio da3 locale in modo che al termine dell'operazione di upload
	//del FW CPU, la GPU attivi una fase di sincronizzazione da3
	{
		sCPUVMCDataFileTimeStamp localDA3TimeStamp;
		localDA3TimeStamp.setInvalid();
		saveVMCDataFileTimeStamp(localDA3TimeStamp);
	}


	//per prima cosa si fa fare il reboot alla CPU
	u8 bufferW[CPUFW_BLOCK_SIZE+16];
	u8 nBytesToSend = cpubridge::buildMsg_restart_U(bufferW, 64);
	chToCPU->sendOnlyAndDoNotWait(bufferW, nBytesToSend, logger);
	//chToCPU->closeAndReopen();

	//aspetto di leggere 'k' dal canale
    if (!chToCPU->waitForASpecificChar('k', 10000))
	{
		if (NULL != subscriber)
			notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_k_notReceived, 0);
		return eWriteCPUFWFileStatus::finishedKO_k_notReceived;
	}

	bufferW[0] = 'k';
	chToCPU->sendOnlyAndDoNotWait(bufferW, 1, logger);

	bufferW[0] = 'M';
	chToCPU->sendOnlyAndDoNotWait(bufferW, 1, logger);

	//aspetto di leggere 'M' dal canale
	if (!chToCPU->waitForASpecificChar('M', 10000))
	{
		if (NULL != subscriber)
			notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_M_notReceived, 0);
		return eWriteCPUFWFileStatus::finishedKO_M_notReceived;
	}

	//erasing flash
	if (NULL != subscriber)
		notify_WRITE_CPUFW_PROGRESS(*subscriber, 0, logger, eWriteCPUFWFileStatus::inProgress_erasingFlash, 0);

	u8 s[512];
	sprintf_s((char*)s, sizeof(s), "%s/last_installed/cpu", rhea::getPhysicalPathToAppFolder());
	rhea::fs::deleteAllFileInFolderRecursively(s, false);


	//aspetto di leggere 'h' dal canale
	if (!chToCPU->waitForASpecificChar('h', 15000))
	{
		if (NULL != subscriber)
			notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_h_notReceived, 0);
		return eWriteCPUFWFileStatus::finishedKO_h_notReceived;
	}



	//Carico il file mhx in memoria
	rhea::Allocator *allocator = rhea::getScrapAllocator();
	u32 fsize = 0;
	u8 *fileInMemory = rhea::fs::fileCopyInMemory(tempFilePathAndName, allocator, &fsize);
	

	//inizio scrittura flash
    u32 ct = 0;
	u8 recType = 0;
	u32 bufferWCT = 0;
	u16 lastKbWriteSent = u16MAX;
	u16 kbWrittenSoFar = 0;
	eWriteCPUFWFileStatus ret = eWriteCPUFWFileStatus::finishedOK;
	do
	{
		if (fileInMemory[ct] != 'S')
		{
			ret = eWriteCPUFWFileStatus::finishedKO_generalError;
			if (NULL != subscriber)
				notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, ret, 5);
			break;
		}

		recType = fileInMemory[ct + 1];
        const u8 numByte = priv_2DigitHexToInt(fileInMemory, ct + 2);
		ct += 4;

		if (recType == '0' || recType == '3' || recType == '8' || recType == '9' || numByte == 4)
		{
			ct += (numByte + 1) * 2;
			continue;
		}

		if (!priv_WriteByteMasterNext(numByte, false, bufferW, bufferWCT))
		{
			ret = eWriteCPUFWFileStatus::finishedKO_generalError;
			if (NULL != subscriber)
				notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, ret, 6);
			break;
		}

		for (u8 i = 0; i < (numByte - 1); i++)
		{
			const u8 dato8 = priv_2DigitHexToInt(fileInMemory, ct);
			ct += 2;
			if (!priv_WriteByteMasterNext(dato8, false, bufferW, bufferWCT))
			{
				ret = eWriteCPUFWFileStatus::finishedKO_generalError;
				if (NULL != subscriber)
					notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, ret, 7);
				break;
			}
		}
		ct += 4;

		//notifico che ho letto un altro Kb
		kbWrittenSoFar = (u16)(ct >> 10);
		if (kbWrittenSoFar != lastKbWriteSent)
		{
			lastKbWriteSent = kbWrittenSoFar;

			if (NULL != subscriber)
				notify_WRITE_CPUFW_PROGRESS(*subscriber, 0, logger, eWriteCPUFWFileStatus::inProgress, kbWrittenSoFar);
		}

	} while (recType < '7');
	
	if (ret == eWriteCPUFWFileStatus::finishedOK)
	{
		priv_WriteByteMasterNext(4, false, bufferW, bufferWCT);
		priv_WriteByteMasterNext(0, true, bufferW, bufferWCT);

		u8 ck = 0;
		for (u32 i = 0; i < bufferWCT; i++)
			ck += bufferW[i];
		if (!chToCPU->waitForASpecificChar(ck, 90))
		{
			ret = eWriteCPUFWFileStatus::finishedKO_generalError;
			if (NULL != subscriber)
				notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedKO_generalError, 8);
		}
		else	
		{
			if (NULL != subscriber)
				notify_WRITE_CPUFW_PROGRESS(*subscriber, handlerID, logger, eWriteCPUFWFileStatus::finishedOK, kbWrittenSoFar);
		}
	}
	RHEAFREE(allocator, fileInMemory);

	//in caso di successo, copio il file mhx nella mia cartella app/last_installed/cpu/nomeFileSrc.mhx
	if (ret == eWriteCPUFWFileStatus::finishedOK)
	{
		sprintf_s((char*)s, sizeof(s), "%s/last_installed/cpu/%s", rhea::getPhysicalPathToAppFolder(), cpuFWFileNameOnly);
		rhea::fs::fileCopy(tempFilePathAndName, s);
		rhea::fs::fileDelete(tempFilePathAndName);

        //reboot CPU
        nBytesToSend = cpubridge::buildMsg_restart_U(bufferW, 64);
        chToCPU->sendOnlyAndDoNotWait(bufferW, nBytesToSend, logger);
	}
	return ret;
}


/***************************************************
 * Dato un file da3 localmente accessibile, lo invia alla CPU.
 * Periodicamente emette un notify_WRITE_VMCDATAFILE_PROGRESS() con lo stato dell'upload.
 * Al termine dell'upload, se tutto � andatao bene, il da3 sorgente viene copiato pari pari (compreso il suo nome originale) in 
 *	app/last_installed/da3/nomeFileSrc.da3 e anche in  app/current/da3/vmcDataFile.da3
 * Al termine della copia, chiede a CPU il timestamp del da3 e lo salva in current/da3/vmcDataFile.timestamp
 */
eWriteDataFileStatus Server::priv_uploadVMCDataFile (cpubridge::sSubscriber *subscriber, u16 handlerID, const u8 *srcFullFileNameAndPath)
{
	u8 fileName[256];
	u8 tempFilePathAndName[512];
	rhea::fs::extractFileNameWithExt(srcFullFileNameAndPath, fileName, sizeof(fileName));

	if (rhea::string::utf8::areEqualWithLen(srcFullFileNameAndPath, (const u8*)"APP:/temp/", false, 10))
	{
		//il file si suppone gi� esistere nella mia cartella temp
		sprintf_s((char*)tempFilePathAndName, sizeof(tempFilePathAndName), "%s/temp/%s", rhea::getPhysicalPathToAppFolder(), fileName);
	}
	else
	{
		//copio il file nella mia directory locale temp
		sprintf_s((char*)tempFilePathAndName, sizeof(tempFilePathAndName), "%s/temp/%s", rhea::getPhysicalPathToAppFolder(), fileName);

		if (!rhea::string::utf8::areEqual(srcFullFileNameAndPath, tempFilePathAndName, true))
		{
			if (!rhea::fs::fileCopy(srcFullFileNameAndPath, tempFilePathAndName))
			{
				notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eWriteDataFileStatus::finishedKO_unableToCopyFile, 0);
				return eWriteDataFileStatus::finishedKO_unableToCopyFile;
			}
		}
	}

	FILE *f = rhea::fs::fileOpenForReadBinary(tempFilePathAndName);
	if (NULL == f)
	{
		notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eWriteDataFileStatus::finishedKO_unableToOpenLocalFile, 0);
		return eWriteDataFileStatus::finishedKO_unableToOpenLocalFile;
	}

	//il meccanismo attuale prevedere che la io mandi pacchetti da 64b di dati alla CPU fino al raggiungimento della
	//dimensione totale del file. Il tutto � hard-codato...
	u32 bytesWrittenSoFar = 0;
	u16 kbWrittenSoFar = 0;
	u16 lastKbWriteSent = u16MAX;
	const u8 TOT_NUM_BLOCKS = VMCDATAFILE_TOTAL_FILE_SIZE_IN_BYTE / VMCDATAFILE_BLOCK_SIZE_IN_BYTE;
	for (u8 blockNum=0; blockNum < TOT_NUM_BLOCKS; blockNum++)
	{
		//leggo da file
		u8 fileBuffer[VMCDATAFILE_BLOCK_SIZE_IN_BYTE];
        rhea::fs::fileRead (f, fileBuffer, VMCDATAFILE_BLOCK_SIZE_IN_BYTE);

		//creo il msg da mandare a CPU
		u8 bufferCPUMsg[VMCDATAFILE_BLOCK_SIZE_IN_BYTE +32];
		const u16 nBytesToSend = buildMsg_writeVMCDataFile(fileBuffer, blockNum, TOT_NUM_BLOCKS, bufferCPUMsg, sizeof(bufferCPUMsg));
		
		//invio
		u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
		if (!priv_sendAndWaitAnswerFromCPU(bufferCPUMsg, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
		{
			//errore, la CPU non ha risposto, abortisco l'operazione
			if (NULL != subscriber)
				notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eWriteDataFileStatus::finishedKO_cpuDidNotAnswer, kbWrittenSoFar);
            rhea::fs::fileClose(f);
			return eWriteDataFileStatus::finishedKO_cpuDidNotAnswer;
		}

		//aggiorno contatore di byte scritti
		bytesWrittenSoFar += VMCDATAFILE_BLOCK_SIZE_IN_BYTE;
		kbWrittenSoFar = (u16)(bytesWrittenSoFar >> 10);

		//notifico che ho letto un altro Kb
		if (kbWrittenSoFar != lastKbWriteSent)
		{
			lastKbWriteSent = kbWrittenSoFar;

			if (NULL != subscriber)
				notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, 0, logger, eWriteDataFileStatus::inProgress, kbWrittenSoFar);
		}
	}

	//finito!
    rhea::fs::fileClose(f);
	assert(bytesWrittenSoFar >= VMCDATAFILE_TOTAL_FILE_SIZE_IN_BYTE);

	//dopo qualche esperimento, ho notato che a seguito di un upload, la CPU cmq fa delle modifiche al DA3.
	//Chiedo quindi quei blocchi che la CPU ha modificato
	{
		u32 sizeOfBuffer= 0;
		u8 *tempBuffer = rhea::fs::fileCopyInMemory(tempFilePathAndName, localAllocator, &sizeOfBuffer);

		u16 block = 151;
		if (!priv_prepareAndSendMsg_readVMCDataFileBlock(block))
		{
			if (NULL != subscriber)
				notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eWriteDataFileStatus::finishedKO_cpuDidNotAnswer2, kbWrittenSoFar);
			return eWriteDataFileStatus::finishedKO_cpuDidNotAnswer2;
		}
		memcpy (&tempBuffer[block*VMCDATAFILE_BLOCK_SIZE_IN_BYTE], &answerBuffer[5], VMCDATAFILE_BLOCK_SIZE_IN_BYTE);

		//sovrascrivo il file con le info
		f = rhea::fs::fileOpenForWriteBinary(tempFilePathAndName);
		fwrite (tempBuffer, sizeOfBuffer, 1, f);
        rhea::fs::fileClose(f);

		RHEAFREE(localAllocator, tempBuffer);
	}



	//a questo punto, copio il file temporaneo nella mia cartella last_installed e nella cartella current (qui gli cambio anche il nome con quello di default)
	u8 s[512];
	sprintf_s((char*)s, sizeof(s), "%s/last_installed/da3", rhea::getPhysicalPathToAppFolder());
	rhea::fs::deleteAllFileInFolderRecursively(s, false);

	sprintf_s((char*)s, sizeof(s), "%s/last_installed/da3/%s", rhea::getPhysicalPathToAppFolder(), fileName);
	rhea::fs::fileCopy(tempFilePathAndName, s);

    sprintf_s((char*)s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
	rhea::fs::fileCopy(tempFilePathAndName, s);

	rhea::fs::fileDelete(tempFilePathAndName);


	//chiedo alla CPU il nuovo timestamp del file ricevuto e lo salvo localmente
	sCPUVMCDataFileTimeStamp	vmcDataFileTimeStamp;
    u8 nRetry = 20;
    while (nRetry--)
    {
        if (priv_askVMCDataFileTimeStampAndWaitAnswer(&vmcDataFileTimeStamp, 2000))
        {
            cpubridge::saveVMCDataFileTimeStamp(vmcDataFileTimeStamp);
            break;
        }
    }

    //salvo data e ora dell'ultima modifica
    rhea::DateTime dt;
    dt.setNow();
    sprintf_s((char*)s, sizeof(s), "%s/last_installed/da3/dateUM.bin", rhea::getPhysicalPathToAppFolder());
    f = rhea::fs::fileOpenForWriteBinary(s);
    u64 u = dt.getInternalRappresentation();
    fwrite (&u, sizeof(u64), 1, f);
    rhea::fs::fileClose(f);

	//aggiorno alcuni dati che conservo in memoria
	priv_retreiveSomeDataFromLocalDA3();

	//notifico il client e finisco
	if (NULL != subscriber)
		notify_WRITE_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eWriteDataFileStatus::finishedOK, kbWrittenSoFar);

	return eWriteDataFileStatus::finishedOK;
}

//***************************************************
u16 Server::priv_prepareAndSendMsg_readVMCDataFileBlock (u16 blockNum)
{
	u8 bufferW[32];
	const u16 nBytesToSend = buildMsg_readVMCDataFile((u8)blockNum, bufferW, sizeof(bufferW));

	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
		return 0;
	return sizeOfAnswerBuffer;
}

/***************************************************
 * Chiede alla CPU il file di da3 e lo salva localmente in app/temp/vmcDataFile%d.da3
 * [%d] � un progressivo che viene comunicato alla fine del download nel messaggio di conferma (e messo anche in out_fileID in caso di successo)
 * Periodicamente notifica emettendo un messaggio notify_READ_VMCDATAFILE_PROGRESS() che contiene lo stato di avanzamento
 * del download
 */
eReadDataFileStatus Server::priv_downloadVMCDataFile(cpubridge::sSubscriber *subscriber, u16 handlerID, u16 *out_fileID)
{
	//il file che scarico dalla CPU lo salvo localmente con un nome ben preciso
	u8 fullFilePathAndName[256];
	u16 fileID = 0;
	while (1)
	{
		sprintf_s((char*)fullFilePathAndName, sizeof(fullFilePathAndName), "%s/temp/vmcDataFile%d.da3", rhea::getPhysicalPathToAppFolder(), fileID);
		if (!rhea::fs::fileExists(fullFilePathAndName))
			break;
		fileID++;
	}

	FILE *f = rhea::fs::fileOpenForWriteBinary(fullFilePathAndName);
	if (NULL == f)
	{
		notify_READ_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eReadDataFileStatus::finishedKO_unableToCreateFile, 0, fileID);
		return eReadDataFileStatus::finishedKO_unableToCreateFile;
	}

	//il meccanismo attuale prevedere che la CPU mandi pacchetti da 64b di dati fino al raggiungimento della
	//dimensione totale del file. Il tutto � hard-codato...
	u32 nPacketSoFar = 0;
	u32 bytesReadSoFar = 0;
	u16 kbReadSoFar = 0;
	u16 lastKbReadSent = u16MAX;
	while (1)
	{
		u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_readVMCDataFileBlock(nPacketSoFar++);
		if (0 == sizeOfAnswerBuffer)
		{
			//errore, la CPU non ha risposto, abortisco l'operazione
			if (NULL == subscriber)
			{
				for (u32 i = 0; i < subscriberList.getNElem(); i++)
					notify_READ_VMCDATAFILE_PROGRESS(subscriberList(i)->q, 0, logger, eReadDataFileStatus::finishedKO_cpuDidNotAnswer, kbReadSoFar, fileID);
			}
			else
				notify_READ_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eReadDataFileStatus::finishedKO_cpuDidNotAnswer, kbReadSoFar, fileID);

            rhea::fs::fileClose(f);
			return eReadDataFileStatus::finishedKO_cpuDidNotAnswer;
		}

		//scrivo su file i dati ricevuti
		const u8 payloadLen = sizeOfAnswerBuffer - 6;
		const u8 *payload = &answerBuffer[5];
		fwrite(payload, payloadLen, 1, f);

		//aggiorno contatore di byte letti
		bytesReadSoFar += payloadLen;
		kbReadSoFar = (u16)(bytesReadSoFar >> 10);


		if (bytesReadSoFar >= VMCDATAFILE_TOTAL_FILE_SIZE_IN_BYTE)
		{
			//finito!
			if (NULL != out_fileID )
				*out_fileID = fileID;
			if (NULL == subscriber)
			{
				for (u32 i = 0; i < subscriberList.getNElem(); i++)
					notify_READ_VMCDATAFILE_PROGRESS(subscriberList(i)->q, 0, logger, eReadDataFileStatus::finishedOK, kbReadSoFar, fileID);
			}
			else
				notify_READ_VMCDATAFILE_PROGRESS(*subscriber, handlerID, logger, eReadDataFileStatus::finishedOK, kbReadSoFar, fileID);

            rhea::fs::fileClose(f);
			return eReadDataFileStatus::finishedOK;
		}

		//notifico che ho letto un altro Kb
		if (kbReadSoFar != lastKbReadSent)
		{
			lastKbReadSent = kbReadSoFar;
            priv_handleMsgQueues(rhea::getTimeNowMSec(), 1);

			if (NULL == subscriber)
			{
				for (u32 i = 0; i < subscriberList.getNElem(); i++)
					notify_READ_VMCDATAFILE_PROGRESS(subscriberList(i)->q, 0, logger, eReadDataFileStatus::inProgress, kbReadSoFar, fileID);
			}
			else
				notify_READ_VMCDATAFILE_PROGRESS(*subscriber, 0, logger, eReadDataFileStatus::inProgress, kbReadSoFar, fileID);
		}
	}
}

/***************************************************
 * Chiede alla CPU il file di data-audit e lo salva localmente in app/temp/dataAudit%d.txt
 * [%d] � un progressivo che viene comunicato alla fine del download nel messaggio di conferma.
 * Periodicamente notifica emettendo un messaggio notify_READ_DATA_AUDIT_PROGRESS() che contiene lo stato di avanzamento
 * del download
 *
 * [out_fileID] � opzionale. Se != NULL, allora il fileID che viene comunicato a fine download viene anche riportato in questa var
 */
eReadDataFileStatus Server::priv_downloadDataAudit (cpubridge::sSubscriber *subscriber,u16 handlerID, bool bIncludeBufferDataInNotify, u16 *out_fileID)
{
    logger->log ("priv_downloadDataAudit:: begin\n");

	//il file che scarico dalla CPU lo salvo localmente con un nome ben preciso
	u8 fullFilePathAndName[256];
	u16 fileID = 0;
	while (1)
	{
		sprintf_s((char*)fullFilePathAndName, sizeof(fullFilePathAndName), "%s/temp/dataAudit%d.txt", rhea::getPhysicalPathToAppFolder(), fileID);
		if (!rhea::fs::fileExists(fullFilePathAndName))
			break;
		fileID++;
	}

    if (NULL != out_fileID)
        *out_fileID= fileID;


    logger->log ("priv_downloadDataAudit:: dts file: %s\n", fullFilePathAndName);

/*#ifdef _DEBUG
	//hack per velocizzare i test
	{
		u8 debug_src_eva[256];
		sprintf_s((char*)debug_src_eva, sizeof(debug_src_eva), "%s/current/eva_test.log", rhea::getPhysicalPathToAppFolder());
		rhea::fs::fileCopy(debug_src_eva, fullFilePathAndName);
		Server::priv_downloadDataAudit_onFinishedOK(fullFilePathAndName, fileID);
		if (NULL != subscriber)
			notify_READ_DATA_AUDIT_PROGRESS(*subscriber, handlerID, logger, eReadDataFileStatus::finishedOK, 15, fileID);

		return eReadDataFileStatus::finishedOK;
	}
#endif
*/
	FILE *f = rhea::fs::fileOpenForWriteBinary(fullFilePathAndName);
	if (NULL == f)
	{
		if (NULL != subscriber)
			notify_READ_DATA_AUDIT_PROGRESS(*subscriber, handlerID, logger, eReadDataFileStatus::finishedKO_unableToCreateFile, 0, fileID, NULL, 0);

        logger->log ("priv_downloadDataAudit:: finishedKO_unableToCreateFile\n");
		return eReadDataFileStatus::finishedKO_unableToCreateFile;
	}


    u8 bufferW[32];
    const u16 nBytesToSend = cpubridge::buildMsg_readDataAudit(bufferW, sizeof(bufferW));

    u32 bytesReadSoFar = 0;
    u16 kbReadSoFar = 0;
    u16 lastKbReadSent = u16MAX;
    while (1)
    {
        u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
        if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
        {
            //errore, la CPU non ha risposto, abortisco l'operazione
            if (NULL != subscriber)
                notify_READ_DATA_AUDIT_PROGRESS (*subscriber, handlerID, logger, eReadDataFileStatus::finishedKO_cpuDidNotAnswer, kbReadSoFar, fileID, NULL, 0);
            rhea::fs::fileClose(f);

            logger->log ("priv_downloadDataAudit:: finishedKO_cpuDidNotAnswer\n");
            return eReadDataFileStatus::finishedKO_cpuDidNotAnswer;
        }

        //scrivo su file i dati ricevuti
        const u8 payloadLen = sizeOfAnswerBuffer - 6;
        const u8 *payload = &answerBuffer[5];
        fwrite (payload, payloadLen, 1, f);

        //aggiorno contatore di byte letti
        bytesReadSoFar += payloadLen;
        kbReadSoFar = (u16)(bytesReadSoFar >> 10);


        if (answerBuffer[3] != 0)
        {
            //finito!
            rhea::fs::fileClose(f);
	
			priv_downloadDataAudit_onFinishedOK(fullFilePathAndName, fileID);
			
			//notifico
			if (NULL != subscriber)
			{
				if (bIncludeBufferDataInNotify)
					notify_READ_DATA_AUDIT_PROGRESS (*subscriber, handlerID, logger, eReadDataFileStatus::finishedOK, kbReadSoFar, fileID, payload, payloadLen);
				else
					notify_READ_DATA_AUDIT_PROGRESS (*subscriber, handlerID, logger, eReadDataFileStatus::finishedOK, kbReadSoFar, fileID, NULL, 0);
			}

            logger->log ("priv_downloadDataAudit:: finishedOK\n");
            return eReadDataFileStatus::finishedOK;
        }

		if (payloadLen > 0)
		{
			if (bIncludeBufferDataInNotify)
			{
				if (NULL != subscriber)
				{
					//notifico il nuovo blocco dati appena ricevuto
					notify_READ_DATA_AUDIT_PROGRESS (*subscriber, 0, logger, eReadDataFileStatus::inProgress, kbReadSoFar, fileID, payload, payloadLen);
				}
			}
			else
			{
				//notifico che ho letto un altro Kb
				if (kbReadSoFar != lastKbReadSent)
				{
					lastKbReadSent = kbReadSoFar;

					if (NULL != subscriber)
						notify_READ_DATA_AUDIT_PROGRESS (*subscriber, 0, logger, eReadDataFileStatus::inProgress, kbReadSoFar, fileID, NULL, 0);
				}
			}
		}
    }
}

//***************************************************
void Server::priv_downloadDataAudit_onFinishedOK(const u8 *fullFilePathAndName, u32 fileID)
{
	//parso il file ricevuto e genero un secondo file di nome "packedDataAudit%d.dat" contenente le info in versione "packed"
	EVADTSParser *parser = RHEANEW(localAllocator, EVADTSParser)();
	if (parser->loadAndParse(fullFilePathAndName))
	{
		priv_retreiveSomeDataFromLocalDA3();

		rhea::Allocator *allocator = rhea::getScrapAllocator();
		u32 bufferSize = 0;
		u8 *buffer = parser->createBufferWithPackedData(allocator, &bufferSize, this->cpu_numDecimalsForPrices);
		if (NULL != buffer)
		{
            u8 s[256];
            rhea::string::utf8::spf (s, sizeof(s), "%s/temp/packedDataAudit%d.dat", rhea::getPhysicalPathToAppFolder(), fileID);
            FILE *f2 = rhea::fs::fileOpenForWriteBinary(s);
            rhea::fs::fileWrite (f2, buffer, bufferSize);
            rhea::fs::fileClose(f2);
			RHEAFREE(allocator, buffer);
		}
	}
	RHEADELETE(localAllocator, parser);
}

//***************************************************
bool Server::priv_askVMCDataFileTimeStampAndWaitAnswer(sCPUVMCDataFileTimeStamp *out, u32 timeoutMSec)
{
	u8 bufferW[16];
	const u8 nBytesToSend = buildMsg_getVMCDataFileTimeStamp(bufferW, sizeof(bufferW));

	//invio richiesta a CPU
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, timeoutMSec))
	{
		//errore, la CPU non ha risposto, abortisco l'operazione
		return false;
	}

	//la CPU risponde con [#] [T] [len] [secondi] [minuti] [ore] [giorno] [mese] [anno] [ck]
	out->readFromBuffer(&answerBuffer[3]);
	return true;
}


/***************************************************
 * Manda il maessaggio B alla CPU e attende la sua risposta.
 *
 * Ritorna 0 se la CPU non ha risposta
 * Ritorna il numero di byte ricevuti se la CPU ha effettivamente risposto. La risposta viene messa nella variabile di classe "answerBuffer"
 */
u16 Server::priv_prepareAndSendMsg_checkStatus_B (u8 btnNumberToSend, bool bForceJug)
{
    //se devo mandare un bottone in particolare (per es a seguito di una richiesta CPUBRIDGE_SUBSCRIBER_ASK_CPU_SEND_BUTTON_NUM,
    //lo faccio qui ma solo quando btnNumberToSend==0.
    //L'idea � che qui sto preparando il solito messaggio di stato...se non avevo nessun btn in particolare da inviare (btnNumberToSend==0) e se
    //ne avevo uno in canna in attesa (nextButtonNumToSend!=0), allora lo mando ora
	if (0 == btnNumberToSend)
	{
		bForceJug = false;
		btnNumberToSend = keepOnSendingThisButtonNum;
	}

    u8 bufferW[32];
    u16 nBytesToSend = cpubridge::buildMsg_checkStatus_B (btnNumberToSend, lang_getErrorCode(&language), bForceJug, bufferW, sizeof(bufferW));
    u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
    if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
        return 0;
    return sizeOfAnswerBuffer;
}

//***************************************************
void Server::priv_resetInternalState(cpubridge::eVMCState s)
{
	memset(&cpuParamIniziali, 0, sizeof(sCPUParamIniziali));
	memset(&cpuStatus, 0, sizeof(sCPUStatus));
	memset(utf16_lastCPUMsg, 0, sizeof(utf16_lastCPUMsg));
	lastCPUMsg_len = 0;
	lastBtnProgStatus = 0;
	keepOnSendingThisButtonNum = 0;

	cpuStatus.statoPreparazioneBevanda = eStatoPreparazioneBevanda::doing_nothing;
	cpuStatus.VMCstate = s;

	memset(jugRepetitions, '0', sizeof jugRepetitions);
}

/***************************************************
 * priv_enterState_compatibilityCheck
 *
 *	Ci si entra all'inizio, quando la connessione con la CPU � stata stabilita.
 *	Serve per verificare che il FW di CPU sia quello giusto
 *	In caso di errore, va nello stato eVMCState::CPU_NOT_COMPATIBLE dal quale non si esce pi�
 *  In caso di successo, si va nello stato DA3SYNC
 */
void Server::priv_enterState_compatibilityCheck()
{
	logger->log("CPUBridgeServer::priv_enterState_compatibilityCheck()\n");

	stato.set(sStato::eStato::compatibilityCheck);
	priv_resetInternalState(eVMCState::COMPATIBILITY_CHECK);

	cpuStatus.LCDMsg.utf16LCDString[0] = 0x00;
	rhea::string::utf16::concatFromASCII(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), "COMPATIBILITY    CHECK");
	cpuStatus.LCDMsg.importanceLevel = 123;

	//segnalo ai miei subscriber lo stato corrente
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
	{
		notify_CPU_STATE_CHANGED(subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
		notify_CPU_SEL_AVAIL_CHANGED(subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
		notify_CPU_NEW_LCD_MESSAGE(subscriberList(i)->q, 0, logger, &cpuStatus.LCDMsg);
	}
}

//***************************************************
void Server::priv_handleState_compatibilityCheck()
{
	u8 bufferW[64];

	//mando il comando "C" una decina di volte per vedere se la CPU esiste
	u8 nRetry = 10;
	while (1)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//invio comando initalParam
		const u8 nBytesToSend = cpubridge::buildMsg_initialParam_C(2, 0, 0, bufferW, sizeof(bufferW));
		u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
        if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
		{
			priv_parseAnswer_initialParam(answerBuffer, sizeOfAnswerBuffer);
			if (strcmp(cpuParamIniziali.CPU_version, "FAKE CPU") == 0)
			{
                sCPUVMCDataFileTimeStamp myTS;
                myTS.setInvalid();
                saveVMCDataFileTimeStamp(myTS);
				priv_retreiveSomeDataFromLocalDA3();
                //priv_handleState_DA3Sync();
				//return;
			}
			break;
		}

		nRetry--;
		if (nRetry == 0)
		{
            //la CPU non ha mai risposto, ne deduco che non esiste oppure non � in grado di rispondere.
			//Essendo questa la procedera di "boot", vado nello stato NOT_SUPPORTED perch� non sono stato
			//in grado di contattare la CPU nemmeno una volta
			priv_enterState_CPUNotSupported();
			return;
		}

		priv_handleMsgQueues(timeNowMSec, 1000);
	}



    //la CPU ha risposto, a questo punto verifico che supporti il comando "c" che � tipico delle CPU fusion2
	priv_handleMsgQueues(rhea::getTimeNowMSec(), 10);
	nRetry = 4;
	while (1)
	{
		sExtendedCPUInfo info;
		if (priv_prepareSendMsgAndParseAnswer_getExtendedCOnfgInfo_c(&info))
		{
			break;
		}

		nRetry--;
		if (nRetry == 0)
		{
			//La CPU non ha risposto a "c", quindi ne deduco che � di una versione precedente alla 2.0... non va bene
			priv_enterState_CPUNotSupported();
			return;
		}

		priv_handleMsgQueues(rhea::getTimeNowMSec(), 1000);
	}

	//tutto ok, passo a da3Sync
    priv_handleMsgQueues(rhea::getTimeNowMSec(), 1);
	priv_enterState_DA3Sync();
}

/***************************************************
 * Questo stato � una sorta di stato invalido e ci si entra quando, durante le fasi iniziali, la GPU non � riuscita a contattare la CPU
 * oppure la CPU contattata non � una versione valida per questo sw.
 * Da questo stato non si esce mai. E' consentito aggiornare il FW di CPU, ma alla fine � comunque necessario fare il reboot
 */
void Server::priv_enterState_CPUNotSupported()
{
	logger->log("CPUBridgeServer::priv_enterState_CPUNotSupported()\n");

	stato.set(sStato::eStato::CPUNotSupported);
	priv_resetInternalState(eVMCState::CPU_NOT_SUPPORTED);

	cpuStatus.LCDMsg.utf16LCDString[0] = 0;
	rhea::string::utf16::concatFromASCII(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), "CPU NOT SUPPORTED");
	cpuStatus.LCDMsg.importanceLevel = 123;

	//segnalo ai miei subscriber lo stato corrente
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
	{
		notify_CPU_STATE_CHANGED(subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
		notify_CPU_SEL_AVAIL_CHANGED(subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
		notify_CPU_NEW_LCD_MESSAGE(subscriberList(i)->q, 0, logger, &cpuStatus.LCDMsg);
	}
}

//***************************************************
void Server::priv_handleState_CPUNotSupported()
{
	while (1)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();
		priv_handleMsgQueues(timeNowMSec, 5000);
	}
}

/***************************************************
 * In questo stato la GPU consulta la CPU per verificare lo stato di sincronia del DA3.
 * Se la CPU riporta un timestamp != da quello della GPU, allora automaticamente parte la procedura di download del DA3 e aggiornamento del timestamp.
 * Lo scopo ultimo di questo stato � fare in modo che il DA3 di GPU sia uguale a quello di CPU.
 * Da questo stato si passa automaticamente nello stato "normal" oppure in "com error"
 */
void Server::priv_enterState_DA3Sync()
{
	logger->log("CPUBridgeServer::priv_enterState_DA3Sync()\n");

	stato.set(sStato::eStato::DA3_sync);

	cpuStatus.statoPreparazioneBevanda = eStatoPreparazioneBevanda::doing_nothing;
	cpuStatus.VMCstate = cpubridge::eVMCState::DA3_SYNC;

	//segnalo ai miei subscriber lo stato corrente
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
		notify_CPU_STATE_CHANGED(subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
}

//***************************************************
void Server::priv_handleState_DA3Sync()
{
	//carico il mio TS
	sCPUVMCDataFileTimeStamp myTS;
	loadVMCDataFileTimeStamp(&myTS);

	//se il file da3 non esiste, invalido il mio ts
	u8 s[256];
	sprintf_s((char*)s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
	if (!rhea::fs::fileExists(s))
		myTS.setInvalid();

    priv_handleMsgQueues(rhea::getTimeNowMSec(), 1);

	//chiedo il timestamp alla CPU
	sCPUVMCDataFileTimeStamp cpuTS;
	u8 nRetry = 5;
	while (1)
	{
		if (priv_askVMCDataFileTimeStampAndWaitAnswer(&cpuTS, 100))
		{
			//se il suo TS � == al mio, ho finito
			if (myTS.isEqual(cpuTS))
			{
				priv_handleState_DA3Sync_onFinishedOK();
				return;
			}
			break;
		}

		nRetry--;
		if (nRetry == 0)
		{
			priv_enterState_comError();
			return;
		}

		priv_handleMsgQueues(rhea::getTimeNowMSec(), 100);
	}
	

	//se arrivo qui vuol dire che il mio da3 non � in sync con quello della CPU, procedo al download
    priv_handleMsgQueues(rhea::getTimeNowMSec(), 1);
	nRetry = 2;
	while (nRetry--)
	{
		u16 fileID = 0;
		if (eReadDataFileStatus::finishedOK == priv_downloadVMCDataFile(NULL, 0, &fileID))
		{
			//tutto ok, il file � stato scaricato in temp/vmcDataFile%d.da3
			//Lo copio in current/da3, aggiorno il timestamp, aggiorno la data di ultima modifica

			u8 s1[256];
			sprintf_s((char*)s, sizeof(s), "%s/temp/vmcDataFile%d.da3", rhea::getPhysicalPathToAppFolder(), fileID);
			sprintf_s((char*)s1, sizeof(s1), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
			rhea::fs::fileDelete(s1);
			rhea::fs::fileCopy(s, s1);

			//aggiorno il ts
			saveVMCDataFileTimeStamp(cpuTS);


			//Se nella cartella last_installed non c'� nulla, allora il da3 scaricato dalla CPU
			//lo chiamo "downloaded_from_cpu.da3", altrimenti mantengo il nome del file precedente
			char dstFilename[128];
			sprintf_s(dstFilename, sizeof(dstFilename), "download_from_cpu.da3");

			sprintf_s((char*)s1, sizeof(s1), "%s/last_installed/da3", rhea::getPhysicalPathToAppFolder());
			OSFileFind ff;
			if (rhea::fs::findFirst(&ff, s1, (const u8*)"*.da3"))
			{
				do
				{
					if (!rhea::fs::findIsDirectory(ff))
					{
						sprintf_s(dstFilename, sizeof(dstFilename), "%s", rhea::fs::findGetFileName(ff));
						break;
					}
				} while (rhea::fs::findNext(ff));
				rhea::fs::findClose(ff);
			}
			rhea::fs::deleteAllFileInFolderRecursively(s1, false);

			//copio anche in cartella last_installed
			sprintf_s((char*)s1, sizeof(s1), "%s/last_installed/da3/%s", rhea::getPhysicalPathToAppFolder(), dstFilename);
			rhea::fs::fileCopy(s, s1);

			//aggiorno il file con data e ora di ultima modifica
			rhea::DateTime dt;
			dt.setNow();
			sprintf_s((char*)s, sizeof(s), "%s/last_installed/da3/dateUM.bin", rhea::getPhysicalPathToAppFolder());
			FILE *f = rhea::fs::fileOpenForWriteBinary (s);
			u64 u = dt.getInternalRappresentation();
			fwrite(&u, sizeof(u64), 1, f);
            rhea::fs::fileClose(f);

			//finito
			priv_handleState_DA3Sync_onFinishedOK();
			return;
		}
	}

	priv_enterState_comError();
}


//***************************************************
void Server::priv_handleState_DA3Sync_onFinishedOK()
{
	priv_retreiveSomeDataFromLocalDA3();
	if (!priceHolding.isPriceHolding)
		priv_enterState_normal();
	else
	{
		if (priceHolding.alreadyAsked)
			priv_enterState_normal();
		else
		{
			//voglio che la procedura di richiesta dei prezzi price-holding venga fatta una sola volta, preferibilmente all'avvio e non venga fatta tutte le volte
			//che si fa un da3 sync. Questo serve ad evitare che ogni volta che si entra in menu prog scatti anche la sync del price holding, visto che � una operazione
			//che tende ad essere piuttosto lunga
			priceHolding.alreadyAsked = 1;
			priv_enterState_downloadPriceHoldingPriceList();
		}
	}
}


/***************************************************
 * Se la CPU ha smesso di rispondere, si entra in questo stato.
 * Una volta qui dentro, la GPU comincer� a mandare il comando "C" perdiodicamente (circa una volta ogni 3 secondi).
 * Se la CPU risponde, si passa automaticamente allo stato DA3Sync.
 * Fintanto che la CPU non risponde, si rimane qui dentro all'infinito, inviando periodicamente il comando "C" per vedere se la CPU � viva
 */
void Server::priv_enterState_comError()
{
	logger->log("CPUBridgeServer::priv_enterState_comError()\n");

    stato.set (sStato::eStato::comError);
	priv_resetInternalState(eVMCState::COM_ERROR);

	cpuStatus.LCDMsg.utf16LCDString[0] = 0;
	rhea::string::utf16::concatFromASCII(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), "COM ERROR");
    cpuStatus.LCDMsg.importanceLevel = 123;

    //segnalo ai miei subscriber che sono in com-error
    for (u32 i = 0; i < subscriberList.getNElem(); i++)
    {
        notify_CPU_STATE_CHANGED (subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
        notify_CPU_SEL_AVAIL_CHANGED (subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
        notify_CPU_NEW_LCD_MESSAGE (subscriberList(i)->q, 0, logger, &cpuStatus.LCDMsg);
    }
}

/***************************************************
 * priv_handleState_comError()
 *
 *	continua a mandare il comando "initialParam" fino a che la CPU non risponde.
 *	Quando la CPU risponde, this passa in stato eStato::normal e ritorna
 */
void Server::priv_handleState_comError()
{
	//preparo il msg da mandare alla CPU
	u8 bufferW[32];
	const u8 nBytesToSend = cpubridge::buildMsg_initialParam_C(2, 0, 0, bufferW, sizeof(bufferW));

	//provo a mandarlo ad oltranza
    while (stato.get() == sStato::eStato::comError)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//invio comando initalParam
		u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
		if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 2000))
		{
			//la CPU ha risposto, elaboro la risposta e passo in stato "normal"
			priv_parseAnswer_initialParam (answerBuffer, sizeOfAnswerBuffer);
			//priv_enterState_normal();
			priv_enterState_DA3Sync();
			return;
		}

		priv_handleMsgQueues (timeNowMSec, 3000);
	}
}


//***************************************************
void Server::priv_parseAnswer_initialParam (const u8 *answer, u16 answerLen)
{
	/*	GIX 2018-12-17
		byte		significato
		0			#
		1			C
		2			lunghezza

		3			anno
		4			mese
		5			giorno

		6			hh
		7			mm
		8			ss

		9			versione sw	8 caratteri del tipo "1.4 WIP"
		10			versione sw
		11			versione sw
		12			versione sw
		13			versione sw
		14			versione sw
		15			versione sw
		16			versione sw

		17			98 btyes composti da 49 prezzi ciascuno da 2 bytes
		....

		Da qui in poi sono dati nuovi, introdotti a dicembre 2018

		115			versione protocollo. Inizialmente = 1, potrebbe cambiare in futuro

		116			ck
	*/

	//CPU version (string)
	memset (cpuParamIniziali.CPU_version, 0, sizeof (cpuParamIniziali.CPU_version));
	memcpy (cpuParamIniziali.CPU_version, &answer[9], 8);
		
	//prezzi
	if (answerLen > 24)
	{
		u8 k = 17;
		for (u8 i = 0; i < NUM_MAX_SELECTIONS; i++)
		{
			cpuParamIniziali.pricesAsInAnswerToCommandC[i] = (u16)answer[k] + 256 * (u16)answer[k + 1];
			k += 2;
		}
	}

	//protocol version
	cpuParamIniziali.protocol_version = 0;
	if (answerLen >= 117)
		cpuParamIniziali.protocol_version = answer[115];

#if defined(PLATFORM_YOCTO_EMBEDDED) || defined(PLATFORM_ROCKCHIP)
#ifndef _DEBUG
    //quando siamo davvero sulla macchina del caff�, � la CPU che determina la data e l'ora
	u16 date_year = answer[3] + 2000;
	u16 date_month = answer[4];
	u16 date_dayOfMonth = answer[5];
	u16 date_hour = answer[6];
	u16 date_min = answer[7];
	u16 date_sec = answer[8];

	if (date_year * date_month * date_dayOfMonth == 0)
	{
		date_year = 2015; date_month = 3; date_dayOfMonth = 15;
		date_hour = 15; date_min = 1; date_sec = 0;
	}

	char s[256];
	sprintf(s, "date -u %02d%02d%02d%02d%04d.%02d", date_month, date_dayOfMonth, date_hour, date_min, date_year, date_sec);
	system(s);
	system("hwclock -w");
#endif
#endif
}

/***************************************************
 * Questo � uno stato particolare, che si attiva solo se c'� PRICE-HOLDING impostato.
 * Serve per recuperare la lista prezzi dalla gettoniera price holding.
 * Questo stato viene invocato automaticamente alla fine del da3Sync (solo se PRICE HOLDING impostato)
 */
void Server::priv_enterState_downloadPriceHoldingPriceList()
{
	logger->log("CPUBridgeServer::priv_enterState_downloadPriceHoldingPriceList()\n");
	stato.set (sStato::eStato::downloadPriceHoldingPriceList);
}

/***************************************************
 * priv_handleState_downloadPriceHoldingPriceList
 * Manda una serie di msg alla CPU allo scopo di recuperare i prezzi memorizzati nella gettoniera in price-holding
 */
void Server::priv_handleState_downloadPriceHoldingPriceList()
{
	u8 bufferW[64];

	//mi devo assicurare che la CPU sia pronta per rispondere alle query sulla gettoniera
	const u32 TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec = 250;
	const u8 ALLOW_N_RETRY_BEFORE_COMERROR = 5;
	u64	nextTimeSendCheckStatusMsgWasMSec = 0;
	u8 nRetry = 0;
logger->log ("handleState_downloadPriceHoldingPriceList() => waiting for CPU to be ready\n");
	while ((cpuStatus.flag1 & sCPUStatus::FLAG1_READY_TO_DELIVER_DATA_AUDIT) == 0)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//ogni tot, invio un msg di stato alla CPU
		if (timeNowMSec >= nextTimeSendCheckStatusMsgWasMSec)
		{
			u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_checkStatus_B(0, false);
			if (0 == sizeOfAnswerBuffer)
			{
				//la CPU non ha risposto al mio comando di stato, passo in com_error
				nRetry++;
				if (nRetry >= ALLOW_N_RETRY_BEFORE_COMERROR)
				{
					priv_enterState_comError();
					return;
				}
			}
			else
			{
				//parso la risposta
				nRetry = 0;
				priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);
			}

			//schedulo il prossimo msg di stato
			nextTimeSendCheckStatusMsgWasMSec = rhea::getTimeNowMSec() + TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec;
		}

		//ci sono messaggi in ingresso?
		priv_handleMsgQueues (timeNowMSec, TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec);
	}


logger->log ("handleState_downloadPriceHoldingPriceList() => ready to ask prices\n");

	//determino quali e quanti prezzi devo recuperare dalla gettoniera
	u16 firstPriceList = 100;
	u16 lastPriceList = 1;
	for (u8 i = 0; i < NUM_MAX_SELECTIONS; i++)
	{
		if (cpuParamIniziali.pricesAsInAnswerToCommandC[i] < firstPriceList)
			firstPriceList = cpuParamIniziali.pricesAsInAnswerToCommandC[i];
		if (cpuParamIniziali.pricesAsInAnswerToCommandC[i] > lastPriceList)
			lastPriceList = cpuParamIniziali.pricesAsInAnswerToCommandC[i];
	}

	if (firstPriceList > lastPriceList)
	{
		u16 swap = firstPriceList;
		firstPriceList = lastPriceList;
		lastPriceList = swap;
	}
		
	if (firstPriceList < 1)		firstPriceList = 1;
	if (firstPriceList > 100)	firstPriceList = 100;
	if (lastPriceList > 100)	lastPriceList = 100;

	//reset dei prezzi nel mio buffer interno
	memset (cpuParamIniziali.pricesAsInPriceHolding, 0xff, sizeof(cpuParamIniziali.pricesAsInPriceHolding));
    cpuParamIniziali.pricesAsInPriceHolding[0] = 0;

	//devo chiedere tutti i prezzi compresi tra [firstPriceList] e [lastPriceList] in gruppi da NUM_PREZZI_PER_QUERY
    //const u8 NUM_PREZZI_PER_QUERY = 4;
	const u8 NUM_PREZZI_PER_QUERY = 1;
    const u8 NUM_MAX_RETRY_PER_QUERY = 5;
	const u64 TIMEOUT_ANSWER_TO_H_Msec = 1500;
	
	u8 currentFirstPriceInRequest = (u8)firstPriceList;
	nRetry = NUM_MAX_RETRY_PER_QUERY;
	bool bQuit = false;
	while (bQuit == false)
	{
		u8 numPricesToAsk = (lastPriceList - currentFirstPriceInRequest) + 1;
		if (numPricesToAsk > NUM_PREZZI_PER_QUERY)
			numPricesToAsk = NUM_PREZZI_PER_QUERY;
		const u8 nBytesToSend = cpubridge::buildMsg_requestPriceHoldingPriceList (currentFirstPriceInRequest, numPricesToAsk, bufferW, sizeof(bufferW));
		u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
        if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, TIMEOUT_ANSWER_TO_H_Msec))
		{
			//risponde con # H [len] [first] [num] [LSB_price] [MSB_price]..[LSB_price] [MSB_price] [ck]
			const u8 first = answerBuffer[3];
			const u8 num = answerBuffer[4];
			for (u8 i = 0; i < num; i++)
				cpuParamIniziali.pricesAsInPriceHolding[first + i] = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5 + 2 * i]);

			currentFirstPriceInRequest = first + num;
			nRetry = NUM_MAX_RETRY_PER_QUERY;

			if (currentFirstPriceInRequest > lastPriceList)
				bQuit = true;
		}
		else
		{
			if (nRetry-- == 0)
			{
                //qualcosa � andato storto, mi arrendo e lascio i prezzi cosi' come sono.
                //Avendoli memsettati a 0xffff, i prezzi dovrebbero essere talmente alti da far capire che qualcosa �
                //andato male. Alla peggio, di fronte ad un prezzo di 655.35 euro, il cliente sicuramente non prover�
                //ad acquistare
				bQuit = true;
			}
		}
	}


	//notifico i client del cambio di prezzi
	u16 prices[NUM_MAX_SELECTIONS + 2];
	for (u8 selNum = 0; selNum < NUM_MAX_SELECTIONS; selNum++)
	{
		u16 index = cpuParamIniziali.pricesAsInAnswerToCommandC[selNum];
        if (index > 100)
            prices[selNum] = 0xffff;
        else
            prices[selNum] = cpuParamIniziali.pricesAsInPriceHolding[index];
	}
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
		notify_CPU_SEL_PRICES_CHANGED(subscriberList(i)->q, 0, logger, NUM_MAX_SELECTIONS, cpu_numDecimalsForPrices, prices);
	


	priv_enterState_normal();
}


/***************************************************
 * Questo � il normale stato operativo. La CPU � stata contattata, � viva, ha riportato una versione supportata e il file DA3 � stato
 * sincronizzato.
 */
void Server::priv_enterState_normal()
{
	logger->log("CPUBridgeServer::priv_enterState_normal()\n");
    stato.set (sStato::eStato::normal);
}

/***************************************************
 *	priv_handleState_normal
 *
 *	Manda periodicamente il msg B (checkStatus) di richiesta di stato alla CPU.
 *	In caso di mancata risposta, passa allo stato com_error.
 *
 *	Se qualche subscriber invia il comando di startSelezione (e la selezione in questione � disponibile), allora si prova a passare in stato "selection"
 */
void Server::priv_handleState_normal()
{
	const u32 TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec = 250;
	const u8 ALLOW_N_RETRY_BEFORE_COMERROR = 5;
	u8 nRetry = 0;

	u64	nextTimeSendCheckStatusMsgWasMSec = 0;
    while (stato.get() == sStato::eStato::normal)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//ogni tot, invio un msg di stato alla CPU
		if (timeNowMSec >= nextTimeSendCheckStatusMsgWasMSec)
		{
            u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_checkStatus_B(0, false);
            if (0 == sizeOfAnswerBuffer)
			{
				//la CPU non ha risposto al mio comando di stato, passo in com_error
				nRetry++;
				if (nRetry >= ALLOW_N_RETRY_BEFORE_COMERROR)
				{
					priv_enterState_comError();
					return;
				}
			}
			else
			{
				//parso la risposta
				nRetry = 0;
				priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);

                if (this->cpuStatus.VMCstate == eVMCState::PROGRAMMAZIONE)
                {
                    priv_enterState_programmazione();
                    return;
                }

				// se la CPU mi sta dicendo che � in telemetria...
				if ((cpuStatus.flag1 & sCPUStatus::FLAG1_TELEMETRY_RUNNING) != 0)
				{
					priv_enterState_telemetry();
					return;
				}
			}

			//schedulo il prossimo msg di stato
			nextTimeSendCheckStatusMsgWasMSec = rhea::getTimeNowMSec() + TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec;
		}

		//ci sono messaggi in ingresso?
		priv_handleMsgQueues (timeNowMSec, TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec);


		//se ho il modulo di telemetria SECO collegato...
		if (NULL != rsProto)
		{
			if (timeNowMSec >= rsProto->nextTimeSendTemperature_msec)
			{
				rsProto->nextTimeSendTemperature_msec = timeNowMSec + 5000;

				u8 bufferW[32];
				u16 nBytesToSend = cpubridge::buildMsg_getVoltAndTemp (bufferW, sizeof(bufferW));
				u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
				if (priv_sendAndWaitAnswerFromCPU (bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
				{
					//rcv: # P [len] 0x17 [temp_camera] [temp_bollitore] [temp_cappuccinatore] [voltage LSB MSB] [Kilowatt hour LSB MSB][ck]
					const u8 temperatureCamCaffe = answerBuffer[4];
					const u8 temperatureEsp = answerBuffer[5];
					const u8 temperatureSol = answerBuffer[5];
					const u8 temperatureMilker = answerBuffer[6];
					const u8 temperatureIce = 0;
					rsProto->sendTemperature (temperatureEsp, temperatureCamCaffe, temperatureSol, temperatureIce, temperatureMilker, logger);
				}
			}
		}
	}
}


/***************************************************
 * In questo stato si entra quando la CPU va in stato PROGRAMMAZIONE.
 *	Da questo stato si esce quando la CPU esce dallo stato PROGRAMMAZIONE.
 *	All'uscita si va in DA3_SYNC
 */
void Server::priv_enterState_programmazione()
{
    logger->log("CPUBridgeServer::priv_enterState_programmazione()\n");
    stato.set (sStato::eStato::programmazione);
}

//***************************************************
void Server::priv_handleState_programmazione()
{
    const u32 TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec = 50;
    const u8 ALLOW_N_RETRY_BEFORE_COMERROR = 8;
    u8 nRetry = 0;

    u64	nextTimeSendCheckStatusMsgWasMSec = 0;
    while (stato.get() == sStato::eStato::programmazione)
    {
        const u64 timeNowMSec = rhea::getTimeNowMSec();

        //ogni tot, invio un msg di stato alla CPU
        if (timeNowMSec >= nextTimeSendCheckStatusMsgWasMSec)
        {
            u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_checkStatus_B(0, false);
            if (0 == sizeOfAnswerBuffer)
            {
                //la CPU non ha risposto al mio comando di stato, passo in com_error
                nRetry++;
                if (nRetry >= ALLOW_N_RETRY_BEFORE_COMERROR)
                {
                    priv_enterState_comError();
                    return;
                }
            }
            else
            {
                //parso la risposta
                nRetry = 0;
                priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);

                if (this->cpuStatus.VMCstate != eVMCState::PROGRAMMAZIONE)
                {
                    //priv_enterState_normal();
                    priv_enterState_DA3Sync();
                    return;
                }
            }

            //schedulo il prossimo msg di stato
            nextTimeSendCheckStatusMsgWasMSec = rhea::getTimeNowMSec() + TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec;
        }

        //ci sono messaggi in ingresso?
        priv_handleMsgQueues (timeNowMSec, TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec);
    }
}


/***************************************************
 * In questo stato si entra quando la CPU tira su un preciso bit durante la risposta al comando
 *	'B'. Una volta entrati in questo stato, ci aspettiamo che la CPU possa non rispondere ad alcun
 *	comando per parecchio tempo. Durante questo stato, ci comportiamo come se fossimo in com_error solo
 *	che il messaggio visualizzato a video � "telemetry in progress" invece che "com error".
 *	Usiamo da qui quando la CPU ricomincia a rispondere e tira giu il bit di cui sopra
 */
void Server::priv_enterState_telemetry()
{
	logger->log("CPUBridgeServer::priv_enterState_telemetry()\n");
	stato.set(sStato::eStato::telemetry);
}

//***************************************************
void Server::priv_handleState_telemetry()
{
	const u32 TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec = 500;

	u64	nextTimeSendCheckStatusMsgWasMSec = 0;
	while (stato.get() == sStato::eStato::telemetry)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//ogni tot, invio un msg di stato alla CPU
		if (timeNowMSec >= nextTimeSendCheckStatusMsgWasMSec)
		{
			u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_checkStatus_B(0, false);
			if (0 != sizeOfAnswerBuffer)
			{
				//parso la risposta
				priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);

				// se non siamo pi� in telemetria, esco da questo stato
				if ((cpuStatus.flag1 & sCPUStatus::FLAG1_TELEMETRY_RUNNING) == 0)
				{
					priv_enterState_DA3Sync();
					return;
				}
			}

			//schedulo il prossimo msg di stato
			nextTimeSendCheckStatusMsgWasMSec = rhea::getTimeNowMSec() + TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec;
		}

		//ci sono messaggi in ingresso?
		priv_handleMsgQueues(timeNowMSec, TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec);
	}
}


/***************************************************
 *	priv_parseAnswer_checkStatus
 *
 *	parsa la risposta della CPU alla mia richiesta di stato.
 *	Triggera alcuni eventi in modo da notificare chiunque sia in ascolta sulla coda di questo thread del fatto
 *	che qualcosa � cambiato.
 *	In particolare:
 *		CPUBRIDGE_NOTIFY_CPU_STATE_CHANGED		se VMCstate/VMCerrorCode/VMCerrorType sono cambiati
 *		CPUBRIDGE_NOTIFY_CPU_NEW_LCD_MESSAGE	se � arrivato un msg LCD diverso dal precedente
 *		CPUBRIDGE_NOTIFY_CPU_CREDIT_CHANGED		se il credito disponibile � cambiato
 *		CPUBRIDGE_NOTIFY_CPU_SEL_AVAIL_CHANGED	se la disponibilit� delle selezioni � cambiata
 */
void Server::priv_parseAnswer_checkStatus (const u8 *answer, u16 answerLen UNUSED_PARAM)
{
	u8 isMultilangage = 0;
	const u8 prevMsgLcdCPUImportanceLevel = cpuStatus.LCDMsg.importanceLevel;
	cpuStatus.LCDMsg.importanceLevel = 0xff;
	
	//prima di protocol v7, nessuna informazione sullo stato del milker veniva passata.
	//Per simulare questo fatto, tiro sempre su il bit relativo in modo che la GPU creda che il milker sia sempre collegato
	//A partire da protocol v7, l'eventuale distacco del milker si riflette su questo bit che viene messo a 0
	u16 newCpuStatusFlag1 = cpuStatus.flag1;
	newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_MILKER_ALIVE; 

	if (answer[1] != (u8)eCPUCommand::checkStatus_B && answer[1] != (u8)eCPUCommand::checkStatus_B_Unicode)
	{
		//mi aspettavo la risposta al mio comando B, invece ho ricevuto dell'altro
		logger->log("WARN: I was expecintg B or Z, received [%c]\n", (char)answer[1]);
		return;
	}

	if (cpuParamIniziali.protocol_version >= 1)
	{
		u8 z = 17 + 32;
		if (answer[1] != (u8)eCPUCommand::checkStatus_B)
			z += 32;
		cpuStatus.beepSelezioneLenMSec = answer[z + 1];
		cpuStatus.beepSelezioneLenMSec *= 256;
		cpuStatus.beepSelezioneLenMSec += answer[z];
		cpuStatus.beepSelezioneLenMSec *= 100; //trasforma in msec
		z += 2;


		if (cpuParamIniziali.protocol_version >= 2)
		{
			//un byte per indicare se la CPU sta usando il suo linguaggio oppure sta usando il linguaggio "ml" (ovvero manda i messaggi con @)
			switch (answer[z++])
			{
				default:
				case '0':
					lang_clearErrorCode(&language);
					z += 2;
					break;

				case '1':
				{
					//cpu usa un linguaggio custom, vediamo che lingua vuole
					isMultilangage = 1;
					char language_requested[4];
					language_requested[0] = answer[z++];
					language_requested[1] = answer[z++];

					//se necessario, cambio lingua
					const char *curLang = lang_getCurLanguage (&language);
					if (curLang[0] != language_requested[0] || curLang[1] != language_requested[1])
					{
						language_requested[2] = 0x00;
						lang_open (&language, language_requested);
					}
				}
				break;
			}


			if (cpuParamIniziali.protocol_version >= 3)
			{
				//1 byte per indicare importanza del msg di CPU (0=poco importante, 1=importante)
				//8 byte stringa con l'attuale credito
				cpuStatus.LCDMsg.importanceLevel = answer[z++];

				//se il credito � cambiato, lo memorizzo e notifico i subscriber
				if (memcmp(cpuStatus.userCurrentCredit, &answer[z], 8) != 0)
				{
					memcpy(cpuStatus.userCurrentCredit, &answer[z], 8);
					cpuStatus.userCurrentCredit[8] = 0;

					for (u32 i = 0; i < subscriberList.getNElem(); i++)
						notify_CPU_CREDIT_CHANGED (subscriberList(i)->q, 0, logger, cpuStatus.userCurrentCredit, 8);
				}
				z += 8;		


				if (cpuParamIniziali.protocol_version >= 4)
				{
					//1 byte per indicare se il btnProg � attualmente in stato di PRESSED
					u8 btnProgIsPressedNOW = answer[z++];
					if (btnProgIsPressedNOW == 1)
					{
						if (lastBtnProgStatus == 0)
						{
							//la CPU mi segnala che � stato premuto il btnPROG, diramo l'evento a tutti
							lastBtnProgStatus = 1;
							for (u32 i = 0; i < subscriberList.getNElem(); i++)
								notify_CPU_BTN_PROG_PRESSED(subscriberList(i)->q, 0, logger);
						}
					}
					else
					{
						lastBtnProgStatus = 0;
					}					

					if (cpuParamIniziali.protocol_version >= 5)
					{
						//1 byte con 8 bit a mo di flag per usi futuri
						const u8 flag = answer[z++];

						//bit 0x01 == 1 quando la CPU � pronta per scaricare eva-dts
						if ((flag & 0x01) != 0)
							newCpuStatusFlag1 |= sCPUStatus::FLAG1_READY_TO_DELIVER_DATA_AUDIT;
						else
							newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_READY_TO_DELIVER_DATA_AUDIT);

						if (cpuParamIniziali.protocol_version >= 6)
						{
							if ((flag & 0x02) != 0)
								newCpuStatusFlag1 |= sCPUStatus::FLAG1_TELEMETRY_RUNNING;
							else
								newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_TELEMETRY_RUNNING);

							if (cpuParamIniziali.protocol_version >= 7)
							{
								if ((flag & 0x04) != 0)
									newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_MILKER_ALIVE;
								else
									newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_IS_MILKER_ALIVE);

								if ((flag & 0x08) != 0)
									newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_FREEVEND;
								else
									newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_IS_FREEVEND);

								if ((flag & 0x10) != 0)
									newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_TESTVEND;
								else
									newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_IS_TESTVEND);

								if ((flag & 0x20) != 0)
									newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_RFID_DEBT;
								else
									newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_IS_RFID_DEBT);

								if ((flag & 0x40) != 0)
									newCpuStatusFlag1 |= sCPUStatus::FLAG1_IS_RFID_JUG;
								else
									newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_IS_RFID_JUG);

							}//if (cpuParamIniziali.protocol_version >= 7)
						}//if (cpuParamIniziali.protocol_version >= 6)						
					}//if (cpuParamIniziali.protocol_version >= 5)
				}//if (cpuParamIniziali.protocol_version >= 4)
			}//if (cpuParamIniziali.protocol_version >= 3)
		} // if (cpuParamIniziali.protocol_version >= 2)
	} //if (cpuParamIniziali.protocol_version >= 1)


	
	//cpuStatus.CupAbsentStatus_flag = answer[9] & 0x08;
	if ((answer[9] & 0x08) != 0)
		newCpuStatusFlag1 |= sCPUStatus::FLAG1_CUP_DETECTED;
	else
		newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_CUP_DETECTED);

	//cpuStatus.bShowDialogStopSelezione = answer[9] & 0x10;
	if ((answer[9] & 0x10) != 0)
		newCpuStatusFlag1 |= sCPUStatus::FLAG1_SHOW_DLG_STOP_SELEZIONE;
	else
		newCpuStatusFlag1 &= (~sCPUStatus::FLAG1_SHOW_DLG_STOP_SELEZIONE);



	//stato della CPU
	u8 bTriggerEvent = 0;
	if (cpuStatus.VMCstate != (eVMCState)answer[3])		{ cpuStatus.VMCstate = (eVMCState)answer[3]; bTriggerEvent = 1; }
	if (cpuStatus.VMCerrorCode != answer[4])			{ cpuStatus.VMCerrorCode = answer[4]; bTriggerEvent = 1; }
	if (cpuStatus.VMCerrorType != answer[5])			{ cpuStatus.VMCerrorType = answer[5]; bTriggerEvent = 1; }
	if (bTriggerEvent || newCpuStatusFlag1 != cpuStatus.flag1)
	{
		cpuStatus.flag1 = newCpuStatusFlag1;
		for (u32 i = 0; i < subscriberList.getNElem(); i++)
			notify_CPU_STATE_CHANGED (subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
	}

	
	

	

	/*  GIX 2018 05 04
		Abbiamo deciso che la GPU deve avere un modo per sapere se la CPU sta preparando
		una bevanda e, se si, se � in attesa che i sistemi di pagamento facciano il loro mestiere.
		Usiamo i bit 0x20 e 0x40 in questo modo:
			se == 0  => la CPU � una versione "vecchia" che non supporta questa nuova procedura
						per cui anche la GPU deve funzionare come faceva prima

			se == 0x01  =>  la CPU � nuova, 0x01 � il suo stato di default e vuol dire "non sto facendo nulla"
			se == 0x02  =>  la CPU ha capito che deve preparare una bevanda, � in attesa che i sistemi di pagamento rispondano
			se == 0x03  =>  la CPU ha dato l'OK alla preparazione (equivalente di BEVANDA IN PREP)
	*/
	{
		const eStatoPreparazioneBevanda statoPrepBevanda = (eStatoPreparazioneBevanda)((answer[9] & 0x60) >> 5);;
		if (statoPrepBevanda != cpuStatus.statoPreparazioneBevanda)
		{
			cpuStatus.statoPreparazioneBevanda = statoPrepBevanda;

			if (stato.get() != sStato::eStato::selection)
			{
				const u8 selection_CPU_current = answer[10];
				if (selection_CPU_current != 0)
				{
					sStartSelectionParams params;
					params.how = eStartSelectionMode_CPUSpontaneous;
					params.asCPUSpontaneous.selNum = selection_CPU_current;
					priv_enterState_selection(params, NULL);
				}
			}
		}
	}




	//la CPU invia un messaggio testuale, pu� essere in ASCII o in unicode. Io lo traduco sempre in utf16
	//A seconda che la risposta CPU sia in ASCII o UNICODE, ci sono quindi 32 o 64 bytes di messaggio
	u16	utf16_msgLCD[sCPULCDMessage::BUFFER_SIZE_IN_U16];
	u8  msgLCD_len = 0;
	memset(utf16_msgLCD, 0, sizeof(utf16_msgLCD));

	u8 z = 11;
	u16 firstGoodChar = ' ';
	for (u8 i = 0; i < 32; i++)
	{
		if (answer[1] == 'B')
			utf16_msgLCD[msgLCD_len] = answer[z++];
		else //answer[1] == 'Z'
		{
			utf16_msgLCD[msgLCD_len] = (u16)answer[z] + (u16)answer[z + 1] * 256;
			z += 2;
		}

		if (utf16_msgLCD[msgLCD_len] != ' ' && firstGoodChar == ' ')
			firstGoodChar = utf16_msgLCD[msgLCD_len];
		msgLCD_len++;

		//mette uno spazio dopo i primi 16 caratteri perch� storicamente il msg di CPU � composto da 2 messaggi da 16 char da visualizzare
		//uno sotto l'altro. Noi invece li visualizziamo sulla stessa riga
		if (msgLCD_len == 16)
		{
			if (!isMultilangage || firstGoodChar != '@')
				utf16_msgLCD[msgLCD_len++] = ' ';
		}
	}
	assert(msgLCD_len <= sCPULCDMessage::BUFFER_SIZE_IN_U16);
	utf16_msgLCD[msgLCD_len] = 0;
	rhea::string::utf16::rtrim(utf16_msgLCD);

	//se rheAPI ha sovraimposto un messaggio testuale, lo applico ora
	const u64 timeNowMSec = rhea::getTimeNowMSec();
	if (screenMsgOverride.timeToStopShowingMSec > 0)
	{
		if (timeNowMSec >= 	screenMsgOverride.timeToStopShowingMSec)
			screenMsgOverride.timeToStopShowingMSec = 0;
		else
		{
			memcpy (utf16_msgLCD, screenMsgOverride.utf16Msg, sizeof(utf16_msgLCD));
			cpuStatus.LCDMsg.importanceLevel = 0xff;
		}
	}



	//se la CPU ha alzato il bit di "telemetria in corso", devo fare un override del messaggio testuale
	if ((cpuStatus.flag1 & sCPUStatus::FLAG1_TELEMETRY_RUNNING) != 0)
	{
		utf16_msgLCD[0] = 0;
		rhea::string::utf16::concatFromASCII (utf16_msgLCD, sizeof(utf16_msgLCD), "TELEMETRY RUNNING...");
		msgLCD_len = rhea::string::utf16::lengthInBytes(utf16_msgLCD) / 2;
	}


	
	//1 bit per ogni selezione per indicare se la selezione � disponibile o no
	//Considerando che NumMaxSelections=48, dovrebbero servire 6 byte
	//ATTENZIONE che bit==0 significa che la selezione � OK, bit==1 significa KO
	//Io invece traduco al contrario, per cui per me cupStatus.selAvailability == 1 se la selezione � disponibile
	u8 anythingChanged = 0;


	//se la CPU ha alzato il bit di "telemetria in corso", devo disabilitare d'ufficio tutte le selezioni
	if ((cpuStatus.flag1 & sCPUStatus::FLAG1_TELEMETRY_RUNNING) != 0)
	{
		if (cpuStatus.selAvailability.areAllNotAvail() == false)
		{
			cpuStatus.selAvailability.reset();
			anythingChanged = 1;
		}
	}
	else
	{
		if (cpuStatus.VMCstate == eVMCState::DISPONIBILE || cpuStatus.VMCstate == eVMCState::PREPARAZIONE_BEVANDA)
		{
			u8 mask = 0x01;
			for (u8 i = 0; i < NUM_MAX_SELECTIONS; i++)
			{
				u8 isSelectionAvail = 1;
				if ((answer[z] & mask) != 0)
					isSelectionAvail = 0;


				//se delle selezioni sono state disabilitate via rheAPI, applico qui 
				if (false == IsSelectionEnable(i+1))
					isSelectionAvail = 0;

				if (isSelectionAvail)
				{
					if (!cpuStatus.selAvailability.isAvail(i + 1))
					{
						anythingChanged = 1;
						cpuStatus.selAvailability.setAsAvail(i + 1);
					}
				}
				else
				{
					if (cpuStatus.selAvailability.isAvail(i + 1))
					{
						anythingChanged = 1;
						cpuStatus.selAvailability.setAsNotAvail(i + 1);
					}
				}

				if (mask == 0x80)
				{
					mask = 0x01;
					z++;
				}
				else
					mask <<= 1;
			}
		}
		else
		{
			//la CPU non � in uno stato valido per fare erogazioni, per cui forzo a priori  la totale
			//indisponibilit� delle bevande
			if (cpuStatus.selAvailability.areAllNotAvail() == false)
			{
				cpuStatus.selAvailability.reset();
				anythingChanged = 1;
			}
		}
	}


	if (anythingChanged)
	{
		for (u32 i = 0; i < subscriberList.getNElem(); i++)
			notify_CPU_SEL_AVAIL_CHANGED (subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
	}

	//se non c'� nemmeno una selezione disponibile, mostro sempre e cmq il msg di CPU anche se non fosse "importante"
	if (cpuStatus.selAvailability.areAllNotAvail())
		cpuStatus.LCDMsg.importanceLevel = 0xff;


	//se il messaggio LCD � cambiato dal giro precedente, oppure lo stato di importanza � cambiato, devo notificare il nuovo messaggio a tutti
	bool bDoNotifyNewLCDMessage = false;
    if (prevMsgLcdCPUImportanceLevel != cpuStatus.LCDMsg.importanceLevel || msgLCD_len != lastCPUMsg_len || memcmp(utf16_msgLCD, utf16_lastCPUMsg, lastCPUMsg_len*2 + 1) != 0)
	{
		memcpy(utf16_lastCPUMsg, utf16_msgLCD, (msgLCD_len + 1) * 2);
		lastCPUMsg_len = msgLCD_len;
		memcpy(cpuStatus.LCDMsg.utf16LCDString, utf16_msgLCD, (msgLCD_len + 1) * 2);

		//lo traduco se necessario
		if (isMultilangage)
		{
			//se il primo ch diverso da "spazio" � "@", allora stiamo parlano di un messaggio custom
			u16 i = 0;
			while (utf16_lastCPUMsg[i] != 0x00)
			{
				if (utf16_lastCPUMsg[i] != ' ')
					break;
				++i;
			}
			if (utf16_lastCPUMsg[i] == LANG_CHIOCCIOLA)
			{
				u16 t = 0;
				while (utf16_lastCPUMsg[i] != 0x00)
					cpuStatus.LCDMsg.utf16LCDString[t++] = utf16_lastCPUMsg[i++];
				cpuStatus.LCDMsg.utf16LCDString[t] = 0x00;
				t = rhea::string::utf16::rtrim(cpuStatus.LCDMsg.utf16LCDString);

				if (t > 0)
					t = lang_translate(&language, cpuStatus.LCDMsg.utf16LCDString, sCPULCDMessage::BUFFER_SIZE_IN_U16 - 1);
			}
		}

		bDoNotifyNewLCDMessage = true;
	}


    //Se sono nella modalit� "mostra la stringa con cpu model and version", per tot secondi prependo il nome modello all'attuale msg di CPU
	/*showCPUStringModelAndVersionUntil_msec = 10;
	utf16_CPUMasterVersionString[0] = 0;
	rhea::string::utf16::concatFromASCII(utf16_CPUMasterVersionString, sizeof(utf16_CPUMasterVersionString), "xxxxxxxxxTS30 02.03.00    GB-GB");
	*/
	if (showCPUStringModelAndVersionUntil_msec)
    {
		const u16 utf16_spacer[] = { 0x0020, 0x002d, 0x0020, 0x0000 }; // � la stringa " - "
		if (bDoNotifyNewLCDMessage)
		{
			//e' arrivato un msg nuovo dalla CPU, devo prependere la stringa con il modello
			if (cpuStatus.LCDMsg.utf16LCDString[0] != 0x0000)
				rhea::string::utf16::prepend(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), utf16_spacer);
			rhea::string::utf16::prepend(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), utf16_CPUMasterVersionString);
            lastCPUMsg_len = rhea::string::utf16::lengthInBytes (cpuStatus.LCDMsg.utf16LCDString) / 2;
            cpuStatus.LCDMsg.importanceLevel = 0xff;
		}
		else
		{
			//rispetto all'ultima volta, CPU non ha mandato nuovi messaggi.
			//A questo punto, se cpuStatus.LCDMsg.utf16LCDString inizia gi� con la stringa col modello, non devo fare nulla, altrimenti la devo prependere
			//e notificare tutti
			u32 n = rhea::string::utf16::lengthInBytes(utf16_CPUMasterVersionString) / 2;
			if (memcmp(cpuStatus.LCDMsg.utf16LCDString, utf16_CPUMasterVersionString, n * 2) != 0)
			{
				if (cpuStatus.LCDMsg.utf16LCDString[0] != 0x0000)
					rhea::string::utf16::prepend(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), utf16_spacer);
				rhea::string::utf16::prepend(cpuStatus.LCDMsg.utf16LCDString, sizeof(cpuStatus.LCDMsg.utf16LCDString), utf16_CPUMasterVersionString);
				bDoNotifyNewLCDMessage = true;
				cpuStatus.LCDMsg.importanceLevel = 0xff;
                lastCPUMsg_len = rhea::string::utf16::lengthInBytes (cpuStatus.LCDMsg.utf16LCDString) / 2;
			}
		}
		
        if (rhea::getTimeNowMSec() >= showCPUStringModelAndVersionUntil_msec)
            showCPUStringModelAndVersionUntil_msec = 0;
    }

	//se richiesto, notifico tutti del nuovo messaggio LCD di CPU
	if (bDoNotifyNewLCDMessage)
	{
		for (u32 i = 0; i < subscriberList.getNElem(); i++)
			notify_CPU_NEW_LCD_MESSAGE (subscriberList(i)->q, 0, logger, &cpuStatus.LCDMsg);
	}


	if (NULL != rsProto)
		rsProto->onSMUStateChanged (cpuStatus, logger);
}

//***************************************************
void Server::priv_notify_CPU_RUNNING_SEL_STATUS (const sSubscription *sub, u16 handlerID, eRunningSelStatus s)
{
	if (NULL == sub)
	{
		for (u32 i = 0; i < subscriberList.getNElem(); i++)
			notify_CPU_RUNNING_SEL_STATUS (subscriberList(i)->q, handlerID, logger, s);
	}
	else
		notify_CPU_RUNNING_SEL_STATUS (sub->q, handlerID, logger, s);
}

/***************************************************
 * priv_enterState_selection
 *
 *	ritorna true se ci sono le condizioni per iniziare una selezione. In questo caso, lo stato passa a stato = eStato::selection.
 *	In caso contrario, ritorna false e non cambia l'attuale stato.
 */
bool Server::priv_enterState_selection (const sStartSelectionParams &params, const sSubscription *sub)
{
	const		u8 selNumber = params.getSelNum();
	bool		ret = false;	// valore precaricato
	logger->log("CPUBridgeServer::priv_enterState_selectionRequest() => [how=%d, selNum=%d, forceJug=%c]\n",
							(u8)params.how, selNumber, params.isForceJUG() == true ? 'Y' : 'N');

    if (stato.get() != sStato::eStato::normal)
	{
		logger->log("  invalid request, CPUServer != eStato::normal, aborting.");
		priv_notify_CPU_RUNNING_SEL_STATUS (sub, 0, eRunningSelStatus::finished_KO);
	}
	else if (cpuStatus.VMCstate != eVMCState::DISPONIBILE)
	{
		logger->log("  invalid request, VMCState != eVMCState::DISPONIBILE, aborting.");
		priv_notify_CPU_RUNNING_SEL_STATUS (sub, 0, eRunningSelStatus::finished_KO);
	}

	/* questa condizione non � pi� valida in quanto abbiamo aggiunto il supporto alle selezion snack le quali partono dal selNum = 50 quindi
	* selezioni > di 48 sono da ritenersi valide
	else if (selNumber < 1 || selNumber > NUM_MAX_SELECTIONS)
	{
		logger->log("  invalid selection number, aborting.");
		priv_notify_CPU_RUNNING_SEL_STATUS (sub, 0, eRunningSelStatus::finished_KO);
	}
	*/
	else if (eLockStatus::locked == priv_getLockStatus())    //se la macchina � "locked", niente selezioni
    {
        logger->log("  machine is locked, aborting.");
        priv_notify_CPU_RUNNING_SEL_STATUS (sub, 0, eRunningSelStatus::finished_KO);
    }
	else
	{
		stato.set(sStato::eStato::selection);
		memcpy(&runningSel.params, &params, sizeof(sStartSelectionParams));
		runningSel.stopSelectionWasRequested = 0;
		runningSel.sub = sub;
		runningSel.status = eRunningSelStatus::wait;
		priv_notify_CPU_RUNNING_SEL_STATUS(sub, 0, runningSel.status);
		ret = true;
	}

	return ret;
}

/***************************************************
 *	priv_handleState_selection()
 *
 *	Durante questo stato ogni tot viene inviata la notifica CPUBRIDGE_NOTIFY_CPU_RUNNING_SEL_STATUS per
 *	informare il subscriber sullo stato della preparazione della bevanda.
 *
 *	Periodicamente invia il messaggio B (checkStatus) alla CPU per rimanere informati sullo stato della erogazione.
 *	In caso di mancata risposta, si passa in com_error.
 *	Quando l'erogazione termina, si torna in stato "normal".
 */
void Server::priv_handleState_selection()
{
	//mando la richiesta di stato alla CPU ogni tot msec
    const u32 TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec = 500;
	
	//la cpu deve passare da DISPONIBILE a "BEVANDA IN PREPARAZIONE" entro il tempo definito qui sotto
	const u32 TIMEOUT_SELEZIONE_1_MSEC = 60000;

	//una volta che la CPU � entrata in "PREPARAZIONE", deve tornare disponibile entro il tempo definito qui sotto
	const u32 TIMEOUT_SELEZIONE_2_MSEC = 240000;

    const u8 ALLOW_N_RETRY_BEFORE_COMERROR = 6;
	u8 nRetry = 0;

	u64	timeStartedMSec = rhea::getTimeNowMSec();
	u8 bBevandaInPreparazione = 0;

	//loop fino alla fine della selezione
	u64	nextTimeSendCheckStatusMsgMSec = 0;
    while (stato.get() == sStato::eStato::selection)
	{
		u64 timeNowMSec = rhea::getTimeNowMSec();

		//ogni tot, invio un msg di stato alla CPU, altrimenti non faccio niente, rimango in attesa della fine della selezione
		//e controllo eventuali msg in ingresso dai subscriber
		if (timeNowMSec < nextTimeSendCheckStatusMsgMSec)
		{
			priv_handleMsgQueues (timeNowMSec, TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec);
			continue;
		}

		u8 selNumberToSend = 0;
		bool bSendForceJugInCommandB = false;

		//al primo giro, mando alla CPU la richiesta di iniziare la selezione
		if (nextTimeSendCheckStatusMsgMSec == 0)
		{
			switch (runningSel.params.how)
			{
			case eStartSelectionMode_default:
				//nel caso di default, devo inviare il numero di selezione da far partire direttamente dentro al comando B
				selNumberToSend = runningSel.getSelNum();
				bSendForceJugInCommandB = runningSel.params.asDefault.bForceJUG;
				break;

			case eStartSelectionMode_CPUSpontaneous:
				//in questo caso, la CPU � gi� partita da sola con la selezione, quindi io non devo mandare nulla nel msg B
				selNumberToSend = 0;
				break;

			case eStartSelectionMode_alreadyPaid:
				//in questo caso, devo inviare uno specifico comando alla CPU per fargli sapere che deve iniziare una selezione
				//senza considerare il pagamento in quanto la GPU ha gi� provveduto a pagare
				{
					u8 bufferW[32];

					const u16 nBytesToSend = cpubridge::buildMsg_startSelectionWithPaymentAlreadyHandledByGPU_V (runningSel.params.asAlreadyPaid.selNum, runningSel.params.asAlreadyPaid.price, runningSel.params.asAlreadyPaid.paymentMode, runningSel.params.asAlreadyPaid.paymentType, runningSel.params.asAlreadyPaid.bForceJUG, bufferW, sizeof(bufferW));
					u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
					if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 5000))
					{
						//cpu ha risposto, diamo per scontato che la risposta sia buona e procediamo.
						//CPU dovrebbe gi� di suo essere entrata in "preparazione bevanda" per cui io non devo fare altre che continuare a mandare
						//B e vedere come prosegue la selezione
						selNumberToSend = 0;
					}
					else
					{
						logger->log("priv_handleState_selection() => aborting, CPU did not answer to command V\n");
						priv_onSelezioneTerminataKO();
						return;
					}
				}
				break;

			default:
				//qui non ci si dovrebbe arrivare mai
				logger->log("priv_handleState_selection() => aborting, invalid runningSel.params.how[%d]\n", runningSel.params.how);
				priv_onSelezioneTerminataKO();
				return;
			}
		}
		else
		{
			//successivamente, il num sel inviato nel comando B � sempre 0 a meno che non sia stato premuto il btn stop selezione
			if (runningSel.stopSelectionWasRequested)
			{
				selNumberToSend = runningSel.getSelNum();
				runningSel.stopSelectionWasRequested = 0;
			}
		}

		//invio il msg di stato alla CPU
        u16 sizeOfAnswerBuffer = priv_prepareAndSendMsg_checkStatus_B(selNumberToSend, bSendForceJugInCommandB);
        if (0 == sizeOfAnswerBuffer)
		{
			//la CPU non ha risposto al mio comando di stato, passo in com_error
			nRetry++;
			if (nRetry >= ALLOW_N_RETRY_BEFORE_COMERROR)
			{
				runningSel.status = eRunningSelStatus::finished_KO;
				priv_notify_CPU_RUNNING_SEL_STATUS(runningSel.sub, 0, runningSel.status);
				priv_enterState_comError();
				return;
			}
		}
		else
		{
			//parso la risposta
			priv_parseAnswer_checkStatus(answerBuffer, sizeOfAnswerBuffer);
			nRetry = 0;
		}

		//schedulo il prossimo msg di stato
		timeNowMSec = rhea::getTimeNowMSec();
		nextTimeSendCheckStatusMsgMSec = timeNowMSec + TIME_BETWEEN_ONE_STATUS_MSG_AND_ANOTHER_MSec;

		/* Teoricamente l'intero processo � ora pilotato da "statoPreparazioneBevanda"
		 * Voglio per� essere sicuro di aver letto almeno una volta uno stato != da eStatoPreparazioneBevanda::doing_nothing
		 * prima di imbarcarmi nel processo.
		 * Appena vedo uno stato != da eStatoPreparazioneBevanda::doing_nothing setto bBevandaInPreparazione e parto.
		 * Se non vedo questa condizione entro 4/5 second, vuol dire che c'� qualcosa che non va e abortisco*/
		if (bBevandaInPreparazione == 0)
		{
			if (cpuStatus.statoPreparazioneBevanda != eStatoPreparazioneBevanda::doing_nothing)
				bBevandaInPreparazione = 1;
			else if (timeNowMSec - timeStartedMSec >= TIMEOUT_SELEZIONE_1_MSEC)
			{
				logger->log("priv_handleState_selection() => aborting, TIMEOUT_SELEZIONE_1_MSEC\n");
				priv_onSelezioneTerminataKO();
				return;
			}
		}
		else
		{
			switch (cpuStatus.statoPreparazioneBevanda)
			{
			default:
				logger->log("priv_handleState_selection() => statoPreparazioneBevanda invalid [%d]\n", cpuStatus.statoPreparazioneBevanda);
				priv_onSelezioneTerminataKO();
				return;

			case eStatoPreparazioneBevanda::wait:
				//sto aspettando che la CPU decida il da farsi
				if (cpuStatus.VMCstate != eVMCState::DISPONIBILE && cpuStatus.VMCstate != eVMCState::PREPARAZIONE_BEVANDA)
				{
					logger->log("priv_handleState_selection() => aborting, ero in WAIT ma CPU � andata in uno stato != da DISP o PREP");
					priv_onSelezioneTerminataKO();
					return;
				}
				else
				{
					runningSel.status = eRunningSelStatus::wait;
					//informo il subscriber che ha richiesto la bevanda che la CPU � ancora in wait
					priv_notify_CPU_RUNNING_SEL_STATUS (runningSel.sub, 0, runningSel.status);
				}
				break;

			case eStatoPreparazioneBevanda::running:
				//la cpu ha dato l'OK, sta preparando la bevanda
				if (bBevandaInPreparazione == 1)
				{
					//� la prima volta che passo di qui
					bBevandaInPreparazione = 2;
					timeStartedMSec = timeNowMSec;

					if (cpuStatus.bShowDialogStopSelezione)
						runningSel.status = eRunningSelStatus::runningCanUseStopBtn;
					else
						runningSel.status = eRunningSelStatus::running;

					//la selezione � in preparazione, mando la notifica al subscriber che ha richiesto la bevanda
					priv_notify_CPU_RUNNING_SEL_STATUS (runningSel.sub, 0, runningSel.status);
				}

				//timeout di sicurezza. Diamo per scontato che una bevanda non possa durare pi� di TIMEOUT_SELEZIONE_2_MSEC
				if (timeNowMSec - timeStartedMSec >= TIMEOUT_SELEZIONE_2_MSEC)
				{
					logger->log("priv_handleState_selection() => aborting, TIMEOUT_SELEZIONE_2_MSEC");
					priv_onSelezioneTerminataKO();
					return;
				}
				break;

			case eStatoPreparazioneBevanda::doing_nothing:
				if (bBevandaInPreparazione == 2)
				{
					//tutto ok, selezione terminata!!
					runningSel.status = eRunningSelStatus::finished_OK;
					priv_notify_CPU_RUNNING_SEL_STATUS (runningSel.sub, 0, runningSel.status);
					priv_enterState_normal();
					priv_telemetry_sendDecounters();
					return;
				}
				else
				{
					//questa condizione non dovrebbe mai verificarsi, la lascio per ogni evenieneza
					logger->log("priv_handleState_selection() => aborting, unknown status");
					priv_onSelezioneTerminataKO();
					return;
				}
				break;
			}
		}
	}
}

//***************************************************
void Server::priv_onSelezioneTerminataKO()
{
	runningSel.status = eRunningSelStatus::finished_KO;
	priv_notify_CPU_RUNNING_SEL_STATUS (runningSel.sub, 0, runningSel.status);
	priv_enterState_normal();
}

/***************************************************
 * priv_enterState_regolazioneAperturaMacina
 *
 *	ritorna true se ci sono le condizioni per iniziare una regolazione della macina. In questo caso, lo stato passa a stato = eStato::regolazioneAperturaMacina.
 *	In caso contrario, ritorna false e non cambia l'attuale stato.
 */
bool Server::priv_enterState_regolazioneAperturaMacina (u8 macina_1to4, u16 target)
{
	logger->log("CPUBridgeServer::priv_enterState_regolazioneAperturaMacina() => [%d] [%d]\n", macina_1to4, target);

	if (stato.get() != sStato::eStato::normal && (stato.get() != sStato::eStato::regolazioneAperturaMacina))
	{
		logger->log("  invalid request, CPUServer != eStato::normal, aborting.");
		return false;
	}

	if (cpuStatus.VMCstate != eVMCState::DISPONIBILE)
	{
		logger->log("  invalid request, VMCState != eVMCState::DISPONIBILE, aborting.");
		return false;
	}

	stato.set(sStato::eStato::regolazioneAperturaMacina);
	regolazioneAperturaMacina.macina_1to4 = macina_1to4;
	regolazioneAperturaMacina.target = target;
	return true;
}

//***************************************************
void Server::priv_handleState_regolazioneAperturaMacina()
{
	static const u8 NRETRY = 5;
	u8 nRetry = NRETRY;

	eCPUProg_macinaMove move = eCPUProg_macinaMove::stop;
	priv_sendAndHandleSetMotoreMacina (regolazioneAperturaMacina.macina_1to4, move);

	//dico a tutti che sono in uno stato speciale
	cpuStatus.VMCstate = eVMCState::REG_APERTURA_MACINA;
	cpuStatus.VMCerrorCode = 0;
	cpuStatus.VMCerrorType = 0;
	cpuStatus.selAvailability.reset();
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
	{
		notify_CPU_STATE_CHANGED(subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
		notify_CPU_SEL_AVAIL_CHANGED(subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
	}


	//in ogni caso metto un limite di tempo massimo per il raggiungimento del target perch� le letture fornite dalla
	//CPU sono ballerine e potrebbe essere che non arrivo mai esattamente dove voglio
	u64 timeToExitMSec = rhea::getTimeNowMSec() + 90000;
	const u16 TOLLERANZA = 0;
	while (stato.get() == sStato::eStato::regolazioneAperturaMacina)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();

		//ci sono messaggi in ingresso?
		priv_handleMsgQueues(timeNowMSec, 1);

			   
		//chiede la posizione della macina
		u16 curpos = 0;
		if (priv_sendAndHandleGetPosizioneMacina (regolazioneAperturaMacina.macina_1to4, &curpos))
		{
			nRetry = NRETRY;

			u16 diff = 0;
			if (curpos >= regolazioneAperturaMacina.target)
				diff = curpos - regolazioneAperturaMacina.target;
			else
				diff = regolazioneAperturaMacina.target - curpos;
			if (diff <= TOLLERANZA || timeNowMSec >= timeToExitMSec)
			{
				//fine
				priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
				//priv_enterState_normal();
				break;
			}

			if (regolazioneAperturaMacina.target > curpos)
			{
				if (move != eCPUProg_macinaMove::open)
				{
					if (move != eCPUProg_macinaMove::stop)
						priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
					move = eCPUProg_macinaMove::open;
					priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, move);
				}
			}
			else
			{
				if (move != eCPUProg_macinaMove::close)
				{
					if (move != eCPUProg_macinaMove::stop)
						priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
					move = eCPUProg_macinaMove::close;
					priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, move);
				}
			}
		}
		else
		{
			nRetry--;
			if (nRetry == 0)
			{
				//abortisco
				priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
				priv_enterState_normal();
				return;
			}
		}
	}

	//a questo punto, sono molto vicino al target, generalmente disto 1 o 2 punti dal valore desiderato, vado di fine tuning
	//In ogni caso, se entro 15 sec non raggiungo l'apertura precisa, esco altrimenti si rischia di stare qui all'infinito visto
	//che le letture riportate dalla CPU sono un po' ballerine
	nRetry = NRETRY;
    timeToExitMSec = rhea::getTimeNowMSec() + 15000;
	while (1)
	{
		//chiede la posizione della macina
		u16 curpos = 0;
		u8 n = 0;
		for (u8 i = 0; i < 3; i++)
		{
			priv_handleMsgQueues(rhea::getTimeNowMSec(), 100);
			rhea::thread::sleepMSec(300);

			u16 pos = 0;
			if (priv_sendAndHandleGetPosizioneMacina(regolazioneAperturaMacina.macina_1to4, &pos))
			{
				curpos += pos;
				n++;
			}

			priv_handleMsgQueues(rhea::getTimeNowMSec(), 100);
		}

		if (n > 0)
		{
			curpos /= n;
			nRetry = NRETRY;

			if (curpos == regolazioneAperturaMacina.target || rhea::getTimeNowMSec() > timeToExitMSec)
			{
				//fine
				priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
				priv_enterState_normal();
				return;
			}

			if (curpos > regolazioneAperturaMacina.target)
				priv_sendAndHandleSetMotoreMacina (regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::close);
			else
				priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::open);

			priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
		}
		else
		{
			nRetry--;
			if (nRetry == 0)
			{
				//abortisco
				priv_sendAndHandleSetMotoreMacina(regolazioneAperturaMacina.macina_1to4, eCPUProg_macinaMove::stop);
				priv_enterState_normal();
				return;
			}
		}
	}
}

//**********************************************
bool Server::priv_sendAndHandleGetPosizioneMacina (u8 macina_1to4, u16 *out)
{
	u8 bufferW[16];
	const u16 nBytesToSend = cpubridge::buildMsg_getPosizioneMacina_AA(bufferW, sizeof(bufferW), macina_1to4);
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (!priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 400))
		return false;

	*out = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[5]);
	return true;
}

//**********************************************
bool Server::priv_sendAndHandleSetMotoreMacina(u8 macina_1to4, eCPUProg_macinaMove m)
{
	u8 bufferW[16];
	const u16 nBytesToSend = cpubridge::buildMsg_setMotoreMacina_AA (bufferW, sizeof(bufferW), macina_1to4, m);
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	
	u8 nRetry = 8;
	while (nRetry--)
	{
		if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 400))
		{
			rhea::thread::sleepMSec(100);
			return true;
		}
	}
	return false;
}

/**********************************************
 * Questa viene chiamata ogni volta che, per qualunque motivo, CPUBridge effettua un DA3Sync.
 * Al termine del download del da3 dalla CPU, questa fn viene chiamata in modo da poter tenere in memoria alcuni
 * valori chiave del da3 ed essere sicuri che questi siano sempre aggiornati
 */
void Server::priv_retreiveSomeDataFromLocalDA3()
{
	u8 s[256];
	sprintf_s((char*)s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
	u32 sizeOfBuffer = 0;
	u8 *da3 = rhea::fs::fileCopyInMemory(s, localAllocator, &sizeOfBuffer);
	if (NULL == da3)
		return;

	//Numero di cifre decimali da utilizzare durante la formattazione dei prezzi. Tale numero lo trovo nel DA3 alla loc 7066
	this->cpu_numDecimalsForPrices = da3[7066];

	this->priceHolding.isPriceHolding = 0;
	if (da3[7058] == 3)
		this->priceHolding.isPriceHolding = 1;

	//il modello di cappuccinatore collegato � indicato nel DA3 alla loc [69]
	if (da3[69] <= 2)
		milkerType = (eCPUMilkerType)da3[69];
	else
		milkerType = eCPUMilkerType::none;

	// copia il valore delle ripetizioni per il jug
	for (u16 count = 0, address = 0xb1;
		count < sizeof jugRepetitions / sizeof jugRepetitions[0];
		count++, address += 0x64)
	{
		if (da3[address] < 10)
			jugRepetitions[count] = da3[address] + '0';
		else
			jugRepetitions[count] = da3[address] - 10 + 'A';
	}

	//machine code A & B
	id101 = rhea::utils::bufferReadU32_LSB_MSB(&da3[7314]);
	quickMenuPinCode = rhea::utils::bufferReadU16_LSB_MSB(&da3[8378]);

	RHEAFREE(localAllocator, da3);
}

/***************************************************
 * priv_enterState_grinderSpeedTest
 *
 *	ritorna true se ci sono le condizioni per iniziare il test. In questo caso, lo stato passa a stato = eStato::grinderSpeedTest.
 *	In caso contrario, ritorna false e non cambia l'attuale stato.
 *	Lo scopo di questo test � quello di muovere la macina per un tot di tempo e leggere (chiedendo alla CPU) il valore di un certo sensore.
 *	Alla fine della macinata, si ritorna la media delle letture del sensore
 */
bool Server::priv_enterState_grinderSpeedTest_AA (u8 macina_1to4, u8 tempoDiMacinataInSec)
{
	logger->log("CPUBridgeServer::priv_enterState_calcGrinderSpeed() => [%d]\n", macina_1to4);

	if (stato.get() != sStato::eStato::normal)
	{
		logger->log("  invalid request, CPUServer != eStato::normal, aborting.");
		return false;
	}

	if (cpuStatus.VMCstate != eVMCState::DISPONIBILE)
	{
		logger->log("  invalid request, VMCState != eVMCState::DISPONIBILE, aborting.");
		return false;
	}

	
	stato.set(sStato::eStato::grinderSpeedTest);
	grinderSpeedTest.motoreID = 10 + macina_1to4;
	grinderSpeedTest.lastCalculatedGrinderSpeed = 0;
	grinderSpeedTest.tempoDiMacinataInSec = tempoDiMacinataInSec;
	return true;
}

//***************************************************
void Server::priv_handleState_grinderSpeedTest()
{
	//dico a tutti che sono in uno stato speciale
	cpuStatus.VMCstate = eVMCState::GRINDER_SPEED_TEST;
	cpuStatus.VMCerrorCode = 0;
	cpuStatus.VMCerrorType = 0;
	cpuStatus.selAvailability.reset();
	for (u32 i = 0; i < subscriberList.getNElem(); i++)
	{
		notify_CPU_STATE_CHANGED(subscriberList(i)->q, 0, logger, cpuStatus.VMCstate, cpuStatus.VMCerrorCode, cpuStatus.VMCerrorType, cpuStatus.flag1);
		notify_CPU_SEL_AVAIL_CHANGED(subscriberList(i)->q, 0, logger, &cpuStatus.selAvailability);
	}

	const u32 TEMPO_DI_MACINATA_MSec = (u32)grinderSpeedTest.tempoDiMacinataInSec * 1000;

	u8 bufferW[16];
	const u8 nBytesToSend = cpubridge::buildMsg_attivazioneMotore(grinderSpeedTest.motoreID, (u8)(TEMPO_DI_MACINATA_MSec/100), 1, 0, bufferW, sizeof(bufferW));
	u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
	if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 4000))
	{
		//aspetto il tempo di macina e poi chiedo a CPU il risultato
		const u64 timeToExitMSec = rhea::getTimeNowMSec() + TEMPO_DI_MACINATA_MSec +1500;
		while (rhea::getTimeNowMSec() < timeToExitMSec)
			rhea::thread::sleepMSec(500);

		//chiedo alla CPU il valore del sensore
		u8 nRetry = 5;
		while (nRetry--)
		{
			const u8 nBytesToSend = cpubridge::buildMsg_getLastGrinderSpeed (bufferW, sizeof(bufferW));
			u16 sizeOfAnswerBuffer = sizeof(answerBuffer);
			if (priv_sendAndWaitAnswerFromCPU(bufferW, nBytesToSend, answerBuffer, &sizeOfAnswerBuffer, 500))
			{
				grinderSpeedTest.lastCalculatedGrinderSpeed = rhea::utils::bufferReadU16_LSB_MSB(&answerBuffer[4]);
				break;
			}
		}
	}
	else
	{
		//in caso di fallimento, aspetto un paio di secondi prima di tornare in NORMAL per dare il tempo alla GUI di vedere i cambi di stato
		rhea::thread::sleepMSec(2000);
	}

	
	//fine
	priv_enterState_normal();

}

//*********************************************************
/* gestione delle abilitazioni alla esecuzione di una ricetta */
bool Server::SelectionsEnableFilename(u8* out_filePathAndName, u32 sizeOfOutFilePathAndName) const
{
	rhea::string::utf8::spf(out_filePathAndName,
							sizeOfOutFilePathAndName,
							"%s/current/SelectionEnabled.rhea",
							rhea::getPhysicalPathToAppFolder());

	return true;
}

//*********************************************************
bool Server::SelectionsEnableSave()
{
	u8		s[512];
	u8		buffer[sizeof(selectionEnable) / sizeof(selectionEnable[0])];
	bool	ret;

	for (u8 i = 0; i < sizeof(selectionEnable) / sizeof(selectionEnable[0]); i++)
		buffer[i] = (true == selectionEnable[i] ? 1 : 0);

	SelectionsEnableFilename(s, sizeof(s));

	FILE* f = rhea::fs::fileOpenForWriteBinary(s);
	if (NULL != f)
	{
		rhea::fs::fileWrite(f, buffer, sizeof(buffer));
		rhea::fs::fileClose(f);
		ret = true;
	}
	else
		ret = false;

	return ret;
}

//*********************************************************
bool Server::SelectionsEnableLoad()
{
	u8 s[512];
	u8 buffer[sizeof(selectionEnable) / sizeof(selectionEnable[0])];

	SelectionsEnableFilename(s, sizeof(s));
	FILE* f = rhea::fs::fileOpenForReadBinary(s);
	if (NULL == f)
	{
		for (u8 i = 0; i < sizeof(selectionEnable) / sizeof(selectionEnable[0]); i++)
			buffer[i] = 1;
	}
	else
	{
		rhea::fs::fileRead(f, buffer, sizeof(buffer));
		rhea::fs::fileClose(f);
	}

	for (u8 i = 0; i < sizeof(selectionEnable) / sizeof(selectionEnable[0]); i++)
		selectionEnable[i] = (buffer[i] != 0);

	return true;
}

//*********************************************************
bool Server::IsSelectionEnable(u8 selNum_1toN)
{
	bool ret;

	if (selNum_1toN >= 1 && selNum_1toN <= NUM_MAX_SELECTIONS)
		ret = selectionEnable[selNum_1toN - 1];
	else
		ret = false;

	return ret;
}

//*********************************************************
bool Server::SelectionEnable (u8 selNum_1toN, bool bEnable)
{
	bool ret = true;
	if (selNum_1toN >= 1 && selNum_1toN <= NUM_MAX_SELECTIONS)
	{
		selectionEnable[selNum_1toN - 1] = bEnable;
		SelectionsEnableSave();
	}
	else
		ret = false;
	return ret;
}


//*********************************************************
void Server::priv_telemetry_sendDecounters()
{
	if (NULL == rsProto)
		return;

	u32 decounters[15];
	if (!priv_askCPUAllDecounters(decounters, sizeof(decounters)))
		return;

	//prodotti
	for (u8 i=0; i<10; i++)
		rsProto->sendDecounterProdotto (i+1, decounters[i], logger);

	//10=water_filter
	//11=coffee_brewer
	//12=coffee_ground
	//13=blocking_counter
	//14=maintenance
	rsProto->sendDecounterOther (RSProto::eDecounter::WATER_FILTER, decounters[10], logger);
	rsProto->sendDecounterOther (RSProto::eDecounter::COFFEE_BREWER, decounters[11], logger);
	rsProto->sendDecounterOther (RSProto::eDecounter::COFFEE_GROUND, decounters[12], logger);
}


//*********************************************************
void Server::priv_retreiveNetworkSettings (sNetworkSettings *out) const
{
	assert (NULL != out);
	memset (out, 0, sizeof(sNetworkSettings));

	//mac address
	secoos::getMACAddress (out->macAddress, sizeof(out->macAddress));

	//lan ip
	secoos::getLANAddress (out->lanIP, sizeof(out->lanIP));

	//modem LTE attivo?
	if (secoos::modemLTE::isEnabled())
		out->isModemLTEEnabled = 1;
	else
		out->isModemLTEEnabled = 0;

	//wifi mode
	out->wifiMode = secoos::wifi::getMode();
	if (eWifiMode::connectTo == out->wifiMode)
	{
		//wifi IP
		secoos::wifi::connectTo_getIP (out->wifiIP, sizeof(out->wifiIP));
		if (secoos::wifi::connectTo_isConnected ())
			out->wifiConnectTo_isConnected = 1;

		//wifi-ssid
		secoos::wifi::connectTo_getSSID (out->wifiConnectTo_SSID, sizeof(out->wifiConnectTo_SSID));
		secoos::wifi::connectTo_getPwd (out->wifiConnectTo_pwd, sizeof(out->wifiConnectTo_pwd));

		//nome hotspot wifi
		out->wifiHotSpotSSID[0] = 0x00;
	}
	else
	{
		out->wifiMode = eWifiMode::hotSpot;

		//wifi IP
		out->wifiIP[0] = 0x00;

		//wifi-ssid
		out->wifiConnectTo_SSID[0] = 0x00;
		out->wifiConnectTo_pwd[0] = 0x00;

		//nome hotspot wifi
		secoos::wifi::hotspot_getSSID (out->wifiHotSpotSSID, sizeof(out->wifiHotSpotSSID));
	}


//#endif
}