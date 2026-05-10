// Copyright (C) 2024 BaoVNC
// All rights reserved.
//
// RA2 crypto provider implementation using Windows Crypto API (wincrypt.h).

#include "Ra2CryptoProvider.h"
#include "win-system/Environment.h"
#include "util/StringStorage.h"
#include <algorithm>
#include <stdio.h>

static void LogHex(const char *label, const UINT8 *data, int len) {
  char path[MAX_PATH];
  GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
  strcat_s(path, "\\ra2-debug.log");
  FILE *f = fopen(path, "a");
  if (!f) return;
  fprintf(f, "%s (%d): ", label, len);
  for (int i = 0; i < len && i < 256; i++) fprintf(f, "%02x", data[i]);
  fprintf(f, "\n");
  fclose(f);
}

static void LogMsg(const char *msg) {
  char path[MAX_PATH];
  GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
  strcat_s(path, "\\ra2-debug.log");
  FILE *f = fopen(path, "a");
  if (f) { fputs(msg, f); fclose(f); }
}

static void LogLastError(const char *op) {
  DWORD err = GetLastError();
  char buf[256];
  sprintf_s(buf, "%s failed: 0x%08x\n", op, err);
  LogMsg(buf);
}

Ra2CryptoProvider::Ra2CryptoProvider()
: m_hProv(0), m_hServerKey(0), m_hClientKey(0),
  m_initialized(false), m_keyLength(0)
{
}

Ra2CryptoProvider::~Ra2CryptoProvider()
{
  destroyClientKey();
  if (m_hServerKey) {
    CryptDestroyKey(m_hServerKey);
  }
  if (m_hProv) {
    CryptReleaseContext(m_hProv, 0);
  }
}

bool Ra2CryptoProvider::initialize()
{
  if (m_initialized) return true;

  // Use named persistent key container so private keys survive across
  // sessions and CryptImportKey works with full private key access.
  // CRYPT_VERIFYCONTEXT does NOT support private key operations.
  LPCTSTR containerName = _T("BaoVNC_RA2_Keys");

  if (!CryptAcquireContext(&m_hProv, containerName, MS_ENH_RSA_AES_PROV,
                           PROV_RSA_AES, 0)) {
    DWORD err = GetLastError();
    if (err == NTE_BAD_KEYSET) {
      // Container doesn't exist yet — create it
      if (!CryptAcquireContext(&m_hProv, containerName, MS_ENH_RSA_AES_PROV,
                               PROV_RSA_AES, CRYPT_NEWKEYSET)) {
        LogLastError("CryptAcquireContext (new keyset)");
        return false;
      }
    } else {
      LogLastError("CryptAcquireContext (open)");
      return false;
    }
  }
  m_initialized = true;
  return true;
}

// === Key Management ===

bool Ra2CryptoProvider::loadOrCreateKeyPair()
{
  // Try to retrieve an existing persisted key from the named container
  if (CryptGetUserKey(m_hProv, AT_KEYEXCHANGE, &m_hServerKey)) {
    DWORD keyLenBits = 0;
    DWORD dataLen = sizeof(keyLenBits);
    CryptGetKeyParam(m_hServerKey, KP_KEYLEN, (BYTE*)&keyLenBits, &dataLen, 0);
    m_keyLength = keyLenBits / 8;
    LogMsg("loadOrCreateKeyPair: loaded existing key from container\n");
    return true;
  }

  // Also try signature key slot
  if (CryptGetUserKey(m_hProv, AT_SIGNATURE, &m_hServerKey)) {
    DWORD keyLenBits = 0;
    DWORD dataLen = sizeof(keyLenBits);
    CryptGetKeyParam(m_hServerKey, KP_KEYLEN, (BYTE*)&keyLenBits, &dataLen, 0);
    m_keyLength = keyLenBits / 8;
    LogMsg("loadOrCreateKeyPair: loaded existing key from container (SIG)\n");
    return true;
  }

  LogMsg("loadOrCreateKeyPair: no persisted key, generating new\n");

  // Generate new 2048-bit RSA key pair (persisted automatically in named container)
  if (!CryptGenKey(m_hProv, AT_KEYEXCHANGE, 2048 << 16 | CRYPT_EXPORTABLE, &m_hServerKey)) {
    LogLastError("CryptGenKey");
    return false;
  }

  m_keyLength = 256; // 2048 bits

  char buf[128];
  sprintf_s(buf, "loadOrCreateKeyPair: generated m_hServerKey=0x%p\n", (void*)m_hServerKey);
  LogMsg(buf);

  return true;
}

