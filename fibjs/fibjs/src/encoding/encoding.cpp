/*
 * encoding.cpp
 *
 *  Created on: Apr 10, 2012
 *      Author: lion
 */

#include "object.h"
#include "encoding.h"
#include "encoding_iconv.h"
#include "Url.h"
#include "libbase58.h"
#include <math.h>

namespace fibjs {

DECLARE_MODULE(encoding);
DECLARE_MODULE(base32);
DECLARE_MODULE(base58);
DECLARE_MODULE(base64);
DECLARE_MODULE(hex);
DECLARE_MODULE(multibase);

static void hexEncode(exlib::string data, bool upper, exlib::string& retVal)
{
    const char* HexChar = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t i;
    size_t sz = data.length();

    retVal.resize(sz * 2);
    char* _retVal = retVal.c_buffer();
    const char* _data = data.c_str();

    for (i = 0; i < sz; i++) {
        unsigned char ch = (unsigned char)_data[i];

        _retVal[i * 2] = HexChar[ch >> 4];
        _retVal[i * 2 + 1] = HexChar[ch & 0xf];
    }
}

result_t hex_base::encode(Buffer_base* data, exlib::string& retVal)
{
    exlib::string strData;
    data->toString(strData);
    hexEncode(strData, false, retVal);
    return 0;
}

static void hexDecode(const char* _data, int32_t len, obj_ptr<Buffer_base>& retVal)
{
    int32_t pos;
    const char* end = _data + len;
    exlib::string strBuf;
    unsigned char ch1, ch2;

    strBuf.resize(len / 2);
    char* _strBuf = strBuf.c_buffer();

    pos = 0;
    while ((_data < end - 1) && (ch1 = (unsigned char)*_data++)) {
        if (qisxdigit(ch1))
            ch1 = qhex(ch1);
        else
            continue;

        ch2 = *_data++;
        if (ch2 == 0)
            break;

        if (qisxdigit(ch2))
            ch2 = qhex(ch2);
        else {
            ch2 = ch1;
            ch1 = 0;
        }

        _strBuf[pos++] = (ch1 << 4) + ch2;
    }

    strBuf.resize(pos);
    retVal = new Buffer(strBuf);
}

result_t hex_base::decode(exlib::string data, obj_ptr<Buffer_base>& retVal)
{
    hexDecode(data.c_str(), (int32_t)data.length(), retVal);
    return 0;
}

static void baseEncode(const char* pEncodingTable, size_t dwBits,
    const char* data, size_t sz, exlib::string& retVal, bool fill)
{
    size_t i, len = 0, bits = 0;
    size_t dwData = 0;
    size_t dwSize = 0;
    char bMask = 0xff >> (8 - dwBits);

    if (dwBits == 6)
        dwSize = (sz + 2) / 3 * 4;
    else if (dwBits == 5)
        dwSize = (sz + 4) / 5 * 8;

    retVal.resize(dwSize);
    char* _retVal = retVal.c_buffer();

    for (i = 0; i < sz; i++) {
        dwData <<= 8;
        dwData |= (unsigned char)data[i];
        bits += 8;

        while (bits >= dwBits) {
            _retVal[len++] = pEncodingTable[(dwData >> (bits - dwBits)) & bMask];
            bits -= dwBits;
        }
    }

    if (bits)
        _retVal[len++] = pEncodingTable[(dwData << (dwBits - bits)) & bMask];

    if (fill)
        while (len < dwSize)
            _retVal[len++] = '=';

    retVal.resize(len);
}

static void baseDecode(const char* pdecodeTable, size_t dwBits,
    const char* _baseString, size_t len, exlib::string& retVal)
{
    size_t nWritten = 0;
    const char* end = _baseString + len;

    retVal.resize(len * dwBits / 8);
    char* _retVal = retVal.c_buffer();

    size_t dwCurr = 0;
    size_t nBits = 0;
    unsigned char ch;

    while ((_baseString < end) && (ch = (unsigned char)*_baseString++)) {
        int32_t nCh = (ch > 0x20 && ch < 0x80) ? pdecodeTable[ch - 0x20] : -1;

        if (nCh != -1) {
            dwCurr <<= dwBits;
            dwCurr |= nCh;
            nBits += dwBits;

            while (nBits >= 8) {
                _retVal[nWritten++] = (char)(dwCurr >> (nBits - 8));
                nBits -= 8;
            }
        }
    }

    retVal.resize(nWritten);
}

static void baseDecode(const char* pdecodeTable, size_t dwBits,
    const char* _baseString, size_t len, obj_ptr<Buffer_base>& retVal)
{
    exlib::string strBuf;
    baseDecode(pdecodeTable, dwBits, _baseString, len, strBuf);
    retVal = new Buffer(strBuf);
}

static void base32Encode(const char* data, size_t sz, bool upper, bool padding, exlib::string& retVal)
{
    baseEncode(upper ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567" : "abcdefghijklmnopqrstuvwxyz234567",
        5, data, sz, retVal, padding);
}

result_t base32_base::encode(Buffer_base* data, exlib::string& retVal)
{
    exlib::string strData;
    data->toString(strData);
    base32Encode(strData.c_str(), strData.length(), false, false, retVal);
    return 0;
}

static void base32Decode(const char* data, size_t sz, obj_ptr<Buffer_base>& retVal)
{
    static const char decodeTable[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 2x  !"#$%&'()*+,-./   */
        14, 11, 26, 27, 28, 29, 30, 31, -1, 6, -1, -1, -1, -1, -1, -1, /* 3x 0123456789:;<=>?   */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, /* 4x @ABCDEFGHIJKLMNO   */
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 5X PQRSTUVWXYZ[\]^_   */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, /* 6x `abcdefghijklmno   */
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1 /* 7X pqrstuvwxyz{\}~DEL */
    };

    baseDecode(decodeTable, 5, data, sz, retVal);
}

result_t base32_base::decode(exlib::string data, obj_ptr<Buffer_base>& retVal)
{
    base32Decode(data.c_str(), data.length(), retVal);
    return 0;
}

static void base64Encode(const char* data, size_t sz, bool url, bool padding, exlib::string& retVal)
{
    if (url)
        baseEncode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
            6, data, sz, retVal, padding);
    else
        baseEncode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
            6, data, sz, retVal, padding);
}

