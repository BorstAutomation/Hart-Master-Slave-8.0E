/*
 *          File: HIPSMacPort.cpp
 *                The Execute method is called directly by the fast cyclic
 *                handler. This basically drives all status machines in
 *                the Hart implementation. Here too, the method is divided
 *                into an Event handler and a ToDo handler.
 *                This version is especially dedicated to a Hart IP server.
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

 // Winsockets
#include <winsock2.h>
#include <ws2tcpip.h>
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "5094"
// End Winsockets
#include <Windows.h>

#include "WbHartSlave.h"
#include "HSipMacPort.h"
#include "WinSystem.h"
#include "HSipProtocol.h"
#include "HSipLayer2.h"
#include "HartChannel.h"
#include "Monitor.h"

// Data
CHSipMacPort::EN_Status      CHSipMacPort::m_status = CHSipMacPort::EN_Status::IDLE;
TY_Byte                      CHSipMacPort::m_rcv_buf[MAX_IP_TXRX_SIZE];
TY_Word                      CHSipMacPort::m_rcv_len = MAX_IP_TXRX_SIZE;
TY_Byte                      CHSipMacPort::m_tx_buf[MAX_IP_TXRX_SIZE];
TY_Word                      CHSipMacPort::m_tx_len = MAX_IP_TXRX_SIZE;
TY_Byte                      CHSipMacPort::m_rx_err = 0;
CHSipMacPort::EN_LastError   CHSipMacPort::m_last_error;
CHSipMacPort::EN_MessageType CHSipMacPort::m_last_message_type;
CHSipMacPort::EN_ToDo        CHSipMacPort::m_to_do;
EN_Bool                      CHSipMacPort::m_close_request = EN_Bool::FALSE8;
TY_Byte                      CHSipMacPort::m_hart_rx_data[MAX_TXRX_SIZE];
TY_Byte                      CHSipMacPort::m_hart_rx_len;
TY_Byte                      CHSipMacPort::m_hart_tx_data[MAX_TXRX_SIZE];
TY_Byte                      CHSipMacPort::m_hart_tx_len;
TY_Byte                      CHSipMacPort::m_hart_ip_version = 0;
TY_Byte                      CHSipMacPort::m_hart_ip_message_type = 0;
TY_Byte                      CHSipMacPort::m_hart_ip_message_id = 0;
TY_Byte                      CHSipMacPort::m_hart_ip_comm_status = 0;
TY_Word                      CHSipMacPort::m_hart_ip_sequence_number = 0;
TY_Word                      CHSipMacPort::m_hart_ip_sq_num_request = 0;
TY_Word                      CHSipMacPort::m_hart_ip_sq_num_burst = 0;
TY_Word                      CHSipMacPort::m_hart_ip_byte_count = 0;
TY_Word                      CHSipMacPort::m_magic_number = 0xe0a3;

// WinSockets
static WSADATA so_wsa_data;
static addrinfo *so_result = NULL;
static addrinfo *so_ptr = NULL;
static addrinfo so_addrinfo;
static SOCKET so_connection_socket = INVALID_SOCKET;
static SOCKET so_listen_socket = INVALID_SOCKET;
static SOCKET so_client_socket = INVALID_SOCKET;
                                               // Ver   Type   ID   Stat   Sequence    ByteCount
static TY_Byte     s_hart_ip_empty_response[] = { 0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int     s_hart_ip_empty_response_len = 8;
static TY_Byte           s_hart_ip_response[] = { 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int           s_hart_ip_response_len = 8;
static TY_Byte              s_hart_ip_burst[] = { 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int              s_hart_ip_burst_len = 8;
// End WinSockets

// Methods

void CHSipMacPort::Execute(TY_Word time_ms_)
{
    // Note: This procedure is called every ms as long as the channel is open
    EN_MessageType msg_type = EN_MessageType::EMPTY_NETWORK_RESPONSE;

    COSAL::CTimer::UpdateTime(time_ms_);

    switch (m_status)
    {
    case EN_Status::IDLE:
        // Do nothing
        break;
    case EN_Status::INITIALIZING:
        if (InitializeSocketHandler() == EN_Bool::TRUE8)
        {
            m_status = EN_Status::WAIT_CONNECT;
        }

        break;
    case EN_Status::WAIT_CONNECT:
        if (ConnectToClient() == EN_Bool::TRUE8)
        {
            m_status = EN_Status::SERVER_READY;
        }
        else
        {
            // Try again to connect
            m_status = EN_Status::INITIALIZING;
        }

        break;
    case EN_Status::SERVER_READY:
        m_last_message_type = ReceiveNetworkMessage();
        switch (m_last_message_type)
        {
        case EN_MessageType::EMPTY_NETWORK_REQUEST:
            m_status = EN_Status::WAIT_RESPONSE;
            break;
        case EN_MessageType::NETWORK_REQUEST:
            m_status = EN_Status::WAIT_RESPONSE;
            break;
        case EN_MessageType::FAILED:
            SignalNetworkError();
            m_status = EN_Status::INITIALIZING;
            break;
        }

        break;
    case EN_Status::WAIT_RESPONSE:
        if (SendNetworkMessage(&msg_type) == EN_Bool::TRUE8)
        {
            if ((msg_type == EN_MessageType::NETWORK_RESPONSE) ||
                (msg_type == EN_MessageType::NETWORK_BURST))
            {
                SignalHartTxDone();
            }

            m_status = EN_Status::SERVER_READY;
        }
        else
        {
            SignalNetworkError();
            m_status = EN_Status::INITIALIZING;
        }

        break;
    }
}

EN_Bool CHSipMacPort::Open(TY_Byte* host_name_, TY_Word port_, EN_CommType type_)
{
    m_status = EN_Status::INITIALIZING;
    m_last_error = EN_LastError::NONE;
    CWinSys::CyclicTaskStart();
    return EN_Bool::TRUE8;
}

void CHSipMacPort::Close()
{
    // Try to tell the cyclic thread to close
    // the connection
    m_close_request = EN_Bool::TRUE8;
    // Wait for the thread
    COSAL::Wait(50);
    if (m_status != EN_Status::IDLE)
    {
        // Thread stucked, kill it
        CWinSys::CyclicTaskKill();
    }
    else
    {
        // Terminate thraed
        CWinSys::CyclicTaskTerminate();
    }

    // Get rid of the leftover mess
    if (so_connection_socket != INVALID_SOCKET)
    {
        closesocket(so_connection_socket);
        so_connection_socket = INVALID_SOCKET;
    }

    WSACleanup();
    m_close_request = EN_Bool::FALSE8;
    m_status = EN_Status::IDLE;
}

void CHSipMacPort::Init()
{
    CHSipL2SM::Init();
}

TY_Word CHSipMacPort::GetStatus()
{
    // .15 .14 .13 .12 .11 .10 .09 .08 .07 .06 .05 .04 .03 .02 .01 .00
    //  +   +   +   +--- Status ----+   +-------- Last Error -------+
    //  |   |   +-- tbd
    //  |   +------ tbd
    //  +---------- Data available

    return (TY_Word)((((TY_Byte)m_status & 0x1f) << 8) + (TY_Byte)m_last_error);
}

TY_Word CHSipMacPort::GetMagicNumber()
{
    return m_magic_number;
}

TY_Byte CHSipMacPort::GetMessageType()
{
    return m_hart_ip_message_type;
}

void CHSipMacPort::SetMessageType(TY_Byte hart_ip_msg_type_)
{
    m_hart_ip_message_type = hart_ip_msg_type_;
    if (hart_ip_msg_type_ == 2)
    {
        m_hart_ip_sequence_number = m_hart_ip_sq_num_burst;
    }
}

TY_Word CHSipMacPort::GetSequenceNumber()
{
    return m_hart_ip_sequence_number;
}

void CHSipMacPort::GetIpFrameForMonitor(TY_Byte* dst_, TY_Byte* dst_len_, TY_Byte* src_, TY_Byte src_len_)
{
    TY_Word del_pos;
    TY_Word word;

    COSAL::CMem::Set(dst_, 0, MAX_TXRX_SIZE);
    word = CHSipMacPort::GetMagicNumber();
    dst_[0] = (TY_Byte)(word >> 8);
    dst_[1] = (TY_Byte)(word);
    dst_[2] = CHSipMacPort::GetMessageType();
    word = CHSipMacPort::GetSequenceNumber();
    dst_[3] = (TY_Byte)(word >> 8);
    dst_[4] = (TY_Byte)(word);
    // Find delimiter pos
    for (TY_Byte i = 0; i < src_len_; i++)
    {
        if (src_[i] != 0xff)
        {
            del_pos = i;
            break;
        }
    }

    *dst_len_ = src_len_ - del_pos;
    // Copy starting from delimiter
    COSAL::CMem::Copy(&dst_[5], &src_[del_pos], *dst_len_);
    *dst_len_ = *dst_len_ + 5;
}

EN_Bool CHSipMacPort::InitializeSocketHandler()
{
    int result;

    WSACleanup();
    so_connection_socket = INVALID_SOCKET;
    so_listen_socket = INVALID_SOCKET;
    so_client_socket = INVALID_SOCKET;
    so_result = NULL;
    so_ptr = NULL;

    result = WSAStartup(MAKEWORD(2, 2), &so_wsa_data);
    if (result != 0)
    {
        m_last_error = EN_LastError::INITIALIZING;
        return EN_Bool::FALSE8;
    }
    else
    {
        ZeroMemory(&so_addrinfo, sizeof(so_addrinfo));
        so_addrinfo.ai_family = AF_UNSPEC;
        so_addrinfo.ai_socktype = SOCK_STREAM;
        so_addrinfo.ai_protocol = IPPROTO_TCP;

        // Resolve the server address and port
        result = getaddrinfo("localhost", DEFAULT_PORT, &so_addrinfo, &so_result);
        if (result != 0)
        {
            m_last_error = EN_LastError::GET_ADDR_INFO;
            WSACleanup();
            // Last error to be added
            return EN_Bool::FALSE8;
        }
    }

    return EN_Bool::TRUE8;
}

EN_Bool CHSipMacPort::ConnectToClient()
{
    int result;

    // Create a SOCKET for the server to listen for client connections.
    so_listen_socket = socket(so_result->ai_family, so_result->ai_socktype, so_result->ai_protocol);
    if (so_listen_socket == INVALID_SOCKET)
    {
        // Socket create failed with error.
        freeaddrinfo(so_result);
        WSACleanup();
        m_last_error = EN_LastError::CREATE_SOCKET;
        return EN_Bool::FALSE8;
    }

    // Setup the TCP listening socket
    result = bind(so_listen_socket, so_result->ai_addr, so_result->ai_addrlen);
    if (result == SOCKET_ERROR) {
        // Bind failed with error.
        freeaddrinfo(so_result);
        closesocket(so_listen_socket);
        so_listen_socket = INVALID_SOCKET;
        WSACleanup();
        m_last_error = EN_LastError::BIND;
        return EN_Bool::FALSE8;
    }

    freeaddrinfo(so_result);

    result = listen(so_listen_socket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        // Listen failed with error.
        closesocket(so_listen_socket);
        so_listen_socket = INVALID_SOCKET;
        WSACleanup();
        m_last_error = EN_LastError::LISTEN;
        return EN_Bool::FALSE8;
    }

    // Accept a client socket
    so_client_socket = accept(so_listen_socket, NULL, NULL);
    if (so_client_socket == INVALID_SOCKET) {
        // Accept failed with error.
        closesocket(so_listen_socket);
        so_listen_socket = INVALID_SOCKET;
        WSACleanup();
        m_last_error = EN_LastError::ACCEPT;
        return EN_Bool::FALSE8;
    }

    // No longer need listen socket
    closesocket(so_listen_socket);
    so_listen_socket = INVALID_SOCKET;
    return EN_Bool::TRUE8;
}

CHSipMacPort::EN_MessageType CHSipMacPort::ReceiveNetworkMessage()
{
    int result;

    m_rcv_len = MAX_IP_TXRX_SIZE;
    result = recv(so_client_socket, (char*)&m_rcv_buf, m_rcv_len, 0);
    if (result > 0)
    {
        m_rcv_len = (TY_Byte)result;
        if (m_rcv_len > HART_IP_HEADER_LEN)
        {
            SaveNextToDo(SignalHartMessage());
            return EN_MessageType::NETWORK_REQUEST;
        }
        else
        {
            SaveNextToDo(SignalHartSilence());
            return EN_MessageType::EMPTY_NETWORK_REQUEST;
        }
    }
    else if (result == 0)
    {
        // No data, connection lost?
        return EN_MessageType::FAILED;
    }

    return EN_MessageType::FAILED;
}

EN_Bool CHSipMacPort::SendNetworkMessage(EN_MessageType* msg_type_)
{
    int result;
    EN_ToDo to_do;

    to_do = FetchNextToDo();

    if (to_do == EN_ToDo::SEND_RESPONSE)
    {
        EncodeHartIpResponse();
        *msg_type_ = EN_MessageType::NETWORK_RESPONSE;
    }
    else if (to_do == EN_ToDo::SEND_BURST)
    {
        EncodeHartIpBurst();
        *msg_type_ = EN_MessageType::NETWORK_BURST;
    }
    else
    {
        EncodeHartIpEmptyResponse();
        *msg_type_ = EN_MessageType::EMPTY_NETWORK_RESPONSE;
    }

    result = send(so_client_socket, (const char*)m_tx_buf, m_tx_len, 0);
    if (result == SOCKET_ERROR)
    {
        return EN_Bool::FALSE8;
    }

    return EN_Bool::TRUE8;
}

CHSipMacPort::EN_ToDo CHSipMacPort::SignalHartMessage()
{
    DecodeNetworkRequest();
    // Call the protocol state machine
    CHSipMacPort::EN_ToDo todo = CHSipProtocol::EventHandler(CHSipProtocol::EN_Event::HART_IP_DATA_RECEIVED, m_hart_rx_data, m_hart_rx_len, 0);
    return todo;
}

CHSipMacPort::EN_ToDo CHSipMacPort::SignalHartSilence()
{
    CHSipMacPort::EN_ToDo todo = CHSipProtocol::EventHandler(CHSipProtocol::EN_Event::HART_IP_EMPTY_RECEIVED, m_hart_rx_data, m_hart_rx_len, 0);

    return todo;
}

void CHSipMacPort::SignalHartTxDone()
{
    CHSipProtocol::EventHandler(CHSipProtocol::EN_Event::HART_IP_TX_DONE, NULL, NULL, 0);
}

void CHSipMacPort::SignalNetworkError()
{
    CHSipProtocol::EventHandler(CHSipProtocol::EN_Event::NETWORK_ERROR, NULL, NULL, 0);
}

void CHSipMacPort::EncodeHartIpResponse()
{
    // Prepare the hart ip payload
    TY_Word del_pos;
    TY_Word tx_len;
    TY_Word idx = 0;
    TY_Word payload_len = 0;
    TY_Byte* dst_ = CHSipProtocol::GetTxData(&tx_len);
    // Search delimiter
    for (TY_Word i = 0; i < tx_len; i++)
    {
        if (dst_[i] != 0xff)
        {
            del_pos = i;
            break;
        }
    }

    // Begin with the header
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_response, s_hart_ip_response_len);
    idx = s_hart_ip_response_len;
    // Add the hart ip payload
    COSAL::CMem::Copy(&m_tx_buf[idx], &dst_[del_pos], tx_len - del_pos);
    payload_len = tx_len - del_pos;
    m_tx_len = s_hart_ip_response_len + tx_len - del_pos;
    // Finalze the packet
    m_tx_buf[4] = (TY_Byte)(m_hart_ip_sq_num_request >> 8);
    m_tx_buf[5] = (TY_Byte)(m_hart_ip_sq_num_request);
    m_tx_buf[6] = (TY_Byte)(payload_len >> 8);
    m_tx_buf[7] = (TY_Byte)(payload_len);
    m_hart_ip_message_type = 1;
    CMonitor::SetAdditionalData(m_tx_buf, m_tx_len);
}

void CHSipMacPort::EncodeHartIpBurst()
{
    // Prepare the hart ip payload
    TY_Word del_pos;
    TY_Word tx_len;
    TY_Word idx = 0;
    TY_Word payload_len = 0;
    TY_Byte* dst_ = CHSipProtocol::GetTxData(&tx_len);
    // Search delimiter
    for (TY_Word i = 0; i < tx_len; i++)
    {
        if (dst_[i] != 0xff)
        {
            del_pos = i;
            break;
        }
    }

    // Begin with the header
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_burst, s_hart_ip_burst_len);
    idx = s_hart_ip_burst_len;
    // Add the hart ip payload
    COSAL::CMem::Copy(&m_tx_buf[idx], &dst_[del_pos], tx_len - del_pos);
    payload_len = tx_len - del_pos;
    m_tx_len = s_hart_ip_burst_len + tx_len - del_pos;
    // Finalize the paket
    m_tx_buf[4] = (TY_Byte)(m_hart_ip_sq_num_burst >> 8);
    m_tx_buf[5] = (TY_Byte)(m_hart_ip_sq_num_burst);
    m_tx_buf[6] = (TY_Byte)(payload_len >> 8);
    m_tx_buf[7] = (TY_Byte)(payload_len);
    m_hart_ip_message_type = 2;
    CMonitor::SetAdditionalData(m_tx_buf, m_tx_len);
}

void CHSipMacPort::EncodeHartIpEmptyResponse()
{
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_empty_response, s_hart_ip_empty_response_len);
    m_tx_len = s_hart_ip_empty_response_len;
}

void CHSipMacPort::DecodeNetworkRequest()
{
    // Get the hart ip header details
    m_hart_ip_version = m_rcv_buf[0];
    m_hart_ip_message_type = m_rcv_buf[1];
    m_hart_ip_message_id = m_rcv_buf[2];
    m_hart_ip_comm_status = m_rcv_buf[3];
    m_hart_ip_sequence_number = (TY_Word)((m_rcv_buf[4] << 8) + m_rcv_buf[5]);
    m_hart_ip_byte_count = (TY_Word)((m_rcv_buf[6] << 8) + m_rcv_buf[7]);

    if (m_hart_ip_message_type == 0)
    {
        // Request
        m_hart_ip_sq_num_request = m_hart_ip_sequence_number;
        m_hart_ip_sq_num_burst = m_hart_ip_sequence_number;
    }

    // Copy frame  to the hart context
    COSAL::CMem::Set(m_hart_rx_data, 0, MAX_TXRX_SIZE);
    m_hart_rx_len = (TY_Byte)(m_rcv_len - HART_IP_HEADER_LEN);
    COSAL::CMem::Copy(m_hart_rx_data, &m_rcv_buf[HART_IP_HEADER_LEN], m_hart_rx_len);
}

void CHSipMacPort::SaveNextToDo(EN_ToDo to_do_)
{
    if (to_do_ == EN_ToDo::SEND_RESPONSE)
    {
        // Highest priority
        m_to_do = to_do_;
        return;
    }

    if (to_do_ == EN_ToDo::SEND_BURST)
    {
        if (m_to_do != EN_ToDo::SEND_RESPONSE)
        {
            // Send burst is next priority
            m_to_do = to_do_;
        }
        return;
    }
}

CHSipMacPort::EN_ToDo CHSipMacPort::FetchNextToDo()
{
    EN_ToDo tmp;
    tmp = m_to_do;
    m_to_do = EN_ToDo::NOTHING;
    return tmp;
}