void Ra2CryptoProvider::getPublicKey(UINT32 *keyLength,
                                      std::vector<UINT8> *n,
                                      std::vector<UINT8> *e)
{
  *keyLength = m_keyLength;

  // Export PUBLICKEYBLOB
  DWORD blobSize = 0;
  CryptExportKey(m_hServerKey, 0, PUBLICKEYBLOB, 0, NULL, &blobSize);
  std::vector<BYTE> blob(blobSize);
  CryptExportKey(m_hServerKey, 0, PUBLICKEYBLOB, 0, blob.data(), &blobSize);

  // Parse blob
  BLOBHEADER *hdr = (BLOBHEADER*)blob.data();
  RSAPUBKEY *rsaKey = (RSAPUBKEY*)(hdr + 1);
  BYTE *modulusLE = (BYTE*)(rsaKey + 1);
  DWORD modSize = rsaKey->bitlen / 8;

  // N: little-endian in blob → big-endian for wire
  n->resize(modSize);
  for (DWORD i = 0; i < modSize; i++) {
    (*n)[i] = modulusLE[modSize - 1 - i];
  }

  // E: DWORD → keyLength bytes big-endian (padded with leading zeros)
  e->resize(modSize);
  memset(e->data(), 0, modSize);
  (*e)[modSize - 1] = (BYTE)(rsaKey->pubexp & 0xFF);
  (*e)[modSize - 2] = (BYTE)((rsaKey->pubexp >> 8) & 0xFF);
  (*e)[modSize - 3] = (BYTE)((rsaKey->pubexp >> 16) & 0xFF);
  (*e)[modSize - 4] = (BYTE)((rsaKey->pubexp >> 24) & 0xFF);
}

bool Ra2CryptoProvider::importClientPublicKey(UINT32 keyLength,
                                               const UINT8 *n,
                                               const UINT8 *e)
{
  destroyClientKey();

  DWORD blobSize = sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) + keyLength;
  std::vector<BYTE> blob(blobSize);

  BLOBHEADER *hdr = (BLOBHEADER*)blob.data();
  hdr->bType = PUBLICKEYBLOB;
  hdr->bVersion = CUR_BLOB_VERSION;
  hdr->reserved = 0;
  hdr->aiKeyAlg = CALG_RSA_KEYX;

  RSAPUBKEY *rsaKey = (RSAPUBKEY*)(hdr + 1);
  rsaKey->magic = 0x31415352; // "RSA1"
  rsaKey->bitlen = keyLength * 8;
  rsaKey->pubexp = ((DWORD)e[keyLength - 1]) |
                   ((DWORD)e[keyLength - 2] << 8) |
                   ((DWORD)e[keyLength - 3] << 16) |
                   ((DWORD)e[keyLength - 4] << 24);

  BYTE *modulusLE = (BYTE*)(rsaKey + 1);
  for (UINT32 i = 0; i < keyLength; i++) {
    modulusLE[i] = n[keyLength - 1 - i];
  }

  LogHex("importClient N BE", n, (int)keyLength);
  LogHex("importClient modulusLE", modulusLE, (int)keyLength);
  {
    char msg[128];
    sprintf_s(msg, "importClient keyLen=%u bitlen=%u pubexp=0x%x\n", keyLength, rsaKey->bitlen, rsaKey->pubexp);
    LogMsg(msg);
  }

  if (!CryptImportKey(m_hProv, blob.data(), blobSize, 0, 0, &m_hClientKey)) {
    LogLastError("CryptImportKey");
    return false;
  }

  // Verify imported key: export and compare N
  {
    DWORD verifySize = 0;
    CryptExportKey(m_hClientKey, 0, PUBLICKEYBLOB, 0, NULL, &verifySize);
    std::vector<BYTE> verifyBlob(verifySize);
    if (CryptExportKey(m_hClientKey, 0, PUBLICKEYBLOB, 0, verifyBlob.data(), &verifySize)) {
      BLOBHEADER *vHdr = (BLOBHEADER*)verifyBlob.data();
      RSAPUBKEY *vRsa = (RSAPUBKEY*)(vHdr + 1);
      BYTE *vMod = (BYTE*)(vRsa + 1);
      DWORD vModSize = vRsa->bitlen / 8;
      LogHex("Verify-imported N (LE)", vMod, (int)vModSize);
      {
        char msg2[128];
        sprintf_s(msg2, "Verify-imported: bitlen=%u pubexp=0x%x blobSize=%u\n", vRsa->bitlen, vRsa->pubexp, verifySize);
        LogMsg(msg2);
      }
      // Compare with expected modulusLE
      bool nMatch = (vModSize == keyLength && memcmp(vMod, modulusLE, keyLength) == 0);
      {
        char msg[128];
        sprintf_s(msg, "Verify-imported N matches expected: %d\n", nMatch);
        LogMsg(msg);
      }
      if (!nMatch) {
        LogHex("Expected modulusLE", modulusLE, (int)keyLength);
      }
    }
  }

  return true;
}

