// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// SChannel TLS stream implementation.
// Reference: Microsoft SChannel SSPI documentation.

#include "network/socket/SchannelTlsStream.h"
#include "io-lib/IOException.h"
#include "util/StringStorage.h"

SchannelTlsStream::SchannelTlsStream(Channel *rawStream)
: m_rawStream(rawStream),
  m_certContext(NULL),
  m_handshakeDone(false),
  m_closed(false),
  m_extraOffset(0)
{
  SecInvalidateHandle(&m_credHandle);
  SecInvalidateHandle(&m_ctxtHandle);
  memset(&m_streamSizes, 0, sizeof(m_streamSizes));
}

SchannelTlsStream::~SchannelTlsStream()
{
  try {
    if (!m_closed) {
      sendShutdown();
    }
  } catch (...) {
  }

  if (SecIsValidHandle(&m_ctxtHandle)) {
    DeleteSecurityContext(&m_ctxtHandle);
  }
  if (SecIsValidHandle(&m_credHandle)) {
    FreeCredentialHandle(&m_credHandle);
  }
  if (m_certContext) {
    CertFreeCertificateContext(m_certContext);
  }
}

PCCERT_CONTEXT SchannelTlsStream::createSelfSignedCert()
{
  // Build subject name: CN=BaoVNC
  const wchar_t *subjectName = L"CN=BaoVNC";
  DWORD nameLen = 0;
  CertStrToName(X509_ASN_ENCODING, subjectName, CERT_X500_NAME_STR, NULL, NULL, &nameLen, NULL);
  std::vector<BYTE> nameBuf(nameLen);
  if (!CertStrToName(X509_ASN_ENCODING, subjectName, CERT_X500_NAME_STR, NULL, nameBuf.data(), &nameLen, NULL)) {
    throw IOException(_T("TLS: CertStrToName failed"));
  }

  CERT_NAME_BLOB subjectBlob;
  subjectBlob.cbData = nameLen;
  subjectBlob.pbData = nameBuf.data();

  PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(0, &subjectBlob, 0, NULL, NULL, NULL, NULL, NULL);
  if (!cert) {
    DWORD err = GetLastError();
    StringStorage msg;
    msg.format(_T("TLS: CertCreateSelfSignCertificate failed (0x%08X)"), (unsigned int)err);
    throw IOException(msg.getString());
  }

  return cert;
}

void SchannelTlsStream::initSChannelCred()
{
  SCHANNEL_CRED cred = {0};
  cred.dwVersion = SCHANNEL_CRED_VERSION;
  cred.cCreds = 1;
  cred.paCred = &m_certContext;
  cred.grbitEnabledProtocols = SP_PROT_TLS1_0_SERVER | SP_PROT_TLS1_1_SERVER | SP_PROT_TLS1_2_SERVER;
  cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER
               | SCH_CRED_NO_DEFAULT_CREDS
               | SCH_CRED_MANUAL_CRED_VALIDATION
               | SCH_SEND_ROOT_CERT;

  TimeStamp tsExpiry;
  SECURITY_STATUS status = AcquireCredentialsHandle(
    NULL,
    (LPTSTR)UNISP_NAME,
    SECPKG_CRED_INBOUND,
    NULL,
    &cred,
    NULL,
    NULL,
    &m_credHandle,
    &tsExpiry);

  if (status != SEC_E_OK) {
    throwSecurityError(status, "AcquireCredentialsHandle");
  }
}

