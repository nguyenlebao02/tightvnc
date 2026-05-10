// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// RA2 protocol handshake implementation.
// Reference: TigerVNC SSecurityRSAAES.cxx, adapted for Windows Crypto API.

#include "Ra2Authenticator.h"
#include "AuthException.h"
#include "util/AnsiStringStorage.h"

Ra2Authenticator::Ra2Authenticator(DataInputStream *input,
                                     DataOutputStream *output,
                                     Ra2CryptoProvider *crypto)
: m_input(input), m_output(output), m_crypto(crypto),
  m_clientKeyLength(0), m_aesKeySize(0)
{
}

bool Ra2Authenticator::performHandshake(StringStorage *username,
                                         StringStorage *password,
                                         int aesKeySize)
{
  m_aesKeySize = aesKeySize;

  if (!m_crypto->getProvHandle()) return false;
  if (m_crypto->getKeyLength() == 0) return false;

  sendPublicKey();
  receivePublicKey();
  writeRandom();
  readRandom();
  deriveKeys();
  writeHash();
  readHash();
  writeSubtype();
  readCredentials(username, password);
  return true;
}

void Ra2Authenticator::sendPublicKey()
{
  UINT32 keyLength = 0;
  std::vector<UINT8> n, e;
  m_crypto->getPublicKey(&keyLength, &n, &e);

  // Wire format: UINT32 keyBits + N(keyLength) + E(keyLength)
  // E is keyLength bytes big-endian (left-padded with zeros)
  m_output->writeUInt32(keyLength * 8);
  m_output->writeFully(n.data(), n.size());
  m_output->writeFully(e.data(), e.size());
}

void Ra2Authenticator::receivePublicKey()
{
  // Wire format: UINT32 keyBits + N(keyLength) + E(keyLength)
  m_clientKeyLength = m_input->readUInt32() / 8;
  if (m_clientKeyLength == 0 || m_clientKeyLength > 512) {
    throw AuthException(_T("RA2: invalid client key length"));
  }

  m_clientN.resize(m_clientKeyLength);
  m_clientE.resize(m_clientKeyLength);
  m_input->readFully(m_clientN.data(), m_clientKeyLength);
  // E is keyLength bytes big-endian (left-padded, exponent in last 4 bytes)
  m_input->readFully(m_clientE.data(), m_clientKeyLength);

  if (!m_crypto->importClientPublicKey(m_clientKeyLength,
                                        m_clientN.data(), m_clientE.data())) {
    throw AuthException(_T("RA2: failed to import client public key"));
  }
}

void Ra2Authenticator::writeRandom()
{
  m_serverRandom = m_crypto->generateRandom(m_aesKeySize);
  if (m_serverRandom.empty()) {
    throw AuthException(_T("RA2: failed to generate random"));
  }

  std::vector<UINT8> ciphertext = m_crypto->rsaEncrypt(m_serverRandom);
  if (ciphertext.empty()) {
    throw AuthException(_T("RA2: RSA encryption failed"));
  }

  m_output->writeUInt16((UINT16)ciphertext.size());
  m_output->writeFully(ciphertext.data(), ciphertext.size());
}

void Ra2Authenticator::readRandom()
{
  UINT16 len = m_input->readUInt16();
  if (len == 0 || len > 512) {
    throw AuthException(_T("RA2: invalid random length from client"));
  }

  std::vector<UINT8> encrypted(len);
  m_input->readFully(encrypted.data(), len);

  m_clientRandom = m_crypto->rsaDecrypt(encrypted);
  if (m_clientRandom.empty()) {
    throw AuthException(_T("RA2: RSA decryption of client random failed"));
  }
}

void Ra2Authenticator::deriveKeys()
{
  m_serverKey = m_crypto->deriveKey(m_clientRandom, m_serverRandom, m_aesKeySize);
  m_clientKey = m_crypto->deriveKey(m_serverRandom, m_clientRandom, m_aesKeySize);

  if (m_serverKey.empty() || m_clientKey.empty()) {
    throw AuthException(_T("RA2: key derivation failed"));
  }
}