// === RSA PKCS#1v1.5 ===

std::vector<UINT8> Ra2CryptoProvider::rsaEncrypt(const std::vector<UINT8> &plaintext)
{
  // MS CryptoAPI uses PKCS#1v1.5 by default (no CRYPT_OAEP flag)
  std::vector<UINT8> buffer(plaintext.begin(), plaintext.end());
  buffer.resize(m_keyLength); // Ensure buffer large enough for RSA output
  DWORD dataLen = (DWORD)plaintext.size();

  LogHex("rsaEncrypt plain", plaintext.data(), (int)plaintext.size());
  if (!CryptEncrypt(m_hClientKey, 0, TRUE, 0, buffer.data(), &dataLen, (DWORD)buffer.size())) {
    LogLastError("CryptEncrypt");
    return std::vector<UINT8>();
  }
  buffer.resize(dataLen);
  LogHex("rsaEncrypt cipher LE (CryptoAPI raw)", buffer.data(), (int)buffer.size());

  // Diagnostic: try to decrypt with m_hServerKey (operates on LE ciphertext)
  {
    std::vector<UINT8> testDec(buffer.begin(), buffer.end());
    DWORD testLen = (DWORD)testDec.size();
    if (CryptDecrypt(m_hServerKey, 0, TRUE, 0, testDec.data(), &testLen)) {
      testDec.resize(testLen);
      LogHex("DIAG decrypt-with-srv OK", testDec.data(), (int)testDec.size());
      LogMsg("DIAG: CryptEncrypt used SERVER key!\n");
    } else {
      LogMsg("DIAG: decrypt-with-srv failed (expected)\n");
    }
  }

  // CryptoAPI returns RSA ciphertext in little-endian. Reverse to big-endian for wire.
  std::reverse(buffer.begin(), buffer.end());
  LogHex("rsaEncrypt cipher BE (wire)", buffer.data(), (int)buffer.size());

  return buffer;
}

std::vector<UINT8> Ra2CryptoProvider::rsaDecrypt(const std::vector<UINT8> &ciphertext)
{
  // Wire format is big-endian. CryptoAPI expects little-endian.
  std::vector<UINT8> buffer(ciphertext.begin(), ciphertext.end());
  std::reverse(buffer.begin(), buffer.end());
  DWORD dataLen = (DWORD)buffer.size();

  if (!CryptDecrypt(m_hServerKey, 0, TRUE, 0, buffer.data(), &dataLen)) {
    LogLastError("CryptDecrypt");
    return std::vector<UINT8>();
  }
  buffer.resize(dataLen);
  return buffer;
}

