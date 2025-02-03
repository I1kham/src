#ifdef LINUX
#include "linuxOS.h"
#include "linuxOSSerialPort.h"
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <sys/ioctl.h>
#include "../../rhea.h"

//*****************************************************
void platform::serialPort_setInvalid (OSSerialPort &sp)
{
    sp.fd = -1;
}

//*****************************************************
bool platform::serialPort_isInvalid (const OSSerialPort &sp)
{
    return (sp.fd == -1);
}

//*****************************************************
bool serialPort_setAsBlocking(OSSerialPort &sp, u8 minNumOfCharToBeReadInOrderToSblock)
{
	if (sp.fd == -1)
		return false;

	sp.config.c_cc[VMIN] = minNumOfCharToBeReadInOrderToSblock; //leggi almeno n char prima di ritornare
	sp.config.c_cc[VTIME] = 0; //attende per sempre

	if (tcsetattr(sp.fd, TCSAFLUSH, &sp.config) < 0)
		return false;

	return true;
}

//*****************************************************
bool serialPort_setAsNonBlocking(OSSerialPort &sp, u8 numOfDSecToWaitBeforeReturn)
{
	if (sp.fd == -1)
		return false;

	sp.config.c_cc[VMIN] = 0;
	sp.config.c_cc[VTIME] = numOfDSecToWaitBeforeReturn;   //numero di dSec massimi di attesa dalla lettura dell'ultimo char

	if (tcsetattr(sp.fd, TCSAFLUSH, &sp.config) < 0)
		return false;

	return true;
}


//*****************************************************
bool platform::serialPort_open (OSSerialPort *out_serialPort, const char *deviceName, eRS232BaudRate baudRate, bool RST_on, bool DTR_on, eRS232DataBits dataBits,
                        eRS232Parity parity, eRS232StopBits stopBits, eRS232FlowControl flowCtrl, bool bBlocking)
{
    out_serialPort->fd = ::open(deviceName, O_RDWR | O_NOCTTY | O_NDELAY);
    if (out_serialPort->fd == -1)
    {
        switch (errno)
        {
        case ENOENT:    return false; //the named file does not exist;
        case EACCES:    return false;
        case EEXIST:    return false;
        case EINTR:     return false;
        case EINVAL:    return false;
        case EIO:       return false;
        case EISDIR:       return false;
        case ELOOP:     return false;
        case EMFILE:    return false;
        default:        return false;
        }
        return false;
    }

    //recupera l'attuale impostazione della porta seriale
    memset(&out_serialPort->config, 0, sizeof(out_serialPort->config));
    if (tcgetattr (out_serialPort->fd, &out_serialPort->config) < 0)
        return false;

    //disabilita tutto l'input/uotput processing, vogliamo un input/output raw
    out_serialPort->config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    out_serialPort->config.c_oflag = 0;
    out_serialPort->config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);


    //SerialPortSettings.c_cflag |= CREAD | CLOCAL;  Enable receiver,Ignore Modem Control lines

    //data bits
    out_serialPort->config.c_cflag &= ~CSIZE;
    switch (dataBits)
    {
        case eRS232DataBits::b5: out_serialPort->config.c_cflag |= CS5; break;
        case eRS232DataBits::b6: out_serialPort->config.c_cflag |= CS6; break;
        case eRS232DataBits::b7: out_serialPort->config.c_cflag |= CS7; break;
        case eRS232DataBits::b8: out_serialPort->config.c_cflag |= CS8; break;
    }

    //parity
    out_serialPort->config.c_cflag &= ~(PARENB | PARODD);
    switch (parity)
    {
        case eRS232Parity::No:      break;
        case eRS232Parity::Even:    out_serialPort->config.c_cflag |= PARENB; break;
        case eRS232Parity::Odd:     out_serialPort->config.c_cflag |= (PARENB | PARODD); break;
    }


    //Stop bits
    switch (stopBits)
    {
        case eRS232StopBits::One: out_serialPort->config.c_cflag &= ~CSTOPB; break;
        case eRS232StopBits::Two: out_serialPort->config.c_cflag |= CSTOPB; break;
    }

    //flow
    switch (flowCtrl)
    {
        case eRS232FlowControl::No:  out_serialPort->config.c_cflag &= ~CRTSCTS; break;
        case eRS232FlowControl::HW:  out_serialPort->config.c_cflag |= CRTSCTS; break;
    }

    //baud rate
    int brate = B2400;
    switch (baudRate)
    {
    case eRS232BaudRate::b1200:      brate = B1200; break;
    case eRS232BaudRate::b2400:      brate = B2400; break;
    case eRS232BaudRate::b4800:      brate = B4800; break;
    case eRS232BaudRate::b9600:      brate = B9600; break;
    case eRS232BaudRate::b19200:     brate = B19200; break;
    case eRS232BaudRate::b38400:     brate = B38400; break;
    case eRS232BaudRate::b57600:     brate = B57600; break;
    case eRS232BaudRate::b115200:    brate = B115200; break;
    case eRS232BaudRate::b230400:    brate = B230400; break;
    }


    if (cfsetispeed (&out_serialPort->config, brate) < 0 || cfsetospeed (&out_serialPort->config, brate) < 0)
    {
        return false;
    }


	if (bBlocking)
	{
		out_serialPort->config.c_cc[VMIN] = 1; //leggi almeno n char prima di ritornare
		out_serialPort->config.c_cc[VTIME] = 0; //attende per sempre
	}
	else
	{
		out_serialPort->config.c_cc[VMIN] = 0;
		out_serialPort->config.c_cc[VTIME] = 0;
	}

    //applica i parametri
    if (tcsetattr(out_serialPort->fd, TCSAFLUSH, &out_serialPort->config) < 0)
        return false;

    serialPort_setRTS (*out_serialPort, RST_on);
    serialPort_setDTR (*out_serialPort, DTR_on);
	
    if (tcsetattr(out_serialPort->fd, TCSAFLUSH, &out_serialPort->config) < 0)
		return false;


    return true;
}


