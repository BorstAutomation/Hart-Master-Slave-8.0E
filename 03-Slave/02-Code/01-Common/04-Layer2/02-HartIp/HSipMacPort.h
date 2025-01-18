/*
 *          File: HSipMacPort.h (CHSipMacPort)
 *                The interface to the MAC port is relatively small
 *                and can be defined generically. However, the implementation
 *                depends on the hardware and software environment.
 *                That's why there is only a header at this point, while
 *                the file HSipMacPort.cpp can be found in the specific branch.
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
#ifndef __hsipmacport_h__
#define __hsipmacport_h__

#include "OSAL.h"
#include "WbHartUser.h"

class CHSipMacPort
{
public:
    // Enum classes
    enum class EN_Status : TY_Byte
    {
        IDLE = 0,
        INITIALIZING = 1,
        WAIT_CONNECT = 2,
        SERVER_READY = 3,
        WAIT_RESPONSE = 4,
        SHUTTING_DOWN = 5
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

    enum class EN_ToDo : TY_Byte
    {
        NOTHING = 0,
        CONNECT = 1,
        DISCONNECT = 2,
        SEND_RESPONSE = 3,
        SEND_BURST = 4,
        RECEIVE_ENABLE = 5,
        RECEIVE_DISABLE = 6
    };

    enum class EN_LastError : TY_Byte
    {
        NONE = 0,
        INITIALIZING = 1,
        GET_ADDR_INFO = 2,
        CREATE_SOCKET = 3,
        BIND = 4,
        LISTEN = 5,
        ACCEPT = 6,
        TX_FAILED = 7,
        SHUTDOWN = 8,
        RECEIVING = 9
    };

    static EN_Bool                  Open(TY_Byte* host_name_, TY_Word port_, EN_CommType type_);
    static void                    Close();
    static void                  Execute(TY_Word time_ms_);
    static void                     Init();
    static TY_Word             GetStatus();
    static TY_Word        GetMagicNumber();
    static TY_Byte        GetMessageType();
    static void           SetMessageType(TY_Byte hart_ip_msg_type_);
    static TY_Word     GetSequenceNumber();
    static void     GetIpFrameForMonitor(TY_Byte* src_, TY_Byte* src_len_, TY_Byte* dst_, TY_Byte dst_len);

private:
    static TY_Byte        m_rcv_buf[MAX_IP_TXRX_SIZE];
    static TY_Word        m_rcv_len;
    static TY_Byte        m_tx_buf[MAX_IP_TXRX_SIZE];
    static TY_Word        m_tx_len;
    static EN_Status      m_status;
    static EN_LastError   m_last_error;
    static TY_Byte        m_rx_err;
    static EN_MessageType m_last_message_type;
    static EN_ToDo        m_to_do;
    static EN_Bool        m_close_request;
    static TY_Byte        m_hart_rx_data[MAX_TXRX_SIZE];
    static TY_Byte        m_hart_rx_len;
    static TY_Byte        m_hart_tx_data[MAX_TXRX_SIZE];
    static TY_Byte        m_hart_tx_len;
    static TY_Byte        m_hart_ip_version;
    static TY_Byte        m_hart_ip_message_type;
    static TY_Byte        m_hart_ip_message_id;
    static TY_Byte        m_hart_ip_comm_status;
    static TY_Word        m_hart_ip_sequence_number;
    static TY_Word        m_hart_ip_sq_num_request;
    static TY_Word        m_hart_ip_sq_num_burst;
    static TY_Word        m_hart_ip_byte_count;
    static TY_Word        m_magic_number;

    // Methods
    static EN_Bool        InitializeSocketHandler();
    static EN_Bool                ConnectToClient();
    static EN_Bool        HandleConnectionClosing();
    static EN_MessageType   ReceiveNetworkMessage();
    static EN_Bool             SendNetworkMessage(EN_MessageType* msg_type_);
    static EN_ToDo              SignalHartMessage();
    static EN_ToDo              SignalHartSilence();
    static void                  SignalHartTxDone();
    static void                SignalNetworkError();
    static void              EncodeHartIpResponse();
    static void                 EncodeHartIpBurst();
    static void         EncodeHartIpEmptyResponse();
    static void              DecodeNetworkRequest();
    static void                      SaveNextToDo(EN_ToDo to_do_);
    static EN_ToDo                  FetchNextToDo();

};
#endif // __hsipmacport_h__