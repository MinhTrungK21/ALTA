#ifndef ACCOUNT_AUTH_H
#define ACCOUNT_AUTH_H

#include <Arduino.h>
#include <Preferences.h>
#include <TOTP.h>
#include <ctype.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <qrcode.h>
#include <sys/time.h>

constexpr uint8_t MAX_ACCOUNTS = 10;
constexpr size_t ACCOUNT_USERNAME_LENGTH = 33;
constexpr size_t ACCOUNT_SALT_LENGTH = 16;
constexpr size_t ACCOUNT_HASH_LENGTH = 32;
constexpr size_t ACCOUNT_TOTP_LENGTH = 20;
constexpr uint8_t ACCOUNT_STORAGE_VERSION = 1;
constexpr const char *ACCOUNT_STORAGE_NAMESPACE = "hub66_auth";

struct __attribute__((packed)) AccountInfo
{
  char username[ACCOUNT_USERNAME_LENGTH];
  uint8_t salt[ACCOUNT_SALT_LENGTH];
  uint8_t passwordHash[ACCOUNT_HASH_LENGTH];
  uint8_t totpSecret[ACCOUNT_TOTP_LENGTH];
  uint8_t totpEnabled;
};

static AccountInfo accounts[MAX_ACCOUNTS] = {};
static uint8_t accountCount = 0;
static String accountQrData;
static uint8_t accountQrSize = 0;