void SchannelTlsStream::doHandshakeLoop()
{
  std::vector<char> buf(16384);
  size_t bufLen = 0;

  while (true) {
    if (bufLen < buf.size()) {
      size_t n = m_rawStream->read(buf.data() + bufLen, buf.size() - bufLen);
      if (n == 0) {
        throw IOException(_T("TLS: connection closed during handshake"));
      }
      bufLen += n;
    }

    SecBuffer inBufs[2];
    inBufs[0].cbBuffer = (unsigned long)bufLen;
    inBufs[0].BufferType = SECBUFFER_TOKEN;
    inBufs[0].pvBuffer = buf.data();
    inBufs[1].cbBuffer = 0;
    inBufs[1].BufferType = SECBUFFER_EMPTY;
    inBufs[1].pvBuffer = NULL;

    SecBufferDesc inDesc;
    inDesc.ulVersion = SECBUFFER_VERSION;
    inDesc.cBuffers = 2;
    inDesc.pBuffers = inBufs;

    SecBuffer outBufs[1];
    outBufs[0].cbBuffer = 0;
    outBufs[0].BufferType = SECBUFFER_TOKEN;
    outBufs[0].pvBuffer = NULL;

    SecBufferDesc outDesc;
    outDesc.ulVersion = SECBUFFER_VERSION;
    outDesc.cBuffers = 1;
    outDesc.pBuffers = outBufs;

    DWORD attrs = 0;
    PCtxtHandle ctxPtr = SecIsValidHandle(&m_ctxtHandle) ? &m_ctxtHandle : NULL;

    DWORD fContextReq = ASC_REQ_SEQUENCE_DETECT
                      | ASC_REQ_REPLAY_DETECT
                      | ASC_REQ_CONFIDENTIALITY
                      | ASC_REQ_STREAM
                      | ASC_REQ_ALLOCATE_MEMORY;

    SECURITY_STATUS status = AcceptSecurityContext(
      &m_credHandle,
      ctxPtr,
      &inDesc,
      fContextReq,
      0,
      &m_ctxtHandle,
      &outDesc,
      &attrs,
      NULL);

    if (outBufs[0].cbBuffer > 0 && outBufs[0].pvBuffer) {
      m_rawStream->write(outBufs[0].pvBuffer, outBufs[0].cbBuffer);
      FreeContextBuffer(outBufs[0].pvBuffer);
    }

    if (status == SEC_E_OK) {
      break;
    }

    if (status == SEC_I_CONTINUE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
      if (inBufs[1].BufferType == SECBUFFER_EXTRA && inBufs[1].cbBuffer > 0) {
        memmove(buf.data(), inBufs[1].pvBuffer, inBufs[1].cbBuffer);
        bufLen = inBufs[1].cbBuffer;
      } else {
        bufLen = 0;
      }
    } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
      continue;
    } else {
      throwSecurityError(status, "AcceptSecurityContext");
    }
  }

  // Get stream sizes
  SECURITY_STATUS status = QueryContextAttributes(
    &m_ctxtHandle, SECPKG_ATTR_STREAM_SIZES, &m_streamSizes);
  if (status != SEC_E_OK) {
    throwSecurityError(status, "QueryContextAttributes");
  }
}

void SchannelTlsStream::performHandshake()
{
  if (m_handshakeDone) return;

  m_certContext = createSelfSignedCert();
  initSChannelCred();

  try {
    doHandshakeLoop();
    m_handshakeDone = true;
  } catch (...) {
    if (m_certContext) {
      CertFreeCertificateContext(m_certContext);
      m_certContext = NULL;
    }
    if (SecIsValidHandle(&m_ctxtHandle)) {
      DeleteSecurityContext(&m_ctxtHandle);
      SecInvalidateHandle(&m_ctxtHandle);
    }
    if (SecIsValidHandle(&m_credHandle)) {
      FreeCredentialHandle(&m_credHandle);
      SecInvalidateHandle(&m_credHandle);
    }
    throw;
  }
}