void Ra2Authenticator::writeHash()
{
  UINT32 serverKeyLen = m_crypto->getKeyLength();
  UINT32 dummyLen;
  std::vector<UINT8> serverN, serverE;
  m_crypto->getPublicKey(&dummyLen, &serverN, &serverE);

  bool useSha256 = (m_aesKeySize == 32);
  std::vector<UINT8> hash = m_crypto->computeKeyHash(
    serverKeyLen, serverN.data(), serverE.data(),
    m_clientKeyLength, m_clientN.data(), m_clientE.data(),
    useSha256);

  std::vector<UINT8> encrypted = m_crypto->aesEncrypt(hash, m_serverKey);
  if (encrypted.empty()) {
    throw AuthException(_T("RA2: hash encryption failed"));
  }
  m_output->writeFully(encrypted.data(), encrypted.size());
}

void Ra2Authenticator::readHash()
{
  bool useSha256 = (m_aesKeySize == 32);
  UINT32 serverKeyLen = m_crypto->getKeyLength();
  UINT32 dummyLen;
  std::vector<UINT8> serverN, serverE;
  m_crypto->getPublicKey(&dummyLen, &serverN, &serverE);

  std::vector<UINT8> expectedHash = m_crypto->computeKeyHash(
    m_clientKeyLength, m_clientN.data(), m_clientE.data(),
    serverKeyLen, serverN.data(), serverE.data(),
    useSha256);

  // AES-ECB encrypted size = (hashLen / 16 + 1) * 16 due to PKCS7 padding
  int hashLen = useSha256 ? 32 : 20;
  int encryptedLen = ((hashLen / 16) + 1) * 16;
  std::vector<UINT8> encrypted(encryptedLen);
  m_input->readFully(encrypted.data(), encryptedLen);

  std::vector<UINT8> decrypted = m_crypto->aesDecrypt(encrypted, m_clientKey);
  if (decrypted.empty()) {
    throw AuthException(_T("RA2: hash decryption failed"));
  }

  if (decrypted.size() < (size_t)hashLen ||
      memcmp(decrypted.data(), expectedHash.data(), hashLen) != 0) {
    throw AuthException(_T("RA2: key verification hash mismatch"));
  }
}

void Ra2Authenticator::writeSubtype()
{
  std::vector<UINT8> subtype(1);
  subtype[0] = 0x01; // secTypeRA2UserPass
  std::vector<UINT8> encrypted = m_crypto->aesEncrypt(subtype, m_serverKey);
  if (encrypted.empty()) {
    throw AuthException(_T("RA2: subtype encryption failed"));
  }
  m_output->writeFully(encrypted.data(), encrypted.size());
}

void Ra2Authenticator::readCredentials(StringStorage *username,
                                        StringStorage *password)
{
  // Read length prefix (UINT16 big-endian) then encrypted data
  UINT16 encLen = m_input->readUInt16();
  if (encLen == 0 || encLen > 512) {
    throw AuthException(_T("RA2: invalid credential length"));
  }

  std::vector<UINT8> encrypted(encLen);
  m_input->readFully(encrypted.data(), encLen);

  // Decrypt entire blob at once (handles PKCS7 padding correctly)
  std::vector<UINT8> combined = m_crypto->aesDecrypt(encrypted, m_clientKey);
  if (combined.empty()) {
    throw AuthException(_T("RA2: credential decryption failed"));
  }

  if (combined.size() < 3) {
    throw AuthException(_T("RA2: credential block too short"));
  }

  UINT8 userLen = combined[0];
  if (userLen == 0 || userLen > 128 || (size_t)(1 + userLen + 1) > combined.size()) {
    throw AuthException(_T("RA2: invalid username length"));
  }

  std::vector<char> userBuf(userLen + 1, 0);
  memcpy(userBuf.data(), &combined[1], userLen);

  size_t passOff = 1 + userLen;
  UINT8 passLen = combined[passOff];
  if (passLen == 0 || passLen > 128 ||
      passOff + 1 + passLen > combined.size()) {
    throw AuthException(_T("RA2: invalid password length"));
  }

  std::vector<char> passBuf(passLen + 1, 0);
  memcpy(passBuf.data(), &combined[passOff + 1], passLen);

  AnsiStringStorage ansiUser(userBuf.data());
  ansiUser.toStringStorage(username);

  AnsiStringStorage ansiPass(passBuf.data());
  ansiPass.toStringStorage(password);
}
