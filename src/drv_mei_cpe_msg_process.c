/******************************************************************************

                          Copyright (c) 2007-2015
                     Lantiq Beteiligungs-GmbH & Co. KG

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

/* ==========================================================================
   Description : Message Handling between the driver and the VRX device.
   ========================================================================== */

/* ============================================================================
   Includes
   ========================================================================= */

/* get at first the driver configuration */
#include "drv_mei_cpe_config.h"

#include "ifx_types.h"

/* add VRX OS Layer */
#include "drv_mei_cpe_os.h"
/* add VRX debug/printout part */
#include "drv_mei_cpe_dbg.h"

#include "drv_mei_cpe_interface.h"
#include "drv_mei_cpe_mei_interface.h"
#include "drv_mei_cpe_api.h"

#include "drv_mei_cpe_msg_process.h"
#include "drv_mei_cpe_driver_msg.h"

#if (MEI_SUPPORT_DSM == 1)
#include "drv_mei_cpe_dsm.h"
#endif /* (MEI_SUPPORT_DSM == 1) */

#include "drv_mei_cpe_dbg_driver.h"

#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
#include "drv_mei_cpe_dbg_access.h"
#endif

#if (MEI_SUPPORT_ROM_CODE == 1)
#include "drv_mei_cpe_rom_handler.h"
#endif

#if (MEI_SUPPORT_DL_DMA_CS == 1)
#include "drv_mei_cpe_download.h"
#endif

#include "drv_mei_cpe_download.h"

#include "drv_mei_cpe_device_cntrl.h"

#if (MEI_DRV_ATM_OAM_ENABLE == 1)
#include "drv_mei_cpe_atmoam_common.h"
#endif

#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
#include "drv_mei_cpe_clear_eoc_common.h"
#endif

#if (MEI_SUPPORT_DEBUG_STREAMS == 1)
#include "drv_mei_cpe_dbg_streams.h"
#endif /* (MEI_SUPPORT_DEBUG_STREAMS == 1) */


/* ============================================================================
   Local Macros & Definitions
   ========================================================================= */

#ifdef MEI_STATIC
#undef MEI_STATIC
#endif

#ifdef MEI_DEBUG
#define MEI_STATIC
#else
#define MEI_STATIC   static
#endif

/* local test: check mailbox code */
#define MEI_TEST_LOCAL_MB_CODE    0

/* ============================================================================
   Local Message Function declarations
   ========================================================================= */

MEI_STATIC IFX_int32_t MEI_SetMailboxAddress(
                              MEI_DEV_T *pMeiDev);

MEI_STATIC IFX_int32_t MEI_SetDescrMailboxAddr(
                              MEI_DEV_T *pMeiDev);

MEI_STATIC IFX_int32_t MEI_MBox_WaitForAck(
                              MEI_DEV_T          *pMeiDev,
                              MEI_DYN_CMD_DATA_T *pDynCmd);

#if (MEI_SUPPORT_DRV_LOOPS == 1)
MEI_STATIC IFX_int32_t MEI_WriteMailboxLoopBack(
                              MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                              MEI_DYN_CMD_DATA_T *pDynCmd,
                              MEI_MEI_MAILBOX_T  *pMBMsg,
                              IFX_int32_t          mbMsgSize);
#endif

MEI_STATIC IFX_void_t MEI_RecvAckMailboxMsg(
                              MEI_DEV_T *pMeiDev);

MEI_STATIC IFX_void_t MEI_RecvAutonomMailboxMsg(
                              MEI_DEV_T    *pMeiDev,
                              IFX_uint16_t   mboxCode);

MEI_STATIC IFX_void_t MEI_Recv_MODEM_FSM_STATE(
                              MEI_DEV_T *pMeiDev,
                              CMV_STD_MESSAGE_T *pMsg);

MEI_STATIC IFX_void_t MEI_Recv_MODEM_EVT_TC(
                              MEI_DEV_T *pMeiDev,
                              CMV_STD_MESSAGE_T *pMsg);

MEI_STATIC IFX_void_t MEI_Recv_AUTO_MODEM_READY(
                              MEI_DEV_T             *pMeiDev,
                              CMV_MESSAGE_MODEM_RDY_T *pMsg);

MEI_STATIC IFX_int32_t MEI_GetNextRdNfcMsg(
                              MEI_DYN_CNTRL_T *pMeiDynCntrl,
                              MEI_RECV_BUF_T  **ppRdBuf,
                              IFX_uint8_t       rdIdxRd);

MEI_STATIC IFX_void_t MEI_FreeNextRdNfcMsg(
                              MEI_DYN_CNTRL_T *pMeiDynCntrl,
                              IFX_uint8_t       rdIdxRd);

MEI_STATIC IFX_int32_t MEI_DriverMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall);


MEI_STATIC IFX_int32_t MEI_ModemNfcRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall);


#if (MEI_DRV_ATM_OAM_ENABLE == 1)
MEI_STATIC IFX_int32_t MEI_AtmOamMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall);
#endif

#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
MEI_STATIC IFX_int32_t MEI_CEocMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall);
#endif


/* ============================================================================
   Helper's functions
   ========================================================================= */

/**
   Log a CMV Mailbox Message.

\param
   pMeiDev:    points to the current device structure.
\param
   pCmvMsg:    CMV message to log.
\param
   pDescr:     user given "info".

\return
   NONE

\attention
   Called on int level
*/
IFX_void_t MEI_LogCmvMsg( MEI_DEV_T *pMeiDev,
                          CMV_STD_MESSAGE_T *pCmvMsg,
                          const char *pDescr, IFX_uint32_t dbgLevel)
{
   IFX_int32_t i;
   unsigned short paylSize;

   PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
       (MEI_DRV_CRLF "--- CMV Mailbox Msg: %s -- size 16 bit -----" MEI_DRV_CRLF, pDescr) );

   PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
       ("Hdr:  MbxId = 0x%04X, FctCode = 0x%04X, Cntrl = 0x%04X" MEI_DRV_CRLF,
         pCmvMsg->header.mbxCode, pCmvMsg->header.fctCode, pCmvMsg->header.paylCntrl) );
   PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
       ("      MsgId = 0x%04X, Index   = 0x%04X, Len   = 0x%04X" MEI_DRV_CRLF,
        P_CMV_MSGHDR_MSGID_GET(pCmvMsg), pCmvMsg->header.index, pCmvMsg->header.length) );

   PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
         ("DATA: " MEI_DRV_CRLF));
   paylSize = P_CMV_MSGHDR_PAYLOAD_SIZE_GET(pCmvMsg);
   paylSize = (paylSize > CMV_USED_PAYLOAD_16BIT_SIZE) ? CMV_USED_PAYLOAD_16BIT_SIZE : paylSize;

   if (P_CMV_MSGHDR_BIT_SIZE_GET(pCmvMsg) == (unsigned short)CMV_MSG_BIT_SIZE_32BIT)
   {
      paylSize = paylSize >> 1;

      for (i=0; i < paylSize; i++)
      {
         PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
            ("Param32[%02d]: 0x%08X " MEI_DRV_CRLF,
             i, pCmvMsg->payload.params_32Bit[i]));
      }
   }
   else
   {
      for (i=0; i < paylSize; i++)
      {
         PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
            ("Param16[%02d]: 0x%04X " MEI_DRV_CRLF,
             i, pCmvMsg->payload.params_16Bit[i]));
      }
   }

   PRN_DBG_INT_NL( MEI_DRV, dbgLevel,
       ("---------------------------------------------------" MEI_DRV_CRLF MEI_DRV_CRLF) );

   return;
}     /*  IFX_void_t MEI_LogCmvMsg(...) */


/**
   Trace a CMV Mailbox Message.

\param
   pMeiDev:    points to the current device structure.
\param
   pCmvMsg:    CMV message to log.
\param
   pDescr:     user given "info".

\return
   NONE

\attention
   For VxWorks: do not use on int level
*/
IFX_void_t MEI_TraceCmvMsg( MEI_DEV_T *pMeiDev,
                              CMV_STD_MESSAGE_T *pCmvMsg,
                              const char *pDescr, IFX_uint32_t dbgLevel)
{
   IFX_int32_t i;
   unsigned short paylSize;

   PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
         (MEI_DRV_CRLF "--- CMV Mailbox Msg: %s -- size 16 bit -----" MEI_DRV_CRLF, pDescr) );

   PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
         ("Hdr:  MbxId = 0x%04X, FctCode = 0x%04X, Cntrl = 0x%04X" MEI_DRV_CRLF,
           pCmvMsg->header.mbxCode, pCmvMsg->header.fctCode, pCmvMsg->header.paylCntrl) );
   PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
         ("      MsgId = 0x%04X, Index   = 0x%04X, Len   = 0x%04X" MEI_DRV_CRLF,
           P_CMV_MSGHDR_MSGID_GET(pCmvMsg), pCmvMsg->header.index, pCmvMsg->header.length) );

   PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
         ("DATA: " MEI_DRV_CRLF));
   paylSize = P_CMV_MSGHDR_PAYLOAD_SIZE_GET(pCmvMsg);
   paylSize = (paylSize > CMV_USED_PAYLOAD_16BIT_SIZE) ? CMV_USED_PAYLOAD_16BIT_SIZE : paylSize;

   if (P_CMV_MSGHDR_BIT_SIZE_GET(pCmvMsg) == (unsigned short)CMV_MSG_BIT_SIZE_32BIT)
   {
      paylSize = paylSize >> 1;

      for (i=0; i < paylSize; i++)
      {
         PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
               ("Param32[%02d]: 0x%08X " MEI_DRV_CRLF,
               i, pCmvMsg->payload.params_32Bit[i]));
      }
   }
   else
   {
      for (i=0; i < paylSize; i++)
      {
         PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
               ("Param16[%02d]: 0x%04X " MEI_DRV_CRLF,
               i, pCmvMsg->payload.params_16Bit[i]));
      }
   }

   PRN_DBG_USR_NL( MEI_DRV, dbgLevel,
         ("---------------------------------------------------" MEI_DRV_CRLF MEI_DRV_CRLF) );

   return;
}     /*  IFX_void_t MEI_TraceCmvMsg(...) */


/* ============================================================================
   Local Message Function definitions
   ========================================================================= */

/**
   Swap the 32 bit payload from host endianess to VRX endianes.

\param
   pMeiDev     points to the VRX device struct.
\param
   pMsg        points to the CMV message
\param
   msgSize     size [byte] of the message

\return
   IFX_SUCCESS in case of success
   IFX_ERROR   in case of error
*/
IFX_int32_t MEI_MsgPayl32Swap( MEI_DEV_T *pMeiDev,
                                 CMV_STD_MESSAGE_T *pMsg,
                                 IFX_int32_t msgSize)
{
   IFX_int32_t payloadCount_32Bit, i;

   /* size field contains number of 16 bit payload elements of the message */
   payloadCount_32Bit = P_CMV_MSGHDR_PAYLOAD_SIZE_GET(pMsg) >> 1;

   /* check against msg size */
   i = (msgSize - CMV_HEADER_8BIT_SIZE) / sizeof(IFX_uint32_t);
   if (i != payloadCount_32Bit)
   {
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d]: ERROR - SWAP size missmatch, "
             "msgid: 0x%X, size %d, payload/count %d / %d" MEI_DRV_CRLF,
            MEI_DRV_LINENUM_GET(pMeiDev), P_CMV_MSGHDR_MSGID_GET(pMsg),
            msgSize, i, payloadCount_32Bit));

      return IFX_ERROR;
   }

   for (i = 0; i < payloadCount_32Bit; i++)
   {
      pMsg->payload.params_32Bit[i] = SWAP32_DMA_WIDTH_ORDER(pMsg->payload.params_32Bit[i]);
   }

   return IFX_SUCCESS;
}     /*  MEI_STATIC IFX_int32_t MEI_MsgPayl32Swap(...) */


/**
   Read and set the mailbox address from the mailbox descriptor.

\param
   pMeiDev  points to the device data

\return
   IFX_SUCCESS in case of success
   IFX_ERROR   error occurred
               - DMA read not successful
               - descriptor still not set

\attention
   - Called on int-level
*/
MEI_STATIC IFX_int32_t MEI_SetDescrMailboxAddr(MEI_DEV_T *pMeiDev)
{
   IFX_uint16_t tmp[2];
   IFX_uint32_t tmpAddr, regVal = 0;

   if ( MEI_ReadDma16Bit( &pMeiDev->meiDrvCntrl, MEI_MAILBOX_DESCR_TABLE_ADDR,
                     tmp, 2 ) != 2)
   {
      /* error - can not read mailbox descriptor */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR - Set mailbox addr, read DMA" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      return IFX_ERROR;
   }

   if (tmp[0] == 0xFFFF)
   {
      /* error - mailbox descriptor still not set */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR - Set mailbox addr, invlid descr. 0x%04X" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), tmp[0]));

      return IFX_ERROR;
   }

   tmpAddr = MEI_MAILBOX_DESCR_TABLE_ADDR +
             (MEI_MB_DESCR_TABLE_NUM_OF_MB_GET(tmp[1]) * 4);

   MEI_ReadDma32Bit( &pMeiDev->meiDrvCntrl, (tmpAddr + 0x00), &regVal, 1);
   pMeiDev->modemData.mBoxDescr.addrArc2Me = regVal;

   MEI_ReadDma32Bit( &pMeiDev->meiDrvCntrl, (tmpAddr + 0x04), &regVal, 1);
   pMeiDev->modemData.mBoxDescr.lenArc2Me  = (regVal << 1);

   MEI_ReadDma32Bit( &pMeiDev->meiDrvCntrl, (tmpAddr + 0x08), &regVal, 1);
   pMeiDev->modemData.mBoxDescr.addrMe2Arc = regVal;

   MEI_ReadDma32Bit( &pMeiDev->meiDrvCntrl, (tmpAddr + 0x0C), &regVal, 1);
   pMeiDev->modemData.mBoxDescr.lenMe2Arc  = (regVal << 1);

   if ( (pMeiDev->modemData.mBoxDescr.lenArc2Me > sizeof(MEI_MEI_MAILBOX_T)) ||
        (pMeiDev->modemData.mBoxDescr.lenArc2Me > sizeof(MEI_MEI_MAILBOX_T)) )
   {
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
           ( "MEI_DRV[%02d]: Mailbox Size Missmatch -"
             "ARC2ME: 0x%X, ME2ARC: 0x%X, Drv: 0x%X" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev),
              pMeiDev->modemData.mBoxDescr.lenArc2Me, pMeiDev->modemData.mBoxDescr.lenMe2Arc,
              sizeof(MEI_MEI_MAILBOX_T) ));

   }

   PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
        ( "MEI_DRV[%02d]: Set online mailbox addr -"
          "ARC2ME: 0x%X, ME2ARC: 0x%X" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev),
           pMeiDev->modemData.mBoxDescr.addrArc2Me, pMeiDev->modemData.mBoxDescr.addrMe2Arc ));


   return IFX_SUCCESS;
}     /*  MEI_STATIC IFX_int32_t MEI_SetDescrMailboxAddr(...) */


/**
   Set mailbox addresses:
   - ROM code mailbox address (fixed)
   - Online Code from Mailbox descriptor

\param
   pMeiDev  points to the device data

\return
   IFX_SUCCESS: if the online mailbox address set successful
   IFX_ERROR:   cannot set the online mailbox address
                - invalid driver state
                - invalid bootmode

\attention
   - called on int-level
*/
MEI_STATIC IFX_int32_t MEI_SetMailboxAddress(MEI_DEV_T *pMeiDev)
{
   IFX_int32_t ret = IFX_ERROR;

   switch ( MEI_DRV_BOOTMODE_GET(pMeiDev) )
   {
      case e_MEI_DRV_BOOT_MODE_AUTO:
         /* request the mailbox code from mailbox descriptor */
         ret = MEI_SetDescrMailboxAddr(pMeiDev);
         break;

      case e_MEI_DRV_BOOT_MODE_7:
         /* request the mailbox address corresponding to the driver state */
         switch ( MEI_DRV_STATE_GET(pMeiDev) )
         {
            case e_MEI_DRV_STATE_SW_INIT_DONE:
            case e_MEI_DRV_STATE_BOOT_WAIT_ROM_ALIVE:
               pMeiDev->modemData.mBoxDescr.addrArc2Me = MEI_MAILBOX_BASE_ARC2ME;
               pMeiDev->modemData.mBoxDescr.lenArc2Me  = MEI_BOOT_MAILBOX_ARC2ME_LEN;
               pMeiDev->modemData.mBoxDescr.addrMe2Arc = MEI_MAILBOX_BASE_ME2ARC;
               pMeiDev->modemData.mBoxDescr.lenMe2Arc  = MEI_BOOT_MAILBOX_ME2ARC_LEN;
               ret = IFX_SUCCESS;
               break;

            case e_MEI_DRV_STATE_WAIT_FOR_FIRST_RESP:
               ret = MEI_SetDescrMailboxAddr(pMeiDev);
               break;

            default:
               PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                    ("MEI_DRV[%02d]: ERROR - Set mailbox addr (DL), invlid drv state (%d)" MEI_DRV_CRLF,
                      MEI_DRV_LINENUM_GET(pMeiDev), MEI_DRV_STATE_GET(pMeiDev)));

               break;
         }        /* switch ( MEI_DRV_STATE_GET(pMeiDev) ) */

         break;

      default:
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: ERROR - Set mailbox addr, invlid boot mode (%d)" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), MEI_DRV_BOOTMODE_GET(pMeiDev)));

         break;
   }        /* switch ( MEI_DRV_BOOTMODE_GET(pMeiDev) ) */

   return ret;
}     /*  MEI_STATIC IFX_int32_t MEI_SetMailboxAddress(...) */