static bool validUsername(const char *value)
{
  const size_t length = value == nullptr ? 0 : strlen(value);
  if (length < 3 || length >= ACCOUNT_USERNAME_LENGTH)
  {
    return false;
  }
  for (size_t i = 0; i < length; ++i)
  {
    const char c = value[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.')
    {
      return false;
    }
  }
  return true;
}

static int findAccount(const char *username)
{
  for (uint8_t i = 0; username != nullptr && i < accountCount; ++i)
  {
    if (strcasecmp(accounts[i].username, username) == 0)
    {
      return i;
    }
  }
  return -1;
}

static bool hashAccountPassword(const char *password, const uint8_t *salt,
                                uint8_t output[ACCOUNT_HASH_LENGTH])
{
  return password != nullptr &&
         mbedtls_pkcs5_pbkdf2_hmac_ext(
             MBEDTLS_MD_SHA256, reinterpret_cast<const uint8_t *>(password),
             strlen(password), salt, ACCOUNT_SALT_LENGTH, 12000,
             ACCOUNT_HASH_LENGTH, output) == 0;
}

static bool setAccountPassword(AccountInfo &account, const char *password)
{
  if (password == nullptr || strlen(password) < 6)
  {
    return false;
  }
  esp_fill_random(account.salt, sizeof(account.salt));
  return hashAccountPassword(password, account.salt, account.passwordHash);
}

static bool checkAccountPassword(const AccountInfo &account, const char *password)
{
  uint8_t hash[ACCOUNT_HASH_LENGTH];
  if (!hashAccountPassword(password, account.salt, hash))
  {
    return false;
  }
  uint8_t difference = 0;
  for (size_t i = 0; i < sizeof(hash); ++i)
  {
    difference |= hash[i] ^ account.passwordHash[i];
  }
  return difference == 0;
}

static bool saveAccounts()
{
  Preferences storage;
  if (!storage.begin(ACCOUNT_STORAGE_NAMESPACE, false, PERSISTENT_STORAGE_PARTITION))
  {
    return false;
  }
  const size_t size = accountCount * sizeof(AccountInfo);
  const bool saved = storage.putBytes("items", accounts, size) == size &&
                     storage.putUChar("count", accountCount) == sizeof(uint8_t) &&
                     storage.putUChar("version", ACCOUNT_STORAGE_VERSION) == sizeof(uint8_t);
  storage.end();
  return saved;
}

static void createDefaultAccount()
{
  memset(accounts, 0, sizeof(accounts));
  accountCount = 1;
  snprintf(accounts[0].username, sizeof(accounts[0].username), "admin");
  setAccountPassword(accounts[0], "admin123");
  saveAccounts();
  Serial.println("Created default account: admin");
}

static void loadAccounts()
{
  Preferences storage;
  if (!storage.begin(ACCOUNT_STORAGE_NAMESPACE, true, PERSISTENT_STORAGE_PARTITION))
  {
    createDefaultAccount();
    return;
  }
  const uint8_t version = storage.getUChar("version", 0);
  const uint8_t count = storage.getUChar("count", 0);
  const size_t size = storage.getBytesLength("items");
  const bool valid = version == ACCOUNT_STORAGE_VERSION && count > 0 &&
                     count <= MAX_ACCOUNTS && size == count * sizeof(AccountInfo) &&
                     storage.getBytes("items", accounts, size) == size;
  storage.end();
  if (!valid)
  {
    createDefaultAccount();
    return;
  }
  accountCount = count;
  for (uint8_t i = 0; i < accountCount; ++i)
  {
    accounts[i].username[ACCOUNT_USERNAME_LENGTH - 1] = '\0';
    if (!validUsername(accounts[i].username))
    {
      createDefaultAccount();
      return;
    }
  }
  Serial.printf("Loaded %u account(s) from NVS\n", accountCount);
}

static bool createAccount(const char *username, const char *password)
{
  if (accountCount >= MAX_ACCOUNTS || !validUsername(username) ||
      password == nullptr || strlen(password) < 6 || findAccount(username) >= 0)
  {
    return false;
  }
  AccountInfo &account = accounts[accountCount];
  account = {};
  snprintf(account.username, sizeof(account.username), "%s", username);
  if (!setAccountPassword(account, password))
  {
    account = {};
    return false;
  }
  ++accountCount;
  if (!saveAccounts())
  {
    accounts[--accountCount] = {};
    return false;
  }
  return true;
}

static bool changeAccountPassword(const char *username, const char *password)
{
  const int index = findAccount(username);
  if (index < 0 || password == nullptr || strlen(password) < 6)
  {
    return false;
  }
  const AccountInfo backup = accounts[index];
  if (!setAccountPassword(accounts[index], password) || !saveAccounts())
  {
    accounts[index] = backup;
    return false;
  }
  return true;
}

static bool deleteAccount(const char *username)
{
  const int index = findAccount(username);
  if (index < 0 || accountCount <= 1)
  {
    return false;
  }
  AccountInfo backup[MAX_ACCOUNTS];
  memcpy(backup, accounts, sizeof(accounts));
  for (uint8_t i = index; i + 1 < accountCount; ++i)
  {
    accounts[i] = accounts[i + 1];
  }
  accounts[--accountCount] = {};
  if (!saveAccounts())
  {
    memcpy(accounts, backup, sizeof(accounts));
    ++accountCount;
    return false;
  }
  return true;
}

static uint32_t accountEpoch(uint32_t browserEpoch)
{
  time_t now = time(nullptr);
  if (browserEpoch >= 1700000000 &&
      (now < 1700000000 || labs(static_cast<long>(browserEpoch - now)) > 60))
  {
    timeval value = {static_cast<time_t>(browserEpoch), 0};
    settimeofday(&value, nullptr);
    now = time(nullptr);
  }
  return static_cast<uint32_t>(now);
}

static bool checkTotp(const uint8_t secret[ACCOUNT_TOTP_LENGTH], const char *code,
                      uint32_t epoch)
{
  if (code == nullptr || strlen(code) != 6 || epoch < 1700000000)
  {
    return false;
  }
  for (uint8_t i = 0; i < 6; ++i)
  {
    if (!isdigit(static_cast<unsigned char>(code[i])))
    {
      return false;
    }
  }
  TOTP totp(const_cast<uint8_t *>(secret), ACCOUNT_TOTP_LENGTH);
  const long step = static_cast<long>(epoch / 30);
  for (int8_t window = -1; window <= 1; ++window)
  {
    if (strcmp(totp.getCodeFromSteps(step + window), code) == 0)
    {
      return true;
    }
  }
  return false;
}

static String base32Secret(const uint8_t *data, size_t length)
{
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  String output;
  uint32_t buffer = 0;
  uint8_t bits = 0;
  for (size_t i = 0; i < length; ++i)
  {
    buffer = (buffer << 8) | data[i];
    bits += 8;
    while (bits >= 5)
    {
      bits -= 5;
      output += alphabet[(buffer >> bits) & 31];
    }
  }
  if (bits)
  {
    output += alphabet[(buffer << (5 - bits)) & 31];
  }
  return output;
}

static void captureAccountQr(esp_qrcode_handle_t qr)
{
  static const char hex[] = "0123456789ABCDEF";
  accountQrSize = esp_qrcode_get_size(qr);
  accountQrData = "";
  accountQrData.reserve((accountQrSize * accountQrSize + 3) / 4);
  for (uint8_t y = 0; y < accountQrSize; ++y)
  {
    for (uint8_t x = 0; x < accountQrSize; x += 4)
    {
      uint8_t nibble = 0;
      for (uint8_t bit = 0; bit < 4; ++bit)
      {
        nibble = (nibble << 1) |
                 (x + bit < accountQrSize && esp_qrcode_get_module(qr, x + bit, y));
      }
      accountQrData += hex[nibble];
    }
  }
}

static bool makeAccountQr(const char *username, const uint8_t *secret)
{
  const String uri = "otpauth://totp/HUB66S:" + String(username) +
                     "?secret=" + base32Secret(secret, ACCOUNT_TOTP_LENGTH) +
                     "&issuer=HUB66S&digits=6&period=30";
  esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
  config.display_func = captureAccountQr;
  config.max_qrcode_version = 8;
  return esp_qrcode_generate(&config, uri.c_str()) == ESP_OK;
}

static bool enableAccountTotp(const char *username,
                              const uint8_t secret[ACCOUNT_TOTP_LENGTH])
{
  const int index = findAccount(username);
  if (index < 0)
  {
    return false;
  }
  const AccountInfo backup = accounts[index];
  memcpy(accounts[index].totpSecret, secret, ACCOUNT_TOTP_LENGTH);
  accounts[index].totpEnabled = 1;
  if (!saveAccounts())
  {
    accounts[index] = backup;
    return false;
  }
  return true;
}

static bool disableAccountTotp(const char *username)
{
  const int index = findAccount(username);
  if (index < 0)
  {
    return false;
  }
  const AccountInfo backup = accounts[index];
  memset(accounts[index].totpSecret, 0, ACCOUNT_TOTP_LENGTH);
  accounts[index].totpEnabled = 0;
  if (!saveAccounts())
  {
    accounts[index] = backup;
    return false;
  }
  return true;
}

#endif // ACCOUNT_AUTH_H