//*****************************************************
void platform::serialPort_close(OSSerialPort &sp)
{
    if (sp.fd != -1)
        ::close (sp.fd);
    sp.fd = -1;
}

//*****************************************************
void platform::serialPort_setRTS (OSSerialPort &sp, bool bON_OFF)
{
    if (sp.fd == -1)
        return;
    int flag = TIOCM_RTS;
    if (bON_OFF)
        ioctl(sp.fd, TIOCMBIS, &flag);//Set RTS pin
    else
        ioctl(sp.fd, TIOCMBIC, &flag);//clear RTS pin
}

//*****************************************************
void platform::serialPort_setDTR (OSSerialPort &sp, bool bON_OFF)
{
    if (sp.fd == -1)
        return;
    int flag = TIOCM_DTR ;
    if (bON_OFF)
        ioctl(sp.fd, TIOCMBIS, &flag);//Set RTS pin
    else
        ioctl(sp.fd, TIOCMBIC, &flag);//clear RTS pin
}

//*****************************************************
void platform::serialPort_flushIO (OSSerialPort &sp)
{
    if (sp.fd == -1)
        return;
    ::tcflush (sp.fd, TCIOFLUSH);

}



//*****************************************************
u32 platform::serialPort_readBuffer (OSSerialPort &sp, void *out_byteRead, u32 numMaxByteToRead)
{
    int ret = ::read (sp.fd, out_byteRead, numMaxByteToRead);
    if (ret >= 0)
        return (u32)ret;
    return 0;
}



//*****************************************************
u32 platform::serialPort_writeBuffer (OSSerialPort &sp, const void *buffer, u32 nBytesToWrite)
{
    return write (sp.fd, buffer, nBytesToWrite);
}
#endif
