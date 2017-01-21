#include <sha1.h>
#include <inttypes.h>

String hmacDigest(String key, String message);
String requestSignature(String key, String path, String body, time_t timestamp);