/**
   Wait for an outstanding ACK message.
   - wait for ACK until timeout
   - clear the pending ACK in case of timeout.

\param
   pMeiDev  points to the current VRX device.
\param
   pDynCmd    points to the dynamic cmd data of the current instance.
\return
   IFX_SUCCESS if the ACK has been received, else
   IFX_ERROR
*/
MEI_STATIC IFX_int32_t MEI_MBox_WaitForAck(
                                    MEI_DEV_T          *pMeiDev,
                                    MEI_DYN_CMD_DATA_T *pDynCmd)
{
   if (pMeiDev->eModePoll == e_MEI_DEV_ACCESS_MODE_PASSIV_POLL)
   {
      /* poll for new message --> call ISR routine maually */
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
             ("MEI_DRV[%02d]: MEI_MBox_WaitForAck - check device" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      MEI_PollIntPerVrxLine(pMeiDev, e_MEI_DEV_ACCESS_MODE_PASSIV_POLL);


      /* setup timeout counter for ACK */
      MEI_SET_TIMEOUT_CNT( pMeiDev,
                     MEI_MaxWaitDfeResponce_ms / MEI_MIN_MAILBOX_POLL_TIME_MS);

      /* wait and then check for new messages */
      while( (MEI_GET_TIMEOUT_CNT(pMeiDev) > 0) &&
             (MEI_DRV_MAILBOX_STATE_GET(pMeiDev) != e_MEI_MB_FREE) )
      {
         MEI_DRVOS_EventWait_timeout(
                     &pMeiDev->eventMailboxRecv,
                     MEI_MIN_MAILBOX_POLL_TIME_MS);
         MEI_PollIntPerVrxLine(pMeiDev, e_MEI_DEV_ACCESS_MODE_PASSIV_POLL);
         MEI_DEC_TIMEOUT_CNT(pMeiDev);
      }
   }
   else
   {
      MEI_SET_DYN_TIMEOUT_CNT(pDynCmd,
                     MEI_MaxWaitDfeResponce_ms / MEI_MIN_MAILBOX_POLL_TIME_MS);

      while( (MEI_GET_DYN_TIMEOUT_CNT(pDynCmd) > 0) &&
             (MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) != e_MEI_MB_BUF_ACK_AVAIL) )
      {
         pMeiDev->bAckNeedWakeUp = IFX_TRUE;
         MEI_DRVOS_EventWait_timeout(
                     &pMeiDev->eventMailboxRecv,
                     MEI_MIN_MAILBOX_POLL_TIME_MS);
         pMeiDev->bAckNeedWakeUp = IFX_FALSE;
         MEI_DEC_DYN_TIMEOUT_CNT(pDynCmd);
      }
   }

   if (MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) == e_MEI_MB_BUF_ACK_AVAIL)
      return IFX_SUCCESS;
   else
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d]: MEI_MBox_WaitForAck - time out (cnt %d)" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pDynCmd->timeoutCount));

      return IFX_ERROR;
   }
}

#if (MEI_SUPPORT_DRV_LOOPS == 1)
/**
   Loop a mailbox message back to the sender instance.
*/
MEI_STATIC IFX_int32_t MEI_WriteMailboxLoopBack(
                                 MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                                 MEI_DYN_CMD_DATA_T *pDynCmd,
                                 MEI_MEI_MAILBOX_T  *pMBMsg,
                                 IFX_int32_t          mbMsgSize)
{
#if ((MEI_DEBUG_PRINT == 1) || (MEI_SUPPORT_STATISTICS == 1))
   MEI_DEV_T *pMeiDev = pMeiDynCntrl->pMeiDev;
#endif

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV[%02d - %02d] Loopback MBox msg[%d]" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, mbMsgSize));

   if ( mbMsgSize > (IFX_int32_t)pDynCmd->cmdAckCntrl.recvDataBuf_s.bufSize_byte )
   {
      /* data lost */
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
             ("MEI_DRV[%02d - %02d] Loopback MBox - data lost (size %d - max %d)" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
              mbMsgSize, pDynCmd->cmdAckCntrl.recvDataBuf_s.bufSize_byte));

      mbMsgSize = (IFX_int32_t)pDynCmd->cmdAckCntrl.recvDataBuf_s.bufSize_byte;
   }

   /*
      ToDo if required:
         - use a CMV msg loopback function
            --> change the direction within the message
   */
   memcpy(pDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer, pMBMsg, mbMsgSize);

   /* set status for this open instance */
   pDynCmd->cmdAckCntrl.msgLen = (IFX_uint32_t)mbMsgSize;
   pDynCmd->cmdAckCntrl.bufCtrl = MEI_RECV_BUF_CTRL_MODEM_ACK_MSG;
   pDynCmd->cmdAckCntrl.recvTime = MEI_DRVOS_GetElapsedTime_ms(0);
   MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_ACK_AVAIL);
   MEI_IF_STAT_INC_SEND_MSG_COUNT(pMeiDev);

   return mbMsgSize;
}
#endif


/**
   Get and distribute an incoming ACK mailbox message to the waiting instances.

\param
   pMeiDev: Points to the VRX device struct.

\return
   NONE - all statistics will be set within the device struct.

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_RecvAckMailboxMsg(MEI_DEV_T *pMeiDev)
{
   IFX_int32_t count;
   IFX_uint32_t recvTime;
   MEI_MEI_MAILBOX_T *pTmpMbMsg = NULL, *pTmpWrMbMsg = NULL;
   IFX_uint16_t prevAckMsgId = 0xFFFF;

   MEI_MEI_MAILBOX_T tmpMbMsg;

   if (pMeiDev->pCurrDynCmd)
   {
      pTmpMbMsg   = (MEI_MEI_MAILBOX_T *)pMeiDev->pCurrDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer;
      pTmpWrMbMsg = (MEI_MEI_MAILBOX_T *)pMeiDev->pCurrDynCmd->cmdWrBuf.pBuffer;
      prevAckMsgId = CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv);

      if (MEI_DRV_DYN_MBBUF_STATE_GET(pMeiDev->pCurrDynCmd) != e_MEI_MB_BUF_ACK_PENDING)
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
              ("MEI_DRV[%02d]: WARNING - no waiting ACK(0x%04X) dynMbState = %d" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev),
                CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv),
                MEI_DRV_DYN_MBBUF_STATE_GET(pMeiDev->pCurrDynCmd)));
      }
   }
#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
   else
   {
      pTmpMbMsg = ( (pMeiDev->gpaBuf.MessageID != 0) ? &tmpMbMsg : NULL );
   }
#endif

   recvTime = MEI_DRVOS_GetElapsedTime_ms(0);

   /* Expect normal message ACK or GPA ACK */
   if (pTmpMbMsg)
   {
      /* set previous read mailbox code */
      pTmpMbMsg->mbRaw.rawMsg[0] = MEI_MBOX_CODE_MSG_ACK;

      /* the function returns the complete message size */
      count = MEI_GetMailBoxMsg( &pMeiDev->meiDrvCntrl,
                                 pMeiDev->modemData.mBoxDescr.addrArc2Me,
                                 pTmpMbMsg,
                                 sizeof(MEI_MEI_MAILBOX_T)/sizeof(IFX_uint16_t),
                                 IFX_TRUE);
      if(count < 0)
      {
         /* Error */
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: Error - MEI_RecvAckMailboxMsg, count = %d [16bit]" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), count));

         MEI_IF_STAT_INC_RECV_MSG_ERR_COUNT(pMeiDev);

         if (pMeiDev->pCurrDynCmd)
         {
            pMeiDev->pCurrDynCmd->cmdAckCntrl.msgLen   = 0;
            pMeiDev->pCurrDynCmd->cmdAckCntrl.bufCtrl  = MEI_RECV_BUF_CTRL_MODEM_MSG_ERROR;
            pMeiDev->pCurrDynCmd->cmdAckCntrl.recvTime = recvTime;
            MEI_DRV_DYN_MBBUF_STATE_SET(pMeiDev->pCurrDynCmd, e_MEI_MB_BUF_ACK_AVAIL);
            pMeiDev->pCurrDynCmd = NULL;

            /* wakeup user */
            goto MEI_RECV_ACK_MB_MSG_WAKEUP;
         }
         else
         {
            return;
         }
      }
#if (MEI_SUPPORT_TIME_TRACE == 1)
      else
      {
         if (pMeiDev->pCurrDynCmd)
         {
            MEI_GET_TICK_MS_TIME_TRACE(pMeiDev->pCurrDynCmd->ackWaitEnd_ms);
         }
#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
         else
         {
            MEI_GET_TICK_MS_TIME_TRACE(pMeiDev->gpaBuf.ackWaitEnd_ms);
         }
#endif
      }
#endif   /* #if (MEI_SUPPORT_TIME_TRACE == 1) */
   }
   else
   {
      /* set previous read mailbox code */
      tmpMbMsg.mbRaw.rawMsg[0] = MEI_MBOX_CODE_MSG_ACK;

      /* the function returns the complete message size */
      count = MEI_GetMailBoxMsg( &pMeiDev->meiDrvCntrl,
                                 pMeiDev->modemData.mBoxDescr.addrArc2Me,
                                 &tmpMbMsg,
                                 sizeof(MEI_MEI_MAILBOX_T)/sizeof(IFX_uint16_t),
                                 IFX_TRUE);
      if(count < 0)
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: MEI_RecvAckMailboxMsg - "\
               "DISCARD ACK (error read: size %d, time = %d)" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), count, recvTime));
      }
      else
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: MEI_RecvAckMailboxMsg - "\
               "DISCARD ACK(ID: 0x%04X Job: %d / curr %d, time = %d)" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev),
                CMV_MSGHDR_MSGID_GET(tmpMbMsg.mbCmv.cmv),
                CMV_MSGHDR_MSGIDX_GET(tmpMbMsg.mbCmv.cmv), MEI_GET_MSG_INDEX(pMeiDev),
                recvTime ));
      }

      /* no ack pending --> discard the msg */
      MEI_IF_STAT_INC_RECV_MSG_DISCARD_COUNT(pMeiDev);

      return;
   }


   /* check recived ACK against previous command */
   if (pTmpWrMbMsg)
   {
      if ( ( (CMV_MSGHDR_MSGID_GET(pTmpWrMbMsg->mbCmv.cmv))^(CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv)) ) &
            ~(MEI_MSGID_BIT_IND_WR_CMD) )
      {
         IFX_uint16_t tmpBuf[8];

         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
              ("MEI_DRV[%02d]: WARNING, MsgId ACK mismatch (t=%d) -  "\
               "CMD[0x%04X] / recvACK[0x%04X] / prevAck[0x%04X]" MEI_DRV_CRLF,
               MEI_DRV_LINENUM_GET(pMeiDev), recvTime,
               CMV_MSGHDR_MSGID_GET(pTmpWrMbMsg->mbCmv.cmv),
               CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv),
               prevAckMsgId ));

         /* dump the current mailbox */
         if ( MEI_ReadDma16Bit( &pMeiDev->meiDrvCntrl, pMeiDev->modemData.mBoxDescr.addrArc2Me,
                           tmpBuf, 8 ) != 8)
         {
            /* error - can not read mailbox descriptor */
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                 ("MEI_DRV[%02d]: ERROR - Set mailbox addr, read DMA" MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev)));
         }
         else
         {
            PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
                 ("MEI_DRV[%02d]: ARC2ME MB, 0..3 - 0x%04X, 0x%04X, 0x%04X, 0x%04X" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev),
                 tmpBuf[0], tmpBuf[1], tmpBuf[2], tmpBuf[3] ));
            PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
                 ("MEI_DRV[%02d]: ARC2ME MB, 4..7 - 0x%04X, 0x%04X, 0x%04X, 0x%04X" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev),
                 tmpBuf[4], tmpBuf[5], tmpBuf[6], tmpBuf[7] ));
         }
      }

      if ( CMV_MSGHDR_MSGIDX_GET(pTmpWrMbMsg->mbCmv.cmv) != CMV_MSGHDR_MSGIDX_GET(pTmpMbMsg->mbCmv.cmv) )
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
              ("MEI_DRV[%02d]: WARNING, MsgId JobId mismatch -  "\
               "CMD(0x%04X Job %d) / ACK(0x%04X Job %d, time = %d)" MEI_DRV_CRLF,
               MEI_DRV_LINENUM_GET(pMeiDev),
               CMV_MSGHDR_MSGID_GET(pTmpWrMbMsg->mbCmv.cmv),
               CMV_MSGHDR_MSGIDX_GET(pTmpWrMbMsg->mbCmv.cmv),
               CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv),
               CMV_MSGHDR_MSGIDX_GET(pTmpMbMsg->mbCmv.cmv),
               recvTime ));
      }
   }        /* if (pTmpWrMbMsg) {...} */

   /*
      Msg ACK received - update to IFX msg ID format.
   */
   if ( !( CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv) & MEI_MSGID_BIT_IND_IFX) &&
         (CMV_MSGHDR_FCT_OPCODE_GET(pTmpMbMsg->mbCmv.cmv) ==  D2H_CMV_WRITE_REPLY) )
   {
      /* CMV message - write replay */
      pTmpMbMsg->mbCmv.cmv.header.MessageID |= MEI_MSGID_BIT_IND_WR_CMD;
   }

   PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
        ("MEI_DRV[%02d]: MEI_RecvAckMailboxMsg[0x%04X] - "\
         "count = %d [16bit], time = %d" MEI_DRV_CRLF,
         MEI_DRV_LINENUM_GET(pMeiDev), CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv), count, recvTime));

   MEI_DRV_MAILBOX_STATE_SET(pMeiDev, e_MEI_MB_FREE);

   /* swap 32bit payload */
   if ( CMV_MSGHDR_BIT_SIZE_GET(pTmpMbMsg->mbCmv.cmv) == (unsigned short)CMV_MSG_BIT_SIZE_32BIT )
   {
      if ( MEI_MsgPayl32Swap( pMeiDev,
                              &pTmpMbMsg->mbCmv.cmv,
                              MEI_PARAM_COUNT_16_TO_8(count) ) != IFX_SUCCESS )
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: ERROR - read MBox ACK msg, SWAP" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev)));

         /* \TODO - set error code in this case ?? */

         MEI_IF_STAT_INC_RECV_MSG_ERR_COUNT(pMeiDev);
      }
   }

   /* update the modem state if the FSM state message has been received */
   if (    (CMV_MSGHDR_MSGID_GET(pTmpMbMsg->mbCmv.cmv) ==  D2H_CMV_MSGID_MODEM_FSM_STATE)
        && (pTmpMbMsg->mbCmv.cmv.header.index == 0) )
   {
      MEI_DRV_MODEM_STATE_SET(pMeiDev, (IFX_uint32_t)pTmpMbMsg->mbCmv.cmv.payload.params_16Bit[0]);
   }

#if (MEI_SUPPORT_TIME_TRACE == 1)
   {
      IFX_uint32_t tick_ms;

      if (pMeiDev->pCurrDynCmd)
      {
         tick_ms = MEI_TIME_TRACE_GET_TICK_MS(pMeiDev->pCurrDynCmd->ackWaitStart_ms,
                                                pMeiDev->pCurrDynCmd->ackWaitEnd_ms);
         MEI_TIME_TRACE_CHECK_WAIT_ACK_MIN_MAX( pMeiDev->timeStat,
                                                  tick_ms,
                                                  MEI_MaxWaitDfeResponce_ms );
      }
#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
      else
      {
         tick_ms = MEI_TIME_TRACE_GET_TICK_MS(pMeiDev->gpaBuf.ackWaitStart_ms,
                                                pMeiDev->gpaBuf.ackWaitEnd_ms);
         MEI_TIME_TRACE_CHECK_WAIT_ACK_MIN_MAX( pMeiDev->timeStat,
                                                  tick_ms,
                                                  MEI_MaxWaitDfeResponce_ms );
      }
#endif
   }
#endif

   MEI_LOG_CMV_MSG( pMeiDev, &pTmpMbMsg->mbCmv.cmv, "ACK msg", MEI_DRV_PRN_LEVEL_LOW);
   MEI_DBG_MSG_LOG_DUMP(pMeiDev, &pTmpMbMsg->mbCmv.cmv);

   /*
      received Msg ACK, check for:
      - CMD - ACK handling
      - GPA handling
   */
   if (pMeiDev->pCurrDynCmd)
   {
      /* CMD - ACK handling */
      pMeiDev->pCurrDynCmd->cmdAckCntrl.msgLen   = MEI_PARAM_COUNT_16_TO_8(count);
      pMeiDev->pCurrDynCmd->cmdAckCntrl.bufCtrl  = MEI_RECV_BUF_CTRL_MODEM_ACK_MSG;
      pMeiDev->pCurrDynCmd->cmdAckCntrl.recvTime = recvTime;
      MEI_DRV_DYN_MBBUF_STATE_SET(pMeiDev->pCurrDynCmd, e_MEI_MB_BUF_ACK_AVAIL);
      pMeiDev->pCurrDynCmd = NULL;
   }
#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
   else
   {
      /* GPA handling */
      MEI_OnlineOnGpaAckRecv(pMeiDev, &pTmpMbMsg->mbCmv.cmv);
   }
#endif

   MEI_IF_STAT_INC_RECV_ACK_COUNT(pMeiDev);


MEI_RECV_ACK_MB_MSG_WAKEUP:
   /*
      inform the user
   */
   if (pMeiDev->bAckNeedWakeUp)
   {
      pMeiDev->bAckNeedWakeUp = IFX_FALSE;
      MEI_DRVOS_EventWakeUp(&pMeiDev->eventMailboxRecv);
   }


   return;
}

#if (MEI_SUPPORT_DSM == 1)
/**
   Receive the DSM autonomous message (EVT)

\param
   pMeiDev: Points to the VRX device struct.
\param
   pMsg:    Points to the received "EVT_DSM" message.

\return
   none

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_Recv_MODEM_EVT_DSM( MEI_DEV_T *pMeiDev,
                                              CMV_STD_MESSAGE_T *pMsg)
{

#if MEI_SUPPORT_DEVICE_VR11 != 1
   EVT_DSM_ErrorVectorReady_t dsmErbParams;
   if (pMsg == NULL)
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MEI_Recv_MODEM_EVT_DSM - null ptr" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      return;
   }

   /* interprete message - (Read Replay || indication) && (msg index == 0) */
   if (   (   (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_READ_REPLY)
           || (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_INDICATE)  )
       && (pMsg->header.index == 0)
      )
   {
      memset(&dsmErbParams, 0x00, sizeof(EVT_DSM_ErrorVectorReady_t));
      dsmErbParams.ErrVecProcResult = pMsg->payload.params_16Bit[0];
      dsmErbParams.ErrVecSize = pMsg->payload.params_16Bit[1];

      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL,
           ("MEI_DRV[%02d]: DSM EVT ErrorVectorReady - ErrVecProcResult = %i, "
            "ErrVecSize = %i" MEI_DRV_CRLF, MEI_DRV_LINENUM_GET(pMeiDev),
             dsmErbParams.ErrVecProcResult, dsmErbParams.ErrVecSize));

      MEI_VRX_DSM_EvtErbHandler(pMeiDev, &dsmErbParams);

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "DSM EVT ErrorVectorReady", MEI_DRV_PRN_LEVEL_LOW);
   }
   else
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR DSM EVT ErrorVectorReady - "
            "func opcode = 0x%02X != 0x%02X/0x%02X, msgId=0x%04X != 0x%04X" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev),
             P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg), D2H_CMV_READ_REPLY, D2H_CMV_INDICATE,
             P_CMV_MSGHDR_MSGID_GET(pMsg), EVT_DSM_ERRORVECTORREADY));

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "ERR DSM EVT ErrorVectorReady", MEI_DRV_PRN_LEVEL_HIGH);
   }
#else
   static IFX_int32_t counter = 0;
   counter++;
   if (counter % 1000)
   {
      /* error receive MODEM_EVT_DSM on VR11 */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MEI_Recv_MODEM_EVT_DSM received" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

   }
#endif

   return;
}
#endif /* (MEI_SUPPORT_DSM == 1) */

