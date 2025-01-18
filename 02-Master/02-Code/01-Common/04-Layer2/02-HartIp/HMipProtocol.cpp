/*
 *          File: HartMProtocol.cpp
 *                This protocol layer controls the UART interface on the
 *                lower level and calls the higher status machines when
 *                necessary (events). After this call, a ToDo Part occurs,
 *                which in turn affects the Uart interface.
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
#include "WbHartM_Structures.h"
#include "HMipProtocol.h"
#include "HMipMacPort.h"
#include "HMipLayer2.h"
#include "Monitor.h"

/* CHIPM_Protocolocol */
CHMipProtocol::EN_Status CHMipProtocol::m_status;
CFrame    CHMipProtocol::m_work_frame;
CFrame    CHMipProtocol::m_junk_frame;
CFrame    CHMipProtocol::m_request_frame;
CFrame    CHMipProtocol::m_response_frame;
CFrame    CHMipProtocol::m_burst_frame;
TY_Byte*  CHMipProtocol::mpu8_TxData;
TY_Len    CHMipProtocol::mu16_TxLen;

// Methods
CHMipMacPort::EN_ToDo CHMipProtocol::EventHandler(EN_Event event_, TY_Byte* rx_bytes_, TY_Word len_)
{
    TY_DWord time = COSAL::CTimer::GetTime();

    CHMipMacPort::EN_ToDo parent_to_do = CHMipMacPort::EN_ToDo::NOTHING;
    EN_ToDo                      to_do = EN_ToDo::NOTHING;

    switch (m_status)
    {
    case EN_Status::IDLE:
        to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::NONE, NULL);
        break;
    case EN_Status::RECEIVING:
        if (event_ == EN_Event::HART_IP_DATA_RECEIVED)
        {
            to_do = CHMipL2RxSM::EventHandler(event_, &m_work_frame, rx_bytes_, len_, &m_junk_frame, &m_request_frame, &m_response_frame, &m_burst_frame);
        }
        else
        {
            to_do = CHMipL2RxSM::EventHandler(event_, &m_work_frame, NULL, 0, &m_junk_frame, &m_request_frame, &m_response_frame, &m_burst_frame);
        }

        if (m_request_frame.IsActive() == EN_Bool::TRUE8)
        {
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::RX_COMPLETED_REQ, &m_request_frame);
            m_request_frame.Uninit();
        }

        if (m_response_frame.IsActive() == EN_Bool::TRUE8)
        {
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::RX_COMPLETED_RSP, &m_response_frame);
            m_response_frame.Uninit();
        }

        if (m_burst_frame.IsActive() == EN_Bool::TRUE8)
        {
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::RX_COMPLETED_BST, &m_burst_frame);
            m_burst_frame.Uninit();
        }

        if (m_junk_frame.IsActive() == EN_Bool::TRUE8)
        {
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::RX_COMPLETED_ERR, &m_junk_frame);
            m_junk_frame.Uninit();
        }

        if (to_do == EN_ToDo::START_TRANSMIT)
        {
            CHMipL2RxSM::Reset();
        }
        else
        {
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::NONE, NULL);
        }
        break;

    case EN_Status::TRANSMITTING:
        // Call the state machine of the 
        // Hart ip protocol
        to_do = CHMipL2TxSM::EventHandler();
        break;
    }

    while (to_do != EN_ToDo::NOTHING)
    {
        switch (to_do)
        {
        case EN_ToDo::NOTHING:
            break;
        case EN_ToDo::RECEIVE_ENABLE:
            to_do = EN_ToDo::NOTHING;
            parent_to_do = CHMipMacPort::EN_ToDo::RECEIVE_ENABLE;
            if (m_status != EN_Status::RECEIVING)
            {
                m_status = EN_Status::RECEIVING;
            }
            break;
        case EN_ToDo::RECEIVE_DISABLE:
            to_do = EN_ToDo::NOTHING;
            parent_to_do = CHMipMacPort::EN_ToDo::RECEIVE_DISABLE;
            if (m_status != EN_Status::IDLE)
            {
                m_status = EN_Status::IDLE;
            }
            break;
        case EN_ToDo::START_TRANSMIT:
            to_do = EN_ToDo::NOTHING;
            mpu8_TxData = CHMipL2SM::GetTxData(&mu16_TxLen);
            if ((mpu8_TxData != 0) && (mu16_TxLen > 0))
            {
                CHMipL2TxSM::SetTxLen(mu16_TxLen);
                CHMipL2TxSM::SetStatus(CHMipL2TxSM::EN_Status::START_TX);
                m_status = EN_Status::TRANSMITTING;
            }
            else
            {
                CHMipL2SM::SetActiveServiceFailed();
                m_status = EN_Status::RECEIVING;
                to_do = EN_ToDo::NOTHING;
            }
            break;
        case EN_ToDo::SEND_DATA:
            to_do = EN_ToDo::NOTHING;
            parent_to_do = CHMipMacPort::EN_ToDo::SEND_DATA;
            if (mu16_TxLen > 0)
            {
                TY_Byte tx_data[MAX_TXRX_SIZE];
                TY_Byte tx_len = 0;

                CMonitor::StartTransmit(COSAL::CTimer::GetTime() + 1);
                CHMipMacPort::SetMessageType(0);
                CHMipMacPort::GetIpFrameForMonitor(tx_data, &tx_len, mpu8_TxData, (TY_Byte)mu16_TxLen);
                CMonitor::StoreData(tx_data, tx_len);
            }
            break;
        case EN_ToDo::END_TRANSMIT:
            CMonitor::EndTransmit(COSAL::CTimer::GetTime() - 1);
            to_do = CHMipL2SM::EventHandler(CHMipL2SM::EN_Event::TX_DONE, NULL);
            if (to_do == EN_ToDo::RECEIVE_ENABLE)
            {
                parent_to_do = CHMipMacPort::EN_ToDo::RECEIVE_ENABLE;
                m_status = EN_Status::RECEIVING;
                to_do = EN_ToDo::NOTHING;
            }
            else
            {
                m_status = EN_Status::IDLE;
            }
            break;
        default:
            to_do = EN_ToDo::NOTHING;
            break;
        }
    }
    return parent_to_do;
}