// === AES-ECB PKCS7 ===

static HCRYPTKEY importAesKey(HCRYPTPROV hProv, const std::vector<UINT8> &key)
{
  int keyBytes = (int)key.size();
  struct AesPlaintextKeyBlob {
    BLOBHEADER header;
    DWORD keySize;
    BYTE keyData[32];
  } blob;
  blob.header.bType = PLAINTEXTKEYBLOB;
  blob.header.bVersion = CUR_BLOB_VERSION;
  blob.header.reserved = 0;
  blob.header.aiKeyAlg = (keyBytes == 16) ? CALG_AES_128 : CALG_AES_256;
  blob.keySize = keyBytes;
  memcpy(blob.keyData, key.data(), keyBytes);

  HCRYPTKEY hKey = 0;
  if (!CryptImportKey(hProv, (BYTE*)&blob, sizeof(BLOBHEADER) + sizeof(DWORD) + keyBytes, 0, 0, &hKey)) {
    return 0;
  }
  DWORD mode = CRYPT_MODE_ECB;
  CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0);
  DWORD padding = PKCS5_PADDING;
  CryptSetKeyParam(hKey, KP_PADDING, (BYTE*)&padding, 0);
  return hKey;
}

std::vector<UINT8> Ra2CryptoProvider::aesEncrypt(const std::vector<UINT8> &plaintext,
                                                   const std::vector<UINT8> &key)
{
  HCRYPTKEY hAes = importAesKey(m_hProv, key);
  if (!hAes) return std::vector<UINT8>();

  std::vector<UINT8> buffer(plaintext.begin(), plaintext.end());
  // PKCS7 may add up to one block (16 bytes)
  buffer.resize(plaintext.size() + 16);
  DWORD dataLen = (DWORD)plaintext.size();
  DWORD bufSize = (DWORD)buffer.size();

  if (!CryptEncrypt(hAes, 0, TRUE, 0, buffer.data(), &dataLen, bufSize)) {
    CryptDestroyKey(hAes);
    return std::vector<UINT8>();
  }
  buffer.resize(dataLen);
  CryptDestroyKey(hAes);
  return buffer;
}

std::vector<UINT8> Ra2CryptoProvider::aesDecrypt(const std::vector<UINT8> &ciphertext,
                                                   const std::vector<UINT8> &key)
{
  HCRYPTKEY hAes = importAesKey(m_hProv, key);
  if (!hAes) return std::vector<UINT8>();

  std::vector<UINT8> buffer(ciphertext.begin(), ciphertext.end());
  DWORD dataLen = (DWORD)buffer.size();

  if (!CryptDecrypt(hAes, 0, TRUE, 0, buffer.data(), &dataLen)) {
    CryptDestroyKey(hAes);
    return std::vector<UINT8>();
  }
  buffer.resize(dataLen);
  CryptDestroyKey(hAes);
  return buffer;
}

// === SHA Hashing ===

std::vector<UINT8> Ra2CryptoProvider::shaHash(const std::vector<UINT8> &data,
                                                bool useSha256)
{
  HCRYPTHASH hHash = 0;
  ALG_ID alg = useSha256 ? CALG_SHA_256 : CALG_SHA;
  if (!CryptCreateHash(m_hProv, alg, 0, 0, &hHash)) {
    return std::vector<UINT8>();
  }
  if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0)) {
    CryptDestroyHash(hHash);
    return std::vector<UINT8>();
  }

  DWORD hashSize = useSha256 ? 32 : 20;
  DWORD dataLen = hashSize;
  std::vector<UINT8> hash(hashSize);
  if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &dataLen, 0)) {
    CryptDestroyHash(hHash);
    return std::vector<UINT8>();
  }
  CryptDestroyHash(hHash);
  hash.resize(dataLen);
  return hash;
}

// === Key Derivation ===