/**
   Receive the MODEM FSM STATE message

\param
   pMeiDev: Points to the VRX device struct.
\param
   pMsg:    Points to the received "FSM STATE" message.

\return
   none

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_Recv_MODEM_FSM_STATE( MEI_DEV_T *pMeiDev,
                                              CMV_STD_MESSAGE_T *pMsg)
{
   if (pMsg == NULL)
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MEI_Recv_MODEM_FSM_STATE - null ptr" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      return;
   }

   /* interprete message - (Read Replay || indication) && (msg index == 0) */
   if (   (   (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_READ_REPLY)
           || (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_INDICATE)  )
       && (pMsg->header.index == 0)
      )
   {
      if (MEI_DRV_STATE_GET(pMeiDev) != e_MEI_DRV_STATE_DFE_READY)
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: WARNING MEI_Recv_MODEM_FSM_STATE - no READY_STATE (%d)" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), MEI_DRV_STATE_GET(pMeiDev) ));

         MEI_DRV_STATE_SET(pMeiDev, e_MEI_DRV_STATE_DFE_READY);
      }

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "FSM State", MEI_DRV_PRN_LEVEL_LOW);
      MEI_DRV_MODEM_STATE_SET(pMeiDev, (IFX_uint32_t)pMsg->payload.params_16Bit[0]);
   }
   else
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MODEM_FSM_STATE - "
            "func opcode = 0x%02X != 0x%02X/0x%02X, msgId=0x%04X != 0x%04X" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev),
             P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg), D2H_CMV_READ_REPLY, D2H_CMV_INDICATE,
             P_CMV_MSGHDR_MSGID_GET(pMsg), D2H_CMV_MSGID_MODEM_FSM_STATE));

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "ERR FSM STATE", MEI_DRV_PRN_LEVEL_HIGH);
   }

   return;
}

/**
   Receive the EVT_TC_StatusGet autonomous message

\param
   pMeiDev: Points to the VRX device struct.
\param
   pMsg:    Points to the received EVT_TC_StatusGet message.

\return
   none

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_Recv_MODEM_EVT_TC(
                              MEI_DEV_T *pMeiDev,
                              CMV_STD_MESSAGE_T *pMsg)
{
   IFX_char_t *pFwTcUsedStr;
   IFX_uint16_t nFwTcUsed;
   IFX_uint32_t nApiTcUsed = 0;

   if (pMsg == NULL)
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MEI_Recv_MODEM_EVT_TC - null ptr" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      return;
   }

   /* interprete message - (Read Replay || indication) && (msg index == 0) */
   if (   (   (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_READ_REPLY)
           || (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) == D2H_CMV_INDICATE)  )
       && (pMsg->header.index == 0)
      )
   {
      nFwTcUsed =  pMsg->payload.params_16Bit[0];

      switch (nFwTcUsed)
      {
         case EVT_TC_StatusGet_EFM_TC:
            nApiTcUsed = DSL_TC_EFM;
            pFwTcUsedStr = "PTM";
            break;

         case EVT_TC_StatusGet_ATM_TC:
            nApiTcUsed = DSL_TC_ATM;
            pFwTcUsedStr = "ATM";
            break;

         default:
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                 ("MEI_DRV[%02d]: ERROR - EVT_TC_StatusGet_t - Invalid TC indicated (%d)"
                  MEI_DRV_CRLF, MEI_DRV_LINENUM_GET(pMeiDev), nFwTcUsed));
            return;
      }

      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH,
           ("MEI_DRV[%02d]: EVT_TC_StatusGet_t - TC indicated is %d (%s)"
            MEI_DRV_CRLF, MEI_DRV_LINENUM_GET(pMeiDev), nFwTcUsed, pFwTcUsedStr));

      MEI_DRV_GET_UNIQUE_CALLBACK_ACCESS(pMeiDev);

      /** request separate thread for TC Layer Request callback handling */
      pMeiDev->bHandleCallback = IFX_TRUE;

      pMeiDev->callbackType = MEI_CALLBACK_TYPE_TC_LAYER_REQUEST;

      pMeiDev->callbackData.tcLayerRequest.func = MEI_TcRequest;
      pMeiDev->callbackData.tcLayerRequest.pMeiDev = pMeiDev;
      pMeiDev->callbackData.tcLayerRequest.line = (IFX_uint8_t)(MEI_DRV_LINENUM_GET(pMeiDev));
      pMeiDev->callbackData.tcLayerRequest.nTcLayer = nApiTcUsed;

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "EVT_TC_StatusGet_t",
                       MEI_DRV_PRN_LEVEL_LOW);
   }
   else
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
         ("MEI_DRV[%02d]: ERROR EVT_TC_StatusGet_t - "
          "func opcode = 0x%02X != 0x%02X/0x%02X, msgId=0x%04X != 0x%04X"
         MEI_DRV_CRLF, MEI_DRV_LINENUM_GET(pMeiDev),
         P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg), D2H_CMV_READ_REPLY, D2H_CMV_INDICATE,
         P_CMV_MSGHDR_MSGID_GET(pMsg), D2H_CMV_MSGID_MODEM_TC_STATUS));

      MEI_LOG_CMV_MSG( pMeiDev, pMsg, "ERR EVT_TC_StatusGet_t",
                       MEI_DRV_PRN_LEVEL_HIGH);
   }

   return;
}

/**
   Receive the AUTONOMOUS MODEM READ message

\param
   pMeiDev: Points to the VRX device struct.
\param
   pMsg:    Points to the received "Modem Ready" message.

\return
   none

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_Recv_AUTO_MODEM_READY( MEI_DEV_T *pMeiDev,
                                               CMV_MESSAGE_MODEM_RDY_T *pMsg)
{
   if (pMsg == NULL)
   {
      /* error receive mailbox message while init state */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR MEI_Recv_AUTO_MODEM_READY - null ptr" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));

      return;
   }

   /* interprete message */
   if (  (   (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) != D2H_CMV_READ_REPLY)
          && (P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg) != D2H_CMV_INDICATE) )
      )
   {
      /* error receive mailbox message while init state */
#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR WAIT FOR MODEM_READY - GP1 Stat = %d "
            "func opcode = 0x%02X != 0x%02X/0x%02X, msgId=0x%04X != 0x%04X" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), pMeiDev->statistics.dfeGp1IntCount,
             P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg), D2H_CMV_READ_REPLY, D2H_CMV_INDICATE,
             P_CMV_MSGHDR_MSGID_GET(pMsg), D2H_CMV_MSGID_MODEM_READY));

#else
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR WAIT FOR MODEM_READY - "
            "func opcode = 0x%02X != 0x%02X/0x%02X, msgId=0x%04X != 0x%04X" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev),
             P_CMV_MSGHDR_FCT_OPCODE_GET(pMsg), D2H_CMV_READ_REPLY, D2H_CMV_INDICATE,
             P_CMV_MSGHDR_MSGID_GET(pMsg), D2H_CMV_MSGID_MODEM_READY));

#endif

      MEI_LOG_CMV_MSG( pMeiDev, (CMV_STD_MESSAGE_T *)pMsg,
                             "NO MODEM RDY", MEI_DRV_PRN_LEVEL_HIGH);
   }
   else
   {
#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
           ("MEI_DRV[%02d]: +++ Modem Ready - GP1 stat = %d +++\n\r",
            MEI_DRV_LINENUM_GET(pMeiDev), pMeiDev->statistics.dfeGp1IntCount));
#endif

      /* modem ready message received --> new state */
      if (P_CMV_MSGHDR_PAYLOAD_SIZE_GET(pMsg))
      {
         if (pMsg->modemRdyParams.params_16Bit[0] != RESULT_AUTO_MODEM_READY_SUCCESS)
         {
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                 ("MEI_DRV[%02d]: ERROR MODEM_READY - received with error 0x%04X" MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev), pMsg->modemRdyParams.params_16Bit[0]));
         }
         else
         {
            MEI_DRV_STATE_SET(pMeiDev, e_MEI_DRV_STATE_DFE_READY);
         }
      }
      else
      {
         MEI_DRV_STATE_SET(pMeiDev, e_MEI_DRV_STATE_DFE_READY);
      }

      MEI_LOG_CMV_MSG( pMeiDev, (CMV_STD_MESSAGE_T *)pMsg,
                             "MODEM RDY", MEI_DRV_PRN_LEVEL_NORMAL);

#if MEI_SUPPORT_DEVICE_VR11 == 1
      MEI_CGU_PPLOMCFG_print(&(pMeiDev->meiDrvCntrl));
#endif /* MEI_SUPPORT_DEVICE_VR11 */
   }

   return;
}

/**
   Set internal callback data block for ethernet extract.
*/
IFX_int32_t MEI_NfcCallBackSet(
                              MEI_DYN_CNTRL_T *pMeiDynCntrl,
                              MEI_InternalMsgRecvCallBack pCallBackFunc,
                              IFX_void_t                    *pNfcCallBackData)
{
   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_NFC_DATA_T *pDynNfc   = pMeiDynCntrl->pInstDynNfc;

   if (pDynNfc)
   {
      /* set call back */
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
            ("MEI_DRV[%02d-%02d]: %s Internal CallBack" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
              (pNfcCallBackData) ? "Set" : "Unset"));

      pDynNfc->pCallBackFunc = pCallBackFunc;
      pDynNfc->pCallBackData = pNfcCallBackData;

      if (!pDynNfc->pCallBackFunc)
         pDynNfc->pCallBackData = IFX_NULL;

      return IFX_SUCCESS;
   }

   PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
         ("MEI_DRV[%02d-%02d]: Error %s Internal CallBack - missing init" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
           (pNfcCallBackData) ? "Set" : "Unset"));

   return IFX_ERROR;
}

/**
   Distribute a message over all open instances.

\param
   pMeiDev: Points to the VRX device struct.
\param
   pNfcRootInstance: points to the root of the open instance list.
\param
   pMsg:    Points to the "Driver" message.
\param
   msgSize: size of the "Driver" message.
\param:
   msgType: type of the driver message.

\return
   Number of distributed messages.
*/
IFX_uint32_t MEI_DistributeAutoMsg(
                              MEI_DEV_T          *pMeiDev,
                              MEI_DYN_NFC_DATA_T *pNfcRootInstance,
                              IFX_uint8_t          *pMsg,
                              IFX_int32_t          msgSize,
                              IFX_uint32_t         msgType )
{
   IFX_uint8_t          rdIdxWr_next;
   IFX_uint32_t         distCount = 0, discardCount = 0, recvTime, processCtrl;
   MEI_DYN_NFC_DATA_T *pNfcInstance_next;

   recvTime = MEI_DRVOS_GetElapsedTime_ms(0);

   switch (msgType)
   {
      case MEI_RECV_BUF_CTRL_MODEM_NFC_MSG:
         /* modem message --> interprete mailbox code */
         processCtrl = (((IFX_uint32_t)(((MEI_CMV_MAILBOX_T *)pMsg)->cmv.header.mbxCode)) & 0x0000FFFF);
         break;

#if (MEI_SUPPORT_DRIVER_MSG == 1)
      case MEI_RECV_BUF_CTRL_DRIVER_MSG:
         /* driver message --> interprete ID */
         processCtrl = ((MEI_DRV_MSG_TYPE_GET(((IFX_uint32_t)((MEI_DRV_MSG_Header_t *)pMsg)->id))) << 16) |
                       ((MEI_DRV_MSG_MODULE_GET(((IFX_uint32_t)((MEI_DRV_MSG_Header_t *)pMsg)->id))) << 24);
         break;
#endif

#if (MEI_DRV_ATM_OAM_ENABLE == 1)
      case MEI_RECV_BUF_CTRL_MODEM_ATMOAM_CELL:
         /* ATM OAM cells (from a modem msg) --> set special code */
         processCtrl = (((IFX_uint32_t)((MEI_ATMOAM_CELL_BUFFER_T *)pMsg)->atmOamId) & 0x0000FFFF);
         break;
#endif

#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
      case MEI_RECV_BUF_CTRL_MODEM_EOC_FRAME:
         /* Clear EOC Frame (from modem msg's) --> set special code */
         processCtrl = (((IFX_uint32_t)((MEI_CEOC_MEI_EOC_FRAME_T *)pMsg)->cEocId) & 0x0000FFFF);
         break;
#endif

      default:
         /* unsupported message --> discard */
         MEI_IF_STAT_INC_RECV_NFC_UNKNOWN_DISCARD_COUNT(pMeiDev);

         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: unknown Msg Type <0x%X> - discard" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), msgType));
         return 0;
   }

   pNfcInstance_next = pNfcRootInstance;
   while(pNfcInstance_next)
   {
      /* check if this autonomous message is enabled */
      if (pNfcInstance_next->msgProcessCtrl & processCtrl)
      {
         rdIdxWr_next = pNfcInstance_next->rdIdxWr;

         if (pNfcInstance_next->pRecvDataCntrl[rdIdxWr_next].bufCtrl == MEI_RECV_BUF_CTRL_FREE)
         {
            /* next instance free buffer found */
            memcpy( pNfcInstance_next->pRecvDataCntrl[rdIdxWr_next].recvDataBuf_s.pBuffer, pMsg, msgSize);
            pNfcInstance_next->pRecvDataCntrl[rdIdxWr_next].msgLen   = msgSize;
            pNfcInstance_next->pRecvDataCntrl[rdIdxWr_next].bufCtrl  = msgType;
            pNfcInstance_next->pRecvDataCntrl[rdIdxWr_next].recvTime = recvTime;

            /* increase write index of buffer */
            pNfcInstance_next->rdIdxWr =
               (rdIdxWr_next < (pNfcInstance_next->numOfBuf -1)) ? (rdIdxWr_next+1) : 0;

            #if (MEI_EXPORT_INTERNAL_API == 1)
            if ( (pNfcInstance_next->pCallBackFunc) &&
                 (pNfcInstance_next->pCallBackData) )
            {
               pNfcInstance_next->pCallBackFunc(pNfcInstance_next->pCallBackData);
            }
            #endif

            distCount++;
         }
         else
         {
            discardCount++;
         }
      }

      /* get next */
      pNfcInstance_next = pNfcInstance_next->pNext;
   }

   /* do statistics */
   switch (msgType)
   {
      case MEI_RECV_BUF_CTRL_MODEM_NFC_MSG:
         if (distCount <= 0)
         {
            /* no waiting user found - discard without any distribution */
            MEI_IF_STAT_INC_RECV_NFC_DISCARD_COUNT(pMeiDev);
         }
         else
         {
            /* waiting user found - number of distributions */
            MEI_IF_STAT_ADD_RECV_NFC_DIST_COUNT(pMeiDev, distCount);
         }

         if (discardCount)
         {
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
                 ("MEI_DRV[%02d]: WARNING - discard ModemMsg for %d instances" MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev), discardCount));

            MEI_IF_STAT_ADD_RECV_NFC_DIST_DISCARD_COUNT(pMeiDev, discardCount);
         }
         break;

#if (MEI_SUPPORT_DRIVER_MSG == 1)
      case MEI_RECV_BUF_CTRL_DRIVER_MSG:
         PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
              ("MEI_DRV[%02d]: DriverMsg - distributed = %d, discarded = %d" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), distCount, discardCount));
         break;
#endif

#if (MEI_DRV_ATM_OAM_ENABLE == 1)
      case MEI_RECV_BUF_CTRL_MODEM_ATMOAM_CELL:
         break;
#endif

#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
      case MEI_RECV_BUF_CTRL_MODEM_EOC_FRAME:
         break;
#endif

      default:
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
              ("MEI_DRV[%02d]: unknown Msg <0x%X> - distributed = %d, discarded = %d" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), msgType, distCount, discardCount));

         break;
   }

   if (distCount > 0)
   {
      /* inform sleeping processes */
      if (pMeiDev->bNfcNeedWakeUp)
      {
         MEI_DRVOS_SelectQueueWakeUp(
               &pMeiDev->selNfcWakeupList,
               MEI_DRVOS_SEL_WAKEUP_TYPE_RD);

         pMeiDev->bNfcNeedWakeUp = IFX_FALSE;

         PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
              ("MEI_DRV[%02d]: WakeUp processes - <msgType 0x%08X>" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), msgType));
      }
      else
      {
         PRN_DBG_INT( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
                ("MEI_DRV[%02d]: skip WakeUp processes - <msgType 0x%08X>" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), msgType));
      }
   }

   return distCount;
}


/**
   Get and distribute an incoming mailbox message to all available instances.

\param
   pMeiDev: Points to the MEI device struct.

\return
   NONE - all statistics will be set within the device struct.

\attention
   - Called on int-level
*/
MEI_STATIC IFX_void_t MEI_RecvAutonomMailboxMsg(
                              MEI_DEV_T *pMeiDev,
                              IFX_uint16_t mboxCode)
{
   IFX_int32_t    count, distCount = 0;
   IFX_uint32_t   msgType = (IFX_uint32_t)MEI_RECV_BUF_CTRL_MODEM_NFC_MSG;
   MEI_MEI_MAILBOX_T  tempMsg;

   /* set the previous read mailbox code */
   tempMsg.mbCmv.cmv.header.mbxCode = mboxCode;

   /* read out the message to temporary buffer */
   count = MEI_GetMailBoxMsg(   &pMeiDev->meiDrvCntrl
                              , pMeiDev->modemData.mBoxDescr.addrArc2Me
                              , &tempMsg
                              , sizeof(MEI_MEI_MAILBOX_T)/sizeof(IFX_uint16_t)
                              , IFX_TRUE);
   if (count <= 0)
   {
      /* ERROR read event msg from mailbox */
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
           ("MEI_DRV[%02d]: ERROR - read MBox Auton msg (count = %d)" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), count));

      MEI_IF_STAT_INC_RECV_MSG_ERR_COUNT(pMeiDev);
      return;
   }

   /* swap 32bit payload: 16 bit msg size --> byte msg size */
   count = MEI_PARAM_COUNT_16_TO_8(count);
   if (CMV_MSGHDR_BIT_SIZE_GET(tempMsg.mbCmv.cmv) == (unsigned short)CMV_MSG_BIT_SIZE_32BIT)
   {
      if ( MEI_MsgPayl32Swap( pMeiDev, &tempMsg.mbCmv.cmv, count ) != IFX_SUCCESS )
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ("MEI_DRV[%02d]: ERROR - read MBox Auton msg, SWAP" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev)));

         MEI_IF_STAT_INC_RECV_MSG_ERR_COUNT(pMeiDev);
         return;
      }
   }

   MEI_IF_STAT_INC_RECV_NFC_COUNT(pMeiDev);
   MEI_DBG_MSG_LOG_DUMP(pMeiDev, &tempMsg.mbCmv.cmv);

   /* check for special notifications */
   switch (CMV_MSGHDR_MSGID_GET(tempMsg.mbCmv.cmv))
   {
#if (MEI_DRV_ATM_OAM_ENABLE == 1)
      case MEI_DRV_EVT_ATMCELLLINEEXTRACT:
      case MEI_DRV_ALM_ATMCELLEXTRACTFAILED:
      case MEI_DRV_EVT_ATMCELLLINEINSERTSTATUSGET:
         /* check if processing required */
         if (pMeiDev->pAtmOamDevCntrl)
         {
            if (MEI_ATMOAM_CheckForWork( pMeiDev,
                                           pMeiDev->pAtmOamDevCntrl,
                                           CMV_MSGHDR_MSGID_GET(tempMsg.mbCmv.cmv)))
            {
               MEI_ATMOAM_AutoMsgHandler( pMeiDev,
                                            CMV_MSGHDR_MSGID_GET(tempMsg.mbCmv.cmv),
                                            (CMV_STD_MESSAGE_T *)&tempMsg);

               /* ATM OAM specific work done */
               return;
            }
         }
         break;
#endif /* (MEI_DRV_ATM_OAM_ENABLE == 1)*/


#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
      case MEI_DRV_EVT_CLEAREOC_READ:
      case MEI_DRV_EVT_CLEAREOCSTATUSGET:
         /* check if processing required */
         if (pMeiDev->pCEocDevCntrl)
         {
            if (MEI_CEOC_CheckForWork( pMeiDev,
                                         pMeiDev->pCEocDevCntrl,
                                         CMV_MSGHDR_MSGID_GET(tempMsg.mbCmv.cmv)))
            {
               MEI_CEOC_AutoMsgHandler(
                     pMeiDev, (CMV_STD_MESSAGE_T *)&tempMsg);

               /* ATM OAM specific work done */
               return;
            }
         }
         break;
#endif /* (MEI_DRV_CLEAR_EOC_ENABLE == 1)*/

#if (MEI_SUPPORT_DSM == 1)
      /* new downstream DSM ERB was written by the DSL FW into the SDRAM */
      case EVT_DSM_ERRORVECTORREADY:
         MEI_Recv_MODEM_EVT_DSM(pMeiDev, (CMV_STD_MESSAGE_T *)&tempMsg);
         break;
#endif /* (MEI_SUPPORT_DSM == 1) */

#if (MEI_SUPPORT_DEBUG_STREAMS == 1)
      case EVT_DBG_DEBUG_STREAM:
         MEI_DBG_STREAM_EventRecv(pMeiDev, pMeiDev->pRootDbgStrmRecvFirst, (CMV_STD_MESSAGE_T *)&tempMsg);
         return;
#endif

      case D2H_CMV_MSGID_MODEM_READY:
         /* "Modem Ready": signals the new driver state "Modem READY" */
         MEI_Recv_AUTO_MODEM_READY(pMeiDev, (CMV_MESSAGE_MODEM_RDY_T *)&tempMsg);
         break;
      case D2H_CMV_MSGID_MODEM_FSM_STATE:
         /* "ModemFSM" state */
         MEI_Recv_MODEM_FSM_STATE(pMeiDev, (CMV_STD_MESSAGE_T *)&tempMsg);
         break;

      case D2H_CMV_MSGID_MODEM_TC_STATUS:
         /* TC layer status */
         MEI_Recv_MODEM_EVT_TC(pMeiDev, (CMV_STD_MESSAGE_T *)&tempMsg);
         break;

      default:
         break;
   }

   /* ==================================================
      Distribute the message
      ================================================== */