void base64Encode(const char* data, size_t sz, bool url, exlib::string& retVal)
{
    base64Encode(data, sz, url, !url, retVal);
}

void base64Decode(const char* data, size_t sz, exlib::string& retVal)
{
    static const char decodeTable[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63, /* 2x  !"#$%&'()*+,-./   */
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 3x 0123456789:;<=>?   */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, /* 4x @ABCDEFGHIJKLMNO   */
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, /* 5X PQRSTUVWXYZ[\]^_   */
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 6x `abcdefghijklmno   */
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1 /* 7X pqrstuvwxyz{\}~DEL */
    };

    baseDecode(decodeTable, 6, data, sz, retVal);
}

static void base64Decode(const char* data, size_t sz, obj_ptr<Buffer_base>& retVal)
{
    exlib::string strBuf;
    base64Decode(data, sz, strBuf);
    retVal = new Buffer(strBuf);
}

result_t base64_base::encode(Buffer_base* data, bool url, exlib::string& retVal)
{
    exlib::string strData;
    data->toString(strData);
    base64Encode(strData.c_str(), strData.length(), url, !url, retVal);
    return 0;
}

result_t base64_base::decode(exlib::string data, obj_ptr<Buffer_base>& retVal)
{
    static const char decodeTable[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63, /* 2x  !"#$%&'()*+,-./   */
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 3x 0123456789:;<=>?   */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, /* 4x @ABCDEFGHIJKLMNO   */
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, /* 5X PQRSTUVWXYZ[\]^_   */
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 6x `abcdefghijklmno   */
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1 /* 7X pqrstuvwxyz{\}~DEL */
    };

    baseDecode(decodeTable, 6, data.c_str(), data.length(), retVal);
    return 0;
}

static void base58Encode(const char* data, size_t sz, exlib::string& retVal)
{
    size_t b58sz = (size_t)(sz * 8 / log2l(58) + 2);

    retVal.resize(b58sz);
    b58enc(retVal.c_buffer(), &b58sz, data, sz);
    retVal.resize(b58sz - 1);
}

result_t base58_base::encode(Buffer_base* data, exlib::string& retVal)
{
    exlib::string buffer;

    data->toString(buffer);
    base58Encode(buffer.c_str(), buffer.length(), retVal);

    return 0;
}

result_t base58_base::encode(Buffer_base* data, int32_t chk_ver, exlib::string& retVal)
{
    exlib::string buffer;

    data->toString(buffer);
    size_t b58sz = (size_t)((buffer.length() + 5) * 8 / log2l(58) + 2);

    retVal.resize(b58sz);
    b58check_enc(retVal.c_buffer(), &b58sz, chk_ver, buffer.c_str(), buffer.length());
    retVal.resize(b58sz - 1);

    return 0;
}

static result_t base58Decode(const char* data, size_t sz, obj_ptr<Buffer_base>& retVal)
{
    size_t binsz = (size_t)(sz * log2l(58) / 8 + 3);
    exlib::string buffer;

    buffer.resize(binsz);
    if (!b58tobin(buffer.c_buffer(), &binsz, data, sz))
        return CHECK_ERROR(Runtime::setError("base58: encode error."));
    if (binsz < buffer.length())
        buffer = buffer.substr(buffer.length() - binsz);

    retVal = new Buffer(buffer);

    return 0;
}

result_t base58_base::decode(exlib::string data, obj_ptr<Buffer_base>& retVal)
{
    return base58Decode(data.c_str(), data.length(), retVal);
}

result_t base58_base::decode(exlib::string data, int32_t chk_ver, obj_ptr<Buffer_base>& retVal)
{
    size_t binsz = (size_t)(data.length() * log2l(58) / 8 + 3);
    exlib::string buffer;

    buffer.resize(binsz);
    if (!b58tobin(buffer.c_buffer(), &binsz, data.c_str(), data.length()))
        return CHECK_ERROR(Runtime::setError("base58: decode error."));
    if (b58check(buffer.c_str() + buffer.length() - binsz, binsz, data.c_str(), data.length()) != chk_ver)
        return CHECK_ERROR(Runtime::setError("base58: check error."));

    if (binsz < buffer.length())
        buffer = buffer.substr(buffer.length() - binsz + 1, binsz - 5);

    retVal = new Buffer(buffer);

    return 0;
}

