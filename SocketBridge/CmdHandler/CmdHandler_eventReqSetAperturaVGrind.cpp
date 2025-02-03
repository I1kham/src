#include "CmdHandler_eventReqSetAperturaVGrind.h"
#include "../SocketBridge.h"
#include "../../CPUBridge/CPUBridge.h"
#include "../../rheaCommonLib/rheaNetBufferView.h"

using namespace socketbridge;

//***********************************************************
void CmdHandler_eventReqSetAperturaVGrind::passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *payload, u16 payloadLen)
{
    assert (payloadLen >= 2);

	rhea::NetStaticBufferViewR nbr;
	nbr.setup(&payload[1], payloadLen-1, rhea::eEndianess::eBigEndian);
	
	u8 macina_1to4 = 0;
	u16 target = 100;
	nbr.readU8(macina_1to4);
	nbr.readU16(target);
	cpubridge::ask_CPU_SET_POSIZIONE_MACINA_AA (from, getHandlerID(), macina_1to4, target);
}

//***********************************************************
void CmdHandler_eventReqSetAperturaVGrind::onCPUBridgeNotification(socketbridge::Server *server UNUSED_PARAM, HSokServerClient &hClient UNUSED_PARAM, const rhea::thread::sMsg &msgFromCPUBridge UNUSED_PARAM)
{
}