#if (MEI_SUPPORT_DSM == 1)
   if (CMV_MSGHDR_MSGID_GET(tempMsg.mbCmv.cmv) == EVT_DSM_ERRORVECTORREADY)
   {
      /* do not distribute DSM messages */
      return;
   }
#endif /* (MEI_SUPPORT_DSM == 1) */

   distCount = MEI_DistributeAutoMsg(
                              pMeiDev,
                              pMeiDev->pRootNfcRecvFirst,
                              (IFX_uint8_t *)&tempMsg, count,
                              msgType);

   if (distCount <= 0)
   {
#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
           ("MEI_DRV[%02d]: WARNING - no waiting user, "\
            "discard modem auto. msg (GP1 Stat = %d)!" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), pMeiDev->statistics.dfeGp1IntCount));
#else
      PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
           ("MEI_DRV[%02d]: WARNING - no waiting user, "\
            "discard modem auto. msg!" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev)));
#endif
   }

   return;
}


/**
   Check the driver mailbox NFC message buffer for a new message and returns
   the length and a pointer to an available message.
   The buffer was filled by the interrupt service routine and the
   identification for a available message is a none zero bufCtrl field.

   Poll mode:
   If poll mode is on, these routine checks also the corresponding MEI mailbox
   for a new message and if available transfers the message to the
   read buffer.

\param
   pMeiDynCntrl: Points to the VRX dynamic device control struct.
\param
   ppBuf:   [OUT] returns the pointer to the message buffer.

\return
   Message available:
   Number of available bytes within the buffer.
   If a message is available the msgLen will be returned and also
   the message buffer address will be set to the ppBuf parameter.

   No message available:
   Returns 0 (no bytes within the buffer) and also the ppBuf parameter
   will be set to NULL.

   Error:
   IFX_ERROR, and also the ppBuf parameter will be set to NULL.

\remarks
   The message lenght is byte

*/
MEI_STATIC IFX_int32_t MEI_GetNextRdNfcMsg(
                              MEI_DYN_CNTRL_T *pMeiDynCntrl,
                              MEI_RECV_BUF_T  **ppRdBuf,
                              IFX_uint8_t       rdIdxRd)
{
   MEI_DYN_NFC_DATA_T *pDynNfc = pMeiDynCntrl->pInstDynNfc;
   MEI_RECV_BUF_T     *pRecvDataCntrl = IFX_NULL;

   /*
      first check for a message
   */
   if ( (pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl & ~MEI_RECV_BUF_CTRL_LOCKED)  &&    /* valid data */
       !(pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl &  MEI_RECV_BUF_CTRL_LOCKED) )      /* not locked */
   {
      /* mark the NFC for processing and get the buffer */
      pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl |= MEI_RECV_BUF_CTRL_LOCKED;
      pRecvDataCntrl = &pDynNfc->pRecvDataCntrl[rdIdxRd];
   }

   /*
      second check for a message (polling mode)
   */
   if (pRecvDataCntrl == IFX_NULL)
   {
      /*
         no message within the buffer and poll mode enabled:
         --> poll this device for new messages
      */
      if ( (pMeiDynCntrl->pMeiDev->eModePoll == e_MEI_DEV_ACCESS_MODE_PASSIV_POLL)
           #if (MEI_SUPPORT_DRV_LOOPS == 1)
           && !pMeiDynCntrl->pMeiDev->bDrvLoop
           #endif
         )
      {
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
                ("MEI_DRV[%02d-%02d]: MEI_GetNextRdNfcMsg - check device" MEI_DRV_CRLF,
                  MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));

         MEI_PollIntPerVrxLine(pMeiDynCntrl->pMeiDev, e_MEI_DEV_ACCESS_MODE_PASSIV_POLL);

         /*
            check NFC buffer again
         */
         if ( (pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl & ~MEI_RECV_BUF_CTRL_LOCKED)  &&    /* valid data */
             !(pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl &  MEI_RECV_BUF_CTRL_LOCKED) )      /* not locked */
         {
            /* mark the NFC for processing and get the buffer */
            pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl |= MEI_RECV_BUF_CTRL_LOCKED;
            pRecvDataCntrl = &pDynNfc->pRecvDataCntrl[rdIdxRd];
         }
      }
   }        /* if (pNextMsg == NULL) {...} */

   if (pRecvDataCntrl)
   {
      *ppRdBuf = pRecvDataCntrl;
      return (pDynNfc->pRecvDataCntrl[rdIdxRd].msgLen);
   }
   else
   {
      *ppRdBuf = IFX_NULL;
   }

   return IFX_SUCCESS;
}

/*
   Release / Free the current read message.
   The upper layer request and process the current message. After processing
   complete the message have to be released (freed).

   Poll mode:
   If poll mode is on, these routine checks also the corresponding
   MEI mailbox for new messages and if available transfers the message
   to the read buffer.

\param
   pMeiDynCntrl: Points to the VRX dynamic device control struct.

\return
   NONE

*/
MEI_STATIC IFX_void_t MEI_FreeNextRdNfcMsg(
                                    MEI_DYN_CNTRL_T *pMeiDynCntrl,
                                    IFX_uint8_t       rdIdxRd )
{
   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_NFC_DATA_T *pDynNfc   = pMeiDynCntrl->pInstDynNfc;

   MEI_DRV_GET_UNIQUE_DRIVER_ACCESS(pMeiDev);
   MEI_DisableDeviceInt(pMeiDev);

   if (pDynNfc->rdIdxRd != rdIdxRd)
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d-%02d]: MEI_FreeNextRdNfcMsg - "
              "index changed (check for reset)" MEI_DRV_CRLF,
               MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
   }
   else
   {
      if ( pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl )
      {
         if ( !(pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl & MEI_RECV_BUF_CTRL_LOCKED) )
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
                   ("MEI_DRV[%02d-%02d]: MEI_FreeNextRdNfcMsg - "
                    "free unprocessed NFC buffer" MEI_DRV_CRLF,
                     MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
         }

         /* increment next receive buffer read index and free last buffer */
         pDynNfc->rdIdxRd = (rdIdxRd < (pDynNfc->numOfBuf -1)) ? (rdIdxRd+1) : 0;
         pDynNfc->pRecvDataCntrl[rdIdxRd].msgLen = 0;
         pDynNfc->pRecvDataCntrl[rdIdxRd].bufCtrl = MEI_RECV_BUF_CTRL_FREE;
      }
   }

   MEI_EnableDeviceInt(pMeiDev);
   MEI_DRV_RELEASE_UNIQUE_DRIVER_ACCESS(pMeiDev);

   return;
}


/**
   Read a Modem NFC out form the NFC receive buffer.

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pRecvDataCntrl - points to the internal buffer control struct.
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: length of the message and number of payload bytes
   Error:   negative value
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   not successful functional operation code
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
MEI_STATIC IFX_int32_t MEI_DriverMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)

{
   IFX_int32_t             ret = IFX_SUCCESS, paylSize_byte;

   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T,            *pMeiDev,  pMeiDynCntrl->pMeiDev);
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DRV_MSG_Header_t, *pDrvMsgHdr, (MEI_DRV_MSG_Header_t *)pRecvDataCntrl->recvDataBuf_s.pBuffer);

   /* no modem message */
   pUserMsg->msgId = 0;
   pUserMsg->msgClassifier = 0;

   /* expect at least the header */
   if ( pRecvDataCntrl->msgLen >= sizeof(MEI_DRV_MSG_Header_t) )
   {
      pUserMsg->msgCtrl = MEI_MSG_CTRL_DRIVER_MSG;

      paylSize_byte = (pRecvDataCntrl->msgLen > pUserMsg->paylSize_byte) ?
                       pUserMsg->paylSize_byte : pRecvDataCntrl->msgLen;

      /* check return part against the buffer size */
      if (pRecvDataCntrl->msgLen > pUserMsg->paylSize_byte)
      {
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: WARNING - "
               "DriverMsgRead[0x%08X] size missmatch recv payl (%d) > usr buf (%d)" MEI_DRV_CRLF
               , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
               , pDrvMsgHdr->id
               , pRecvDataCntrl->msgLen, pUserMsg->paylSize_byte));
      }

      if (bInternCall)
      {
         memcpy(pUserMsg->pPayload, pRecvDataCntrl->recvDataBuf_s.pBuffer, paylSize_byte);
         pUserMsg->paylSize_byte = paylSize_byte;
      }
      else
      {
         if ( (MEI_DRVOS_CpyToUser(pUserMsg->pPayload, pRecvDataCntrl->recvDataBuf_s.pBuffer, paylSize_byte)) == IFX_NULL )
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: DriverMsgRead[0x%08X] - copy_to_user() failed!" MEI_DRV_CRLF
                   , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                   , pDrvMsgHdr->id));
            pUserMsg->paylSize_byte = 0;
            ret = -e_MEI_ERR_RETURN_ARG;
         }
         else
         {
            pUserMsg->paylSize_byte = paylSize_byte;
         }
      }
   }
   else
   {
      ret = ((IFX_int32_t)pRecvDataCntrl->msgLen < 0) ? -e_MEI_ERR_DEV_INVAL_RESP : 0;
      pUserMsg->paylSize_byte = 0;
   }

   return ret;
}


#if (MEI_DRV_ATM_OAM_ENABLE == 1)
/**
   Read a ATM OAM autonomous msg out form the NFC receive buffer.

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pRecvDataCntrl - points to the internal buffer control struct.
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: length of the message and number of payload bytes
   Error:   negative value
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   not successful functional operation code
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
MEI_STATIC IFX_int32_t MEI_AtmOamMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)
{
   IFX_int32_t                      ret = IFX_SUCCESS, paylSize_byte;
   MEI_ATMOAM_CELL_BUFFER_T       *pAtmOamMsg = (MEI_ATMOAM_CELL_BUFFER_T *)pRecvDataCntrl->recvDataBuf_s.pBuffer;
   IOCTL_MEI_ATMOAM_drvAtmCells_t *pUsrAtmOamCells = (IOCTL_MEI_ATMOAM_drvAtmCells_t *)pUserMsg->pPayload;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   /* no modem message */
   pUserMsg->msgId = 0;
   pUserMsg->msgClassifier = 0;

   /* expect at least one cell */
   if ( pRecvDataCntrl->msgLen < (2 * sizeof(IFX_uint32_t)))
   {
      /* invalid message */
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d - %02d]: ERROR - "
            "AtmOamMsgRead[0x%08X] invalid msg, msgLen = %d (min 8)" MEI_DRV_CRLF,
            MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
            pAtmOamMsg->atmOamId, pRecvDataCntrl->msgLen));

      goto MEI_ATM_OAM_MSG_READ_ERR;
   }
   else
   {
      paylSize_byte = (  (pAtmOamMsg->cellCnt * sizeof(IOCTL_MEI_ATMOAM_rawCell_t))
                       + (sizeof(pAtmOamMsg->cellCnt)) );

      if (paylSize_byte != (IFX_int32_t)(pRecvDataCntrl->msgLen - sizeof(pAtmOamMsg->atmOamId)) )
      {
         /* size missmatch */
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: ERROR - "
               "AtmOamMsgRead[0x%08X] size missmatch, msgLen = %d (- 4), payl Size = %d, cell cnt = %d" MEI_DRV_CRLF,
               MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
               pAtmOamMsg->atmOamId, pRecvDataCntrl->msgLen, paylSize_byte, pAtmOamMsg->cellCnt));

         goto MEI_ATM_OAM_MSG_READ_ERR;
      }
   }

   /* check return part against the buffer size */
   if (paylSize_byte > (IFX_int32_t)pUserMsg->paylSize_byte)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d - %02d]: WARNING - "
            "AtmOamMsgRead[0x%08X] size missmatch recv payl (%d - 4) > usr buf (%d)" MEI_DRV_CRLF
            , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
            , pAtmOamMsg->atmOamId
            , pRecvDataCntrl->msgLen, pUserMsg->paylSize_byte));

      paylSize_byte = (IFX_int32_t)pUserMsg->paylSize_byte;
   }

   pUserMsg->msgCtrl = MEI_MSG_CTRL_ATMOAM_CELL_MSG;

   if (bInternCall)
   {
      pUsrAtmOamCells->ictl.retCode = 0;
      pUsrAtmOamCells->cellCount    = pAtmOamMsg->cellCnt;
      memcpy(pUsrAtmOamCells->atmCells, pAtmOamMsg->atmCells, (paylSize_byte - sizeof(pAtmOamMsg->cellCnt)));

      pUserMsg->paylSize_byte = paylSize_byte;
   }
   else
   {
      if ( ((MEI_DRVOS_CpyToUser( &pUsrAtmOamCells->cellCount,
                                  &pAtmOamMsg->cellCnt,
                                  sizeof(pUsrAtmOamCells->cellCount))) == IFX_NULL) ||
           ((MEI_DRVOS_CpyToUser( pUsrAtmOamCells->atmCells,
                                  pAtmOamMsg->atmCells,
                                  (paylSize_byte - sizeof(pAtmOamMsg->cellCnt)))) == IFX_NULL)
         )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: AtmOamMsgRead[0x%08X] - copy_to_user() failed!" MEI_DRV_CRLF
                , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                , pAtmOamMsg->atmOamId));
         pUserMsg->paylSize_byte = 0;
         ret = -e_MEI_ERR_RETURN_ARG;
      }
      else
      {
         pUserMsg->paylSize_byte = paylSize_byte;
      }
   }

   return ret;

MEI_ATM_OAM_MSG_READ_ERR:
   ret = -e_MEI_ERR_DEV_INVAL_RESP;
   pUserMsg->paylSize_byte = 0;

   return ret;
}
#endif   /* #if (MEI_DRV_ATM_OAM_ENABLE == 1) */


#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
/**
   Read a Clear EOC autonomous msg out form the NFC receive buffer.

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pRecvDataCntrl - points to the internal buffer control struct.
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: length of the message and number of payload bytes
   Error:   negative value
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   not successful functional operation code
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
MEI_STATIC IFX_int32_t MEI_CEocMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)
{
   IFX_int32_t                ret = IFX_SUCCESS, paylSize_byte;
   MEI_CEOC_MEI_EOC_FRAME_T *pCEocMsg = (MEI_CEOC_MEI_EOC_FRAME_T *)pRecvDataCntrl->recvDataBuf_s.pBuffer;
   IOCTL_MEI_CEOC_frame_t   *pUsrCEocFrame = (IOCTL_MEI_CEOC_frame_t *)pUserMsg->pPayload;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   /* no modem message */
   pUserMsg->msgId = 0;
   pUserMsg->msgClassifier = 0;

   /* expect at least one CEOC Id, len, and prot ident field [+ 1 payload value] */
   if ( pRecvDataCntrl->msgLen < (  sizeof(pCEocMsg->cEocId)
                                  + sizeof(pCEocMsg->length_byte)
                                  + sizeof(pCEocMsg->protIdent)) )
   {
      /* invalid message */
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d - %02d]: ERROR - "
            "CEocMsgRead[0x%08X] invalid msg, msgLen = %d (min 8)" MEI_DRV_CRLF,
            MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
            pCEocMsg->cEocId, pRecvDataCntrl->msgLen));

      goto MEI_CEOC_MSG_READ_ERR;
   }
   else
   {
      paylSize_byte = (IFX_int32_t)pCEocMsg->length_byte - sizeof(pCEocMsg->protIdent);

      if (paylSize_byte !=
          (IFX_int32_t)(pRecvDataCntrl->msgLen -
                        (sizeof(pCEocMsg->cEocId) + sizeof(pCEocMsg->length_byte) + sizeof(pCEocMsg->protIdent)) ) )
      {
         /* size missmatch */
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: ERROR - CEocMsgRead[0x%08X], "
                "size missmatch, msgLen = %d, paylSize = %d, frameSize = %d" MEI_DRV_CRLF,
               MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
               pCEocMsg->cEocId, pRecvDataCntrl->msgLen, paylSize_byte, pCEocMsg->length_byte));

         goto MEI_CEOC_MSG_READ_ERR;
      }
   }

   /* check return part against the buffer size */
   if (paylSize_byte > (IFX_int32_t)pUserMsg->paylSize_byte)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d - %02d]: WARNING - "
            "CEocMsgRead[0x%08X] size missmatch recv payl (%d) > usr buf (%d)" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
             pCEocMsg->cEocId, pRecvDataCntrl->msgLen, pUserMsg->paylSize_byte));

      paylSize_byte = (IFX_int32_t)pUserMsg->paylSize_byte;
   }

   pUserMsg->msgCtrl = MEI_MSG_CTRL_CLEAR_EOC_FRAME;

   if (bInternCall)
   {
      pUsrCEocFrame->ictl.retCode  = 0;
      pUsrCEocFrame->dataSize_byte = (unsigned int)paylSize_byte;
      pUsrCEocFrame->protIdent     = (unsigned int)pCEocMsg->protIdent;
      memcpy(pUsrCEocFrame->pEocData, pCEocMsg->cEocRawData.d_8, paylSize_byte);

      pUserMsg->paylSize_byte = paylSize_byte
                                + sizeof(IOCTL_MEI_ioctl_t)
                                + sizeof(pUsrCEocFrame->dataSize_byte)
                                + sizeof(pUsrCEocFrame->protIdent);
   }
   else
   {
      unsigned int tmpVal = (unsigned int)pCEocMsg->protIdent;

      if ( ((MEI_DRVOS_CpyToUser( &pUsrCEocFrame->dataSize_byte,
                                  &paylSize_byte,
                                  sizeof(pUsrCEocFrame->dataSize_byte))) == IFX_NULL) ||
           ((MEI_DRVOS_CpyToUser( &pUsrCEocFrame->protIdent,
                                  &tmpVal,
                                  sizeof(pUsrCEocFrame->protIdent))) == IFX_NULL) ||
           ((MEI_DRVOS_CpyToUser( pUsrCEocFrame->pEocData,
                                  pCEocMsg->cEocRawData.d_8,
                                  paylSize_byte)) == IFX_NULL)
         )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ERROR - CEocMsgRead[0x%08X], "
                "copy_to_user() failed!" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                pCEocMsg->cEocId));
         pUserMsg->paylSize_byte = 0;
         ret = -e_MEI_ERR_RETURN_ARG;
      }
      else
      {
         pUserMsg->paylSize_byte = paylSize_byte
                                   + sizeof(IOCTL_MEI_ioctl_t)
                                   + sizeof(pUsrCEocFrame->dataSize_byte)
                                   + sizeof(pUsrCEocFrame->protIdent);
      }
   }

   return ret;

