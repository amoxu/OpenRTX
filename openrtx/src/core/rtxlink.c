/***************************************************************************
 *   Copyright (C) 2022 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN                             *
 *                         Frederik Saraci IU2NRO                          *
 *                         Silvano Seva IU2KWO                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <interfaces/com_port.h>
#include <rtxlink.h>
#include <stdbool.h>
#include <string.h>
#include <slip.h>

enum dataMode
{
    DATAMODE_CAT,
    DATAMODE_FILETRANSFER,
    DATAMODE_XMODEM
};

enum errno
{
    EBADR   = 53,   // Invalid request descriptor
    EBADRQC = 56    // Invalid request code
};

uint8_t       dataBuf[128];
size_t        dataBufPos = 0;
enum dataMode mode = DATAMODE_CAT;

/**
 * \internal
 * Pack data into a SLIP-encoded data frame and send it over the com port.
 *
 * @param data: payload data.
 * @param size: payload size.
 */
static inline void sendSlipFrame(const uint8_t *data, const size_t size)
{
    size_t len = slip_encode(data, dataBuf, size, true);
    com_writeBlock(dataBuf, len);
}

/**
 * \internal
 * Send a CAT NACK reply.
 *
 * @param errcode: POSIX error code.
 */
static inline void cat_sendNack(const uint8_t errcode)
{
    uint8_t reply[2];
    reply[0] = 0x41;    // Ack
    reply[1] = errcode;

    sendSlipFrame(reply, 2);
}

/**
 * \internal
 * Handle the CAT "GET" command.
 *
 */
static void cat_cmdGet(const uint8_t *param)
{

}

/**
 * \internal
 * Handle the CAT "SET" command.
 *
 */
static void cat_cmdSet(const uint8_t *param)
{
    if(dataBufPos != 7) cat_sendNack(EBADRQC);

    uint16_t id  = (param[0] << 8) | param[1];
    uint32_t val = *((uint32_t *) (param + 2));
    uint8_t  reply[5] = {0};
    reply[0] = 0x41;

    switch(id)
    {
        case 0x4749:    // Radio ID
            reply[1] = 'T';
            reply[2] = 'E';
            reply[3] = 'S';
            reply[4] = 'T';
            break;

        case 0x5043:    // Reboot
            break;

        case 0x4654:    // File transfer mode
            mode = DATAMODE_FILETRANSFER;
            break;

        case 0x5346:    // Set frequency
            (void) val;
            break;

        default:
            cat_sendNack(EBADR);
            break;
    }

    sendSlipFrame(reply, 5);
}

/**
 * \internal
 * Handle the CAT "PEEK" command.
 *
 */
static void cat_cmdPeek(const uint8_t *param)
{
    if(dataBufPos != 5) cat_sendNack(EBADRQC);

    uint8_t  reply[5];
    uint32_t addr = *((uint32_t *) param);

    reply[0] = 0x44;    // Data
    for(int i = 0; i < 4; i++)
    {
        reply[i + 1] = ((uint8_t *) addr)[i];
    }

    sendSlipFrame(reply, 5);
}



void rtxlink_init()
{

}

void rtxlink_task()
{
    // Fetch data from com port, exit on error or no data received.
    uint8_t buf[64];
    ssize_t bLen = com_readBlock(&buf, 64);
    if(bLen <= 0) return;

    // Decode SLIP-encoded data
    ssize_t end = slip_searchFrameEnd(buf, bLen);
    ssize_t len = slip_decodeBlock(buf, bLen);

    if((len > 0) && ((dataBufPos + len) < 128))
    {
        memcpy(&dataBuf[dataBufPos], buf, len);
        dataBufPos += len;
    }

    // Still some data to receive, wait for next round.
    if(end < 0) return;

    // Handle data
    switch(mode)
    {
        case DATAMODE_CAT:
        {
            uint8_t cmd = dataBuf[0];
            switch(cmd)
            {
                case 0x47: cat_cmdGet(&dataBuf[1]);  break; // Get parameter
                case 0x50: cat_cmdPeek(&dataBuf[1]); break; // Peek memory
                case 0x53: cat_cmdSet(&dataBuf[1]);  break; // Set parameter
                default:   cat_sendNack(EBADRQC);    break; // Invalid opcode
            }
        }
            break;

        case DATAMODE_FILETRANSFER: break;
        case DATAMODE_XMODEM:       break;
        default: break;
    }
}

void rtxlink_terminate()
{

}
