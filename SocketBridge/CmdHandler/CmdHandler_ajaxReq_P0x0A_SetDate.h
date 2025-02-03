#ifndef _CmdHandler_ajaxReq_P0x0A_SetDate_h_
#define _CmdHandler_ajaxReq_P0x0A_SetDate_h_
#include "../CmdHandler_ajaxReq.h"


namespace socketbridge
{
    /*********************************************************
     * CmdHandler_ajaxReq_P0x0A_SetDate
     *
     *
        Input:
            command: setDate
            params:
				y: intero >= 2000
				m: intero 1-12
				d: intero 1-31

		Output
			OK
			KO
     */


    class CmdHandler_ajaxReq_P0x0A_SetDate : public CmdHandler_ajaxReq
    {
    public:
		CmdHandler_ajaxReq_P0x0A_SetDate(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec, u8 ajaxRequestID) :
                                CmdHandler_ajaxReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec, ajaxRequestID)
                                { }

		bool				needToPassDownToCPUBridge() const { return true; }
        void                passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *params);
        void                onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);

        static const char*  getCommandName()                            { return "setDate"; }
    };

} // namespace socketbridge

#endif // _CmdHandler_ajaxReq_P0x0A_SetDate_h_