MEI_CEOC_MSG_READ_ERR:
   ret = -e_MEI_ERR_DEV_INVAL_RESP;
   pUserMsg->paylSize_byte = 0;

   return ret;
}
#endif   /* #if (MEI_DRV_CLEAR_EOC_ENABLE == 1) */


/**
   Read a Modem NFC out form the NFC receive buffer.

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pRecvDataCntrl - points to the internal buffer control struct.
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: length of the message and number of payload bytes
   Error:   negative value
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   not successful functional operation code
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
MEI_STATIC IFX_int32_t MEI_ModemNfcRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              MEI_RECV_BUF_T      *pRecvDataCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)

{
   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_MEI_MAILBOX_T  *pMailbox  = (MEI_MEI_MAILBOX_T *)pRecvDataCntrl->recvDataBuf_s.pBuffer;
   IFX_char_t           *pSource;
   IFX_int32_t          ret = IFX_SUCCESS, paylSize_byte;

   /* no driver message */
   pUserMsg->msgCtrl = MEI_MSG_CTRL_MODEM_MSG;

   /* expect at least the header */
   if ( (pRecvDataCntrl->msgLen >= (int)(CMV_HEADER_8BIT_SIZE)) && (pMailbox) )
   {
      pUserMsg->msgId = CMV_MSGHDR_MSGID_GET(pMailbox->mbCmv.cmv);
      pUserMsg->msgClassifier = (pMailbox->mbCmv.cmv.header.mbxCode & 0x00FF) |
                                ( (CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv)) << 8);

      /*
         Return IFX/modem message
         - index and length fields becomes part of the appl. payload
      */
      pSource = (unsigned char *)&pMailbox->mbCmv.cmv.header.index;

      /* size field contains number of 16 bit payload elements of the message */
      paylSize_byte = (CMV_MSGHDR_PAYLOAD_SIZE_GET(pMailbox->mbCmv.cmv)) << CMV_MSG_BIT_SIZE_16BIT;

      if ( (pRecvDataCntrl->msgLen - (CMV_HEADER_8BIT_SIZE)) < (IFX_uint32_t)paylSize_byte )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
               ("MEI_DRV[%02d - %02d]: "
               "WARNING - ModemNfcRead, size missmatch len (%d - %d) / payl (%d)" MEI_DRV_CRLF,
               MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
               pRecvDataCntrl->msgLen, CMV_HEADER_8BIT_SIZE, paylSize_byte));

         MEI_TRACE_CMV_MSG( pMeiDev, &pMailbox->mbCmv.cmv,
                                  "ModemNfcRead, size missmatch", MEI_DRV_PRN_LEVEL_HIGH);

         paylSize_byte = pRecvDataCntrl->msgLen - CMV_HEADER_8BIT_SIZE;
      }

      /* add index and length field to the return field */
      paylSize_byte += ( sizeof(pMailbox->mbCmv.cmv.header.index) +
                         sizeof(pMailbox->mbCmv.cmv.header.length) );

      /* check return part against the buffer size */
      if (paylSize_byte > (int)pUserMsg->paylSize_byte)
      {
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: WARNING - "
               "ModemNfcRead[0x%04X] size missmatch recv payl (%d) / usr buf (%d)" MEI_DRV_CRLF
               , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
               , pUserMsg->msgId, paylSize_byte, pUserMsg->paylSize_byte));

         paylSize_byte = pUserMsg->paylSize_byte;
      }

      if (bInternCall)
      {
         memcpy(pUserMsg->pPayload, pSource, paylSize_byte);
         pUserMsg->paylSize_byte = paylSize_byte;
      }
      else
      {
         if ( (MEI_DRVOS_CpyToUser(pUserMsg->pPayload, pSource, paylSize_byte)) == IFX_NULL )
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: ModemNfcRead[0x%04X] - copy_to_user() failed!" MEI_DRV_CRLF
                   , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                   , pUserMsg->msgId));
            ret = -e_MEI_ERR_RETURN_ARG;
         }
         else
         {
            pUserMsg->paylSize_byte = paylSize_byte;
         }
      }

      if ( (CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv)) & 0x00F0 )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ModemNfcRead[0x%04X] - FctOP ERROR 0x%02X!" MEI_DRV_CRLF
                , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                , pUserMsg->msgId
                , CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv) ));

         ret = -e_MEI_ERR_DEV_NEG_RESP;
      }
   }
   else     /* if (len >= (int)(CMV_HEADER_8BIT_SIZE)) {...} else ... */
   {
      if (pRecvDataCntrl->msgLen != 0)
      {
         if (pMailbox)
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: ERROR - ModemNfcRead invalid msg (size %d)"
                   "[0x%04X 0x%04X 0x%04X 0x%04X ...]" MEI_DRV_CRLF
                   , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, pRecvDataCntrl->msgLen
                   , (pRecvDataCntrl->msgLen > 1) ? pMailbox->mbRaw.rawMsg[0] : 0xFFFF
                   , (pRecvDataCntrl->msgLen > 3) ? pMailbox->mbRaw.rawMsg[1] : 0xFFFF
                   , (pRecvDataCntrl->msgLen > 5) ? pMailbox->mbRaw.rawMsg[2] : 0xFFFF
                   , (pRecvDataCntrl->msgLen > 7) ? pMailbox->mbRaw.rawMsg[3] : 0xFFFF ));
         }
         else
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: ERROR - ModemNfcRead invalid msg (size %d)" MEI_DRV_CRLF
                   , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, pRecvDataCntrl->msgLen));
         }
      }

      if ((IFX_int32_t)pRecvDataCntrl->msgLen < 0)
      {
         ret = -e_MEI_ERR_DEV_INVAL_RESP;
      }
      else
      {
         ret = 0;
      }

      /* clear the payload size count */
      pUserMsg->paylSize_byte = 0;
   }        /* if (len >= (int)(CMV_HEADER_8BIT_SIZE)) {...} else {...} */

   return ret;
}


/* ============================================================================
   Global Message Function definitions
   ========================================================================= */

#if (MEI_SUPPORT_DRV_LOOPS == 1)
/*
   Enable the mailbox loop within the driver.

\param
   pMeiDev: Points to the VRX device control struct.

\return
   New loop status:
   - TRUE:  loop on (enabled)
   - FALSE: loop off (disabled)
*/
IFX_boolean_t MEI_MailboxLoop( MEI_DEV_T *pMeiDev,
                                     IFX_boolean_t loopOn)
{
   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV [%02d]: drv loop switch %s" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), (loopOn)?"ON" : "OFF") );

   /* set new setup */
   if ( pMeiDev->eModePoll == e_MEI_DEV_ACCESS_MODE_IRQ)
   {
      if (loopOn == IFX_TRUE)
      {

         /* loop enable
            --> disable msg available interrupt within the default mask
            --> set new mask
         */
         MEI_DisableDeviceInt(pMeiDev);
         pMeiDev->bDrvLoop = loopOn;

         pMeiDev->intMask &= (~(pMeiDev->meiDrvCntrl.intMsgMask));
         MEI_EnableDeviceInt(pMeiDev);

         PRN_DBG_USR(MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV [%02d]: LOOP, Int changed - disable" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev)) );
      }
      else
      {

         /* loop disable
            --> enable msg available interrupt within the default mask
            --> set new mask
         */
         MEI_DisableDeviceInt(pMeiDev);
         pMeiDev->bDrvLoop = loopOn;
         pMeiDev->intMask |= (pMeiDev->meiDrvCntrl.intMsgMask);
         MEI_EnableDeviceInt(pMeiDev);

         PRN_DBG_USR(MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV [%02d]: LOOP, Int changed - enable" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev)) );
      }
   }
   else
   {
      /* driver in poll mode - do not change interrupts */
      pMeiDev->bDrvLoop = loopOn;
   }

   return pMeiDev->bDrvLoop;
}
#endif

/**
   Mailbox Wait - wait for an expected mailbox msg.

\param
   pMeiDev  points to the current VRX device.

\return
   IFX_SUCCESS:   response received.
   IFX_ERROR:     no response received - timeout
*/
IFX_int32_t MEI_WaitForMailbox(MEI_DEV_T *pMeiDev)
{

   /* still waiting --> setup wait time */
   if (MEI_GET_TIMEOUT_CNT(pMeiDev) > 0)
   {
      /* wait */
      MEI_DRVOS_EventWait_timeout(
                     &pMeiDev->eventMailboxRecv,
                     MEI_MIN_MAILBOX_POLL_TIME_MS);

      MEI_DEC_TIMEOUT_CNT(pMeiDev);
   }
   else
   {
      /* timeout */
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d]: Mailbox timeout <Wait for device> - MB state = %d" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), MEI_DRV_MAILBOX_STATE_GET(pMeiDev) ));

      return IFX_ERROR;
   }

   return IFX_SUCCESS;
}


/**
   Setup an CMV header for mailbox communication.
   Note:
      - the CMV header fields index and length are already set

   - set msgId
   - interprete BIT_SIZE, Size, Function Opcode
   - set Mailbox Code

\param
   pMeiDev     points to the VRX device struct.
\param
   pMsg        points to the CMV message
\param
   pUsrMsgWr   input msg from user ioctl.

\return
   IFX_SUCCESS in case of success
   IFX_ERROR   in case of error
*/
IFX_int32_t MEI_SetCmvHeader( MEI_DEV_T *pMeiDev,
                                    CMV_STD_MESSAGE_T *pMsg,
                                    IOCTL_MEI_message_t *pUsrMsgWr)
{
   IFX_int32_t msgSize;

   if ( pUsrMsgWr->paylSize_byte < (sizeof(pMsg->header.index) + sizeof(pMsg->header.length)) )
   {
      /* ERROR - invalid size */
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
            ("MEI_DRV[%02d]: ERROR - payload size = %d, missing index/length field" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pUsrMsgWr->paylSize_byte));

      return IFX_ERROR;
   }

   /* [0] */
   pMsg->header.mbxCode   = MEI_MBOX_CODE_MSG_WRITE;
   /* [1] */
   pMsg->header.fctCode   = 0;
   /* [2] */
   pMsg->header.paylCntrl = 0;

   /* set the following CMV header fields:
      - BIT_SIZE, Function Opcode, message ID
   */
   MEI_MsgId2CmvHeader(pMeiDev,pMsg, pUsrMsgWr->msgId);

   /* index and length are part of the payload
      - (index + length)
   */
   msgSize = pUsrMsgWr->paylSize_byte -
               (sizeof(pMsg->header.index) + sizeof(pMsg->header.length));

    /* size field contains number of 16 bit payload elements of the message */
   P_CMV_MSGHDR_PAYLOAD_SIZE_SET( pMsg, ((IFX_uint32_t)msgSize >> CMV_MSG_BIT_SIZE_16BIT) );

   if ( !(pUsrMsgWr->msgId & MEI_MSGID_BIT_IND_IFX) &&
        !(pUsrMsgWr->msgId & MEI_MSGID_BIT_IND_WR_CMD) )
   {
      /* CMV read command - size is 0  (currently) */
      P_CMV_MSGHDR_PAYLOAD_SIZE_SET(pMsg, 0);
      if (msgSize != 0)
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d]: ERROR - CMV msg header - payload for CMV Read given (%d / %d)" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), msgSize, pMsg->header.length));

         return IFX_ERROR;
      }
   }

   return (msgSize + CMV_HEADER_8BIT_SIZE);
}


/**
   Interprete the msg ID and set the corresponding CMV header fields.
   Note:
      - the CMV header fields index and length are already set

   - set msgId
   - interprete BIT_SIZE, Function Opcode, message ID

\param
   pMeiDev     points to the VRX device struct.
\param
   pMsg        points to the CMV message
\param
   msgId       input msgId (IFX format) from user ioctl.

\return
   none
*/
IFX_void_t MEI_MsgId2CmvHeader( MEI_DEV_T *pMeiDev,
                                CMV_STD_MESSAGE_T *pMsg,
                                IFX_uint16_t msgId)
{
   /* CMV msg[3]:
         clear the CMV RD/WR indication flag
   */
   pMsg->header.MessageID = msgId & ~(MEI_MSGID_BIT_IND_WR_CMD);

   /* CMV msg[1]: Function OP Code, BIT size */
   P_CMV_MSGHDR_FCT_OPCODE_SET( pMsg,
      ((msgId & MEI_MSGID_BIT_IND_WR_CMD) ? H2D_CMV_WRITE : H2D_CMV_READ) );

   if (msgId & MEI_MSGID_BIT_IND_IFX)
      P_CMV_MSGHDR_BIT_SIZE_SET(pMsg, CMV_MSG_BIT_SIZE_32BIT);
   else
      P_CMV_MSGHDR_BIT_SIZE_SET(pMsg, CMV_MSG_BIT_SIZE_16BIT);

   /* set the message index and increment */
   MEI_DRV_GET_UNIQUE_DRIVER_ACCESS(pMeiDev);
   P_CMV_MSGHDR_MSGIDX_SET(pMsg, (MEI_GET_MSG_INDEX(pMeiDev)));
   MEI_INC_MSG_INDEX(pMeiDev);
   MEI_DRV_RELEASE_UNIQUE_DRIVER_ACCESS(pMeiDev);

   return;
}

/**
   Check the own instance buffer and free it if busy.

\param
   pMeiDynCntrl: Points to the dynamic VRX device control struct.
\param
   pMeiDev     points to the VRX device struct.
\param
   pDynCmd:      points to the dynamic command block

*/
IFX_int32_t MEI_WaitForInstance(
                              MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                              MEI_DEV_T          *pMeiDev,
                              MEI_DYN_CMD_DATA_T *pDynCmd)
{
   if (MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) != e_MEI_MB_BUF_FREE)
   {
      /* block int, block other tasks - check if this struct is currently in use */
      MEI_DRV_GET_UNIQUE_ACCESS(pMeiDev);
      if (pMeiDev->pCurrDynCmd == pDynCmd)
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: WaitForInstance - reset dyn recv (activ), state = %d " MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                 MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) ));

         pMeiDev->pCurrDynCmd = NULL;
      }
      else
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: WaitForInstance - reset dyn recv (passiv), state = %d " MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                 MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) ));
      }
      MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_FREE);

      MEI_DRV_RELEASE_UNIQUE_ACCESS(pMeiDev);
   }

   return IFX_SUCCESS;
}

/*
   Write a mailbox message from the user standard interface via the
   device access functions (MEI) to the VRX device.

\param
   pMeiDynCntrl: Points to the dynamic VRX device control struct.
\param
   pMBMsg:         Points to the mailbox message (mailbox code + message).
\param
   mbMsgSize:      Size [byte] of the message (with mailbox Code).

\return
   Number of transfered bytes.
   Negativ value in case of error.

\attention
   Before you call this function, make sure that the mailbox is free
   !!! NO ACK Pending !!!
*/
IFX_int32_t MEI_WriteMailbox( MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                              MEI_DYN_CMD_DATA_T *pDynCmd,
                              MEI_MEI_MAILBOX_T  *pMBMsg,
                              IFX_int32_t         mbMsgSize)
{
   MEI_DEV_T *pMeiDev = pMeiDynCntrl->pMeiDev;
   IFX_int32_t total=0, recvTimeout = 0;

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV[%02d - %02d] Write MBox msg[%d]" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, mbMsgSize));

   MEI_DBG_MSG_TRC_DUMP(pMeiDev, &pMBMsg->mbCmv.cmv);
   MEI_TRACE_CMV_MSG(pMeiDev, &pMBMsg->mbCmv.cmv, "CMD msg wr", MEI_DRV_PRN_LEVEL_LOW);

   /* first - protect the access against other user and wait for response form modem */
   MEI_DRV_GET_UNIQUE_DRIVER_ACCESS(pMeiDev);

   /* Prepare the message  - swap 32bit payload */
   if (CMV_MSGHDR_BIT_SIZE_GET(pMBMsg->mbCmv.cmv) == (unsigned short)CMV_MSG_BIT_SIZE_32BIT)
   {
      if ( MEI_MsgPayl32Swap(pMeiDev, &pMBMsg->mbCmv.cmv, mbMsgSize) != IFX_SUCCESS )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                ("MEI_DRV[%02d - %02d] ERROR - write MBox msg, SWAP" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));

         MEI_IF_STAT_INC_ERROR_COUNT(pMeiDev);

         MEI_DRV_RELEASE_UNIQUE_DRIVER_ACCESS(pMeiDev);
         return e_MEI_ERR_MSG_PARAM;
      }
   }

   /* check now if the mailbox is free */
   if (MEI_DRV_MAILBOX_STATE_GET(pMeiDev) != e_MEI_MB_FREE)
   {
#if (MEI_SUPPORT_TIME_TRACE == 1)
     IFX_uint32_t start_ms = 0, end_ms = 0;
#endif

      /* setup timeout counter for ACK */
      MEI_SET_TIMEOUT_CNT( pMeiDev,
                  MEI_MaxWaitDfeResponce_ms / MEI_MIN_MAILBOX_POLL_TIME_MS);

      /*
         poll for response - remember
         the mutex is lock, so no other task can interrupt
      */
      MEI_GET_TICK_MS_TIME_TRACE(start_ms);
      while(1)
      {
         /* wait: for an response from the VRX boot loader */
         if (pMeiDev->eModePoll == e_MEI_DEV_ACCESS_MODE_PASSIV_POLL)
         {
            /** poll for interrupts manually */
            MEI_ProcessIntPerVrxLine(pMeiDev);
         }

         if (MEI_DRV_MAILBOX_STATE_GET(pMeiDev) != e_MEI_MB_FREE)
         {
            if ( MEI_WaitForMailbox(pMeiDev) != IFX_SUCCESS)
            {
               recvTimeout = 1;
               break;
            }
         }
         else
         {
            recvTimeout = 0;
            break;
         }
      }

      MEI_GET_TICK_MS_TIME_TRACE(end_ms);

#if (MEI_SUPPORT_TIME_TRACE == 1)
      {
         IFX_uint32_t tick_ms = MEI_TIME_TRACE_GET_TICK_MS(start_ms, end_ms);
         MEI_TIME_TRACE_CHECK_WAIT_SEND_MIN_MAX( pMeiDev->timeStat,
                                                   tick_ms,
                                                   MEI_MaxWaitDfeResponce_ms );
      }

      if (recvTimeout)
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
            ("MEI_DRV[%02d - %02d]: WARNING wait for MB timeout - %d - %d = %d [ms]" MEI_DRV_CRLF,
            MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
            start_ms, end_ms, MEI_TIME_TRACE_GET_TICK_MS(start_ms, end_ms)));
      }