std::vector<UINT8> Ra2CryptoProvider::deriveKey(const std::vector<UINT8> &data1,
                                                  const std::vector<UINT8> &data2,
                                                  int keySizeBytes)
{
  std::vector<UINT8> combined;
  combined.reserve(data1.size() + data2.size());
  combined.insert(combined.end(), data1.begin(), data1.end());
  combined.insert(combined.end(), data2.begin(), data2.end());

  bool useSha256 = (keySizeBytes == 32);
  std::vector<UINT8> hash = shaHash(combined, useSha256);
  if (hash.empty()) return hash;

  hash.resize(keySizeBytes);
  return hash;
}

// === Key Hash for Verification ===

std::vector<UINT8> Ra2CryptoProvider::computeKeyHash(UINT32 keyLen1,
                                                       const UINT8 *n1,
                                                       const UINT8 *e1,
                                                       UINT32 keyLen2,
                                                       const UINT8 *n2,
                                                       const UINT8 *e2,
                                                       bool useSha256)
{
  std::vector<UINT8> data;

  UINT32 keyBits1 = keyLen1 * 8;
  data.push_back((UINT8)(keyBits1 >> 24));
  data.push_back((UINT8)(keyBits1 >> 16));
  data.push_back((UINT8)(keyBits1 >> 8));
  data.push_back((UINT8)(keyBits1));
  data.insert(data.end(), n1, n1 + keyLen1);
  data.insert(data.end(), e1, e1 + keyLen1);

  UINT32 keyBits2 = keyLen2 * 8;
  data.push_back((UINT8)(keyBits2 >> 24));
  data.push_back((UINT8)(keyBits2 >> 16));
  data.push_back((UINT8)(keyBits2 >> 8));
  data.push_back((UINT8)(keyBits2));
  data.insert(data.end(), n2, n2 + keyLen2);
  data.insert(data.end(), e2, e2 + keyLen2);

  return shaHash(data, useSha256);
}

// === Random ===

std::vector<UINT8> Ra2CryptoProvider::generateRandom(int sizeBytes)
{
  std::vector<UINT8> buf(sizeBytes);
  if (!CryptGenRandom(m_hProv, sizeBytes, buf.data())) {
    return std::vector<UINT8>();
  }
  return buf;
}

// === Key Blob File I/O ===

bool Ra2CryptoProvider::loadKeyBlob(std::vector<BYTE> *blob)
{
  StringStorage folder;
  if (!Environment::getCurrentModuleFolderPath(&folder)) {
    return false;
  }
  StringStorage path;
  path.format(_T("%s\\ra2-key.dat"), folder.getString());

  FILE *f = _tfopen(path.getString(), _T("rb"));
  if (!f) return false;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size <= 0 || size > 32768) {
    fclose(f);
    return false;
  }

  blob->resize(size);
  size_t read = fread(blob->data(), 1, size, f);
  fclose(f);
  return read == (size_t)size;
}

bool Ra2CryptoProvider::saveKeyBlob(const std::vector<BYTE> &blob)
{
  StringStorage folder;
  if (!Environment::getCurrentModuleFolderPath(&folder)) {
    return false;
  }
  StringStorage path;
  path.format(_T("%s\\ra2-key.dat"), folder.getString());

  FILE *f = _tfopen(path.getString(), _T("wb"));
  if (!f) return false;

  size_t written = fwrite(blob.data(), 1, blob.size(), f);
  fclose(f);
  return written == blob.size();
}

// === Self-Test ===

