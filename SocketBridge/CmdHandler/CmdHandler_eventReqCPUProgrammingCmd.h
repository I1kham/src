#ifndef _CmdHandler_eventReqCPUProgrammingCmd_h_
#define _CmdHandler_eventReqCPUProgrammingCmd_h_
#include "../CmdHandler_eventReq.h"



namespace socketbridge
{
    /*********************************************************
     * CmdHandler_eventReqCPUProgrammingCmd
     *  
     *  Il client ha inviato un comando di "programmazione", vedi lista dei comandi P
     *  La richiesta viene passata direettamente a CPUBridge e SocketBridge non si aspetta di ricevere in ritorno
     *  alcuna specifica notifica (infatti onCPUBridgeNotification() � vuoto).
     */
    class CmdHandler_eventReqCPUProgrammingCmd : public CmdHandler_eventReq
    {
    public:
        static const eEventType EVENT_TYPE_FROM_SOCKETCLIENT = eEventType::cpuProgrammingCmd;
		static const u16		EVENT_ID_FROM_CPUBRIDGE = CPUBRIDGE_NOTIFY_MAX_ALLOWED;		//vuol dire che questa classe non risponde ad alcuna notifica di CPUBridge

		CmdHandler_eventReqCPUProgrammingCmd(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec) :
                        CmdHandler_eventReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec)
                    {  }

		bool		needToPassDownToCPUBridge() const																									{ return true; }
		void        passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *payload, u16 payloadLen);
        void        onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);
    };
} // namespace socketbridge

#endif // _CmdHandler_eventReqCPUProgrammingCmd_h_
