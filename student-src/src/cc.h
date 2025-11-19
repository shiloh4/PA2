#pragma once

#include <ccc.h>
#include <chrono>
#include <map>
#include <udt.h>

/// Your Vegas congestion control implementation
///
/// * Start by declaring the required variables in the `Vegas` class. See the
///   comments in the `Vegas::init()` function for details.
/// * Implement the `Vegas::onACK()` function to update the congestion window
///   size.
/// * Tune the alpha, beta, and linear increase factors in the `Vegas::init()`
///   function. It may take multiple tries to find the right values.
///
/// When you are ready to test your implementation, upload this file -- complete
/// with your implementation -- to Gradescope.
class Vegas : public CCC {
  public:
    void init() {
        // Variables we initialize for you
        m_dCWndSize = 5.0;
        m_dBaseRTT = 10000;
        m_iMSS = 1000;

        // *********************************************************************
        // Add additional initializations below the comments.
        //
        // Your Vegas implementation may require the following variables:
        // - `m_dAlpha` is the alpha factor used in the Vegas congestion control
        //   algorithm. It is used to determine when to increase the congestion
        //   window size.
        // - `m_dBeta` is the beta factor used in the Vegas congestion control
        //   algorithm. It is used to determine when to decrease the congestion
        //   window size.
        // - `m_dLinearIncreaseFactor` is the linear increase factor used in the
        //   Vegas congestion control algorithm. It is used to determine how
        //   much to increase the congestion window size when the difference
        //   between the expected and actual throughput is below the alpha
        //   factor or how much to decrease the congestion window size when the
        //   difference is above the beta factor.
        //
        // Declare these variables in the `protected` section of the `Vegas`
        // class, then assign them values in this function. A reasonable
        // starting point for these variables is:
        // - `m_dAlpha` = 1.0 * (1024.0)
        // - `m_dBeta` = 50.0 * (1024.0)
        // - `m_dLinearIncreaseFactor` = 5.0 / (1024.0)
        //
        // Think about what these values mean and how they will affect the
        // performance of your CC algorithm.
        //
        // After you implemented the `Vegas::onACK()` function, come back to
        // this function to tune the alpha, beta, and linear increase factors.

        // TODO: Initialize the additional variables that you declared
        m_dAlpha = 10.0 * 1024.0;
        m_dBeta = 70.0 * 1024.0;
        m_dLinearIncreaseFactor = 0.1 / 1024.0;

        // Complete your implementation above this line
        // *********************************************************************
    }

    void onACK(int32_t ack) {
        // Convert the estimated RTT provided by UDT4 (stored in m_iRTT) in
        // microseconds to be in milliseconds.
        m_dCurrentRTT = m_iRTT / 1000.0;

        // Since base RTT should keep track of the uncongested RTT, update the
        // variable if our current measured RTT is below the previously recorded
        // base RTT.
        if (m_dCurrentRTT < m_dBaseRTT) {
            m_dBaseRTT = m_dCurrentRTT;
        }

        // *********************************************************************
        // Add your logics below the comments. Your implementation should:
        //
        // 1. Obtain the current window size (`CWND`). You may be interested in
        //    a few variables provided to you by the `ccc.h` header file in
        //    `udt4/`, namely:
        //    - `m_dCWndSize` provides the current congestion window size in the
        //      number of packets.
        //    - `m_iMSS` provides the segment size, which is the max number of
        //      bytes in a single packet.
        //    Your calculated `CWND` should have the unit in bytes.
        //
        // 2. Calculate the expected throughput in bytes per second. Keep in
        //    mind that `m_dCurrentRTT` and `m_dBaseRTT` are in milliseconds, so
        //    you may need to do some unit conversions here:
        //    - Expected throughput = (CWND * 1000) / base RTT (in milliseconds)
        //
        // 3. Calculate the actual throughput in bytes per second. In a
        //    traditional Vegas implementation, the actual throughput should be
        //    (number of bytes in flight / current RTT).
        //    - You may be interested in the `m_iSndCurrSeqNo` variable in
        //      `ccc.h`. This tells you the last sequence number that was
        //      recently sent out.
        //    - You know the sequence number of the current ACKed packet from
        //      the `ack` parameter.
        //    - Actual throughput = (bytes inflight * 1000) / current RTT (in
        //      milliseconds)
        //
        // 4. Compare the differences between the expected and actual throughput
        //    to your alpha and beta factors.
        //
        // 5. If the difference is below your alpha factor, increase the window
        //    size by (difference * linear factor). If the difference is above
        //    your beta factor, decrease the window size by (difference * linear
        //    factor). Otherwise, do nothing. You can change the current window
        //    size by modifying the `m_dCWndSize` variable.

        // TODO: Implement Vegas::onACK()
        if (m_dBaseRTT > 0.0 && m_dCurrentRTT > 0.0) {
            const double cwnd_bytes = m_dCWndSize * static_cast<double>(m_iMSS); // 1
            const double expected_throughput = (cwnd_bytes * 1000.0) / m_dBaseRTT; // 2

            long long unacked_pkts = static_cast<long long>(m_iSndCurrSeqNo) - static_cast<long long>(ack);
            if (unacked_pkts < 0) {
                unacked_pkts = 0;
            }
            const double bytes_in_flight = static_cast<double>(unacked_pkts) * static_cast<double>(m_iMSS);
            const double actual_throughput = (bytes_in_flight * 1000.0) / m_dCurrentRTT; // 3

            double diff = expected_throughput - actual_throughput; //4
            if (diff < 0.0) {
                diff = 0.0;
            }

            if (diff < m_dAlpha) { // 5
                m_dCWndSize += diff * m_dLinearIncreaseFactor;
            } else if (diff > m_dBeta) {
                m_dCWndSize -= diff * m_dLinearIncreaseFactor;
            }
        }

        // Complete your implementation above this line
        // *********************************************************************

        // Leave the following three lines at the end of the `onACK` function.
        // This will ensure that the packets never stop flowing.
        if (m_dCWndSize < 2) {
            m_dCWndSize = 2;
        }
    }

  protected:
    /// The lowest measured RTT value in millisecond that we have seen so far.
    ///
    /// This value will be used as the base RTT in the Vegas congestion control
    /// algorithm.
    double m_dBaseRTT;

    /// Recorded RTT of the current packet in milliseconds.
    double m_dCurrentRTT;

    // *************************************************************************
    // Add additional variables for your implementation below.

    // TODO: Declare additional variables required for your Vegas implementation
    double m_dAlpha;
    double m_dBeta;
    double m_dLinearIncreaseFactor;

    // Complete your implementation above this line
    // *************************************************************************
};
