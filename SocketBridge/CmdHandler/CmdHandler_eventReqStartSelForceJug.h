#ifndef _CmdHandler_eventReqStartSelForceJug_h_
#define _CmdHandler_eventReqStartSelForceJug_h_
#include "../CmdHandler_eventReq.h"



namespace socketbridge
{
    /*********************************************************
     * CmdHandler_eventReqStartSelForceJug
     *
     *  Il client richiede che sia erogata la selezione N, e che sia forzata la modalit� JUG
     *  La richiesta viene passata a CPUBridge.
     *  Non sono previste risposte da parte di CPUBridge (ie: CPUBRIDGE_NOTIFY_MAX_ALLOWED)
     */
    class CmdHandler_eventReqStartSelForceJug : public CmdHandler_eventReq
    {
    public:
        static const eEventType EVENT_TYPE_FROM_SOCKETCLIENT = eEventType::startSelectionForceJug;
		static const u16		EVENT_ID_FROM_CPUBRIDGE = CPUBRIDGE_NOTIFY_MAX_ALLOWED;		//vuol dire che questa classe non risponde ad alcuna notifica di CPUBridge

                    CmdHandler_eventReqStartSelForceJug (const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec) :
                        CmdHandler_eventReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec)
                    {  }

		bool		needToPassDownToCPUBridge() const																								{ return true; }
        void        passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *payload, u16 payloadLen);
        void        onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);
    };
} // namespace socketbridge
#endif // _CmdHandler_eventReqStartSelForceJug_h_
