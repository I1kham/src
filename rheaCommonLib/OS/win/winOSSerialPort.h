#ifdef WIN32
#ifndef _winOSSerialPort_h_
#define _winOSSerialPort_h_
#include "winOSInclude.h"
#include "../../rheaEnumAndDefine.h"

namespace platform
{
	void			serialPort_setInvalid (OSSerialPort &sp);
	bool			serialPort_isInvalid(const OSSerialPort &sp);

	bool            serialPort_open(OSSerialPort *out_serialPort, const char *deviceName,
									eRS232BaudRate baudRate,
									bool RST_on, bool DTR_on,
									eRS232DataBits dataBits, eRS232Parity parity,
									eRS232StopBits stop, eRS232FlowControl flowCtrl, bool bBlocking);

	void            serialPort_close(OSSerialPort &sp);

	void            serialPort_setRTS(OSSerialPort &sp, bool bON_OFF);
	void            serialPort_setDTR(OSSerialPort &sp, bool bON_OFF);

	void            serialPort_flushIO(OSSerialPort &sp);

	u32             serialPort_readBuffer(OSSerialPort &sp, void *out_byteRead, u32 numMaxByteToRead);
	u32             serialPort_writeBuffer(OSSerialPort &sp, const void *buffer, u32 nBytesToWrite);
}


#endif // _winOSSerialPort_h_
#endif // WIN32