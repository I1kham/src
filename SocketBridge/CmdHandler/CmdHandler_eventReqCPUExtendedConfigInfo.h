#ifndef _CmdHandler_eventReqCPUExtendedConfigInfo_h_
#define _CmdHandler_eventReqCPUExtendedConfigInfo_h_
#include "../CmdHandler_eventReq.h"



namespace socketbridge
{
    /*********************************************************
     * CmdHandler_eventReqCPUExtendedConfigInfo
     *
     *  Il client richiede le extende config info.
     *  La richiesta viene passata a CPUBridge la quale risponde con una notifica CPUBRIDGE_NOTIFY_EXTENDED_CONFIG_INFO
     *
     */
    class CmdHandler_eventReqCPUExtendedConfigInfo : public CmdHandler_eventReq
    {
    public:
        static const eEventType EVENT_TYPE_FROM_SOCKETCLIENT = eEventType::cpuExtendedConfigInfo;
		static const u16		EVENT_ID_FROM_CPUBRIDGE = CPUBRIDGE_NOTIFY_EXTENDED_CONFIG_INFO;

		CmdHandler_eventReqCPUExtendedConfigInfo(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec) :
                        CmdHandler_eventReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec)
                    {  }

		bool		needToPassDownToCPUBridge() const { return true; }
        void        passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *payload, u16 payloadLen);
        void        onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);
    };
} // namespace socketbridge

#endif // _CmdHandler_eventReqCPUExtendedConfigInfo_h_
