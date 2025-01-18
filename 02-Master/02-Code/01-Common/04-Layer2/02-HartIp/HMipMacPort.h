/*
 *          File: HMipMacPort.h 
 *                The interface to the MAC port is relatively small
 *                and can be defined generically. However, the implementation
 *                depends on the hardware and software environment.
 *                That's why there is only a header at this point, while
 *                the file HIPMMacPort.cpp can be found in the specific branch.
 *                The present module is executed by the fast cyclic handler.
 *
 *        Author: Walter Borst
 *
 *        E-Mail: info@borst-automation.de
 *          Home: https://www.borst-automation.de
 *
 * No Warranties: https://www.borst-automation.com/legal/warranty-disclaimer
 *
 * Copyright 2006-2024 Walter Borst, Cuxhaven, Germany
 */

// Once
#ifndef __hmipmacport_h__
#define __hmipmacport_h__

#include "OSAL.h"
#include "WbHartUser.h"

class CHMipMacPort
{
public:
    // Enum classes
    enum class EN_Status : TY_Byte
    {
        IDLE = 0,
        INITIALIZING = 1,
        WAIT_CONNECT = 2,
        CLIENT_READY = 3,
        WAIT_TX_END = 4,
        WAIT_NETWORK_RESPONSE = 5,
        SHUTTING_DOWN = 6
    };

    enum class EN_ToDo : TY_Byte
    {
        NOTHING = 0,
        CONNECT = 1,
        DISCONNECT = 2,
        SEND_DATA = 3,
        RECEIVE_ENABLE = 4, // ??
        RECEIVE_DISABLE = 5 // ??
    };

    enum class EN_LastError : TY_Byte
    {
        NONE = 0,
        INITIALIZING = 1,
        GET_ADDR_INFO = 2,
        CREATE_SOCKET = 3,
        NO_SERVER = 4,
        TX_FAILED = 5,
        SHUTDOWN = 6,
        RECEIVING = 7
    };

    enum class EN_MessageType : TY_Byte
    {
        EMPTY_NETWORK_REQUEST = 0,
        EMPTY_NETWORK_RESPONSE = 1,
        NETWORK_REQUEST = 2,
        NETWORK_RESPONSE = 3,
        NETWORK_BURST = 4,
        FAILED = 5
    };

    static EN_Bool                 Open(TY_Byte* host_name_, TY_Word port_, EN_CommType type_);
    static void                   Close();
    static void                 Execute(TY_Word time_ms_);
    static void                    Init();
    static TY_Word            GetStatus();
    static TY_Word       GetMagicNumber();
    static TY_Byte       GetMessageType();
    static void          SetMessageType(TY_Byte hart_ip_msg_type_);
    static TY_Word    GetSequenceNumber();
    static void    GetIpFrameForMonitor(TY_Byte* dst_, TY_Byte* dst_len_, TY_Byte* src_, TY_Byte src_len);
    static TY_Word    GetAdditionalData(TY_Byte* data_);

private:
    // Data
    static TY_Byte        m_rcv_buf[MAX_IP_TXRX_SIZE];
    static int            m_rcv_len;
    static TY_Byte        m_tx_buf[MAX_IP_TXRX_SIZE];
    static int            m_tx_len;
    static TY_Byte        m_hart_ip_data[MAX_IP_TXRX_SIZE];
    static TY_Word        m_hart_ip_len;
    static EN_LastError   m_last_error;
    static TY_Byte        m_rx_err;
    static EN_MessageType m_last_message_type;
    static EN_ToDo        m_to_do;
    static EN_Bool        m_close_request;
    static TY_Byte        m_hart_rx_data[MAX_TXRX_SIZE];
    static TY_Byte        m_hart_rx_len;
    static TY_Byte        m_hart_ip_version;
    static TY_Byte        m_hart_ip_message_type;
    static TY_Byte        m_hart_ip_message_id;
    static TY_Byte        m_hart_ip_comm_status;
    static TY_Word        m_hart_ip_sequence_number;
    static TY_Word        m_hart_ip_byte_count;
    static TY_Word        m_magic_number;
        // Methods
    static EN_Bool         InitializeSocketHandler();
    static EN_Bool                 ConnectToServer();
    static EN_Bool         HandleConnectionClosing();
    static EN_Bool              SendNetworkMessage(EN_MessageType* msg_type_);
    static EN_MessageType    ReceiveNetworkMessage();
    static void                EncodeHartIpRequest();
    static void           EncodeHartIpEmptyRequest();
    static void              DecodeNetworkResponse();
    static EN_ToDo               SignalHartMessage();
    static EN_ToDo               SignalHartSilence();
    static void                   SignalHartTxDone();
    static void                 SignalNetworkError();
    static void                       SaveNextToDo(EN_ToDo to_do_);
    static EN_ToDo                   FetchNextToDo();






    static EN_Bool HandleReveiving();
    static EN_Bool SendEmptyRequest();
    static EN_Bool SendHartRequest();
    static void    HandleHartResponse();
    static void    HandleHartBurst();
    static void    RestartConnection();

public:
    static EN_Status  m_status;
};
#endif // __hmipmacport_h__