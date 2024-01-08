/**
 * \file IfxEbu.c
 * \brief EBU  basic functionality
 *
 * \version iLLD_1_0_1_17_0_1
 * \copyright Copyright (c) 2018 Infineon Technologies AG. All rights reserved.
 *
 *
 *                                 IMPORTANT NOTICE
 *
 * Use of this file is subject to the terms of use agreed between (i) you or
 * the company in which ordinary course of business you are acting and (ii)
 * Infineon Technologies AG or its licensees. If and as long as no such terms
 * of use are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer, must
 * be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are
 * solely in the form of machine-executable object code generated by a source
 * language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/

#include "IfxEbu.h"

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/

void IfxEbu_setExternalClockRatio(Ifx_EBU *ebu, IfxEbu_ExternalClockRatio ratio)
{
    Ifx_EBU_CLC clc;
    clc.U = ebu->CLC.U;

    switch (ratio)
    {
    case IfxEbu_ExternalClockRatio_1:
        clc.B.EBUDIV = 0;
        clc.B.DIV2   = 0;
        break;
    case IfxEbu_ExternalClockRatio_2:
        clc.B.EBUDIV = 1;
        clc.B.DIV2   = 0;
        break;
    case IfxEbu_ExternalClockRatio_3:
        clc.B.EBUDIV = 2;
        clc.B.DIV2   = 0;
        break;
    case IfxEbu_ExternalClockRatio_4:
        clc.B.EBUDIV = 3;
        clc.B.DIV2   = 0;
        break;
    case IfxEbu_ExternalClockRatio_6:
        clc.B.EBUDIV = 2;
        clc.B.DIV2   = 1;
        break;
    case IfxEbu_ExternalClockRatio_8:
        clc.B.EBUDIV = 3;
        clc.B.DIV2   = 1;
        break;
    }

    ebu->CLC.U = clc.U;
}


void IfxEbu_setByteControlEnable(Ifx_EBU *ebu, IfxEbu_ByteControlEnable byteControlEnable)
{
    ebu->USERCON.B.BCEN = byteControlEnable;
}


void IfxEbu_disableModule(Ifx_EBU *ebu)
{
    uint16 psw = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(psw); /* clears the endinit protection*/
    ebu->CLC.B.DISR = 1;
    IfxScuWdt_setCpuEndinit(psw);   /* sets the endinit protection back on*/
}