result_t commonEncode(exlib::string codec, exlib::string data, exlib::string& retVal)
{
    if ((codec == "utf8") || (codec == "utf-8") || (codec == "undefined")) {
        retVal = data;
    } else {
        if ((codec == "hex"))
            hexEncode(data, false, retVal);
        else if ((codec == "base32"))
            base32Encode(data.c_str(), data.length(), false, false, retVal);
        else if ((codec == "base58"))
            base58Encode(data.c_str(), data.length(), retVal);
        else if ((codec == "base64"))
            base64Encode(data.c_str(), data.length(), false, true, retVal);
        else if ((codec == "base64url"))
            base64Encode(data.c_str(), data.length(), true, false, retVal);
        else if ((codec == "ascii")) {
            size_t len, i;

            len = data.length();
            retVal.resize(len);

            char* _retVal = retVal.c_buffer();
            const char* _data = data.c_str();

            for (i = 0; i < len; i++)
                _retVal[i] = _data[i] & 0x7f;
        } else
            return encoding_iconv(codec).decode(data, retVal);
    }
    return 0;
}

result_t multibase_base::encode(Buffer_base* data, exlib::string codec, exlib::string& retVal)
{
    exlib::string strData;
    exlib::string strBuffer;

    data->toString(strData);

    if ((codec == "base16")) {
        hexEncode(strData, false, strBuffer);
        retVal.assign(1, 'f');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base16upper")) {
        hexEncode(strData, true, strBuffer);
        retVal.assign(1, 'F');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base32")) {
        base32Encode(strData.c_str(), strData.length(), false, false, strBuffer);
        retVal.assign(1, 'b');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base32upper")) {
        base32Encode(strData.c_str(), strData.length(), true, false, strBuffer);
        retVal.assign(1, 'B');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base32pad")) {
        base32Encode(strData.c_str(), strData.length(), false, true, strBuffer);
        retVal.assign(1, 'c');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base32padupper")) {
        base32Encode(strData.c_str(), strData.length(), true, true, strBuffer);
        retVal.assign(1, 'C');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base58btc")) {
        base58Encode(strData.c_str(), strData.length(), strBuffer);
        retVal.assign(1, 'z');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base64")) {
        base64Encode(strData.c_str(), strData.length(), false, false, strBuffer);
        retVal.assign(1, 'm');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base64pad")) {
        base64Encode(strData.c_str(), strData.length(), false, true, strBuffer);
        retVal.assign(1, 'M');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base64url")) {
        base64Encode(strData.c_str(), strData.length(), true, false, strBuffer);
        retVal.assign(1, 'u');
        retVal.append(strBuffer);
        return 0;
    } else if ((codec == "base64urlpad")) {
        base64Encode(strData.c_str(), strData.length(), true, true, strBuffer);
        retVal.assign(1, 'U');
        retVal.append(strBuffer);
        return 0;
    }

    return CHECK_ERROR(Runtime::setError("multibase: unknown codec."));
}

result_t multibase_base::decode(exlib::string data, obj_ptr<Buffer_base>& retVal)
{
    const char* p = data.c_str();
    size_t sz = data.length();
    char type = p[0];

    if (type == 'f' || type == 'F') {
        hexDecode(p + 1, (int32_t)sz - 1, retVal);
        return 0;
    } else if (type == 'b' || type == 'B' || type == 'c' || type == 'C') {
        base32Decode(p + 1, (int32_t)sz - 1, retVal);
        return 0;
    } else if (type == 'm' || type == 'M' || type == 'u' || type == 'U') {
        base64Decode(p + 1, (int32_t)sz - 1, retVal);
        return 0;
    } else if (type == 'z') {
        return base58Decode(p + 1, (int32_t)sz - 1, retVal);
    }

    return CHECK_ERROR(Runtime::setError("multibase: unknown codec."));
}

static const char* URITable = " ! #$ &'()*+,-./0123456789:; = ?@ABCDEFGHIJKLMNOPQRSTUVWXYZ    _ abcdefghijklmnopqrstuvwxyz   ~ ";
static const char* URIComponentTable = " !     '()*  -. 0123456789       ABCDEFGHIJKLMNOPQRSTUVWXYZ    _ abcdefghijklmnopqrstuvwxyz   ~ ";

result_t encoding_base::encodeURI(exlib::string url, exlib::string& retVal)
{
    Url::encodeURI(url, retVal, URITable);
    return 0;
}

result_t encoding_base::encodeURIComponent(exlib::string url, exlib::string& retVal)
{
    Url::encodeURI(url, retVal, URIComponentTable);
    return 0;
}

result_t encoding_base::decodeURI(exlib::string url, exlib::string& retVal)
{
    Url::decodeURI(url, retVal);
    return 0;
}
}
