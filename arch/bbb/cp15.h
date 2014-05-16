/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/
/*
* Some of this code is pulled from TI's BeagleBone Black StarterWare
* Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
*
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#ifndef CP15_H
#define CP15_H

/*
  Macros which can be passed to CP15ControlFeatureDisable/Enable APIs 
  as 'features'. Any, or an OR combination of the below macros can be
  passed to disable/enable the corresponding feature.
*/
#define CP15_CONTROL_TEXREMAP                  (0x10000000) 
#define CP15_CONTROL_ACCESSFLAG                (0x20000000)
#define CP15_CONTROL_ALIGN_CHCK                (0x00000002)
#define CP15_CONTROL_MMU                       (0x00000001)

/*
  API prototypes
*/
extern void CP15AuxControlFeatureEnable(unsigned int enFlag);
extern void CP15AuxControlFeatureDisable(unsigned int disFlag);
extern void CP15DCacheCleanBuff(unsigned int bufPtr, unsigned int size);
extern void CP15DCacheCleanFlushBuff(unsigned int bufPtr, unsigned int size);
extern void CP15DCacheFlushBuff(unsigned int bufPtr, unsigned int size);
extern void CP15ICacheFlushBuff(unsigned int bufPtr, unsigned int size);
extern void CP15ICacheDisable(void);
extern void CP15DCacheDisable(void);
extern void CP15ICacheEnable(void);
extern void CP15DCacheEnable(void);
extern void CP15DCacheCleanFlush(void);
extern void CP15DCacheClean(void);
extern void CP15DCacheFlush(void);
extern void CP15ICacheFlush(void);
extern void CP15Ttb0Set(unsigned int ttb);
extern void CP15TlbInvalidate(void);
extern void CP15MMUDisable(void);
extern void CP15MMUEnable(void);
extern void CP15VectorBaseAddrSet(unsigned int addr);
extern void CP15BranchPredictorInvalidate(void);
extern void CP15BranchPredictionEnable(void);
extern void CP15BranchPredictionDisable(void);
extern void CP15DomainAccessClientSet(void);
extern void CP15ControlFeatureDisable(unsigned int features);
extern void CP15ControlFeatureEnable(unsigned int features);
extern void CP15TtbCtlTtb0Config(void);
extern unsigned int CP15MainIdPrimPartNumGet(void);

#endif /* CP15_H */
