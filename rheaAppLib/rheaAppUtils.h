#ifndef _rheaAppUtils_h_
#define _rheaAppUtils_h_
#include "../rheaCommonLib/rhea.h"
#include "../SocketBridge/SocketBridgeEnumAndDefine.h"
#include "../CPUBridge/CPUBridgeEnumAndDefine.h"
#include "rheaAppEnumAndDefine.h"

namespace rhea
{
	namespace app
	{
		namespace utils
		{
			const char*		verbose_eVMCState (cpubridge::eVMCState s);
			const char*		verbose_eRunningSelStatus (cpubridge::eRunningSelStatus s);
			void			verbose_SokBridgeClientVer(const socketbridge::SokBridgeClientVer &s, char *out, u32 sizeOfOut);

			const char*		verbose_fileTransferStatus (eFileTransferStatus status);
			const char*		verbose_fileTransferFailReason(socketbridge::eFileTransferFailReason failReason);

			const char*		verbose_readDataFileStatus(cpubridge::eReadDataFileStatus status);
			const char*		verbose_writeDataFileStatus(cpubridge::eWriteDataFileStatus status);
			const char*		verbose_WriteCPUFWFileStatus(cpubridge::eWriteCPUFWFileStatus status);

			const char*		verbose_CPUMachineType(cpubridge::eCPUMachineType type);
		} //namespace utils
	} // namespace app

} // namespace rhea


#endif // _rheaAppUtils_h_

