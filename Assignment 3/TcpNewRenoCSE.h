#ifndef TCPNEWRENOCSE_H
#define TCPNEWRENOCSE_H

#include "ns3/tcp-congestion-ops.h"


namespace ns3 {

/**
 * \ingroup congestionOps
 *
 * \brief Modified TCP NewReno for COL334 assignment 3 part 3
*/
class TcpNewRenoCSE : public TcpCongestionOps
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  TcpNewRenoCSE ();

  /**
   * \brief Copy constructor.
   * \param sock object to copy.
   */
  TcpNewRenoCSE (const TcpNewRenoCSE& sock);

  ~TcpNewRenoCSE ();

  std::string GetName () const;

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);

  virtual Ptr<TcpCongestionOps> Fork ();

protected:
  virtual uint32_t SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
};

} // namespace ns3

#endif // TCPNEWRENOCSE_H