#endif

   }

   /* second - now protect against interrupts */
   MEI_DRV_GET_UNIQUE_MAILBOX_ACCESS(pMeiDev);

   if (recvTimeout == 1)
   {
      /* clear the pending instance */
      if (pMeiDev->pCurrDynCmd)
      {
         pMeiDev->pCurrDynCmd->cmdAckCntrl.msgLen = 0;
         pMeiDev->pCurrDynCmd->cmdAckCntrl.bufCtrl = MEI_RECV_BUF_CTRL_FREE;
         MEI_DRV_DYN_MBBUF_STATE_SET(pMeiDev->pCurrDynCmd, e_MEI_MB_BUF_TIMEOUT);
         pMeiDev->pCurrDynCmd = NULL;
      }

      MEI_DRV_MAILBOX_STATE_SET(pMeiDev, e_MEI_MB_FREE);
   }


#if (MEI_SUPPORT_TIME_TRACE == 1)
   if (pDynCmd)
   {
      MEI_GET_TICK_MS_TIME_TRACE(pDynCmd->ackWaitStart_ms);
   }
#if (MEI_SUPPORT_DFE_GPA_ACCESS == 1)
   else
   {
      MEI_GET_TICK_MS_TIME_TRACE(pMeiDev->gpaBuf.ackWaitStart_ms);
   }
#endif
#endif

#if (MEI_TEST_LOCAL_MB_CODE == 1)
   if (pMBMsg->mbCmv.cmv.header.mbxCode != MEI_MBOX_CODE_MSG_WRITE)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
             ("MEI_DRV[%02d - %02d]: Warning - write MB-Code 0x%04X" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
              pMBMsg->mbCmv.cmv.header.mbxCode));
   }
#endif      /* #if (MEI_TEST_LOCAL_MB_CODE == 1) */

   if ( (total = MEI_WriteMailBox( &pMeiDev->meiDrvCntrl,
                                   pMeiDev->modemData.mBoxDescr.addrMe2Arc,
                                   pMBMsg,
                                   mbMsgSize/2 )) < 0 )
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: ERROR - write MBox msg, error=%d" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, total));

      MEI_IF_STAT_INC_ERROR_COUNT(pMeiDev);

      total = -e_MEI_ERR_OP_FAILED;
   }
   else
   {
      /* set status for this open instance */
      if (pDynCmd)
      {
         /* no GPA msg */
         MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_ACK_PENDING);
         pMeiDev->pCurrDynCmd = pDynCmd;
      }

      /* set VRX device status */
      MEI_DRV_MAILBOX_STATE_SET(pMeiDev, e_MEI_MB_PENDING_ACK_1);

      /* return value and statistics */
      MEI_IF_STAT_INC_SEND_MSG_COUNT(pMeiDev);

      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
             ("MEI_DRV[%02d - %02d] MBox msg written - size = %d" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, total));
   }

   MEI_DRV_RELEASE_UNIQUE_MAILBOX_ACCESS(pMeiDev);
   MEI_DRV_RELEASE_UNIQUE_DRIVER_ACCESS(pMeiDev);

   return total;
}


/**
   Read a mailbox message from the VRX mailbox via the device access
   functions (MEI) to the internal driver buffers.

\param
   pMeiDev: Points to the VRX device struct.

\return
   NONE - all statistics will be set within the device struct.

\remarks
   These function will be called in polling mode from kernel space
   also within the interrupt service routine.
   !!! so keep it clean form blocking calls !!!

*/
IFX_void_t MEI_ReadMailbox( MEI_DEV_T *pMeiDev )
{
   IFX_uint16_t  mboxCode;

   /* Check if the mailbox address is already set */
   if ( ( MEI_DRV_STATE_GET(pMeiDev) == e_MEI_DRV_STATE_SW_INIT_DONE) ||
        ( MEI_DRV_STATE_GET(pMeiDev) == e_MEI_DRV_STATE_BOOT_WAIT_ROM_ALIVE) ||
        ( MEI_DRV_STATE_GET(pMeiDev) == e_MEI_DRV_STATE_WAIT_FOR_FIRST_RESP) )
   {
      /* verify mailbox address */
      if ( MEI_SetMailboxAddress(pMeiDev) != IFX_SUCCESS)
      {
         PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
              ( "MEI_DRV[%02d]: ERROR - "
                "cannot set mailbox address, discard message" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev)) );

         MEI_ReleaseMailboxMsg(&pMeiDev->meiDrvCntrl);

         return;
      }
   }

   /*
      - check the mail box for new messages
         --> read the header and interpete
             - distribute the message to the NFC instances
             - put the message to the ack buffer (if expected)
   */

   /* get the MailBox code */
   mboxCode = MEI_GetMailBoxCode( &pMeiDev->meiDrvCntrl,
                                  pMeiDev->modemData.mBoxDescr.addrArc2Me);

   switch(mboxCode)
   {
      case MEI_MBOX_CODE_MSG_ACK:
         /* ACK recieved --> process pending ack  */
         MEI_RecvAckMailboxMsg(pMeiDev);
         break;

      case MEI_MBOX_CODE_NFC_REQ:
      case MEI_MBOX_CODE_EVT_REQ:
      case MEI_MBOX_CODE_ALM_REQ:
      case MEI_MBOX_CODE_DBG_REQ:
         /* NFC received --> distribute to open instances */
         MEI_RecvAutonomMailboxMsg(pMeiDev, mboxCode);
         break;

#if (MEI_SUPPORT_ROM_CODE == 1)
      case MEI_MBOX_CODE_BOOT:
         /* BOOT received --> indicate Firmware download handler */
         MEI_RecvRomBootMsg(pMeiDev);
         break;
#endif

      case MEI_MBOX_CODE_CS_STAT_REQ:
      case MEI_MBOX_CODE_CS_DYN_REQ:
         /* save and start code swap */
         MEI_IF_STAT_INC_CODESWAP_COUNT(pMeiDev);

         #if (MEI_SUPPORT_DL_DMA_CS == 1)
         MEI_Recv_CODE_SWAP_REQ(pMeiDev, mboxCode);
         #else
         {
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
                  ("MEI_DRV[%02d]: WARNING - "
                  "not supported MailBox Code(0x%04X), CodeSwap not enabled" MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev), mboxCode));
         }
         #endif
         break;

      case MEI_MBOX_CODE_FAST_RD_REQ:
         {
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
                  ("MEI_DRV[%02d]: WARNING - "
                 "not supported MailBox Code MEI_MBOX_CODE_FAST_RD_REQ(0x%04X)" MEI_DRV_CRLF,
                  MEI_DRV_LINENUM_GET(pMeiDev), mboxCode));
         }
         break;

      default:
         {
            PRN_ERR_INT_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                 ("MEI_DRV[%02d]: ERROR - unknown MailBox Code 0x%04X " MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev), mboxCode));

            MEI_ReleaseMailboxMsg(&pMeiDev->meiDrvCntrl);
         }
   }

   return;
}


/**
   Wait and check for an pending ack of a previous written command.

\param
   pMeiDynCntrl   points to the dyn control struct of the device data.

\return
   ACK available:       size of ACK msg (byte)
   NO ACK available:    0 (no bytes for read)
   TIMEOUT/ACK pending: < 0 Error (Timeout, no reponce, reset)

*/
IFX_int32_t MEI_CheckAck(MEI_DYN_CNTRL_T *pMeiDynCntrl)
{
   IFX_int32_t len = 0;

   if (MEI_DRV_DYN_MBBUF_STATE_GET(pMeiDynCntrl->pInstDynCmd) == e_MEI_MB_BUF_ACK_PENDING)
   {
      MEI_MBox_WaitForAck(pMeiDynCntrl->pMeiDev, pMeiDynCntrl->pInstDynCmd);
   }

   /* Now the ACK has been received or timeout */
   switch(MEI_DRV_DYN_MBBUF_STATE_GET(pMeiDynCntrl->pInstDynCmd))
   {
      case e_MEI_MB_BUF_FREE:
         /* no ack available */
         len = 0;
         break;

      case e_MEI_MB_BUF_ACK_AVAIL:
         /* ack message available - dump the receiption */
         if (pMeiDynCntrl->pInstDynCmd->cmdAckCntrl.bufCtrl == MEI_RECV_BUF_CTRL_MODEM_ACK_MSG)
         {
            MEI_TRACE_CMV_MSG( pMeiDynCntrl->pMeiDev,
                                     (CMV_STD_MESSAGE_T *)pMeiDynCntrl->pInstDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer,
                                     "ACK", MEI_DRV_PRN_LEVEL_LOW);

            len = (IFX_int32_t)pMeiDynCntrl->pInstDynCmd->cmdAckCntrl.msgLen;
         }
         else
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: ERROR ReadAck - Message Error" MEI_DRV_CRLF,
                   MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
            len = -e_MEI_ERR_DEV_INVAL_RESP;
         }
         break;

      case e_MEI_MB_BUF_TIMEOUT:
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ERROR ReadAck - Timeout" MEI_DRV_CRLF,
                MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
         len = -e_MEI_ERR_DEV_TIMEOUT;
         break;

      case e_MEI_MB_BUF_RESET_DFE:
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ERROR ReadAck - RESET VRX" MEI_DRV_CRLF,
                MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
         len = -e_MEI_ERR_DEV_RESET;
         break;

      default:
         {
            /* FATAL ERROR: ACK still pending */
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: FATAL ERROR ReadAck - Msg ACK still pending" MEI_DRV_CRLF,
                   MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance));
            len = -e_MEI_ERR_DEV_NO_RESOURCE;
         }
   }        /* switch(pDynCmd->wrState) {...} */

   return len;
}


/**
   Init/Enable the receive NFC part for this open instance.
   - allocates the corresponding buffers and structs
   - add the buffer to the device NFC list.

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\attention
   These function is also allowded before the device init ioctl(). So
   ensure that no MEI access will be done before the device init ioctl call.
   See: interrupt enable / disable
*/
IFX_int32_t MEI_IoctlNfcEnable(
                              MEI_DYN_CNTRL_T *pMeiDynCntrl,
                              IFX_uint32_t      nfcBufPerInst,
                              IFX_uint32_t      nfcBufSize)
{
   IFX_uint32_t         structSize = 0, bufIdx;
   IFX_uint8_t          *pAll      = IFX_NULL, *pNfcBuffAll;
   MEI_RECV_BUF_T     *pRecvDataCntrl = IFX_NULL;
   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_NFC_DATA_T *pDynNfc   = pMeiDynCntrl->pInstDynNfc;

   if (pDynNfc)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d-%02d]: WARNING - NFC Feature already enabled" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
      return 0;
   }

   if (nfcBufSize < sizeof(MEI_MEI_MAILBOX_T))
   {
      nfcBufSize =  sizeof(MEI_MEI_MAILBOX_T);
   }
   nfcBufSize    += (nfcBufSize & 0x3);

   if (nfcBufPerInst == 0)
   {
      nfcBufPerInst =  MEI_MAX_RD_DEV_BUF_PER_DEV;
   }

   structSize =
        /* MEI_DYN_NFC_DATA_T struct + alignement */
        ( (sizeof(MEI_DYN_NFC_DATA_T) + (sizeof(MEI_DYN_NFC_DATA_T) & 0x3)) )
        /* x * MEI_RECV_BUF_T struct + alignement */
      + ( nfcBufPerInst * (sizeof(MEI_RECV_BUF_T) + (sizeof(MEI_RECV_BUF_T) & 0x3)) )
        /* x * NFC message buffer + alignement */
      + ( nfcBufPerInst * nfcBufSize );

   /* allocate required block for standard handling of this instance */
   pAll = (IFX_uint8_t *)MEI_DRVOS_Malloc(structSize);
   if (!pAll)
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d]: ERROR NFC dyn cntrl data - no memory, size %d (curr user: %d)" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), structSize, pMeiDev->openCount));

      return -e_MEI_ERR_NO_MEM;
   }
   memset(pAll, 0x00, structSize);

   /* setup struct pointer */
   pDynNfc                 = (MEI_DYN_NFC_DATA_T*)pAll;
   /* pDynNfc->msgProcessCtrl = MEI_MSG_CNTRL_MODEM_MSG_MASK_DEFAULT; */

   pRecvDataCntrl = (MEI_RECV_BUF_T *)(pAll + (sizeof(MEI_DYN_NFC_DATA_T) + (sizeof(MEI_DYN_NFC_DATA_T) & 0x3))) ;
   pNfcBuffAll    = pAll
      + ( sizeof(MEI_DYN_NFC_DATA_T) + (sizeof(MEI_DYN_NFC_DATA_T) & 0x3) )
      + ( nfcBufPerInst * (sizeof(MEI_RECV_BUF_T) + (sizeof(MEI_RECV_BUF_T) & 0x3)) );

   for (bufIdx = 0; bufIdx < nfcBufPerInst; bufIdx++)
   {
      pRecvDataCntrl[bufIdx].recvDataBuf_s.bufSize_byte = sizeof(MEI_MEI_MAILBOX_T);
      pRecvDataCntrl[bufIdx].recvDataBuf_s.pBuffer      = &pNfcBuffAll[bufIdx * nfcBufSize];
   }
   pDynNfc->numOfBuf = (IFX_uint8_t)nfcBufPerInst;

   /* chain the open instance to the VRX device list for NFC handling */
   pDynNfc->pRecvDataCntrl     = pRecvDataCntrl;
   pMeiDynCntrl->pInstDynNfc = pDynNfc;

   /* first: protect against other users */
   MEI_DRV_GET_UNIQUE_ACCESS(pMeiDev);

   MEI_AddNfcToDevList( pMeiDynCntrl, pDynNfc);

   MEI_DRV_RELEASE_UNIQUE_ACCESS(pMeiDev);

   return 0;
}

/**
   Cleanup/Disable the receive NFC part for this open instance.
   - remove the buffer from the device NFC list.
   - free the corresponding buffers and structs

\param
   pMeiDynCntrl   points to the dynamic data struct of the open instance.

\return
   IFX_SUCCESS

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\attention
   These function is also allowded before the device init ioctl(). So
   ensure that no MEI access will be done before the device init ioctl call.
   See: interrupt enable / disable
*/
IFX_int32_t MEI_IoctlNfcDisable(MEI_DYN_CNTRL_T *pMeiDynCntrl)
{
   MEI_DEV_T            *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_NFC_DATA_T   *pDynNfc;

   /* dechain the instance form the VRX device NFC list */
   if (pMeiDynCntrl->pInstDynNfc)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d-%02d]: disable NFC Feature" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));

      MEI_DRV_GET_UNIQUE_ACCESS(pMeiDev);

      MEI_RemoveNfcFromDevList(pMeiDynCntrl, &pDynNfc);

      MEI_DRV_RELEASE_UNIQUE_ACCESS(pMeiDev);

      /* free allocated mem */
      if (pDynNfc)
      {
         MEI_DRVOS_Free(pDynNfc);
         pDynNfc = IFX_NULL;
      }
   }
   else     /* if (pMeiDynCntrl->pDynNfc) {...} else ... */
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d-%02d]: WARNING - NFC Feature already disabled" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
   }        /* if (pMeiDynCntrl->pDynNfc) {...} else {...} */

   return IFX_SUCCESS;
}

/**
   Set the autonomous message control for this instance.
   Therefore the NFC receive part will be initialized if not already done.

\param
   pMeiDynCntrl    points to the dynamic data struct of the open instance.
\param
   pAutoMsgCtrl      points to the argument structure.

\return
   IFX_SUCCESS

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\attention
   These function is also allowded before the device init ioctl(). So
   ensure that no MEI access will be done before the device init ioctl call.
   See: interrupt enable / disable
*/
IFX_int32_t MEI_IoctlAutoMsgCtlSet(
                              MEI_DYN_CNTRL_T         *pMeiDynCntrl,
                              IOCTL_MEI_autoMsgCtrl_t *pAutoMsgCtrl)
{
   IFX_int32_t          ret;
   IFX_uint32_t         bufSize = sizeof(MEI_MEI_MAILBOX_T), numOfBuffer = MEI_MAX_RD_DEV_BUF_PER_DEV;
   MEI_DYN_NFC_DATA_T *pDynNfc   = pMeiDynCntrl->pInstDynNfc;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   /* set new autonomous message controls */
   pAutoMsgCtrl->modemMsgMask  &=
      (  MEI_DRV_MSG_CTRL_IF_MODEM_ALL_ON
      #if (MEI_DRV_ATM_OAM_ENABLE == 1)
       | MEI_DRV_MSG_CTRL_IF_MODEM_ATMOAM_CELL_ON
      #endif
      #if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
       | MEI_DRV_MSG_CTRL_IF_MODEM_EOC_FRAME_ON
      #endif
      );
   pAutoMsgCtrl->driverMsgMask &=
      (  MEI_DRV_MSG_CTRL_IF_DRIVER_ALL_ON
      #if (MEI_DRV_ATM_OAM_ENABLE == 1)
       | MEI_DRV_MSG_CTRL_IF_DRIVER_MODULE_REM_ATM_ON
      #endif
      #if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
       | MEI_DRV_MSG_CTRL_IF_DRIVER_MODULE_REM_EOC_ON
      #endif
      );

#  if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
   /* check for EOC feature change */
   if (pDynNfc)
   {
      if ( (pAutoMsgCtrl->modemMsgMask ^ pDynNfc->msgProcessCtrl) &
           MEI_DRV_MSG_CTRL_IF_MODEM_EOC_FRAME_ON )
      {
         /* reconfig of Clear EOC not allowed */
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d-%02d]: ERROR - reconfig of CEOC not allowed" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
         return -e_MEI_ERR_INVAL_CONFIG;
      }
   }
   else
   {
      if (pAutoMsgCtrl->modemMsgMask & MEI_DRV_MSG_CTRL_IF_MODEM_EOC_FRAME_ON)
      {
         bufSize     = sizeof(MEI_CEOC_MEI_EOC_FRAME_T);
         numOfBuffer = MEI_MAX_RD_DEV_BUF_PER_DEV;

         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d-%02d]: Msg control - EOC buffer size = %d (modem = 0x%X)" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                 bufSize, pAutoMsgCtrl->modemMsgMask));
      }
   }
