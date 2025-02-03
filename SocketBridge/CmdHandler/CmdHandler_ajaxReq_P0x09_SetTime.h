#ifndef _CmdHandler_ajaxReq_P0x09_SetTime_h_
#define _CmdHandler_ajaxReq_P0x09_SetTime_h_
#include "../CmdHandler_ajaxReq.h"


namespace socketbridge
{
    /*********************************************************
     * CmdHandler_ajaxReq_P0x09_SetTime
     *
     *
        Input:
            command: setTime
            params:
				h: intero 0-23
				m: intero 0-59
				s: intero 0-59

		Output
			OK
			KO
     */


    class CmdHandler_ajaxReq_P0x09_SetTime : public CmdHandler_ajaxReq
    {
    public:
		CmdHandler_ajaxReq_P0x09_SetTime(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec, u8 ajaxRequestID) :
                                CmdHandler_ajaxReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec, ajaxRequestID)
                                { }

		bool				needToPassDownToCPUBridge() const { return true; }
        void                passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *params);
        void                onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);

        static const char*  getCommandName()                            { return "setTime"; }
    };

} // namespace socketbridge

#endif // _CmdHandler_ajaxReq_P0x09_SetTime_h_