bool Ra2CryptoProvider::selfTest()
{
  LogMsg("=== RSA Self-Test ===\n");

  // Check m_hServerKey BEFORE generating temp key
  {
    DWORD preCheck = 0;
    if (CryptExportKey(m_hServerKey, 0, PRIVATEKEYBLOB, 0, NULL, &preCheck)) {
      char buf[128];
      sprintf_s(buf, "selfTest: m_hServerKey HAS private BEFORE hTempKey gen (size=%u)\n", preCheck);
      LogMsg(buf);
    } else {
      LogLastError("selfTest: m_hServerKey NO private BEFORE hTempKey gen");
    }
  }

  // 1. Generate temporary RSA key pair
  HCRYPTKEY hTempKey = 0;
  if (!CryptGenKey(m_hProv, CALG_RSA_KEYX, 2048 << 16 | CRYPT_EXPORTABLE, &hTempKey)) {
    LogLastError("CryptGenKey (temp)");
    return false;
  }

  // Check m_hServerKey AFTER generating temp key
  {
    DWORD postCheck = 0;
    if (CryptExportKey(m_hServerKey, 0, PRIVATEKEYBLOB, 0, NULL, &postCheck)) {
      char buf[128];
      sprintf_s(buf, "selfTest: m_hServerKey HAS private AFTER hTempKey gen (size=%u)\n", postCheck);
      LogMsg(buf);
    } else {
      LogLastError("selfTest: m_hServerKey NO private AFTER hTempKey gen");
    }
  }

  // 2. Export PUBLICKEYBLOB
  DWORD blobSize = 0;
  CryptExportKey(hTempKey, 0, PUBLICKEYBLOB, 0, NULL, &blobSize);
  std::vector<BYTE> pubBlob(blobSize);
  if (!CryptExportKey(hTempKey, 0, PUBLICKEYBLOB, 0, pubBlob.data(), &blobSize)) {
    LogLastError("CryptExportKey (temp)");
    CryptDestroyKey(hTempKey);
    return false;
  }

  // 3. Parse N and E from the blob (same as getPublicKey)
  BLOBHEADER *hdr = (BLOBHEADER*)pubBlob.data();
  RSAPUBKEY *rsaPub = (RSAPUBKEY*)(hdr + 1);
  BYTE *modulusLE = (BYTE*)(rsaPub + 1);
  DWORD modSize = rsaPub->bitlen / 8;

  // N: LE -> BE for wire
  std::vector<UINT8> nBe(modSize);
  for (DWORD i = 0; i < modSize; i++) nBe[i] = modulusLE[modSize - 1 - i];

  // 4. Now reconstruct PUBLICKEYBLOB from N and E (same as importClientPublicKey)
  DWORD rebuiltSize = sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) + modSize;
  std::vector<BYTE> rebuilt(rebuiltSize);
  BLOBHEADER *reHdr = (BLOBHEADER*)rebuilt.data();
  reHdr->bType = PUBLICKEYBLOB;
  reHdr->bVersion = CUR_BLOB_VERSION;
  reHdr->reserved = 0;
  reHdr->aiKeyAlg = CALG_RSA_KEYX;

  RSAPUBKEY *reRsa = (RSAPUBKEY*)(reHdr + 1);
  reRsa->magic = 0x31415352;
  reRsa->bitlen = modSize * 8;
  reRsa->pubexp = rsaPub->pubexp;

  BYTE *reMod = (BYTE*)(reRsa + 1);
  for (DWORD i = 0; i < modSize; i++) reMod[i] = nBe[modSize - 1 - i];

  // Compare original and rebuilt blobs
  bool blobMatch = (pubBlob.size() == rebuilt.size() &&
                    memcmp(pubBlob.data(), rebuilt.data(), pubBlob.size()) == 0);
  char msg[256];
  sprintf_s(msg, "Blob match: %d (size orig=%u rebuilt=%u)\n", blobMatch, (UINT32)pubBlob.size(), (UINT32)rebuilt.size());
  LogMsg(msg);
  if (!blobMatch) {
    LogHex("Original pub blob", pubBlob.data(), (int)pubBlob.size());
    LogHex("Rebuilt pub blob", rebuilt.data(), (int)rebuilt.size());
  }

  // 5. Import the rebuilt PUBLICKEYBLOB
  HCRYPTKEY hImported = 0;
  if (!CryptImportKey(m_hProv, rebuilt.data(), (DWORD)rebuilt.size(), 0, 0, &hImported)) {
    LogLastError("CryptImportKey (rebuilt)");
    CryptDestroyKey(hTempKey);
    return false;
  }

  // 6. Encrypt test data with imported public key
  std::vector<UINT8> testPlain(16);
  CryptGenRandom(m_hProv, 16, testPlain.data());
  std::vector<UINT8> cipherBuf(testPlain.begin(), testPlain.end());
  cipherBuf.resize(modSize);
  DWORD cipherLen = 16;
  if (!CryptEncrypt(hImported, 0, TRUE, 0, cipherBuf.data(), &cipherLen, (DWORD)cipherBuf.size())) {
    LogLastError("CryptEncrypt (self-test)");
    CryptDestroyKey(hImported);
    CryptDestroyKey(hTempKey);
    return false;
  }
  cipherBuf.resize(cipherLen);

  // 7. Decrypt with original private key
  std::vector<UINT8> decBuf(cipherBuf.begin(), cipherBuf.end());
  DWORD decLen = cipherLen;
  if (!CryptDecrypt(hTempKey, 0, TRUE, 0, decBuf.data(), &decLen)) {
    LogLastError("CryptDecrypt (self-test)");
    CryptDestroyKey(hImported);
    CryptDestroyKey(hTempKey);
    return false;
  }
  decBuf.resize(decLen);

  bool match = (testPlain.size() == decBuf.size() &&
                memcmp(testPlain.data(), decBuf.data(), testPlain.size()) == 0);
  sprintf_s(msg, "Self-test encrypt/decrypt: %s\n", match ? "PASS" : "FAIL");
  LogMsg(msg);
  if (!match) {
    LogHex("Original", testPlain.data(), (int)testPlain.size());
    LogHex("Decrypted", decBuf.data(), (int)decBuf.size());
  }

  // Diagnostic: test m_hServerKey too
  {
    char buf[256];
    sprintf_s(buf, "selfTest: m_hServerKey=0x%p m_hProv=0x%p\n",
             (void*)m_hServerKey, (void*)m_hProv);
    LogMsg(buf);

    // Check if m_hServerKey can export PRIVATEKEYBLOB (proves it has private key)
    DWORD privSize = 0;
    if (CryptExportKey(m_hServerKey, 0, PRIVATEKEYBLOB, 0, NULL, &privSize)) {
      sprintf_s(buf, "selfTest: m_hServerKey HAS private key (priv blob size=%u)\n", privSize);
      LogMsg(buf);

      // Test decrypt capability of m_hServerKey
      DWORD pubSize = 0;
      CryptExportKey(m_hServerKey, 0, PUBLICKEYBLOB, 0, NULL, &pubSize);
      std::vector<BYTE> pubBlob2(pubSize);
      if (CryptExportKey(m_hServerKey, 0, PUBLICKEYBLOB, 0, pubBlob2.data(), &pubSize)) {
        HCRYPTKEY hPub2 = 0;
        if (CryptImportKey(m_hProv, pubBlob2.data(), pubSize, 0, 0, &hPub2)) {
          std::vector<UINT8> tst(16, 0x66);
          tst.resize(m_keyLength);
          DWORD el = 16;
          if (CryptEncrypt(hPub2, 0, TRUE, 0, tst.data(), &el, m_keyLength)) {
            tst.resize(el);
            DWORD dl = el;
            if (CryptDecrypt(m_hServerKey, 0, TRUE, 0, tst.data(), &dl)) {
              LogMsg("selfTest: m_hServerKey decrypt OK!\n");
            } else {
              LogLastError("selfTest: m_hServerKey decrypt");
            }
          } else {
            LogLastError("selfTest: encrypt with own pub");
          }
          CryptDestroyKey(hPub2);
        }
      }
    } else {
      LogLastError("selfTest: m_hServerKey NO private key");
    }
  }

  CryptDestroyKey(hImported);
  CryptDestroyKey(hTempKey);
  return match;
}

// === Helpers ===

void Ra2CryptoProvider::destroyClientKey()
{
  if (m_hClientKey) {
    CryptDestroyKey(m_hClientKey);
    m_hClientKey = 0;
  }
}