#  endif

   if (!pDynNfc)
   {
      if ( (ret = MEI_IoctlNfcEnable(pMeiDynCntrl, numOfBuffer, bufSize)) != IFX_SUCCESS )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d-%02d]: ERROR - set autonomous msg control" MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
         return ret;
      }
      pDynNfc = pMeiDynCntrl->pInstDynNfc;
   }

   pDynNfc->msgProcessCtrl = pAutoMsgCtrl->modemMsgMask | (pAutoMsgCtrl->driverMsgMask << 16);

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
         ("MEI_DRV[%02d-%02d]: Set msg control - modem = 0x%X, drv = 0x%X (ctrl 0x%08X)" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
           pAutoMsgCtrl->modemMsgMask, pAutoMsgCtrl->driverMsgMask,
           pDynNfc->msgProcessCtrl));

   return IFX_SUCCESS;
}


/**
   Returns the current autonomous message control settings for this instance.

\param
   pMeiDynCntrl   points to the dynamic data struct of the open instance.
\param
   pAutoMsgCtrl      points to the argument structure.

\return
   IFX_SUCCESS

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\attention
   These function is also allowded before the device init ioctl(). So
   ensure that no MEI access will be done before the device init ioctl call.
   See: interrupt enable / disable
*/
IFX_int32_t MEI_IoctlAutoMsgCtlGet(
                              MEI_DYN_CNTRL_T         *pMeiDynCntrl,
                              IOCTL_MEI_autoMsgCtrl_t *pAutoMsgCtrl)
{
   MEI_DYN_NFC_DATA_T *pDynNfc   = pMeiDynCntrl->pInstDynNfc;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   if (!pDynNfc)
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
            ("MEI_DRV[%02d-%02d]: ERROR - get auto. msg control, not configured" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
      return -e_MEI_ERR_INVAL_CONFIG;
   }

   pAutoMsgCtrl->modemMsgMask  = pDynNfc->msgProcessCtrl;
   pAutoMsgCtrl->driverMsgMask = pDynNfc->msgProcessCtrl;

   return IFX_SUCCESS;
}



/**
   Add a new dynamic NFC receive struct to the device specific list.

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\param
   pMeiDynCntrl: Points to the VRX dynamic device control struct.
\param
   pVrxDynNfc:   Points to the new dynamic NFC handling struct.

\return
   - TRUE:  NFC struct added to the device.
   - FALSE: ERROR - not able to add NFC struct to the device.
*/
IFX_boolean_t MEI_AddNfcToDevList( MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                                     MEI_DYN_NFC_DATA_T *pVrxDynNfc)
{
   MEI_DEV_T          *pMeiDev;

   /*
      check params
   */
   if ( !pMeiDynCntrl || !pVrxDynNfc )
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV [??]: AddNfcToDevList - invalid params" MEI_DRV_CRLF) );
      return IFX_FALSE;
   }

   /* get VRX device struct */
   pMeiDev = pMeiDynCntrl->pMeiDev;

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV [%02d-%02d]: add the NFC struct to the device list" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance) );

   /*
      Add the NFC element to the end of the list.
   */
   pVrxDynNfc->pNext = NULL;

   if (pMeiDev->pRootNfcRecvLast)
   {
      /* list not empty */
      pVrxDynNfc->pPrev                = pMeiDev->pRootNfcRecvLast;
      pMeiDev->pRootNfcRecvLast->pNext = pVrxDynNfc;
      pMeiDev->pRootNfcRecvLast        = pVrxDynNfc;
   }
   else
   {
      /* list empty */
      pVrxDynNfc->pPrev          = NULL;
      pMeiDev->pRootNfcRecvLast  = pVrxDynNfc;
      pMeiDev->pRootNfcRecvFirst = pVrxDynNfc;
   }

   return IFX_TRUE;
}

/**
   Remove a dynamic NFC receive struct from the device specific list.

\remarks
   To allow receiving notifications from the VRX device the current
   driver instance must be able to handle and store the incoming messages.
   The required buffers must be set in a "per open instance" way.
   Therefore the device instance manages a list of all open instances which are
   enabled to receive incoming NFC's

\param
   pMeiDynCntrl: Points to the VRX dynamic device control struct.
\param
   pVrxDynNfc:   Points to the new dynamic NFC handling struct.

\return
   - TRUE:  NFC struct added to the device.
   - FALSE: ERROR - not able to add NFC struct to the device.
*/
IFX_boolean_t MEI_RemoveNfcFromDevList( MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                                              MEI_DYN_NFC_DATA_T **ppVrxDynNfc)
{
   MEI_DEV_T          *pMeiDev;
   MEI_DYN_NFC_DATA_T *pDynNfc;

   /*
      check params, check NFC enabled
   */
   if ( !pMeiDynCntrl || !ppVrxDynNfc )
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV [??]: RemoveNfcFromDevList - invalid params" MEI_DRV_CRLF) );

      return IFX_FALSE;
   }

   /* get VRX device struct */
   pMeiDev = pMeiDynCntrl->pMeiDev;

   if ( (pDynNfc = pMeiDynCntrl->pInstDynNfc) == NULL )
   {
      /* NFC struct not available (not enabled) */
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
             ("MEI_DRV [%02d-%02d]: RemoveNfcFromDevList - no NFC struct" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance) );

      return IFX_FALSE;
   }

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV [%02d-%02d]: remove the NFC struct from the device list" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance) );

   /*
      Dechain the NFC element from the list.
   */
   if (pDynNfc->pPrev != NULL)
   {
      pDynNfc->pPrev->pNext = pDynNfc->pNext;
   }
   else
   {
      /* begin of list */
      pMeiDev->pRootNfcRecvFirst = pDynNfc->pNext;
   }

   if (pDynNfc->pNext != NULL)
   {
      pDynNfc->pNext->pPrev = pDynNfc->pPrev;
   }
   else
   {
      /* end of list */
      pMeiDev->pRootNfcRecvLast = pDynNfc->pPrev;
   }

   /* remove it form dynamic control struct and return it back */
   pMeiDynCntrl->pInstDynNfc = NULL;
   *ppVrxDynNfc = pDynNfc;

   return IFX_TRUE;
}

/**
   Write a message and check for the ack.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pDynCmd          - points to the instanc write data
\param
   pMbMsg           - points to the already prepared mailbox message.
\param
   modemMsgSize     - size of the modem message.

\return
   IFX_SUCCESS if send was successful
   IFX_ERROR   in case of error.

\remaks
   Already in modem message format.

*/
IFX_int32_t MEI_WriteMsgAndCheck(
                              MEI_DYN_CNTRL_T    *pMeiDynCntrl,
                              MEI_DYN_CMD_DATA_T *pDynCmd,
                              MEI_MEI_MAILBOX_T  *pMbMsg,
                              IFX_int32_t          modemMsgSize)
{
   CMV_STD_MESSAGE_T    *pAckMsg;
   IFX_int32_t          ackSize;


   if ( (MEI_WriteMailbox( pMeiDynCntrl, pDynCmd
                            ,pMbMsg
                            ,modemMsgSize)) <= 0 )
   {
      return IFX_ERROR;
   }

   ackSize = MEI_CheckAck(pMeiDynCntrl);
   if (ackSize >= (int)(CMV_HEADER_8BIT_SIZE))
   {
      pAckMsg = (CMV_STD_MESSAGE_T *)pDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer;

      if ( (P_CMV_MSGHDR_FCT_OPCODE_GET(pAckMsg)) & 0x00F0 )
      {
         ackSize = IFX_ERROR;
      }
      else
      {
         ackSize = IFX_SUCCESS;
      }
   }
   else
   {
      ackSize = IFX_ERROR;
   }

   MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_FREE);
   pDynCmd->cmdAckCntrl.msgLen = 0;
   pDynCmd->cmdAckCntrl.bufCtrl = MEI_RECV_BUF_CTRL_FREE;

   return ackSize;
}


/* ============================================================================
   ioctl Message Function definitions
   ========================================================================= */


/**
   Write a message (IFX format) to the VRX device.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: Number of written bytes (message size: header + payload)
   Error:
      -e_MEI_ERR_MSG_PARAM: invalid message parameter
      -e_MEI_ERR_GET_ARG:   cannot get arguments
*/
IFX_int32_t MEI_IoctlCmdMsgWrite(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)
{
   int total = 0, cmvMbSize;
   unsigned char *pDestPtr;

   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_CMD_DATA_T *pDynCmd   = pMeiDynCntrl->pInstDynCmd;

   MEI_MEI_MAILBOX_T     *pMailbox;

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
         ("MEI_DRV[%02d - %02d]: MEI_IoctlCmdMsgWrite(0x%04X) - count %d" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, pUserMsg->msgId, pUserMsg->paylSize_byte));

   /* ===============================================
      check if the instance specific buffer is free
      =============================================== */
   MEI_WaitForInstance(pMeiDynCntrl, pMeiDev, pDynCmd);

   /* ===============================================
      now the local instance struct is free
      =============================================== */
   pMailbox = (MEI_MEI_MAILBOX_T *)pDynCmd->cmdWrBuf.pBuffer;

   /*
      modem message
      - index and length fields are part of the appl. payload
   */
   cmvMbSize = CMV_HEADER_8BIT_SIZE +
                  pUserMsg->paylSize_byte - (sizeof(IFX_uint16_t) * 2);
   pDestPtr = (unsigned char *)&pMailbox->mbCmv.cmv.header.index;

   if ( cmvMbSize > (int)(sizeof(MEI_CMV_MAILBOX_T)) )
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d - %02d]: Error WriteMsg - invalid mb msg size (%d)" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, cmvMbSize));
      total = -e_MEI_ERR_MSG_PARAM;
      goto MEI_WRITE_MSG_ERROR;
   }

   /* copy payload data to driver internal buffer */
   if (pUserMsg->paylSize_byte)
   {
      if (bInternCall)
      {
         memcpy(pDestPtr, pUserMsg->pPayload, pUserMsg->paylSize_byte);
      }
      else
      {
         if ( (MEI_DRVOS_CpyFromUser( pDestPtr, pUserMsg->pPayload,
                                      pUserMsg->paylSize_byte)) == IFX_NULL )
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: write - copy_from_user() failed!" MEI_DRV_CRLF,
                   MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
            total = -e_MEI_ERR_GET_ARG;
            goto MEI_WRITE_MSG_ERROR;
         }
      }

      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
            ("MEI_DRV[%02d - %02d]: WriteMsg - idx = 0x%x, len = 0x%x payl[0] 0x%X!" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
             pMailbox->mbCmv.cmv.header.index,
             pMailbox->mbCmv.cmv.header.length,
             pMailbox->mbCmv.cmv.payload.params_32Bit[0]));
   }

   /* set the CMV header */
   if ( (MEI_SetCmvHeader(pMeiDev, &pMailbox->mbCmv.cmv , pUserMsg)) == IFX_ERROR )
   {
      total = -e_MEI_ERR_MSG_PARAM;
      goto MEI_WRITE_MSG_ERROR;
   }

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV[%02d - %02d]: write(.., .., %d) to mailbox" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, cmvMbSize));

   #if (MEI_SUPPORT_DRV_LOOPS == 1)
   if (pMeiDev->bDrvLoop)
      total = MEI_WriteMailboxLoopBack( pMeiDynCntrl, pMeiDynCntrl->pInstDynCmd,
                                              pMailbox, cmvMbSize);
   else
   #endif
      total = MEI_WriteMailbox( pMeiDynCntrl, pMeiDynCntrl->pInstDynCmd,
                                  pMailbox, cmvMbSize);

MEI_WRITE_MSG_ERROR:

   if (total < 0)
   {
      pUserMsg->paylSize_byte = 0;
      return total;
   }

   return IFX_SUCCESS;
}


/**
   Reads an ACK msg from the device (IFX format).

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pUserMsg       - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   Success: Number of read payload bytes
   Error:
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   negative acknowledge
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.
*/
IFX_int32_t MEI_IoctlAckMsgRead(
                              MEI_DYN_CNTRL_T     *pMeiDynCntrl,
                              IOCTL_MEI_message_t *pUserMsg,
                              IFX_boolean_t         bInternCall)
{
   int len = 0, paylSize_byte;
   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_CMD_DATA_T *pDynCmd   = pMeiDynCntrl->pInstDynCmd;
   MEI_MEI_MAILBOX_T  *pMailbox  = (MEI_MEI_MAILBOX_T *)pDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer;
   unsigned char        *pSource;

   len = MEI_CheckAck(pMeiDynCntrl);

   /* expect at least the header */
   if (len >= (int)(CMV_HEADER_8BIT_SIZE))
   {
      /* set return parameter */
      pUserMsg->msgId = CMV_MSGHDR_MSGID_GET(pMailbox->mbCmv.cmv);
      pUserMsg->msgClassifier = ( pMailbox->mbCmv.cmv.header.mbxCode & 0x00FF) |
                                ( (CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv)) << 8);

      /*
         Return IFX/modem message
         - index and length fields becomes part of the appl. payload
      */
      pSource = (unsigned char *)&pMailbox->mbCmv.cmv.header.index;

      /* size field contains number of 16 bit payload elements of the message */
      paylSize_byte = (CMV_MSGHDR_PAYLOAD_SIZE_GET(pMailbox->mbCmv.cmv)) << CMV_MSG_BIT_SIZE_16BIT;

      /* check msg length against the received size */
      if ( (len - (int)(CMV_HEADER_8BIT_SIZE)) < paylSize_byte )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
               ("MEI_DRV[%02d - %02d]: WARNING - ReadAck[0x%04X], size missmatch len (%d - %d) / payl (%d)" MEI_DRV_CRLF
               , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
               , pUserMsg->msgId
               , len, CMV_HEADER_8BIT_SIZE, paylSize_byte));

         MEI_TRACE_CMV_MSG( pMeiDev, &pMailbox->mbCmv.cmv,
                                  "ReadAck, size missmatch", MEI_DRV_PRN_LEVEL_HIGH);

         paylSize_byte = len - CMV_HEADER_8BIT_SIZE;
      }

      /* ====================================================================
         Return the payload
         - check if a user buffer is available (the user wants to catch the ACK)
         - set the return values
         ==================================================================== */

      /* copy payload to user space */
      if (pUserMsg->pPayload != NULL)
      {
         /* add index and length field to the return field */
         paylSize_byte += ( sizeof(pMailbox->mbCmv.cmv.header.index) +
                            sizeof(pMailbox->mbCmv.cmv.header.length) );

         /* check return part against the buffer size */
         if (paylSize_byte > (int)pUserMsg->paylSize_byte)
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_WRN,
                  ("MEI_DRV[%02d - %02d]: WARNING - "
                  "ReadAck[0x%04X] size missmatch recv payl (+idx + len) (%d) / usr buf (%d)" MEI_DRV_CRLF
                  , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                  , pUserMsg->msgId
                  , paylSize_byte, pUserMsg->paylSize_byte));

            paylSize_byte = pUserMsg->paylSize_byte;
         }

         if (bInternCall)
         {
            memcpy(pUserMsg->pPayload, pSource, paylSize_byte);
            pUserMsg->paylSize_byte = paylSize_byte;
         }
         else
         {
            if ( (MEI_DRVOS_CpyToUser( pUserMsg->pPayload,
                                       pSource, paylSize_byte) ) == IFX_NULL )
            {
               PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                     ("MEI_DRV[%02d - %02d]: ReadAck[0x%04X] - CopyToUser(..,..,%d) failed!" MEI_DRV_CRLF
                      , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                      , pUserMsg->msgId, paylSize_byte));

               len = -e_MEI_ERR_RETURN_ARG;
               pUserMsg->paylSize_byte = 0;
            }
            else
            {
               pUserMsg->paylSize_byte = paylSize_byte;
            }
         }
      }
      else     /* if (pUserMsg->pPayload != NULL) */
      {
         /* there is no user buffer - check if payload is available */
         if ( paylSize_byte > 0 )
         {
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: WARNING - "
                  "ReadAck[0x%04X] discard payload: [index + len + %d]" MEI_DRV_CRLF
                  , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                  , pUserMsg->msgId, paylSize_byte));
         }
         else
         {
            PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
                  ("MEI_DRV[%02d - %02d]: INFO - "
                  "ReadAck[0x%04X] discard default payload: [index + len]" MEI_DRV_CRLF
                  , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                  , pUserMsg->msgId));
         }

         pUserMsg->paylSize_byte =   paylSize_byte
                                   + sizeof(pMailbox->mbCmv.cmv.header.index)
                                   + sizeof(pMailbox->mbCmv.cmv.header.length);
      }

      if ( (CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv)) & 0x00F0 )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ReadAck[0x%04X] - FctOP ERROR 0x%02X!" MEI_DRV_CRLF
                , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance
                , pUserMsg->msgId
                , CMV_MSGHDR_FCT_OPCODE_GET(pMailbox->mbCmv.cmv) ));

         len = -e_MEI_ERR_DEV_NEG_RESP;
      }
   }
   else
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d - %02d]: ERROR - ReadAck invalid msg (size %d)"
             "[0x%04X 0x%04X 0x%04X 0x%04X ...]" MEI_DRV_CRLF
             , MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, len
             , (len > 1) ? pMailbox->mbRaw.rawMsg[0] : 0xFFFF
             , (len > 3) ? pMailbox->mbRaw.rawMsg[1] : 0xFFFF
             , (len > 5) ? pMailbox->mbRaw.rawMsg[2] : 0xFFFF
             , (len > 7) ? pMailbox->mbRaw.rawMsg[3] : 0xFFFF ));

      len = -e_MEI_ERR_DEV_INVAL_RESP;
      pUserMsg->paylSize_byte = 0;
   }     /* if (len >= CMV_HEADER_8BIT_SIZE) */

   /* free dynamic ACK buffer */
   MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_FREE);
   pDynCmd->cmdAckCntrl.msgLen  = 0;
   pDynCmd->cmdAckCntrl.bufCtrl = MEI_RECV_BUF_CTRL_FREE;

   return len;
}


