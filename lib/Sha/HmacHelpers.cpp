#include <inttypes.h>
#include <sha1.h>

String bin2hex(const uint8_t* bin, const int length) {
  String hex = "";
  
  for (int i = 0; i < length; i++) {
    if (bin[i] < 16) {
      hex += "0";
    }
    hex += String(bin[i], HEX);
  }
  
  return hex;
}

String hmacDigest(String key, String message) {
  Sha1.initHmac((uint8_t*)key.c_str(), key.length());
  Sha1.print(message);
  return bin2hex(Sha1.resultHmac(), HASH_LENGTH);
}

String requestSignature(String key, String path, String body, time_t timestamp) {
  String payload = path + body + String(timestamp);
  return hmacDigest(key, payload);
}