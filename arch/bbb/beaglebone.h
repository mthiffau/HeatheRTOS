#ifndef _BEALGEBONE_H_
#define _BEAGLEBONE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
**                    FUNCTION PROTOTYPES
*****************************************************************************/
extern void GPIO1ModuleClkConfig(void);
extern void GPIO1Pin23PinMuxSetup(void);
extern void GPIO0ModuleClkConfig(void);
extern void UARTPinMuxSetup(unsigned int instanceNum);
extern void CPSWPinMuxSetup(void);
extern void CPSWClkEnable(void);
extern unsigned int RTCRevisionInfoGet(void);
extern void EDMAModuleClkConfig(void);
extern void EVMMACAddrGet(unsigned int addrIdx, unsigned char *macAddr);
extern void WatchdogTimer1ModuleClkConfig(void);
extern void DMTimer2ModuleClkConfig(void);
extern void DMTimer3ModuleClkConfig(void);
extern void DMTimer4ModuleClkConfig(void);
extern void DMTimer7ModuleClkConfig(void);
extern void EVMPortMIIModeSelect(void);
extern void RTCModuleClkConfig(void);
extern void HSMMCSDModuleClkConfig(void);
extern void HSMMCSDPinMuxSetup(void);
extern void I2C0ModuleClkConfig(void);
extern void I2C1ModuleClkConfig(void);
extern void I2CPinMuxSetup(unsigned int instance);
extern void GpioPinMuxSetup(unsigned int offsetAddr,
                            unsigned int padConfValue);

#ifdef __cplusplus
}
#endif

#endif
