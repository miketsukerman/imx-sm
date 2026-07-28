/*
** ###################################################################
**
**     Copyright 2023-2025 NXP
**
**     Redistribution and use in source and binary forms, with or without modification,
**     are permitted provided that the following conditions are met:
**
**     o Redistributions of source code must retain the above copyright notice, this list
**       of conditions and the following disclaimer.
**
**     o Redistributions in binary form must reproduce the above copyright notice, this
**       list of conditions and the following disclaimer in the documentation and/or
**       other materials provided with the distribution.
**
**     o Neither the name of the copyright holder nor the names of its
**       contributors may be used to endorse or promote products derived from this
**       software without specific prior written permission.
**
**     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
**     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
**     WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
**     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
**     ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
**     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
**     ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**     SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** ###################################################################
*/

#ifndef DEV_SM_COMMON_H
#define DEV_SM_COMMON_H

/*==========================================================================*/
/*!
 * @addtogroup DEV_SM_MX95
 * @{
 *
 * @file
 * @brief
 *
 * Header file containing the SM API for the common device.
 */
/*==========================================================================*/

/* Includes */

#include "sm.h"

/* Defines */

/*! Max number of device agents */
#define DEV_SM_NUM_AGENT   6U
/*! Number of devices */
#define DEV_SM_NUM_DEVICE  0U
/*! Number of device bases */
#define DEV_SM_NUM_BASE    (DEV_SM_NUM_AGENT + DEV_SM_NUM_DEVICE)

/*!
 * @name Base agent indexes
 */
/** @{ */
#define DEV_SM_BASE_AGENT_0  0U  /*!< Agent 0 */
#define DEV_SM_BASE_AGENT_1  1U  /*!< Agent 1 */
#define DEV_SM_BASE_AGENT_2  2U  /*!< Agent 2 */
#define DEV_SM_BASE_AGENT_3  3U  /*!< Agent 3 */
#define DEV_SM_BASE_AGENT_4  4U  /*!< Agent 4 */
#define DEV_SM_BASE_AGENT_5  5U  /*!< Agent 5 */
/** @} */

/*!
 * @name Silicon version IDs
 */
/** @{ */
#define DEV_SM_SIVER_A0  0x00000000U  /*!< A0 */
#define DEV_SM_SIVER_A1  0x00000001U  /*!< A1 */
#define DEV_SM_SIVER_B0  0x00010000U  /*!< B0 */
/** @} */

/*!
 * @name Rev A (A0/A1) quirk selection
 *
 * The SM applies a set of workarounds when running on Rev A (A0/A1) silicon.
 * SM_REVA_QUIRKS allows these to be forced on/off at build time to bisect
 * problems suspected to be caused by the Rev A code paths:
 *
 * - 0: never apply the Rev A quirks (always use the B0 code paths)
 * - 1: apply the Rev A quirks based on the silicon revision (default)
 * - 2: always apply the Rev A quirks (as if running on A0/A1)
 */
/** @{ */
#ifndef SM_REVA_QUIRKS
/*! Rev A quirk selection */
#define SM_REVA_QUIRKS  1U
#endif

#if (SM_REVA_QUIRKS == 0U)
/*! Check if the Rev A code paths should be used */
#define DEV_SM_IS_REVA()  false
#elif (SM_REVA_QUIRKS == 2U)
/*! Check if the Rev A code paths should be used */
#define DEV_SM_IS_REVA()  true
#else
/*! Check if the Rev A code paths should be used */
#define DEV_SM_IS_REVA()  (DEV_SM_SiVerGet() < DEV_SM_SIVER_B0)
#endif

#ifdef SM_REVA_SKIP_MIX_SSI
/*! Check if MIX-level SSI transaction blocking should be skipped */
#define DEV_SM_SKIP_MIX_SSI()  DEV_SM_IS_REVA()
#else
/*! Check if MIX-level SSI transaction blocking should be skipped */
#define DEV_SM_SKIP_MIX_SSI()  false
#endif
/** @} */

/*!
 * @name Boot stages
 *
 * Records the progress of DEV_SM_Init(). Reported as extended info in the
 * reset record for faults that occur during SM init.
 */
/** @{ */
#define DEV_SM_BOOT_STAGE_START     0U   /*!< Before device init */
#define DEV_SM_BOOT_STAGE_SYSTEM    1U   /*!< DEV_SM_SystemInit() */
#define DEV_SM_BOOT_STAGE_FAULT     2U   /*!< DEV_SM_FaultInit() */
#define DEV_SM_BOOT_STAGE_PERF      3U   /*!< DEV_SM_PerfInit() */
#define DEV_SM_BOOT_STAGE_POWER     4U   /*!< DEV_SM_PowerInit() */
#define DEV_SM_BOOT_STAGE_MEM       5U   /*!< DEV_SM_MemInit() */
#define DEV_SM_BOOT_STAGE_CPU       6U   /*!< DEV_SM_CpuInit() */
#define DEV_SM_BOOT_STAGE_SENSOR    7U   /*!< DEV_SM_SensorInit() */
#define DEV_SM_BOOT_STAGE_RDC       8U   /*!< DEV_SM_RdcInit() */
#define DEV_SM_BOOT_STAGE_PWRUP     9U   /*!< DEV_SM_PowerUpPost() loop */
#define DEV_SM_BOOT_STAGE_BBM       10U  /*!< DEV_SM_BbmInit() */
#define DEV_SM_BOOT_STAGE_DONE      11U  /*!< Device init complete */
/** @} */

/*!
 * @name Device init error flags
 */
/** @{ */
#define DEV_SM_ERR_INITCLOCKS   BIT32(0U)  /*!< BOARD_InitClocks() error */
#define DEV_SM_ERR_INITCONSOLE  BIT32(1U)  /*!< BOARD_InitDebugConsole() error */
#define DEV_SM_ERR_INITTIMERS   BIT32(2U)  /*!< BOARD_InitTimers() error */
#define DEV_SM_ERR_INITSERIAL   BIT32(3U)  /*!< BOARD_InitSerialBus() error */
/** @} */

/* Types */

/*!
 * Syslog
 */
typedef struct
{
    /*! System sleep record */
    dev_sm_sys_sleep_rec_t sysSleepRecord;

    /*! Device error log */
    uint32_t devErrLog;

#ifdef DEV_SM_MSG_PROF_CNT
    /*! Message profiling record */
    dev_sm_sys_msg_rec_t sysMsgRecord;
#endif
} dev_sm_syslog_t;

/* Global variables */

/*! Structure to hold the syslog */
extern dev_sm_syslog_t g_syslog;

/*! Current boot stage (see @ref DEV_SM_BOOT_STAGE_START) */
extern uint32_t g_bootStage;

/*! Power domain of the last power state transition requested */
extern uint32_t g_bootStageDomain;

/* Functions */

/** @} */

/* Include SM device API */

/* coverity[misra_c_2012_rule_20_1_violation] */
#include "dev_sm_common_api.h"

#endif /* DEV_SM_COMMON_H */

