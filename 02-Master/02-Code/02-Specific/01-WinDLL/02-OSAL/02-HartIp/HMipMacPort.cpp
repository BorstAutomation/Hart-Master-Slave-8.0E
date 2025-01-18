/*
 *          File: HMipMacPort.cpp (CHMipMacPort)
 *                The Execute method is called directly by the fast cyclic
 *                handler. This basically drives all status machines in
 *                the Hart implementation. Here too, the method is divided
 *                into an Event handler and a ToDo handler.
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
// #define DEFAULT_PORT "27015"
#define DEFAULT_PORT "5094"
// End Winsockets
#include "WinSystem.h"
#include "WbHartUser.h"
#include "HMipMacPort.h"
#include "HMipProtocol.h"
#include "HMipLayer2.h"
#include "WbHartM_Structures.h"
#include "Monitor.h"

// Data
CHMipMacPort::EN_Status      CHMipMacPort::m_status = CHMipMacPort::EN_Status::IDLE;
TY_Byte                      CHMipMacPort::m_rcv_buf[MAX_IP_TXRX_SIZE];
int                          CHMipMacPort::m_rcv_len = 0;
TY_Byte                      CHMipMacPort::m_tx_buf[MAX_IP_TXRX_SIZE];
int                          CHMipMacPort::m_tx_len = 0;
TY_Byte                      CHMipMacPort::m_hart_ip_data[MAX_IP_TXRX_SIZE];
TY_Word                      CHMipMacPort::m_hart_ip_len;
CHMipMacPort::EN_LastError   CHMipMacPort::m_last_error = CHMipMacPort::EN_LastError::NONE;
TY_Byte                      CHMipMacPort::m_rx_err = 0;
CHMipMacPort::EN_MessageType CHMipMacPort::m_last_message_type;
CHMipMacPort::EN_ToDo        CHMipMacPort::m_to_do;
EN_Bool                      CHMipMacPort::m_close_request = EN_Bool::FALSE8;
TY_Byte                      CHMipMacPort::m_hart_rx_data[MAX_TXRX_SIZE];
TY_Byte                      CHMipMacPort::m_hart_rx_len = 0;
TY_Byte                      CHMipMacPort::m_hart_ip_version = 0;
TY_Byte                      CHMipMacPort::m_hart_ip_message_type = 0;
TY_Byte                      CHMipMacPort::m_hart_ip_message_id = 0;
TY_Byte                      CHMipMacPort::m_hart_ip_comm_status = 0;
TY_Word                      CHMipMacPort::m_hart_ip_sequence_number = 0;
TY_Word                      CHMipMacPort::m_hart_ip_byte_count = 0;
TY_Word                      CHMipMacPort::m_magic_number = 0xe0a3;

// WinSockets
// Data
static WSADATA so_wsa_data;
static addrinfo *so_result = NULL;
static addrinfo *so_ptr = NULL;
static addrinfo so_addrinfo;
static SOCKET  so_connection_socket = INVALID_SOCKET;
static SOCKET so_listen_socket = INVALID_SOCKET;
static SOCKET so_client_socket = INVALID_SOCKET;
                                          // Ver   Type   ID   Stat   Sequence    ByteCount
static TY_Byte s_hart_ip_empty_request[] = { 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int     s_hart_ip_empty_request_len = 8;
static TY_Byte s_hart_ip_request_header[] = { 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
static int     s_hart_ip_request_header_len = 8;
// End WinSockets

// Methods
void CHMipMacPort::Execute(TY_Word time_ms_)
{
    // Note: This procedure is called every ms as long as the channel is open

    EN_MessageType        msg_type = EN_MessageType::EMPTY_NETWORK_RESPONSE;


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
        if (ConnectToServer() == EN_Bool::TRUE8)
        {
            // Get the response from connecting
            if (HandleReveiving() == EN_Bool::TRUE8)
            {
                m_status = EN_Status::CLIENT_READY;
            }
            else
            {
                // Try again to connect
                m_status = EN_Status::INITIALIZING;
            }
        }
        else
        {
            // Try again to connect
            m_status = EN_Status::INITIALIZING;
        }

        break;
    case EN_Status::CLIENT_READY:
        //m_to_do = CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::NONE, NULL, 0);
        if (SendNetworkMessage(&msg_type) == EN_Bool::TRUE8)
        {
            if (msg_type == EN_MessageType::NETWORK_REQUEST)
            {
                SignalHartTxDone();
            }
            else if (msg_type == EN_MessageType::EMPTY_NETWORK_REQUEST)
            {
                SaveNextToDo(SignalHartSilence());
            }

            m_status = EN_Status::WAIT_NETWORK_RESPONSE;
        }
        else
        {
            SignalNetworkError();
            m_status = EN_Status::INITIALIZING;
        }

        break;
    case EN_Status::WAIT_NETWORK_RESPONSE:
        m_last_message_type = ReceiveNetworkMessage();
        switch (m_last_message_type)
        {
        case EN_MessageType::EMPTY_NETWORK_RESPONSE:
            SaveNextToDo(SignalHartSilence());
            m_status = EN_Status::CLIENT_READY;
            break;
        case EN_MessageType::NETWORK_RESPONSE:
            SaveNextToDo(SignalHartMessage());
            m_status = EN_Status::CLIENT_READY;
            break;
        case EN_MessageType::FAILED:
            SignalNetworkError();
            m_status = EN_Status::INITIALIZING;
            break;
        }

        break;
    case EN_Status::SHUTTING_DOWN:
        HandleConnectionClosing();
        closesocket(so_connection_socket);
        so_connection_socket = INVALID_SOCKET;
        WSACleanup();

        CWinSys::CyclicTaskTerminate();
        m_status = EN_Status::IDLE;
    }

    switch (m_to_do)
    {
    case EN_ToDo::NOTHING:
        break;
    case EN_ToDo::CONNECT:
        break;
    case EN_ToDo::DISCONNECT:
        m_status = EN_Status::INITIALIZING;
        break;
    case EN_ToDo::SEND_DATA:
        break;
    case EN_ToDo::RECEIVE_ENABLE:
        break;
    case EN_ToDo::RECEIVE_DISABLE:
        break;
    }
}

EN_Bool CHMipMacPort::Open(TY_Byte* host_name_, TY_Word port_, EN_CommType type_)
{
    // Start the thread for the cyclic handler
    m_status = EN_Status::INITIALIZING;
    m_last_error = EN_LastError::NONE;
    CWinSys::CyclicTaskStart();
    return EN_Bool::TRUE8;
}

void CHMipMacPort::Close()
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

void CHMipMacPort::Init()
{
    CHMipL2SM::Init();
}

TY_Word CHMipMacPort::GetStatus()
{
    // .15 .14 .13 .12 .11 .10 .09 .08 .07 .06 .05 .04 .03 .02 .01 .00
    //  +   +   +   +--- Status ----+   +-------- Last Error -------+
    //  |   |   +-- tbd
    //  |   +------ tbd
    //  +---------- Data available

    return (TY_Word)((((TY_Byte)m_status & 0x1f) << 8) + (TY_Byte)m_last_error);
}

TY_Word CHMipMacPort::GetMagicNumber()
{
    return m_magic_number;
}

TY_Byte CHMipMacPort::GetMessageType()
{
    return m_hart_ip_message_type;
}

void CHMipMacPort::SetMessageType(TY_Byte hart_ip_msg_type_)
{
    m_hart_ip_message_type = hart_ip_msg_type_;
    if (hart_ip_msg_type_ == 0)
    {
        m_hart_ip_sequence_number++;
    }
}

TY_Word CHMipMacPort::GetSequenceNumber()
{
    return m_hart_ip_sequence_number;
}

void CHMipMacPort::GetIpFrameForMonitor(TY_Byte* dst_, TY_Byte* dst_len_, TY_Byte* src_, TY_Byte src_len_)
{
    TY_Word del_pos;
    TY_Word word;

    COSAL::CMem::Set(dst_, 0, MAX_TXRX_SIZE);

    word = CHMipMacPort::GetMagicNumber();
    dst_[0] = (TY_Byte)(word >> 8);
    dst_[1] = (TY_Byte)(word);
    dst_[2] = CHMipMacPort::GetMessageType();
    word = CHMipMacPort::GetSequenceNumber();
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

EN_Bool CHMipMacPort::InitializeSocketHandler()
{
    int result;

    WSACleanup();
    so_connection_socket = INVALID_SOCKET;
    so_listen_socket = INVALID_SOCKET;
    so_client_socket = INVALID_SOCKET;
    so_result = NULL;
    so_ptr = NULL;

    result = WSAStartup(MAKEWORD(2, 2), &so_wsa_data);
    if (result != 0) {
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
            return EN_Bool::FALSE8;
        }
    }

    return EN_Bool::TRUE8;
}

EN_Bool CHMipMacPort::ConnectToServer()
{
    int result;

    // Attempt to connect to an address until one succeeds
    for (so_ptr = so_result; so_ptr != NULL; so_ptr = so_ptr->ai_next) {

        // Create a SOCKET for connecting to server
        so_connection_socket = socket(so_ptr->ai_family, so_ptr->ai_socktype,
            so_ptr->ai_protocol);
        if (so_connection_socket == INVALID_SOCKET)
        {
            // Socket failed with error.
            WSACleanup();
            m_last_error = EN_LastError::CREATE_SOCKET;
            return EN_Bool::FALSE8;
        }

        // Try connect to server.
        result = connect(so_connection_socket, so_ptr->ai_addr, (int)so_ptr->ai_addrlen);
        if (result == SOCKET_ERROR) {
            closesocket(so_connection_socket);
            so_connection_socket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(so_result);

    if (so_connection_socket == INVALID_SOCKET)
    {
        m_last_error = EN_LastError::NO_SERVER;
        WSACleanup();
        return EN_Bool::FALSE8;
    }
    else
    {
        // Send an initial buffer
        result = send(so_connection_socket,(const char*) s_hart_ip_empty_request, s_hart_ip_empty_request_len, 0);
        if (result == SOCKET_ERROR) {
            // Send failed with error.
            closesocket(so_connection_socket);
            WSACleanup();
            m_last_error = EN_LastError::TX_FAILED;
            return EN_Bool::FALSE8;
        }
    }

    return EN_Bool::TRUE8;
}

EN_Bool CHMipMacPort::HandleConnectionClosing()
{
    int result;

    // Receive until the peer closes the connection
    do {
        m_rcv_len = MAX_IP_TXRX_SIZE;
        result = recv(so_connection_socket, (char*)(&m_rcv_buf), m_rcv_len, 0);
        if (result > 0)
            // Bytes received
            m_rcv_len = result;
        else if (result == 0)
            // Connection closed
            m_rcv_len = 0;
        else
            m_last_error = EN_LastError::SHUTDOWN;

    } while (result > 0);

    if (result == 0)
    {
        return EN_Bool::TRUE8;
    }

    return EN_Bool::FALSE8;
}

EN_Bool CHMipMacPort::SendNetworkMessage(EN_MessageType* msg_type_)
{
    int result;

    if (FetchNextToDo() == EN_ToDo::SEND_DATA)
    {
        EncodeHartIpRequest();
        *msg_type_ = EN_MessageType::NETWORK_REQUEST;
    }
    else
    {
        EncodeHartIpEmptyRequest();
        *msg_type_ = EN_MessageType::EMPTY_NETWORK_REQUEST;
    }

    result = send(so_connection_socket, (const char*)m_tx_buf, m_tx_len, 0);
    if (result == SOCKET_ERROR)
    {
        return EN_Bool::FALSE8;
    }

    return EN_Bool::TRUE8;
}

CHMipMacPort::EN_MessageType CHMipMacPort::ReceiveNetworkMessage()
{
    int result;

    m_rcv_len = MAX_IP_TXRX_SIZE;
    result = recv(so_connection_socket, (char*)&m_rcv_buf, m_rcv_len, 0);
    if (result > 0)
    {
        m_rcv_len = (TY_Byte)result;
        if (m_rcv_len > HART_IP_HEADER_LEN)
        {
            return EN_MessageType::NETWORK_RESPONSE;
        }
        else
        {
            return EN_MessageType::EMPTY_NETWORK_RESPONSE;
        }

    }
    else if (result == 0)
    {
        // No data, connection lost?
        return EN_MessageType::FAILED;
    }

    return EN_MessageType::FAILED;
}

void CHMipMacPort::EncodeHartIpRequest()
{
    // Prepare the hart ip payload
    TY_Word del_pos;
    TY_Word tx_len;
    TY_Word idx = 0;
    TY_Word payload_len = 0;
    TY_Byte* tx_data = CHMipL2SM::GetTxData(&tx_len);
    // Find delimiter
    for (TY_Word i = 0; i < tx_len; i++)
    {
        if (tx_data[i] != 0xff)
        {
            del_pos = i;
            break;
        }
    }

    // Begin with the header
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_request_header, s_hart_ip_request_header_len);
    idx = s_hart_ip_request_header_len;
    // Add the hart ip payload
    COSAL::CMem::Copy(&m_tx_buf[idx], &tx_data[del_pos], tx_len - del_pos);
    payload_len = tx_len - del_pos;
    m_tx_len = s_hart_ip_request_header_len + tx_len - del_pos;
    // Sequence number was already increased when 
    // passing the frane to the monitor
    m_tx_buf[4] = (TY_Byte)(m_hart_ip_sequence_number >> 8);
    m_tx_buf[5] = (TY_Byte)(m_hart_ip_sequence_number);
    m_tx_buf[6] = (TY_Byte)(payload_len >> 8);
    m_tx_buf[7] = (TY_Byte)(payload_len);
    m_hart_ip_message_type = 0;
    CMonitor::SetAdditionalData(m_tx_buf, m_tx_len);
}

void CHMipMacPort::EncodeHartIpEmptyRequest()
{
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_empty_request, s_hart_ip_empty_request_len);
    m_tx_len = s_hart_ip_empty_request_len;
}

void CHMipMacPort::DecodeNetworkResponse()
{
    // Get the hart ip header details
    m_hart_ip_version = m_rcv_buf[0];
    m_hart_ip_message_type = m_rcv_buf[1];
    m_hart_ip_message_id = m_rcv_buf[2];
    m_hart_ip_comm_status = m_rcv_buf[3];
    m_hart_ip_sequence_number = (TY_Word)((m_rcv_buf[4] << 8) + m_rcv_buf[5]);
    m_hart_ip_byte_count = (TY_Word)((m_rcv_buf[6] << 8) + m_rcv_buf[7]);

    // Get length
    // Get status byte

    // Copy frame  to the hart context
    COSAL::CMem::Set(m_hart_rx_data, 0, MAX_TXRX_SIZE);
    m_hart_rx_len = (TY_Byte)(m_rcv_len - HART_IP_HEADER_LEN);
    COSAL::CMem::Copy(m_hart_rx_data, &m_rcv_buf[HART_IP_HEADER_LEN], m_hart_rx_len);
}

CHMipMacPort::EN_ToDo CHMipMacPort::SignalHartMessage()
{
    DecodeNetworkResponse();
    // Call the protocol state machine
    CHMipMacPort::EN_ToDo todo = CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::HART_IP_DATA_RECEIVED, m_hart_rx_data, m_hart_rx_len);
    return todo;
}

CHMipMacPort::EN_ToDo CHMipMacPort::SignalHartSilence()
{
    CHMipMacPort::EN_ToDo todo = CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::HART_IP_EMPTY_RECEIVED, m_hart_rx_data, m_hart_rx_len);

    return todo;
}

void CHMipMacPort::SignalHartTxDone()
{
    CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::HART_IP_TX_DONE, NULL, NULL);
}

void CHMipMacPort::SignalNetworkError()
{
    CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::NETWORK_ERROR, NULL, NULL);
}

void CHMipMacPort::SaveNextToDo(EN_ToDo to_do_)
{
    if (to_do_ == EN_ToDo::SEND_DATA)
    {
        m_to_do = to_do_;
    }

    // Test
    if (m_to_do == EN_ToDo::NOTHING)
    {
        m_to_do = to_do_;
    }
}

CHMipMacPort::EN_ToDo CHMipMacPort::FetchNextToDo()
{
    EN_ToDo tmp;
    tmp = m_to_do;
    m_to_do = EN_ToDo::NOTHING;
    return tmp;
}

TY_Word CHMipMacPort::GetAdditionalData(TY_Byte* data_)
{
    COSAL::CMem::Copy(data_, m_hart_ip_data, m_hart_ip_len);
    return m_hart_ip_len;
}

EN_Bool CHMipMacPort::HandleReveiving()
{
    int result = 0;

    m_last_error = EN_LastError::NONE;
    m_rcv_len = MAX_IP_TXRX_SIZE;
    result = recv(so_connection_socket, (char*)(&m_rcv_buf), m_rcv_len, 0);
    if (result > 0)
    {
        // Bytes received
        m_rcv_len = result;
        return EN_Bool::TRUE8;
    }
    else if (result == 0)
    {
        // No data
        m_rcv_len = 0;
        m_last_error = EN_LastError::RECEIVING;
    }
    else
    {
        m_last_error = EN_LastError::RECEIVING;
    }

    return EN_Bool::FALSE8;
}

EN_Bool CHMipMacPort::SendEmptyRequest()
{
    int result;
    
    // Send an empty request
    result = send(so_connection_socket, (const char*)s_hart_ip_empty_request, s_hart_ip_empty_request_len, 0);
    if (result == SOCKET_ERROR) {
        // Send failed with error.
        closesocket(so_connection_socket);
        WSACleanup();
        m_last_error = EN_LastError::TX_FAILED;
        return EN_Bool::FALSE8;
    }

    return EN_Bool::TRUE8;
}

EN_Bool CHMipMacPort::SendHartRequest()
{
    int result;
    TY_Byte test_buffer[MAX_TXRX_SIZE];

    // Prepare the hart ip payload
    TY_Word del_pos;
    TY_Word tx_len ;
    TY_Word idx = 0;
    TY_Byte* tx_data = CHMipL2SM::GetTxData(&tx_len);
    COSAL::CMem::Set(test_buffer, 0, MAX_TXRX_SIZE);
    // Find delimiter
    for (TY_Word i = 0; i < tx_len; i++)
    {
        if (tx_data[i] != 0xff)
        {
            del_pos = i;
            break;
        }
    }

    // Begin with the header
    COSAL::CMem::Copy(m_tx_buf, s_hart_ip_request_header, s_hart_ip_request_header_len);
    idx = s_hart_ip_request_header_len;
    // Add the hart ip payload
    COSAL::CMem::Copy(&m_tx_buf[idx], &tx_data[del_pos], tx_len - del_pos);
    m_tx_len = s_hart_ip_request_header_len + tx_len - del_pos;
    // Just for debugging
    COSAL::CMem::Copy(test_buffer, tx_data, tx_len);
    // Send the paket
    m_hart_ip_sequence_number++;
    m_tx_buf[4] = (TY_Byte)(m_hart_ip_sequence_number >> 8);
    m_tx_buf[5] = (TY_Byte)(m_hart_ip_sequence_number);
    m_hart_ip_message_type = 0;
    result = send(so_connection_socket, (const char*)m_tx_buf, m_tx_len, 0);
    if (result == SOCKET_ERROR)
    {
        // Send failed with error.
        closesocket(so_connection_socket);
        WSACleanup();
        m_last_error = EN_LastError::TX_FAILED;
        return EN_Bool::FALSE8;
    }

    return EN_Bool::TRUE8;
}

void HandleHartResponse()
{
    // To be coded later
}

void CHMipMacPort::RestartConnection()
{
    if (so_connection_socket != INVALID_SOCKET)
    {
        closesocket(so_connection_socket);
        so_connection_socket = INVALID_SOCKET;
    }
    
    WSACleanup();
    m_status = EN_Status::INITIALIZING;
}

void CHMipMacPort::HandleHartResponse()
{
    // Get the hart ip header details
    m_hart_ip_version = m_rcv_buf[0];
    m_hart_ip_message_type = m_rcv_buf[1];
    m_hart_ip_message_id = m_rcv_buf[2];
    m_hart_ip_comm_status = m_rcv_buf[3];
    m_hart_ip_sequence_number = (TY_Word)((m_rcv_buf[4] << 8) + m_rcv_buf[5]);
    m_hart_ip_byte_count = (TY_Word)((m_rcv_buf[6] << 8) + m_rcv_buf[7]);

    // Get length
    // Get status byte
    
    // Copy frame  to the hart context
    COSAL::CMem::Set(m_hart_rx_data, 0, MAX_TXRX_SIZE);
    m_hart_rx_len = (TY_Byte)(m_rcv_len - HART_IP_HEADER_LEN);
    COSAL::CMem::Copy(m_hart_rx_data, &m_rcv_buf[HART_IP_HEADER_LEN], m_hart_rx_len);
    // Call the protocol state machine
    CHMipMacPort::EN_ToDo todo = CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::HART_IP_DATA_RECEIVED, m_hart_rx_data, m_hart_rx_len);

    return; //EN_Bool::TRUE8;
}

void CHMipMacPort::HandleHartBurst()
{
    if (m_rcv_len < 9)
    {
        return;
    }

    // Get the hart ip header details
    m_hart_ip_version = m_rcv_buf[0];
    m_hart_ip_message_type = m_rcv_buf[1];
    m_hart_ip_message_id = m_rcv_buf[2];
    m_hart_ip_comm_status = m_rcv_buf[3];
    m_hart_ip_sequence_number = (TY_Word)((m_rcv_buf[4] << 8) + m_rcv_buf[5]);
    m_hart_ip_byte_count = (TY_Word)((m_rcv_buf[6] << 8) + m_rcv_buf[7]);

    // Get length
    // Get status byte

    // Copy frame  to the hart context
    COSAL::CMem::Set(m_hart_rx_data, 0, MAX_TXRX_SIZE);
    m_hart_rx_len = (TY_Byte)(m_rcv_len - HART_IP_HEADER_LEN);
    COSAL::CMem::Copy(m_hart_rx_data, &m_rcv_buf[HART_IP_HEADER_LEN], m_hart_rx_len);
    // Call the protocol state machine
    CHMipMacPort::EN_ToDo todo = CHMipProtocol::EventHandler(CHMipProtocol::EN_Event::HART_IP_DATA_RECEIVED, m_hart_rx_data, m_hart_rx_len);

    return; //EN_Bool::TRUE8;
}