size_t SchannelTlsStream::encryptAndSend(const void *buffer, size_t len)
{
  if (len == 0) return 0;

  const size_t maxMsg = m_streamSizes.cbMaximumMessage;
  const size_t header = m_streamSizes.cbHeader;
  const size_t trailer = m_streamSizes.cbTrailer;

  const char *data = static_cast<const char *>(buffer);
  size_t totalSent = 0;

  while (totalSent < len) {
    size_t chunk = len - totalSent;
    if (chunk > maxMsg) chunk = maxMsg;

    std::vector<char> msg(header + chunk + trailer);
    memcpy(&msg[header], data + totalSent, chunk);

    SecBuffer bufs[4];
    bufs[0].cbBuffer = (unsigned long)header;
    bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
    bufs[0].pvBuffer = &msg[0];

    bufs[1].cbBuffer = (unsigned long)chunk;
    bufs[1].BufferType = SECBUFFER_DATA;
    bufs[1].pvBuffer = &msg[header];

    bufs[2].cbBuffer = (unsigned long)trailer;
    bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
    bufs[2].pvBuffer = &msg[header + chunk];

    bufs[3].cbBuffer = 0;
    bufs[3].BufferType = SECBUFFER_EMPTY;
    bufs[3].pvBuffer = NULL;

    SecBufferDesc desc;
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = bufs;

    SECURITY_STATUS status = EncryptMessage(&m_ctxtHandle, 0, &desc, 0);
    if (status != SEC_E_OK) {
      throwSecurityError(status, "EncryptMessage");
    }

    for (int i = 0; i < 4; i++) {
      if (bufs[i].cbBuffer > 0 && bufs[i].pvBuffer) {
        m_rawStream->write(bufs[i].pvBuffer, bufs[i].cbBuffer);
      }
    }
    totalSent += chunk;
  }

  return totalSent;
}

size_t SchannelTlsStream::receiveAndDecrypt(void *buffer, size_t len)
{
  if (m_extraOffset < m_extraBuf.size()) {
    size_t avail = m_extraBuf.size() - m_extraOffset;
    if (avail > len) avail = len;
    memcpy(buffer, &m_extraBuf[m_extraOffset], avail);
    m_extraOffset += avail;
    if (m_extraOffset >= m_extraBuf.size()) {
      m_extraBuf.clear();
      m_extraOffset = 0;
    }
    return avail;
  }

  // Read TLS record header (5 bytes)
  char tlsHeader[5];
  size_t totalRead = 0;
  while (totalRead < 5) {
    size_t n = m_rawStream->read(tlsHeader + totalRead, 5 - totalRead);
    if (n == 0) {
      throw IOException(_T("TLS: connection closed during read"));
    }
    totalRead += n;
  }

  // Parse record length (big-endian, bytes 3-4)
  size_t recordLen = ((unsigned char)tlsHeader[3] << 8) | (unsigned char)tlsHeader[4];
  if (recordLen > 18432) { // 16KB max + some overhead
    throw IOException(_T("TLS: record length too large"));
  }

  std::vector<char> record(5 + recordLen);
  memcpy(&record[0], tlsHeader, 5);

  totalRead = 0;
  while (totalRead < recordLen) {
    size_t n = m_rawStream->read(&record[5 + totalRead], recordLen - totalRead);
    if (n == 0) {
      throw IOException(_T("TLS: connection closed during record read"));
    }
    totalRead += n;
  }

  SecBuffer bufs[4];
  bufs[0].cbBuffer = (unsigned long)(5 + recordLen);
  bufs[0].BufferType = SECBUFFER_DATA;
  bufs[0].pvBuffer = &record[0];

  bufs[1].cbBuffer = 0;
  bufs[1].BufferType = SECBUFFER_EMPTY;
  bufs[1].pvBuffer = NULL;

  bufs[2].cbBuffer = 0;
  bufs[2].BufferType = SECBUFFER_EMPTY;
  bufs[2].pvBuffer = NULL;

  bufs[3].cbBuffer = 0;
  bufs[3].BufferType = SECBUFFER_EMPTY;
  bufs[3].pvBuffer = NULL;

  SecBufferDesc desc;
  desc.ulVersion = SECBUFFER_VERSION;
  desc.cBuffers = 4;
  desc.pBuffers = bufs;

  SECURITY_STATUS status = DecryptMessage(&m_ctxtHandle, &desc, 0, NULL);
  if (status != SEC_E_OK && status != SEC_I_CONTEXT_EXPIRED) {
    throwSecurityError(status, "DecryptMessage");
  }

  // Find decrypted data
  char *decData = NULL;
  size_t decLen = 0;
  for (int i = 0; i < 4; i++) {
    if (bufs[i].BufferType == SECBUFFER_DATA && bufs[i].pvBuffer) {
      decData = static_cast<char *>(bufs[i].pvBuffer);
      decLen = bufs[i].cbBuffer;
      break;
    }
  }

  if (!decData || decLen == 0) {
    return 0;
  }

  size_t toCopy = len < decLen ? len : decLen;
  memcpy(buffer, decData, toCopy);

  if (toCopy < decLen) {
    m_extraBuf.resize(decLen - toCopy);
    memcpy(&m_extraBuf[0], decData + toCopy, decLen - toCopy);
    m_extraOffset = 0;
  }

  return toCopy;
}

