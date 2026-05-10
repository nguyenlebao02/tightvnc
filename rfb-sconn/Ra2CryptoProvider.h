// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// RA2 (RealVNC Authentication 2) crypto provider using Windows Crypto API.
// Wraps RSA key management, RSA PKCS#1v1.5 encrypt/decrypt,
// AES-ECB+PKCS7 encrypt/decrypt, and SHA1/SHA256 hashing.
//

#ifndef __RA2_CRYPTO_PROVIDER_H__
#define __RA2_CRYPTO_PROVIDER_H__

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincrypt.h>
#include <vector>
#include "util/inttypes.h"

class Ra2CryptoProvider
{
public:
  Ra2CryptoProvider();
  ~Ra2CryptoProvider();

  bool initialize();

  // Key management
  bool loadOrCreateKeyPair();
  void getPublicKey(UINT32 *keyLength,
                    std::vector<UINT8> *n,
                    std::vector<UINT8> *e);
  bool importClientPublicKey(UINT32 keyLength,
                             const UINT8 *n,
                             const UINT8 *e);

  // RSA PKCS#1v1.5
  std::vector<UINT8> rsaEncrypt(const std::vector<UINT8> &plaintext);
  std::vector<UINT8> rsaDecrypt(const std::vector<UINT8> &ciphertext);

  // AES-ECB PKCS7
  std::vector<UINT8> aesEncrypt(const std::vector<UINT8> &plaintext,
                                 const std::vector<UINT8> &key);
  std::vector<UINT8> aesDecrypt(const std::vector<UINT8> &ciphertext,
                                 const std::vector<UINT8> &key);

  // SHA hashing
  std::vector<UINT8> shaHash(const std::vector<UINT8> &data, bool useSha256);

  // Key derivation: SHA*(data1 || data2) truncated to keySizeBytes
  std::vector<UINT8> deriveKey(const std::vector<UINT8> &data1,
                                const std::vector<UINT8> &data2,
                                int keySizeBytes);

  // Hash for key verification: SHA*(uint32BE(k1) || n1 || e1 || uint32BE(k2) || n2 || e2)
  std::vector<UINT8> computeKeyHash(UINT32 keyLen1, const UINT8 *n1, const UINT8 *e1,
                                     UINT32 keyLen2, const UINT8 *n2, const UINT8 *e2,
                                     bool useSha256);

  // Random
  std::vector<UINT8> generateRandom(int sizeBytes);

  HCRYPTPROV getProvHandle() const { return m_hProv; }
  UINT32 getKeyLength() const { return m_keyLength; }

  // Self-test: verify RSA round-trip with manually-constructed PUBLICKEYBLOB
  bool selfTest();

private:
  bool loadKeyBlob(std::vector<BYTE> *blob);
  bool saveKeyBlob(const std::vector<BYTE> &blob);
  void reverseBytes(std::vector<UINT8> *data);
  void destroyClientKey();

  HCRYPTPROV m_hProv;
  HCRYPTKEY  m_hServerKey;
  HCRYPTKEY  m_hClientKey;
  bool       m_initialized;
  UINT32     m_keyLength;
};

#endif // __RA2_CRYPTO_PROVIDER_H__
