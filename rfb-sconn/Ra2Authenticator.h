// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// RA2 (RealVNC Authentication 2) protocol handler.
// Implements the 10-step RSA+AES handshake for secure credential exchange.

#ifndef __RA2_AUTHENTICATOR_H__
#define __RA2_AUTHENTICATOR_H__

#include "Ra2CryptoProvider.h"
#include "io-lib/DataInputStream.h"
#include "io-lib/DataOutputStream.h"
#include "util/StringStorage.h"

class Ra2Authenticator
{
public:
  Ra2Authenticator(DataInputStream *input, DataOutputStream *output,
                   Ra2CryptoProvider *crypto);

  // Perform full RA2 handshake. aesKeySize: 16 for RA2 (SHA1/AES-128),
  // 32 for RA2_256 (SHA256/AES-256).
  // On success returns true and populates username/password.
  bool performHandshake(StringStorage *username, StringStorage *password,
                        int aesKeySize);

private:
  void sendPublicKey();
  void receivePublicKey();
  void writeRandom();
  void readRandom();
  void deriveKeys();
  void writeHash();
  void readHash();
  void writeSubtype();
  void readCredentials(StringStorage *username, StringStorage *password);

  DataInputStream *m_input;
  DataOutputStream *m_output;
  Ra2CryptoProvider *m_crypto;

  UINT32 m_clientKeyLength;
  std::vector<UINT8> m_clientN;
  std::vector<UINT8> m_clientE;
  std::vector<UINT8> m_serverRandom;
  std::vector<UINT8> m_clientRandom;
  std::vector<UINT8> m_serverKey;
  std::vector<UINT8> m_clientKey;
  int m_aesKeySize;
};

#endif // __RA2_AUTHENTICATOR_H__
