#ifndef _CmdHandler_ajaxReq_P0x08_GetDate_h_
#define _CmdHandler_ajaxReq_P0x08_GetDate_h_
#include "../CmdHandler_ajaxReq.h"


namespace socketbridge
{
    /*********************************************************
     * CmdHandler_ajaxReq_P0x08_GetDate
     *
     * la GUI richiede la data di sistema
     *
        Input:
            command: getDate
            params:  none

        Output:
			json
			{
				y: intero 
				m: intero
				d: intero
			}
     */


    class CmdHandler_ajaxReq_P0x08_GetDate : public CmdHandler_ajaxReq
    {
    public:
		CmdHandler_ajaxReq_P0x08_GetDate(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec, u8 ajaxRequestID) :
                                CmdHandler_ajaxReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec, ajaxRequestID)
                                { }

		bool				needToPassDownToCPUBridge() const { return true; }
        void                passDownRequestToCPUBridge (cpubridge::sSubscriber &from, const u8 *params);
        void                onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);

        static const char*  getCommandName()                            { return "getDate"; }
    };

} // namespace socketbridge

#endif // _CmdHandler_ajaxReq_P0x08_GetDate_h_
