// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// SChannel-based TLS stream wrapping a raw TCP Channel.
// Provides encrypted read/write via Windows SSPI/SChannel.

#ifndef SCHANNEL_TLS_STREAM_H
#define SCHANNEL_TLS_STREAM_H

#include "io-lib/Channel.h"
#define SECURITY_WIN32
#include <windows.h>
#include <schannel.h>
#include <security.h>
#include <wincrypt.h>
#include <vector>

class SchannelTlsStream : public Channel
{
public:
  SchannelTlsStream(Channel *rawStream);
  virtual ~SchannelTlsStream();

  void performHandshake();

  virtual size_t read(void *buffer, size_t len);
  virtual size_t write(const void *buffer, size_t len);
  virtual void close() throw(Exception);
  virtual size_t available();

private:
  SchannelTlsStream(const SchannelTlsStream &);
  SchannelTlsStream &operator=(const SchannelTlsStream &);

  PCCERT_CONTEXT createSelfSignedCert();
  void initSChannelCred();
  void doHandshakeLoop();
  size_t encryptAndSend(const void *buffer, size_t len);
  size_t receiveAndDecrypt(void *buffer, size_t len);
  void sendShutdown();
  void throwSecurityError(SECURITY_STATUS status, const char *context);

  Channel *m_rawStream;
  CredHandle m_credHandle;
  CtxtHandle m_ctxtHandle;
  PCCERT_CONTEXT m_certContext;
  bool m_handshakeDone;
  bool m_closed;

  SecPkgContext_StreamSizes m_streamSizes;

  std::vector<char> m_extraBuf;
  size_t m_extraOffset;
};

#endif