/**
   Write a message to the MEI CPE device and wait for the corresponding ACK.

\param
   pMeiDynCntrl - private dynamic device data (per open instance)
\param
   pUserMsgs      - points to the user message information data
\param
   bInternCall    - indicates if the call is form the internal interface
                    (image and data already in kernel space)

\return
   0 if success
   negative value in case of error.

*/
IFX_int32_t MEI_IoctlMsgSend(
                              MEI_DYN_CNTRL_T          *pMeiDynCntrl,
                              IOCTL_MEI_messageSend_t  *pUserMsgs,
                              IFX_boolean_t              bInternCall)
{
   int ret_wr, ret_rd;

#if (MEI_SUPPORT_TIME_TRACE == 1)
   IFX_uint32_t sendStart = 0, sendEnd = 0;
#endif

   if ((MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev) != e_MEI_DRV_STATE_DFE_READY) &&
       (MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev) != e_MEI_DRV_STATE_DFE_RESET))
   {
      /* invalid state */
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
         ("MEI_DRV[%02d]: ERROR message send - invalid drv state %d" MEI_DRV_CRLF,
          MEI_DRV_LINENUM_GET(pMeiDynCntrl->pMeiDev),
          MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev)));

      return -e_MEI_ERR_INVAL_STATE;
   }

   /* check and do pre-work before sending this message */
   MEI_MsgSendPreAction(pMeiDynCntrl, pUserMsgs, bInternCall);

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
       ("MEI_DRV[%02d - %02d]: Send Msg" MEI_DRV_CRLF,
          MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance) );


   /* lock the the current instance */
   MEI_DRVOS_SemaphoreLock(&pMeiDynCntrl->pInstanceRWlock);

   MEI_GET_TICK_MS_TIME_TRACE(sendStart);

   /*
      write a message to the device
   */
   ret_wr = MEI_IoctlCmdMsgWrite( pMeiDynCntrl, &pUserMsgs->write_msg, bInternCall);
   if (ret_wr < 0)
   {
#if (MEI_SUPPORT_TIME_TRACE == 1)
      MEI_GET_TICK_MS_TIME_TRACE(sendEnd);

#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] write failed "\
              "(cnt: %d, t: %d - %d = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              pMeiDynCntrl->pMeiDev->statistics.sendMsgCount,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#else
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] write failed "\
              "(t: %d - %d = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#endif
#else
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] write failed "\
              "(ret = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId, ret_wr));
#endif

      /* set indication into the corresponding struct field */
      pUserMsgs->write_msg.ictl.retCode = ret_wr;

      /* Unlock the the current instance */
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);

      return ret_wr;
   }

   /* write successful --> set number of written 16bit units into the struct */
   pUserMsgs->write_msg.ictl.retCode = IFX_SUCCESS;

   /*
      read the corresponding ACK
   */
   ret_rd = MEI_IoctlAckMsgRead( pMeiDynCntrl, &pUserMsgs->ack_msg, bInternCall);
   if (ret_rd < 0)
   {
#if (MEI_SUPPORT_TIME_TRACE == 1)
      MEI_GET_TICK_MS_TIME_TRACE(sendEnd);

#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] read ACK failed "\
              "(cnt: %d, t: %d - %d = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              pMeiDynCntrl->pMeiDev->statistics.sendMsgCount,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#else
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] read ACK failed "\
              "(t: %d - %d = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#endif
#else
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
             ("MEI_DRV[%02d - %02d]: Send Msg[0x%04X] read ACK failed "\
              "(ret = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId, ret_rd));
#endif

      /* set indication into the corresponding struct field */
      pUserMsgs->ack_msg.paylSize_byte = 0;
      pUserMsgs->ack_msg.ictl.retCode = ret_rd;
      /* Unlock the the current instance */
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
      return ret_rd;
   }

   /*
      number of read bytes is already set in structure
   */
   pUserMsgs->ack_msg.ictl.retCode = IFX_SUCCESS;

#if (MEI_SUPPORT_TIME_TRACE == 1)
   MEI_GET_TICK_MS_TIME_TRACE(sendEnd);

   if (MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd) > 100)
   {
#if (MEI_SUPPORT_STATISTICS == 1)
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
             ("MEI_DRV[%02d - %02d]: Send msg[0x%04X] - "\
              "cnt: %d time: %d - %d = %d" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              pMeiDynCntrl->pMeiDev->statistics.sendMsgCount,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#else
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
             ("MEI_DRV[%02d - %02d]: Send msg[0x%04X] - "\
              "time: %d - %d = %d" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              sendStart, sendEnd, MEI_TIME_TRACE_GET_TICK_MS(sendStart, sendEnd)));
#endif
   }
#else
   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
             ("MEI_DRV[%02d - %02d]: Send msg[0x%04X] - "\
              "written %d, ack read %d [byte]" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance,
              pUserMsgs->write_msg.msgId,
              ret_wr, ret_rd));
#endif

   /* Unlock the the current instance */
   MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
   return 0;
}


/**
   Reads a NFC msg from the device (IFX format).

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pUserMsg       - points to the user message information data

\return
   Success: length of the message and number of payload bytes
   Error:   negative value
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   not successful functional operation code
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
IFX_int32_t MEI_IoctlNfcMsgRead(
                              MEI_DYN_CNTRL_T       *pMeiDynCntrl,
                              IOCTL_MEI_message_t   *pUserMsg,
                              IFX_boolean_t           bInternCall)

{
   IFX_int32_t      ret = 0, recvLen = 0;
   IFX_uint8_t      rdIdxRd;
   MEI_RECV_BUF_T *pRecvDataCntrl = IFX_NULL;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   /* Lock the the current instance for mark the NFC */
   MEI_DRVOS_SemaphoreLock(&pMeiDynCntrl->pInstanceRWlock);

   if (!pMeiDynCntrl->pInstDynNfc)
   {
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
      return IFX_ERROR;
   }

   rdIdxRd = pMeiDynCntrl->pInstDynNfc->rdIdxRd;

   /* check for new a NFC message and get the buffer pointer */
   recvLen = MEI_GetNextRdNfcMsg( pMeiDynCntrl, &pRecvDataCntrl, rdIdxRd);
   if (pRecvDataCntrl)
   {
      switch (pRecvDataCntrl->bufCtrl & ~(MEI_RECV_BUF_CTRL_LOCKED))
      {
         case MEI_RECV_BUF_CTRL_MODEM_NFC_MSG:
            ret = MEI_ModemNfcRead(pMeiDynCntrl, pRecvDataCntrl, pUserMsg, bInternCall);
            break;

         case MEI_RECV_BUF_CTRL_DRIVER_MSG:
            ret = MEI_DriverMsgRead(pMeiDynCntrl, pRecvDataCntrl, pUserMsg, bInternCall);
            break;

#if (MEI_DRV_ATM_OAM_ENABLE == 1)
         case MEI_RECV_BUF_CTRL_MODEM_ATMOAM_CELL:
            ret = MEI_AtmOamMsgRead(pMeiDynCntrl, pRecvDataCntrl, pUserMsg, bInternCall);
            break;
#endif

#if (MEI_DRV_CLEAR_EOC_ENABLE == 1)
         case MEI_RECV_BUF_CTRL_MODEM_EOC_FRAME:
            ret = MEI_CEocMsgRead(pMeiDynCntrl, pRecvDataCntrl, pUserMsg, bInternCall);
            break;
#endif

         default:
            PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
                  ("MEI_DRV[%02d - %02d]: MEI_IoctlNfcMsgRead() - invalid buffer 0x%08X" MEI_DRV_CRLF,
                    MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, pRecvDataCntrl->bufCtrl));

            ret = IFX_ERROR;
            break;
      }

      /* release current message */
      MEI_FreeNextRdNfcMsg(pMeiDynCntrl, rdIdxRd);
   }
   else
   {
      pUserMsg->paylSize_byte = 0;
   }

   MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);

   return ret;
}

#if (MEI_SUPPORT_RAW_MSG == 1)

/**
   Write a raw message to the VRX device.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pMsg           - points to the raw user message
\param
   msgCount_16Bit - message size in 16 bit units.

*/
IFX_int32_t MEI_IoctlRawMsgWrite( MEI_DYN_CNTRL_T *pMeiDynCntrl,
                                        IFX_uint16_t *pMsg, IFX_int32_t  msgCount_16Bit)
{
   int total = 0, cmvMbSize;
   IFX_uint16_t *pDestPtr;

   MEI_DEV_T          *pMeiDev = pMeiDynCntrl->pMeiDev;
   MEI_DYN_CMD_DATA_T *pDynCmd   = pMeiDynCntrl->pInstDynCmd;

   MEI_MEI_MAILBOX_T     *pMailbox;

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_NORMAL, MEI_DRV_LINENUM_GET(pMeiDev),
         ("MEI_DRV[%02d - %02d]: MEI_IoctlRawMsgWrite() - count %d" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, msgCount_16Bit));

   /* ===============================================
      check if the instance specific buffer is free
      =============================================== */
   MEI_DRV_GET_UNIQUE_ACCESS(pMeiDev);

   if (MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) != e_MEI_MB_BUF_FREE)
   {
      if (pMeiDev->pCurrDynCmd == pDynCmd)
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: MEI_IoctlRawMsgWrite - reset dyn recv (activ), state = %d " MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                 MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) ));

         pMeiDev->pCurrDynCmd = NULL;
      }
      else
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: MEI_IoctlRawMsgWrite - reset dyn recv (passiv), state = %d " MEI_DRV_CRLF,
                 MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                 MEI_DRV_DYN_MBBUF_STATE_GET(pDynCmd) ));
      }
      MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_FREE);
   }
   MEI_DRV_RELEASE_UNIQUE_ACCESS(pMeiDev);

   /* ===============================================
      now the local instance struct is free
      =============================================== */

   /* copy data to driver space */
   pMailbox = (MEI_MEI_MAILBOX_T *)pDynCmd->cmdWrBuf.pBuffer;


   /* set mailbox message size */
   cmvMbSize = msgCount_16Bit << 1;
   pDestPtr = pMailbox->mbRaw.rawMsg;

   if ( cmvMbSize > (int)(sizeof(MEI_CMV_MAILBOX_T)) )
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d - %02d]: Error WriteMsg - invalid mb msg size (%d)" MEI_DRV_CRLF,
              MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, cmvMbSize));
      total = -e_MEI_ERR_MSG_PARAM;
      goto MEI_WRITE_MSG_ERROR;
   }

   if ( (MEI_DRVOS_CpyFromUser(pDestPtr, pMsg, cmvMbSize)) == IFX_NULL)
   {
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
            ("MEI_DRV[%02d - %02d]: write - copy_from_user() failed!" MEI_DRV_CRLF,
             MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
      total = -e_MEI_ERR_GET_ARG;
      goto MEI_WRITE_MSG_ERROR;
   }

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_LINENUM_GET(pMeiDev),
          ("MEI_DRV[%02d - %02d]: write(.., .., %d) to mailbox" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, cmvMbSize));

   #if (MEI_SUPPORT_DRV_LOOPS == 1)
   if (pMeiDev->bDrvLoop)
      total = MEI_WriteMailboxLoopBack( pMeiDynCntrl, pMeiDynCntrl->pInstDynCmd,
                                              pMailbox, cmvMbSize);
   else
   #endif
      total = MEI_WriteMailbox( pMeiDynCntrl, pMeiDynCntrl->pInstDynCmd,
                                      pMailbox, cmvMbSize);

MEI_WRITE_MSG_ERROR:

   return total;
}


/**
   Reads an ACK msg from the device.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pMsgAck        - points to the raw user ACK message
\param
   msgCount_16Bit - message size in 16 bit units.

\return
   Success: Number of read payload bytes
   Error:
      -e_MEI_ERR_RETURN_ARG:     cannot return arguments
      -e_MEI_ERR_DEV_NEG_RESP:   negative acknowledge
      -e_MEI_ERR_DEV_INVAL_RESP: invalid response.

*/
IFX_int32_t MEI_IoctlRawAckRead( MEI_DYN_CNTRL_T *pMeiDynCntrl,
                                       IFX_uint16_t *pMsgAck, IFX_int32_t msgCount_16Bit)
{
   int len = 0;
   MEI_DYN_CMD_DATA_T *pDynCmd   = pMeiDynCntrl->pInstDynCmd;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   len = MEI_CheckAck(pMeiDynCntrl);

   /* expect at least the header */
   if (len >= (int)(CMV_HEADER_8BIT_SIZE))
   {
      if (len > (msgCount_16Bit << 1) )
      {
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: Warning ReadAck - data lost (msg[%d] --> buf[%d])" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance,
                len, msgCount_16Bit << 1));
         len = msgCount_16Bit << 1;
      }

      if ( (MEI_DRVOS_CpyToUser(pMsgAck, pDynCmd->cmdAckCntrl.recvDataBuf_s.pBuffer, len)) == IFX_NULL)
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: ReadAck - copy_to_user() failed!" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));
         /* error during copy */
         len = -e_MEI_ERR_RETURN_ARG;
      }
   }

   /* free dynamic ACK buffer */
   MEI_DRV_DYN_MBBUF_STATE_SET(pDynCmd, e_MEI_MB_BUF_FREE);
   pDynCmd->cmdAckCntrl.msgLen = 0;
   pDynCmd->cmdAckCntrl.bufCtrl = MEI_RECV_BUF_CTRL_FREE;

   return (IFX_int32_t)len;
}


/**
   Write a raw message to the VRX device and wait for the corresponding ACK.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pMBoxSend      - points to the user messages information


*/
IFX_int32_t MEI_IoctlRawMsgSend( MEI_DYN_CNTRL_T *pMeiDynCntrl,
                                      IOCTL_MEI_mboxSend_t *pMBoxSend)
{
   int ret_wr, ret_rd;


   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
         ("MEI_DRV[%02d - %02d]: Send Msg (raw)" MEI_DRV_CRLF,
          MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance) );

   if ((MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev) != e_MEI_DRV_STATE_DFE_READY) &&
       (MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev) != e_MEI_DRV_STATE_DFE_RESET))
   {
      /* invalid state */
      PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
         ("MEI_DRV[%02d]: ERROR message raw send - invalid drv state %d" MEI_DRV_CRLF,
           MEI_DRV_LINENUM_GET(pMeiDynCntrl->pMeiDev),
           MEI_DRV_STATE_GET(pMeiDynCntrl->pMeiDev)));

      return -e_MEI_ERR_INVAL_STATE;
   }

   /* Lock the the current instance */
   MEI_DRVOS_SemaphoreLock(&pMeiDynCntrl->pInstanceRWlock);

   /*
      write a message to the device
   */
   ret_wr = MEI_IoctlRawMsgWrite( pMeiDynCntrl,
                                        pMBoxSend->write_msg.pData_16,
                                        pMBoxSend->write_msg.count_16bit);
   if (ret_wr <= 0)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
             ("MEI_DRV[%02d - %02d]: Send Msg (raw) write failed (ret = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance, ret_wr));

      /* set indication into the corresponding struct field */
      pMBoxSend->write_msg.count_16bit = 0;
      /* Unlock the the current instance */
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
      return ret_wr;
   }

   /* write successful --> set number of written 16bit units into the struct */
   pMBoxSend->write_msg.count_16bit = ret_wr >> 1;

   /*
      read the corresponding ACK
   */
   ret_rd = MEI_IoctlRawAckRead( pMeiDynCntrl,
                                       pMBoxSend->ack_msg.pData_16,
                                       pMBoxSend->ack_msg.count_16bit);
   if (ret_rd < 0)
   {
      PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
             ("MEI_DRV[%02d - %02d]: Send Msg (raw) read ACK failed (ret = %d)" MEI_DRV_CRLF,
              MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance, ret_rd));

      /* set indication into the corresponding struct field */
      pMBoxSend->ack_msg.count_16bit = 0;
      /* Unlock the the current instance */
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
      return ret_rd;
   }

   /* read ack successful --> set number of read 16bit units into the struct */
   pMBoxSend->ack_msg.count_16bit = ret_rd >> 1;

   /* Unlock the the current instance */
   MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);

   PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_LOW, MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl),
          ("MEI_DRV[%02d - %02d]: send msg (raw) - written %d, ack read %d [byte]" MEI_DRV_CRLF,
           MEI_DRV_DYN_LINENUM_GET(pMeiDynCntrl), pMeiDynCntrl->openInstance, ret_wr, ret_rd));

   return 0;
}


/**
   Reads a NFC raw msg from the device.

\param
   pMeiDynCntrl   - private dynamic device data (per open instance)
\param
   pMBoxNfc       - points to the raw user ACK message
\param
   msgCount_16Bit - message size in 16 bit units.

*/
IFX_int32_t MEI_IoctlRawNfcRead( MEI_DYN_CNTRL_T *pMeiDynCntrl,
                                   IFX_uint16_t *pMBoxNfc,
                                   IFX_int32_t msgCount_16Bit)
{
   IFX_int32_t      ret = IFX_SUCCESS;
   IFX_uint32_t     msgSize = 0;
   IFX_uint8_t      rdIdxRd;
   MEI_RECV_BUF_T *pRecvDataCntrl = IFX_NULL;
   MEI_DRV_PRN_LOCAL_VAR_CREATE(MEI_DEV_T, *pMeiDev, pMeiDynCntrl->pMeiDev);

   /* Lock the the current instance for mark the NFC */
   MEI_DRVOS_SemaphoreLock(&pMeiDynCntrl->pInstanceRWlock);

   if (!pMeiDynCntrl->pInstDynNfc)
   {
      MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);
      return IFX_ERROR;
   }

   rdIdxRd = pMeiDynCntrl->pInstDynNfc->rdIdxRd;


   /* check for new a NFC message and get the buffer pointer */
   if ( (ret = MEI_GetNextRdNfcMsg( pMeiDynCntrl, &pRecvDataCntrl, rdIdxRd)) > 0 )
   {
      msgSize = pRecvDataCntrl->msgLen;
      /* message available - with mailbox code */
      if (msgSize > (IFX_uint32_t)(msgCount_16Bit << 1))
      {
         PRN_DBG_USR( MEI_DRV, MEI_DRV_PRN_LEVEL_HIGH, MEI_DRV_LINENUM_GET(pMeiDev),
               ("MEI_DRV[%02d - %02d]: Warning read NFC - data lost (max %d, avail %d)" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance, msgCount_16Bit << 1, msgSize));

         msgSize = msgCount_16Bit << 1;
      }

      /* return message */
      if ( (MEI_DRVOS_CpyToUser(pMBoxNfc, pRecvDataCntrl->recvDataBuf_s.pBuffer, msgSize)) == IFX_NULL )
      {
         PRN_ERR_USR_NL( MEI_DRV, MEI_DRV_PRN_LEVEL_ERR,
               ("MEI_DRV[%02d - %02d]: read NFC - copy_to_user() failed!" MEI_DRV_CRLF,
                MEI_DRV_LINENUM_GET(pMeiDev), pMeiDynCntrl->openInstance));

         /* error during copy */
         ret = -e_MEI_ERR_RETURN_ARG;
      }

      /* release current message */
      MEI_FreeNextRdNfcMsg(pMeiDynCntrl, rdIdxRd);
   }

   MEI_DRVOS_SemaphoreUnlock(&pMeiDynCntrl->pInstanceRWlock);

   return ret;
}

#endif      /* #if (MEI_SUPPORT_RAW_MSG == 1) */

