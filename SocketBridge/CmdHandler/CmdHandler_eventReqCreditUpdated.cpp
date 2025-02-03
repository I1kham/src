#include "CmdHandler_eventReqCreditUpdated.h"
#include "../SocketBridge.h"
#include "../../CPUBridge/CPUBridge.h"

using namespace socketbridge;

//***********************************************************
void CmdHandler_eventReqCreditUpdated::passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *payload UNUSED_PARAM, u16 payloadLen UNUSED_PARAM)
{
	cpubridge::ask_CPU_QUERY_CURRENT_CREDIT(from, getHandlerID());
}

//***********************************************************
void CmdHandler_eventReqCreditUpdated::onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge)
{
	//NB: se modifichi questo, modifica anche rhea::app::CurrentCredit::decodeAnswer()

    //rispondo con:
	//8 byte "stringa" con il prezzo già formattato con i decimali e la punteggiatura al posto giusto

	u8 buffer[16];
	cpubridge::translateNotify_CPU_CREDIT_CHANGED(msgFromCPUBridge, buffer, sizeof(buffer));

    buffer[8] = 0;
    server->sendEvent (hClient, EVENT_TYPE_FROM_SOCKETCLIENT, buffer, 9);
}