size_t SchannelTlsStream::read(void *buffer, size_t len)
{
  if (!m_handshakeDone) {
    throw IOException(_T("TLS: read before handshake"));
  }
  return receiveAndDecrypt(buffer, len);
}

size_t SchannelTlsStream::write(const void *buffer, size_t len)
{
  if (!m_handshakeDone) {
    throw IOException(_T("TLS: write before handshake"));
  }
  return encryptAndSend(buffer, len);
}

size_t SchannelTlsStream::available()
{
  return m_rawStream->available();
}

void SchannelTlsStream::close() throw(Exception)
{
  if (!m_closed) {
    m_closed = true;
    try {
      sendShutdown();
    } catch (...) {
    }
    try {
      m_rawStream->close();
    } catch (...) {
    }
  }
}

void SchannelTlsStream::sendShutdown()
{
  if (!m_handshakeDone) return;
  if (!SecIsValidHandle(&m_ctxtHandle)) return;

  DWORD shutdownToken = 1; // SCHANNEL_SHUTDOWN = 1

  SecBuffer buf;
  buf.cbBuffer = sizeof(shutdownToken);
  buf.BufferType = SECBUFFER_TOKEN;
  buf.pvBuffer = &shutdownToken;

  SecBufferDesc desc;
  desc.ulVersion = SECBUFFER_VERSION;
  desc.cBuffers = 1;
  desc.pBuffers = &buf;

  SECURITY_STATUS status = ApplyControlToken(&m_ctxtHandle, &desc);
  if (status != SEC_E_OK) return;

  // Generate shutdown TLS record
  const size_t header = m_streamSizes.cbHeader;
  const size_t trailer = m_streamSizes.cbTrailer;
  std::vector<char> msg(header + trailer);

  SecBuffer bufs[4];
  bufs[0].cbBuffer = (unsigned long)header;
  bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
  bufs[0].pvBuffer = &msg[0];

  bufs[1].cbBuffer = 0;
  bufs[1].BufferType = SECBUFFER_DATA;
  bufs[1].pvBuffer = NULL;

  bufs[2].cbBuffer = (unsigned long)trailer;
  bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
  bufs[2].pvBuffer = &msg[header];

  bufs[3].cbBuffer = 0;
  bufs[3].BufferType = SECBUFFER_EMPTY;
  bufs[3].pvBuffer = NULL;

  SecBufferDesc encDesc;
  encDesc.ulVersion = SECBUFFER_VERSION;
  encDesc.cBuffers = 4;
  encDesc.pBuffers = bufs;

  status = EncryptMessage(&m_ctxtHandle, 0, &encDesc, 0);
  if (status == SEC_E_OK) {
    for (int i = 0; i < 4; i++) {
      if (bufs[i].cbBuffer > 0 && bufs[i].pvBuffer) {
        m_rawStream->write(bufs[i].pvBuffer, bufs[i].cbBuffer);
      }
    }
  }
}

void SchannelTlsStream::throwSecurityError(SECURITY_STATUS status, const char *context)
{
  StringStorage msg;
  msg.format(_T("TLS %hs failed: 0x%08X"), context, (unsigned int)status);
  throw IOException(msg.getString());
}
