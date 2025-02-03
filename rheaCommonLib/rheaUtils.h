#ifndef _rheaUtils_h_
#define _rheaUtils_h_
#include "rheaDataTypes.h"
#include <stdio.h>

namespace rhea
{
    namespace utils
    {

        size_t      base64_howManyBytesNeededForEncoding (size_t sizeInBytesOfBufferToEncode);
                    /* ritorna il numero di bytes necessari a contenere la rappresentazione
                     * in base64 di dei primi [sizeInBytesOfIn] di [in]
                     */

        bool        base64_encode (void *out, size_t sizeOfOutInBytes, const void *in, size_t sizeInBytesOfIn);
                    /* prende i primi [sizeInBytesOfIn] di [in] e li converte nella rappresentazione in base64.
                     * Mette il risultato in out appendendo un 0x00 a fine buffer.
                     *
                     * Ritorna false se [sizeOfOutInBytes] non è suff ad ospitare la conversione
                     */

        int         base64_decode (u8 *binary, size_t *binary_length, const char *base64IN, size_t sizeInBytesOfbase64IN);

        bool        sha1 (void *out, size_t sizeOfOutInBytes, const void *in, size_t sizeInBytesOfIn);
                    /* Dato un buffer [in], mette in [out] un hash di 20 byte secondo l'algoritmo sha1
                     *
                     * out deve essere di almeno 20 bytes
                     */

        bool        md5 (char *out, size_t sizeOfOutInBytes, const void *in, u32 sizeInBytesOfIn);
                    /* Dato un buffer [in], mette in [out] un hash di 32 byte secondo l'algoritmo md5
                     *
                     * out deve essere di almeno 32 bytes
                     */


        u8          simpleChecksum8_calc (const void *bufferIN, u32 lenInBytes);
                    /*  calcola un semplice checksum 8 bit */

        u16         simpleChecksum16_calc (const void *bufferIN, u32 lenInBytes);
                    /*  calcola un semplice checksum 16 bit */

        u16         Crc16_calc(const uint8_t* data, const size_t leninBytes);
                    /* calcola un crc a 16 bit */

		void		dumpBufferInASCII(FILE *f, const u8 *buffer, u32 lenInBytes);
					/* dumpa su file i primi [lenInBytes] bytes di buffer. Se possibile, usa caratteri ASCII printabili
						altrimenti scrive la rappresentazione in hex del numero racchiuso tra quadre (es: [A5]).
						NON appende un \n alla fine
					*/

        u32         copyBufferAsMuchAsYouCan (void *dst, u32 sizeOfDest, const void *src, u32 numBytesToCopyMax);
                    /* copia un massimo di [numBytesToCopyMax] da [src] in [dst].
                     * Se [sizeOfDest] � minore di [numBytesToCopyMax], copia tutto quello che pu� senza andare in buffer overflow.
                     * Ritorna il num di bytes copiati in dst
                     */


		inline void	bufferWriteU32(u8 *buffer, u32 val) { buffer[0] = (u8)((val & 0xFF000000) >> 24); buffer[1] = (u8)((val & 0x00FF0000) >> 16); buffer[2] = (u8)((val & 0x0000FF00) >> 8); buffer[3] = (u8)(val & 0x000000FF); }
		inline void	bufferWriteU32_LSB_MSB (u8 *buffer, u32 val) { buffer[3] = (u8)((val & 0xFF000000) >> 24); buffer[2] = (u8)((val & 0x00FF0000) >> 16); buffer[1] = (u8)((val & 0x0000FF00) >> 8); buffer[0] = (u8)(val & 0x000000FF); }

		inline void	bufferWriteU24(u8 *buffer, u32 val) { buffer[0] = (u8)((val & 0x00FF0000) >> 16); buffer[1] = (u8)((val & 0x0000FF00) >> 8); buffer[2] = (u8)(val & 0x000000FF); }
		inline void	bufferWriteU24_LSB_MSB (u8 *buffer, u32 val) { buffer[2] = (u8)((val & 0x00FF0000) >> 16); buffer[1] = (u8)((val & 0x0000FF00) >> 8); buffer[0] = (u8)(val & 0x000000FF); }

		inline void	bufferWriteU16(u8 *buffer, u16 val) { buffer[0] = (u8)((val & 0xFF00) >> 8); buffer[1] = (u8)(val & 0x00FF); }
		inline void	bufferWriteU16_LSB_MSB(u8 *buffer, u16 val) { buffer[1] = (u8)((val & 0xFF00) >> 8); buffer[0] = (u8)(val & 0x00FF); }

		inline u32	bufferReadU32(const u8 *buffer) { return (((u32)buffer[0]) << 24) | (((u32)buffer[1]) << 16) | (((u32)buffer[2]) << 8) | ((u32)buffer[3]); }
		inline u32	bufferReadU32_LSB_MSB(const u8 *buffer) { return (((u32)buffer[3]) << 24) | (((u32)buffer[2]) << 16) | (((u32)buffer[1]) << 8) | ((u32)buffer[0]); }

		inline u32	bufferReadU24(const u8 *buffer) { return (((u32)buffer[0]) << 16) | (((u32)buffer[1]) << 8) | ((u32)buffer[2]); }
		inline u32	bufferReadU24_LSB_MSB(const u8 *buffer) { return (((u32)buffer[2]) << 16) | (((u32)buffer[1]) << 8) | ((u32)buffer[0]); }

		inline u16	bufferReadU16(const u8 *buffer) { return (((u16)buffer[0]) << 8) | ((u16)buffer[1]); }
		inline u16	bufferReadU16_LSB_MSB(const u8 *buffer) { return (((u16)buffer[1]) << 8) | ((u16)buffer[0]); }


    } //namespace utils
} //namespace rhea

#endif // _rheaUtils_h_

