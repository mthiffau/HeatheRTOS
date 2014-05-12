#ifndef DMTIMER_H
#define DMTIMER_H

#include "hw_dmtimer.h"
#include "hw_types.h"

/*****************************************************************************/
/*
  Values that can be passed to DMTimerPreScalerClkEnable as ptv so as to derive
  Pre-Scalar clock from timer clock.
*/
/* Value used to divide timer clock by 2 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_2      ((0 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 4 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_4	    ((1 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 8 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_8	    ((2 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 16 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_16	    ((3 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 32 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_32	    ((4 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 64 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_64	    ((5 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 128 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_128    ((6 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/* Value used to divide timer clock by 256 */
#define DMTIMER_PRESCALER_CLK_DIV_BY_256    ((7 << DMTIMER_TCLR_PTV_SHIFT) | \
                                              DMTIMER_TCLR_PRE)

/******************************************************************************/
/*
  Values that can be passed to DMTimerModeConfigure as timerMode.
*/
/* Value used to enable the timer in one-shot and compare mode */
#define DMTIMER_ONESHOT_CMP_ENABLE          (DMTIMER_TCLR_CE)

/* Value used to enable the timer only in one-shot mode */
#define DMTIMER_ONESHOT_NOCMP_ENABLE        (0x0000000)

/* Value used to enable the timer in auto-reload and compare mode */
#define DMTIMER_AUTORLD_CMP_ENABLE          (DMTIMER_TCLR_AR | DMTIMER_TCLR_CE)

/* Value used to enable the timer only in auto-reload mode */
#define DMTIMER_AUTORLD_NOCMP_ENABLE        (DMTIMER_TCLR_AR)

/******************************************************************************/
/*
  Values that can be passed to DMTimerGPOConfigure as gpoCfg.
*/
/* Value used to drive 0 on PORGPOCFG pin */
#define DMTIMER_GPO_CFG_0		    (DMTIMER_TCLR_GPO_CFG_DRIVE0)

/* Value used to drive 1 on PORGPOCFG pin */
#define DMTIMER_GPO_CFG_1		    (DMTIMER_TCLR_GPO_CFG_DRIVE1 << \
					     DMTIMER_TCLR_GPO_CFG_SHIFT)

/******************************************************************************/
/*
  Values that can be passed to DMTimerIntStatusClear/DMTimerIntRawStatusSet/
  as intFlags. Also these values can be used while checking the status got from 
  DMTimerIntRawStatusGet/DMTimerIntStatusGet.
  Any combination is also followed.
  Example- (DMTIMER_INT_TCAR_IT_FLAG | DMTIMER_INT_OVF_IT_FLAG)
*/
/* Value used for capture event of DMTimer */
#define DMTIMER_INT_TCAR_IT_FLAG             (DMTIMER_IRQSTATUS_RAW_TCAR_IT_FLAG)

/* Value used for overflow event of DMTimer */
#define DMTIMER_INT_OVF_IT_FLAG              (DMTIMER_IRQSTATUS_RAW_OVF_IT_FLAG) 

/* Value used for Match event of DMTimer */
#define DMTIMER_INT_MAT_IT_FLAG              (DMTIMER_IRQSTATUS_RAW_MAT_IT_FLAG)

/******************************************************************************/
/*
  Values that can be passed to DMTimerIntEnable/DMTimerIntDisable as intFlags.
  Also these values can be used while checking the status got from 
  DMTimerIntEnableGet.
  Any combination is also followed.
  Example- (DMTIMER_INT_TCAR_EN_FLAG | DMTIMER_INT_OVF_EN_FLAG)
*/
/* Value used for capture event of DMTimer */
#define DMTIMER_INT_TCAR_EN_FLAG             (DMTIMER_IRQENABLE_SET_TCAR_EN_FLAG)

/* Value used for overflow event of DMTimer */
#define DMTIMER_INT_OVF_EN_FLAG              (DMTIMER_IRQENABLE_SET_OVF_EN_FLAG)

/* Value used for Match event of DMTimer */
#define DMTIMER_INT_MAT_EN_FLAG              (DMTIMER_IRQENABLE_SET_MAT_EN_FLAG)

/******************************************************************************/
/*
  Values that can be passed to DMTimerResetConfigure as rstOption.
*/
/* Value used to enable software reset for DMTimer */
#define DMTIMER_SFT_RESET_ENABLE             (DMTIMER_TSICR_SFT_RESETENABLE)

/* Value used to disable software reset for DMTimer */
#define DMTIMER_SFT_RESET_DISABLE            (DMTIMER_TSICR_SFT)

/******************************************************************************/
/*
  Values that can be used while checking the status received from 
  DMTimerIsResetDone.
*/
/* Value used to check whether reset is done */
#define DMTIMER_IS_RESET_DONE                (DMTIMER_TIOCP_CFG_SOFTRESET_DONE)

/* Value used to check whether reset is ongoing */
#define DMTIMER_IS_RESET_ONGOING             (DMTIMER_TIOCP_CFG_SOFTRESET_ONGOING)

/******************************************************************************/
/*
  Structure to store the DM timer context
*/
typedef struct dmtimerContext{
    unsigned int tldr;
    unsigned int tmar;
    unsigned int irqenableset;
    unsigned int tcrr;
    unsigned int tclr;
} DMTIMERCONTEXT;

/*
  Prototype of the APIs
*/
extern void DMTimerEnable(unsigned int baseAdd);
extern void DMTimerDisable(unsigned int baseAdd);
extern void DMTimerModeConfigure(unsigned int baseAdd, unsigned int timerMode);
extern void DMTimerPreScalerClkEnable(unsigned int baseAdd, unsigned int ptv);
extern void DMTimerPreScalerClkDisable(unsigned int baseAdd);
extern void DMTimerCounterSet(unsigned int baseAdd, unsigned int counter);
extern unsigned int DMTimerCounterGet(unsigned int baseAdd);
extern void DMTimerReloadSet(unsigned int baseAdd, unsigned int reload);
extern unsigned int DMTimerReloadGet(unsigned int baseAdd);
extern void DMTimerGPOConfigure(unsigned int baseAdd, unsigned int gpoCfg);
extern void DMTimerCompareSet(unsigned int baseAdd, unsigned int compareVal);
extern unsigned int DMTimerCompareGet(unsigned int baseAdd);
extern void DMTimerEmulationModeConfigure(unsigned int baseAdd, unsigned int emuMode);
extern void DMTimerIntRawStatusSet(unsigned int baseAdd, unsigned int intFlags);
extern unsigned int DMTimerIntRawStatusGet(unsigned int baseAdd);
extern unsigned int DMTimerIntStatusGet(unsigned int baseAdd);
extern void DMTimerIntStatusClear(unsigned int baseAdd, unsigned int intFlags);
extern void DMTimerIntEnable(unsigned int baseAdd, unsigned int intFlags);
extern void DMTimerIntDisable(unsigned int baseAdd, unsigned int intFlags);
extern void DMTimerTriggerSet(unsigned int baseAdd);
extern unsigned int DMTimerIntEnableGet(unsigned int baseAdd);
extern void DMTimerResetConfigure(unsigned int baseAdd, unsigned int rstOption);
extern void DMTimerReset(unsigned int baseAdd);
extern void DMTimerContextSave(unsigned int baseAdd, DMTIMERCONTEXT *contextPtr);
extern void DMTimerContextRestore(unsigned int baseAdd, DMTIMERCONTEXT *contextPtr);

extern void DMTimer2ModuleClkConfig(void);

#endif
